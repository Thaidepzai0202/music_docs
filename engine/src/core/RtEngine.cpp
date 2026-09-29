#include "core/RtEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace le::core {

RtEngine::RtEngine(CommandQueue& commands, RtToNrtQueue& toNrt, StatePublisher& publisher)
    : commands_(commands), toNrt_(toNrt), publisher_(publisher) {
    clock_.init();
}

void RtEngine::setRecordBuffer(float* data, int capacityFrames) noexcept {
    recordBuf_ = data;
    recordCapacity_ = capacityFrames;
}

void RtEngine::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000.0;
    maxBlock_ = std::max(1, maxBlockSize);
    scratch_ = std::make_unique<float[]>((std::size_t) maxBlock_ * 2);
    cpu_.prepare(sampleRate_, clock_);
    xruns_.prepare(sampleRate_);
    spike_.prepare(sampleRate_, recordBuf_, recordCapacity_);
    load_.prepare(sampleRate_, maxBlock_);
    // Probe chỉ cấp phát lại khi SR đổi: worker có thể đang analyze() buffer của lượt trước.
    if (std::fabs(probeRate_ - sampleRate_) > 0.5) {
        probe_.prepare(sampleRate_, maxBlock_);
        probeRate_ = sampleRate_;
    }
    peakDecay_ = (float) std::exp(-1.0 / (0.3 * sampleRate_));   // về ~37% sau 300ms
    inputPeak_ = masterPeak_[0] = masterPeak_[1] = 0.0f;
    lastBlock_ = 0;
}

// Vòng lặp 1 block (04 §1). Các bước chưa có được ghi mã task ở chỗ sẽ nối vào:
//   adoptPendingSnapshot()            P1-06   nhận GraphSnapshot mới, thu hồi bản cũ qua rtToNrt
//   drainRtCommands()                 P1-04   (hiện: lệnh spike + LoadGenerator)
//   drainMidiInput(ctx)               P4
//   transport.beginBlock(n, ctx)      P1-08
//   for seg in splitter.split(...)    P1-09   scheduler.applyDue (P1-15), tracks[t].render (P1-12/13), metronome (P1-10)
//   mixer.sumToMaster(out)            P1-13/14
//   recorder.capture(in)              P1-19
//   meters.update / publisher.publish (P1-07: đủ trường LeState)
void RtEngine::process(const float* const* in, int numIn, float* const* out, int numOut, int numFrames,
                       const io::CallbackContext& ctx) noexcept [[clang::nonblocking]] {
    cpu_.begin();

    // Xrun chỉ được đếm ở đây; event LE_EVT_XRUN do timer 30Hz phía main phát khi thấy xrunCount tăng.
    const std::uint64_t hostNs = ctx.hostTimeNs != 0 ? ctx.hostTimeNs : clock_.nowNs();
    xruns_.onCallback(hostNs, numFrames);

    drainCommands();

    const float* in0 = (numIn > 0 && in != nullptr) ? in[0] : nullptr;

    // Device có thể gửi block lớn hơn maxBlock_ (hiếm, iOS khi khoá màn hình) → chia nhỏ.
    for (int done = 0; done < numFrames;) {
        const int n = std::min(maxBlock_, numFrames - done);
        float* L = numOut > 0 ? out[0] + done : scratch_.get();
        float* R = numOut > 1 ? out[1] + done : scratch_.get() + maxBlock_;
        processChunk(in0 != nullptr ? in0 + done : nullptr, L, R, n);
        done += n;
    }
    for (int ch = 2; ch < numOut; ++ch) std::memset(out[ch], 0, sizeof(float) * (std::size_t) numFrames);

    if (spike_.consumeRecordingFinished())
        pushEvent(LE_EVT_RECORDING_FINISHED, -1, -1, (double) spike_.recordedFrames());

    lastBlock_ = numFrames;
    publishState(numFrames);
    cpu_.end(numFrames);
}

void RtEngine::processChunk(const float* in0, float* outL, float* outR, int numFrames) noexcept
    [[clang::nonblocking]] {
    std::memset(outL, 0, sizeof(float) * (std::size_t) numFrames);   // JUCE không bảo đảm buffer ra đã sạch
    std::memset(outR, 0, sizeof(float) * (std::size_t) numFrames);

    spike_.process(in0, outL, outR, numFrames);
    float* const chans[2] = {outL, outR};
    load_.processRt(chans, 2, numFrames);            // cộng dồn
    probe_.processRt(in0, chans, 2, numFrames);      // khi đang đo: GHI ĐÈ output bằng chirp (phải đứng cuối)

    const float d = peakDecay_;
    float ip = inputPeak_, l = masterPeak_[0], r = masterPeak_[1];
    for (int i = 0; i < numFrames; ++i) {
        ip = std::max(in0 != nullptr ? std::fabs(in0[i]) : 0.0f, ip * d);
        l = std::max(std::fabs(outL[i]), l * d);
        r = std::max(std::fabs(outR[i]), r * d);
    }
    inputPeak_ = ip;
    masterPeak_[0] = l;
    masterPeak_[1] = r;
}

void RtEngine::drainCommands() noexcept [[clang::nonblocking]] {
    for (int i = 0; i < kMaxCommandsPerBlock; ++i) {
        const LeCommand* c = commands_.front();
        if (c == nullptr) break;
        if (c->type == LE_CMD_SPIKE_LOAD_VOICES) {
            load_.setVoices(c->i0);   // [[clang::nonblocking]]: clamp + 1 atomic store
        } else {
            spike_.handleCommand(*c);
        }   // P1-04: dispatch lệnh transport/clip/track
        // pop() chỉ có assert (bản debug) kiểm tra đã gọi front() trước → không cấp phát, không lock.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfunction-effects"
        commands_.pop();
#pragma clang diagnostic pop
    }
}

void RtEngine::pushEvent(std::int32_t type, std::int32_t a, std::int32_t b, double value) noexcept
    [[clang::nonblocking]] {
    RtMessage m;
    m.kind = RtMessage::Event;
    m.type = type;
    m.a = a;
    m.b = b;
    m.value = value;
    if (!toNrt_.try_push(m)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);   // queue đầy: bỏ, không chờ
}

void RtEngine::publishState(int numFrames) noexcept [[clang::nonblocking]] {
    LeState s;
    std::memset(&s, 0, sizeof(s));
    s.publishCounter = ++publishCounter_;
    s.bpm = 120.0;
    s.sampleRate = sampleRate_;
    s.bufferSize = numFrames;
    s.beatsPerBar = 4;
    s.quantize = LE_Q_1_BAR;
    s.latencyRoundTripSamples = latencyRoundTrip_.load(std::memory_order_relaxed);
    s.cpuLoad = cpu_.average();
    s.cpuPeak = cpu_.peak();
    const auto dev = (std::uint32_t) std::max(0, deviceXruns_.load(std::memory_order_relaxed));
    s.xrunCount = std::max(xruns_.count(), dev);   // cùng 1 xrun thường được cả hai cách đếm → lấy max
    s.activeVoices = load_.activeVoices();
    s.anyRecording = spike_.isRecording() ? 1 : 0;
    s.inputPeak = inputPeak_;
    s.masterPeak[0] = masterPeak_[0];
    s.masterPeak[1] = masterPeak_[1];
    for (int t = 0; t < LE_MAX_TRACKS; ++t) s.trackPlayingSlot[t] = -1;
    publisher_.publish(s);
}

} // namespace le::core
