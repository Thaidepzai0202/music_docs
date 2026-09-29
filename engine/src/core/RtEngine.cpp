#include "core/RtEngine.h"

#include "core/Denormals.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace le::core {

RtEngine::RtEngine(CommandQueue& commands, RtToNrtQueue& toNrt, MidiQueue& midiIn, StatePublisher& publisher)
    : commands_(commands), toNrt_(toNrt), midiIn_(midiIn), publisher_(publisher) {
    clock_.init();
    for (auto& t : tracks_) addGenerationUser(&t);   // voice / clip player giữ snapshot cũ tới khi fade xong
    for (auto& f : fx_) addGenerationUser(&f);       // FX cũ đang fade out (P3-12)
}

RtEngine::~RtEngine() {
    delete current_;
    for (int i = 0; i < retiringCount_; ++i) delete retiring_[i];
}

void RtEngine::setInitialSnapshot(std::unique_ptr<GraphSnapshot> s) noexcept {
    delete current_;
    current_ = s.release();
    if (current_ != nullptr) remap(*current_);
}

void RtEngine::addGenerationUser(GenerationUser* u) noexcept {
    if (u != nullptr && numUsers_ < kMaxGenerationUsers) users_[numUsers_++] = u;
}

bool RtEngine::generationInUse(std::uint32_t g) const noexcept [[clang::nonblocking]] {
    for (int i = 0; i < numUsers_; ++i)
        if (users_[i]->usesGeneration(g)) return true;
    return false;
}

// [RT] 03 §4.2 + luật generation (P1-06):
//  1) Snapshot cũ nào không còn ai dùng generation của nó → push Retire (main delete). Queue đầy thì giữ lại.
//  2) Có snapshot mới và còn chỗ trong retiring[] → đổi sang bản mới, bản đang dùng vào retiring[].
//     retiring[] đầy → không lấy bản mới ở block này (vẫn nằm trong exchange, main có thể thay bằng bản mới hơn)
//     và fade nhanh thứ đang giữ bản cũ nhất để giải phóng chỗ.
void RtEngine::retireUnused() noexcept [[clang::nonblocking]] {
    for (int i = 0; i < retiringCount_;) {
        if (generationInUse(retiring_[i]->generation)) {
            ++i;
            continue;
        }
        RtMessage m;
        m.kind = RtMessage::Retire;
        m.snapshot = retiring_[i];
        if (!toNrt_.try_push(m)) return;   // queue đầy: thử lại ở block sau
        for (int j = i; j + 1 < retiringCount_; ++j) retiring_[j] = retiring_[j + 1];
        retiring_[--retiringCount_] = nullptr;
    }
}

void RtEngine::adoptPendingSnapshot() noexcept [[clang::nonblocking]] {
    retireUnused();
    if (retiringCount_ >= kMaxRetiring) {
        for (int u = 0; u < numUsers_; ++u) users_[u]->fastReleaseGeneration(retiring_[0]->generation);
        return;
    }
    GraphSnapshot* next = exchange_.take();
    if (next == nullptr) return;
    if (current_ != nullptr) retiring_[retiringCount_++] = current_;   // KHÔNG delete trên RT
    current_ = next;
    remap(*current_);
    // Sau remap, thứ còn đọc bản cũ đều báo qua GenerationUser → bản không ai dùng trả về main ngay block này.
    retireUnused();
}

// [RT] Khớp trạng thái RT (clip đang phát, instrument…) sang snapshot mới. Không cấp phát.
void RtEngine::remap(const GraphSnapshot& s) noexcept [[clang::nonblocking]] {
    transport_.setTimeSignature(s.beatsPerBar, s.beatUnit);
    scheduler_.remap(s);
    for (int t = 0; t < LE_MAX_TRACKS; ++t) tracks_[t].remap(s, t, scheduler_);   // sau scheduler: biết slot đang phát
    for (int t = 0; t < LE_MAX_TRACKS; ++t) fx_[t].remap(s.tracks[t].fx, s.generation);
}

void RtEngine::setRecordBuffer(float* data, int capacityFrames) noexcept {
    recordBuf_ = data;
    recordCapacity_ = capacityFrames;
}

void RtEngine::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000.0;
    maxBlock_ = std::max(1, maxBlockSize);
    if (current_ == nullptr) setInitialSnapshot(std::make_unique<GraphSnapshot>());   // [main] dùng không qua Engine (test)
    scratch_ = std::make_unique<float[]>((std::size_t) maxBlock_ * 2);
    mixScratch_ = std::make_unique<float[]>((std::size_t) maxBlock_ * 2);
    for (auto& t : tracks_) t.prepare(sampleRate_, maxBlock_);
    for (auto& g : monitorGain_) g.snap(0.0f);
    for (auto& f : fx_) f.prepare(sampleRate_);
    masterEq_.prepare(sampleRate_);
    if (std::fabs(fxRate_ - sampleRate_) > 0.5) {
        // Sample rate đổi: Processor do main tạo ở SR cũ → prepare lại (giữ tham số, xoá state). Mọi instance RT có
        // thể chạm tới nằm trong current_, retiring_ hoặc bản đang chờ (EngineModel ⊆ bản build gần nhất).
        std::vector<dsp::Processor*> seen;
        auto visit = [&](const GraphSnapshot* s) {
            if (s == nullptr) return;
            for (const auto& tr : s->tracks)
                for (const auto& f : tr.fx) {
                    dsp::Processor* p = f.proc.get();
                    if (p == nullptr || std::find(seen.begin(), seen.end(), p) != seen.end()) continue;
                    seen.push_back(p);
                    p->prepare(sampleRate_, FxChain::kMaxFxBlock);
                }
        };
        visit(current_);
        for (int i = 0; i < retiringCount_; ++i) visit(retiring_[i]);
        visit(exchange_.peekPending());
        fxRate_ = sampleRate_;
    }
    mixer_.prepare(sampleRate_);
    recorder_.prepare(sampleRate_);
    inputMeter_.prepare(sampleRate_);
    cpu_.prepare(sampleRate_, clock_);
    transport_.prepare(sampleRate_);
    metronome_.prepare(sampleRate_);
    xruns_.prepare(sampleRate_);
    spike_.prepare(sampleRate_, recordBuf_, recordCapacity_);
    load_.prepare(sampleRate_, maxBlock_);
    // Probe chỉ cấp phát lại khi SR đổi: worker có thể đang analyze() buffer của lượt trước.
    if (std::fabs(probeRate_ - sampleRate_) > 0.5) {
        probe_.prepare(sampleRate_, maxBlock_);
        probeRate_ = sampleRate_;
    }
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
    const ScopedFlushDenormals ftz;   // R3: subnormal → 0 trong suốt block (khôi phục khi ra)
    cpu_.begin();

    // Xrun chỉ được đếm ở đây; event LE_EVT_XRUN do timer 30Hz phía main phát khi thấy xrunCount tăng.
    const std::uint64_t hostNs = ctx.hostTimeNs != 0 ? ctx.hostTimeNs : clock_.nowNs();
    xruns_.onCallback(hostNs, numFrames);

    for (auto& t : tracks_) t.beginBlock();   // nốt live của block này (NOTE_ON / MIDI input) có offset riêng
    adoptPendingSnapshot();
    drainCommands();
    drainMidiInput(hostNs, numFrames);
    blockStartBeat_ = transport_.beatNow();

    const float* in0 = (numIn > 0 && in != nullptr) ? in[0] : nullptr;
    const float* in1 = (numIn > 1 && in != nullptr) ? in[1] : nullptr;

    // Device có thể gửi block lớn hơn maxBlock_ (hiếm, iOS khi khoá màn hình) → chia nhỏ.
    for (int done = 0; done < numFrames;) {
        const int n = std::min(maxBlock_, numFrames - done);
        float* L = numOut > 0 ? out[0] + done : scratch_.get();
        float* R = numOut > 1 ? out[1] + done : scratch_.get() + maxBlock_;
        processChunk(in0 != nullptr ? in0 + done : nullptr, in1 != nullptr ? in1 + done : nullptr, L, R, n, done);
        done += n;
    }
    for (int ch = 2; ch < numOut; ++ch) std::memset(out[ch], 0, sizeof(float) * (std::size_t) numFrames);

    if (spike_.consumeRecordingFinished())
        pushEvent(LE_EVT_RECORDING_FINISHED, -1, -1, (double) spike_.recordedFrames());

    lastBlock_ = numFrames;
    publishState(numFrames);
    cpu_.end(numFrames);
}

void RtEngine::processChunk(const float* in0, const float* in1, float* outL, float* outR, int numFrames, int blockOffset) noexcept
    [[clang::nonblocking]] {
    std::memset(outL, 0, sizeof(float) * (std::size_t) numFrames);   // JUCE không bảo đảm buffer ra đã sạch
    std::memset(outR, 0, sizeof(float) * (std::size_t) numFrames);

    // 1) Chia segment tại ranh giới sự kiện (P1-09): quantize của lệnh chờ, kết thúc thu, điểm loop (P1-15).
    const std::int64_t chunkStart = transport_.samplePos();
    const double chunkBeat = transport_.beatNow();
    splitter_.begin(numFrames);
    scheduler_.addBoundaries(transport_, chunkStart, numFrames, splitter_);
    const int numSegs = splitter_.split(transport_, chunkStart, segs_);
    for (auto& t : tracks_) t.beginChunk(numFrames, blockOffset);
    const double spb = transport_.samplesPerBeat();
    for (int i = 0; i < numSegs; ++i) {
        const Segment& seg = segs_[i];
        scheduler_.applyDue(transport_, seg.startSample);   // queued → playing/stopped/recording đúng sample
        metronome_.setCountingIn(scheduler_.countingIn(transport_, seg.startSample));
        metronome_.setRecording(scheduler_.anyRecording());
        if (pedalTrack_ < 0) metronome_.render(transport_, seg, outL, outR);   // vòng đầu pedal: im
        for (int t = 0; t < LE_MAX_TRACKS; ++t) tracks_[t].renderSegment(*current_, t, scheduler_, recorder_, seg, spb);
    }
    // Thu âm (P1-19/20): sau khi scheduler đã chuyển trạng thái trong khối, trước khi transport tiến.
    bool liveInUse[LE_MAX_TRACKS], isMidi[LE_MAX_TRACKS];
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        liveInUse[t] = tracks_[t].usingLiveTake();
        isMidi[t] = current_->tracks[t].kind == TrackKind::Instrument;   // track instrument → take MIDI
    }
    recorder_.process(transport_, scheduler_, chunkStart, in0, in1, numFrames, latencyRoundTrip_.load(std::memory_order_relaxed),
                      liveInUse, isMidi, current_->projectEpoch, current_->cellEpoch, toNrt_);
    recorder_.processOverdub(transport_, scheduler_, *current_, chunkStart, in0, in1, numFrames,
                             latencyRoundTrip_.load(std::memory_order_relaxed), toNrt_);
    recorder_.processCapture(in0, in1, numFrames, toNrt_);   // P3-02
    // P1-30: lượt overdub MIDI kết thúc (tắt, dừng clip, TRANSPORT_STOP, launch clip khác) → main đóng nốt đang giữ,
    // trộn vào clip rồi phát RECORDING_FINISHED(track, slot, số nốt).
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        const int slot = scheduler_.playingSlot(t);
        const bool midiOd = slot >= 0 && scheduler_.soundState(t) == LE_CLIP_OVERDUBBING &&
                            current_->clips[t][slot].kind == ClipKind::Midi;
        const int now = midiOd ? slot : -1;
        if (midiOdSlot_[t] >= 0 && midiOdSlot_[t] != now) {
            const ClipSnapshot& c = current_->clips[t][midiOdSlot_[t]];
            RtMessage m;
            m.kind = RtMessage::MidiOverdubFinished;
            m.a = t;
            m.b = midiOdSlot_[t];
            m.value2 = c.lengthBeats;
            if (c.lengthBeats > 0.0) {   // cuối khối này (transport chưa advance)
                const double endBeat = transport_.beatNow() + (transport_.playing() ? (double) numFrames / transport_.samplesPerBeat() : 0.0);
                m.value = std::fmod(endBeat - midiOdLaunch_[t], c.lengthBeats);
                if (m.value < 0.0) m.value += c.lengthBeats;
            }
            if (!toNrt_.try_push(m)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);
        }
        midiOdSlot_[t] = now;
        if (midiOd) midiOdLaunch_[t] = scheduler_.launchBeat(t);
    }
    transport_.advance(numFrames);

    // 1b) Input monitoring (P1-23, 04 §5.5): On → luôn nghe; Auto → chỉ khi track được arm VÀ có tai nghe có dây /
    //     interface (tránh hú qua loa trong máy). Input vào bus của track → đi qua gain/pan/mute của mixer. Ramp 5 ms.
    const bool phones = headphones_.load(std::memory_order_relaxed);
    const int ramp = std::max(1, (int) (0.005 * sampleRate_));
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        const bool on = in0 != nullptr && (monitor_[t] == 2 || (monitor_[t] == 1 && armed_[t] && phones));
        LinearRamp& g = monitorGain_[t];
        if ((g.target > 0.5f) != on) g.set(on ? 1.0f : 0.0f, ramp);
        if (!on && g.current <= 0.0f && g.remaining == 0) continue;
        float* bl = tracks_[t].busLMutable();
        float* br = tracks_[t].busRMutable();
        for (int i = 0; i < numFrames; ++i) {
            const float k = g.next();
            const float l = in0 != nullptr ? in0[i] : 0.0f;
            bl[i] += k * l;
            br[i] += k * (in1 != nullptr ? in1[i] : l);
        }
    }

    // 1c) FX của từng track (P3-12), sau monitoring: input nghe qua FX của track như clip.
    dsp::ProcessContext fxCtx;
    fxCtx.numFrames = numFrames;
    fxCtx.sampleRate = sampleRate_;
    fxCtx.bpm = transport_.bpm();
    fxCtx.beat = chunkBeat;
    fxCtx.playing = transport_.playing();
    for (int t = 0; t < LE_MAX_TRACKS; ++t) fx_[t].process(tracks_[t].busLMutable(), tracks_[t].busRMutable(), numFrames, fxCtx);

    // 2) Mixer: bus từng track → master (gain, pan -3 dB, mute/solo ramp), rồi nguồn spike P0.
    float* sL = mixScratch_.get();
    float* sR = sL + maxBlock_;
    for (int t = 0; t < LE_MAX_TRACKS; ++t) mixer_.mixTrack(t, tracks_[t].busL(), tracks_[t].busR(), outL, outR, sL, sR, numFrames);
    spike_.process(in0, outL, outR, numFrames);
    float* const chans[2] = {outL, outR};
    load_.processRt(chans, 2, numFrames);            // cộng dồn

    // 3) Master gain → EQ3 (P3-15, bỏ qua khi phẳng) → limiter (-0.3 dBFS) → meter (P1-14).
    mixer_.applyMasterGain(outL, outR, numFrames);
    masterEq_.process(outL, outR, numFrames, fxCtx);
    mixer_.limitAndMeter(outL, outR, numFrames);
    recordJam(outL, outR, numFrames);                // P3-17: ghi đúng tiếng người chơi nghe (trước chirp đo latency)
    probe_.processRt(in0, chans, 2, numFrames);      // khi đang đo: GHI ĐÈ output bằng chirp (phải đứng cuối)
    if (in0 != nullptr) inputMeter_.push(in0, numFrames);
}

void RtEngine::drainCommands() noexcept [[clang::nonblocking]] {
    for (int i = 0; i < kMaxCommandsPerBlock; ++i) {
        const LeCommand* c = commands_.front();
        if (c == nullptr) break;
        handleCommand(*c);
        // pop() chỉ có assert (bản debug) kiểm tra đã gọi front() trước → không cấp phát, không lock.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfunction-effects"
        commands_.pop();
#pragma clang diagnostic pop
    }
}

// Lệnh đã được validate ở phía main (CommandValidation.h), RT không kiểm lại dải.
void RtEngine::handleCommand(const LeCommand& c) noexcept [[clang::nonblocking]] {
    switch (c.type) {
        case LE_CMD_TRANSPORT_PLAY: transport_.play(); return;
        case LE_CMD_TRANSPORT_STOP:
            scheduler_.transportStopping(transport_);   // dừng ngay mọi clip (fade ở clip player)
            transport_.stop();
            pedalTrack_ = -1;   // vòng đầu đang thu bị bỏ (luật TRANSPORT_STOP: Recording → bỏ take)
            return;
        // Clip & scene (P1-15/16). Transport đang dừng mà launch/record → Play tại beat 0, lệnh áp dụng ngay (04 §3.2).
        case LE_CMD_CLIP_LAUNCH:
            transport_.play();
            scheduler_.launch(transport_, c.track, c.slot);
            return;
        case LE_CMD_SCENE_LAUNCH:
            transport_.play();
            scheduler_.sceneLaunch(transport_, c.slot);
            return;
        case LE_CMD_CLIP_RECORD:
            // Pedal mode, chưa có tempo (04 §2.5): vòng đầu thu NGAY — không quantize, count-in, metronome. Transport
            // chạy "tạm" từ beat 0 = sample đầu của take (LeState báo chưa chạy) để Recorder dùng đúng đường cũ.
            if (pedalMode_.load(std::memory_order_acquire) && !pedalHasTempo_.load(std::memory_order_relaxed) &&
                pedalTrack_ < 0 && !transport_.playing()) {
                transport_.play();
                if (scheduler_.recordNow(transport_, c.track, c.slot)) pedalTrack_ = c.track;
                else transport_.stop();
                return;
            }
            transport_.play();
            scheduler_.record(transport_, c.track, c.slot, c.i0);
            return;
        case LE_CMD_CLIP_STOP: scheduler_.stop(transport_, c.track); return;
        case LE_CMD_STOP_ALL: scheduler_.stopAll(transport_); return;
        case LE_CMD_RECORD_STOP:
            if (c.track == pedalTrack_) finishFirstLoop(c.track);
            else if (pedalMode_.load(std::memory_order_acquire) && pedalHasTempo_.load(std::memory_order_relaxed))
                scheduler_.recordStopMultiple(transport_, c.track,   // vòng sau: bội số vòng đầu (04 §2.5)
                                              pedalLoopBeats_.load(std::memory_order_relaxed) > 0.0
                                                  ? pedalLoopBeats_.load(std::memory_order_relaxed)
                                                  : (double) transport_.beatsPerBar());
            else scheduler_.recordStop(transport_, c.track);
            return;
        case LE_CMD_OVERDUB_TOGGLE: overdubCommand(c.track, c.i0 != 0); return;   // i0 do main ghi: 1 bật / 0 tắt
        case LE_CMD_SET_COUNT_IN: scheduler_.setCountIn(c.i0); return;
        case LE_CMD_SET_BPM: transport_.setBpm(c.d0); return;
        case LE_CMD_SET_QUANTIZE: transport_.setQuantize(c.i0); return;
        case LE_CMD_METRONOME: metronome_.setMode(c.i0, c.f0); return;
        case LE_CMD_SELECT_TRACK: selectedTrack_ = c.track; return;
        // Track + master (P1-13)
        case LE_CMD_TRACK_GAIN: mixer_.setGainDb(c.track, c.f0); return;
        case LE_CMD_TRACK_PAN: mixer_.setPan(c.track, c.f0); return;
        case LE_CMD_TRACK_MUTE: mixer_.setMute(c.track, c.i0 != 0); return;
        case LE_CMD_TRACK_SOLO: mixer_.setSolo(c.track, c.i0 != 0); return;
        case LE_CMD_MASTER_GAIN: mixer_.setMasterGainDb(c.f0); return;
        // FX (P3-12/15). Track: d0 = instanceId do main ghi. Master (track -1): slot 0 = EQ3, slot 1 = limiter.
        case LE_CMD_FX_PARAM:
            if (c.track >= 0) fx_[c.track].setParam(c.slot, (std::uint32_t) c.d0, c.i0, c.f0);
            else if (c.slot == 0) masterEq_.setGainDb(c.i0, c.f0);
            else mixer_.setLimiterParam(c.i0, c.f0);
            return;
        case LE_CMD_FX_BYPASS:
            if (c.track >= 0) fx_[c.track].setBypass(c.slot, (std::uint32_t) c.d0, c.i0 != 0);
            else masterEq_.setBypass(c.i0 != 0);
            return;
        case LE_CMD_TRACK_ARM: armed_[c.track] = c.i0 != 0; return;   // buffer đã được main cấp phát trước khi push
        case LE_CMD_TRACK_MONITOR: monitor_[c.track] = c.i0; return;
        // Nốt (bàn phím / pad) → sampler của track, áp dụng ngay đầu block
        case LE_CMD_NOTE_ON: noteEvent(c.track, c.i0, c.f0, true, 0); return;
        case LE_CMD_NOTE_OFF: noteEvent(c.track, c.i0, 0.0f, false, 0); return;
        case LE_CMD_ALL_NOTES_OFF:
            for (int t = 0; t < LE_MAX_TRACKS; ++t) {
                if (c.track >= 0 && c.track != t) continue;
                tracks_[t].dropPendingLiveNotes();   // NOTE_ON gửi trước trong cùng block cũng bị huỷ
                tracks_[t].allNotesOff(false);
            }
            return;
        case LE_CMD_SPIKE_LOAD_VOICES: load_.setVoices(c.i0); return;   // [[clang::nonblocking]]: clamp + 1 atomic store
        default:
            if (!spike_.handleCommand(c)) ignoredCommands_.fetch_add(1, std::memory_order_relaxed);
    }
}

// [RT] Nốt từ bàn phím/pad (và MIDI input ở P4): phát qua sampler NGAY, đồng thời ghi lại nếu track đang thu MIDI
// (Recorder quyết định nốt nào nằm trong cửa sổ take) hoặc đang overdub một clip MIDI (P1-30).
double RtEngine::beatAtBlockOffset(int offset) const noexcept [[clang::nonblocking]] {
    return transport_.beatNow() + (transport_.playing() ? (double) offset / transport_.samplesPerBeat() : 0.0);
}

// [RT] Nốt (bàn phím / pad / MIDI input) tại frame `blockOffset` của block: sampler kêu ĐÚNG sample đó, đồng thời ghi
// lại nếu track đang thu MIDI (Recorder quyết định nốt nào nằm trong cửa sổ take) hoặc đang overdub một clip MIDI (P1-30).
void RtEngine::noteEvent(int track, int pitch, float velocity01, bool on, int blockOffset) noexcept [[clang::nonblocking]] {
    if (track < 0 || track >= LE_MAX_TRACKS) return;
    tracks_[track].addLiveNote(blockOffset, pitch, velocity01, on);
    const int vel = on ? std::clamp((int) std::lround(velocity01 * 127.0f), 1, 127) : 0;
    const double beat = beatAtBlockOffset(blockOffset);
    recorder_.addNote(track, beat, pitch, vel, on);

    const int slot = scheduler_.playingSlot(track);
    if (slot < 0 || scheduler_.cellState(track, slot) != LE_CLIP_OVERDUBBING) return;
    const ClipSnapshot& clip = current_->clips[track][slot];
    if (clip.kind != ClipKind::Midi || clip.lengthBeats <= 0.0) return;
    double pos = std::fmod(beat - scheduler_.launchBeat(track), clip.lengthBeats);
    if (pos < 0.0) pos += clip.lengthBeats;
    RtMessage m;
    m.kind = RtMessage::MidiOverdubNote;
    m.a = track;
    m.b = slot;
    m.value = pos;
    m.value2 = clip.lengthBeats;
    m.i0 = pitch;
    m.i1 = vel;
    if (!toNrt_.try_push(m)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);
}

// [RT] P1-22 + R1/R6. Bật: clip audio cần VÉ của main (bản copy cho đúng ô) và phát 1:1 (cùng SR, originalBpm = BPM);
// không đủ → không vào OVERDUBBING, trả vé, phát LE_EVT_ERROR(LE_ERR_OVERDUB_UNSUPPORTED, track, slot).
// Clip MIDI: overdub nốt (P1-30), không cần vé. Tắt: OVERDUBBING → PLAYING (Recorder ghi nốt phần bù L rồi trả vé).
void RtEngine::overdubCommand(int track, bool on) noexcept [[clang::nonblocking]] {
    const int slot = scheduler_.playingSlot(track);
    const bool overdubbing = scheduler_.soundState(track) == LE_CLIP_OVERDUBBING;
    if (!on) {
        if (overdubbing) scheduler_.overdubToggle(track);
        return;
    }
    Recorder::OverdubTicket* tk = recorder_.takeOverdubTicket(track);   // lấy luôn: vé không dùng thì trả ngay
    if (overdubbing || slot < 0 || scheduler_.soundState(track) != LE_CLIP_PLAYING) {   // đã bật / không có clip phát
        if (tk != nullptr) recorder_.returnTicket(track, *tk, toNrt_);
        return;
    }
    const ClipSnapshot& c = current_->clips[track][slot];
    if (c.kind == ClipKind::Midi) {
        if (tk != nullptr) recorder_.returnTicket(track, *tk, toNrt_);
        scheduler_.overdubToggle(track);
        return;
    }
    const bool oneToOne = c.kind == ClipKind::Audio && c.audio != nullptr &&
                          std::fabs(c.audio->sampleRate() - sampleRate_) < 0.5 &&
                          (c.originalBpm <= 0.0 || std::fabs(c.originalBpm - transport_.bpm()) < 1e-6);
    if (tk == nullptr || tk->slot != slot || !oneToOne || recorder_.overdubBusy(track)) {
        if (tk != nullptr) recorder_.returnTicket(track, *tk, toNrt_);
        pushEvent(LE_EVT_ERROR, LE_ERR_OVERDUB_UNSUPPORTED, track, (double) slot);
        return;
    }
    recorder_.armOverdub(track, *tk);
    scheduler_.overdubToggle(track);   // PLAYING → OVERDUBBING
}

void RtEngine::recordJam(const float* L, const float* R, int n) noexcept [[clang::nonblocking]] {
    auto ack = [this] {
        RtMessage m;
        m.kind = RtMessage::JamStopped;
        m.a = (std::int32_t) jam_->id;
        if (!toNrt_.try_push(m)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);   // main giữ ring (không crash)
        jam_ = nullptr;
    };
    if (JamRing* next = jamOffer_.exchange(nullptr, std::memory_order_acq_rel)) {
        if (jam_ != nullptr) ack();
        jam_ = next;
    }
    if (jam_ != nullptr && jamStop_.load(std::memory_order_acquire) == jam_->id) ack();
    if (jam_ == nullptr) return;
    const std::int64_t cap = jam_->capacity;
    const std::int64_t w = jam_->written.load(std::memory_order_relaxed);
    const std::int64_t space = cap - (w - jam_->read.load(std::memory_order_acquire));
    const int m = (int) std::clamp<std::int64_t>(space, 0, n);
    float* dl = jam_->L.get();
    float* dr = jam_->R.get();
    for (int i = 0; i < m; ++i) {
        const std::int64_t idx = (w + i) % cap;
        dl[idx] = L[i];
        dr[idx] = R[i];
    }
    jam_->written.store(w + m, std::memory_order_release);
    if (m < n) jam_->dropped.fetch_add((std::uint32_t) (n - m), std::memory_order_relaxed);
}

// [RT] Chốt vòng đầu (04 §2.5): T = sample từ đầu take tới lúc bấm (đầu block). nb = beatsPerBar·2^k sao cho
// bpm = 60·nb·sr/T ∈ [80, 160); T quá ngắn (bpm ≥ 160 ngay với 1 bar) → nb = beatsPerBar, bpm tối đa 300 (khi đó độ
// dài theo beat là T ở 300 BPM để vòng vẫn liền). Neo lại transport → beat hiện tại = nb, clip phát tiếp liền mạch
// (đường live-take: vòng đầu đọc thẳng buffer thu). Phát TEMPO_CHANGED(value = bpm, value2 = nb cho main).
void RtEngine::finishFirstLoop(int track) noexcept [[clang::nonblocking]] {
    pedalTrack_ = -1;
    const std::int64_t T = transport_.samplePos();
    if (T < (std::int64_t) (0.1 * sampleRate_)) {   // bấm thu rồi dừng ngay (< 100 ms): bỏ take, vẫn chưa có tempo
        scheduler_.transportStopping(transport_);
        transport_.stop();
        return;
    }
    const int bpb = transport_.beatsPerBar();
    double nb = bpb;
    double bpm = 60.0 * nb * sampleRate_ / (double) T;
    while (bpm < 80.0) {
        nb *= 2.0;
        bpm *= 2.0;
    }
    double beats = nb;
    if (bpm > Transport::kMaxBpm) beats = (double) T * Transport::kMaxBpm / (60.0 * sampleRate_);
    transport_.retimeFromLength(beats);
    scheduler_.recordStopExact(transport_, track);   // kết thúc đúng bây giờ = beat `beats`
    pedalHasTempo_.store(true, std::memory_order_relaxed);
    pedalLoopBeats_.store(beats, std::memory_order_relaxed);
    RtMessage m;
    m.kind = RtMessage::Event;
    m.type = LE_EVT_TEMPO_CHANGED;
    m.value = transport_.bpm();
    m.value2 = beats;
    if (!toNrt_.try_push(m)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);
}

void RtEngine::setMidiSources(MidiQueue* const* queues, int n) noexcept {
    numMidiSources_ = std::clamp(n, 0, kMaxMidiSources);
    for (int i = 0; i < numMidiSources_; ++i) midiSources_[i] = queues[i];
}

// [RT] P4-02 (04 §13): message của mọi nguồn → frame offset theo host time của block → sắp theo offset → định tuyến.
void RtEngine::drainMidiInput(std::uint64_t blockHostNs, int numFrames) noexcept [[clang::nonblocking]] {
    midiRouter_.setScheduleLatencyNs(midiLatencyNs_.load(std::memory_order_relaxed));
    midiEvents_.clear();
    midiRouter_.drain(midiIn_, (std::int64_t) blockHostNs, sampleRate_, numFrames, midiEvents_);
    for (int i = 0; i < numMidiSources_; ++i)
        midiRouter_.drain(*midiSources_[i], (std::int64_t) blockHostNs, sampleRate_, numFrames, midiEvents_);
    const int n = midiEvents_.size();
    if (n == 0) return;
    int order[midi::MidiInputEventList::kCapacity];
    for (int i = 0; i < n; ++i) {   // insertion sort ổn định (mỗi queue vốn đã theo thời gian)
        int j = i;
        while (j > 0 && midiEvents_[order[j - 1]].offset > midiEvents_[i].offset) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = i;
    }
    for (int i = 0; i < n; ++i) handleMidiEvent(midiEvents_[order[i]]);
}

// [RT] Learn (bắt message kế tiếp) → mapping (preset Launchpad / người dùng) → còn lại: nốt tới track đang chọn.
void RtEngine::handleMidiEvent(const midi::MidiInputEvent& e) noexcept [[clang::nonblocking]] {
    const midi::ParsedMidi pm = midi::parseMidi(e.status, e.data1, e.data2);
    using K = midi::ParsedMidi::Kind;
    midi::LearnKey key;
    const bool keyed = midi::learnKeyFor(e, key);
    if (keyed && learnArmed_.load(std::memory_order_acquire)) {   // đang learn: không phát, không kích hoạt mapping
        if ((pm.kind == K::NoteOn && pm.value > 0) || pm.kind == K::ControlChange) {
            learnArmed_.store(false, std::memory_order_release);
            RtMessage m;
            m.kind = RtMessage::MidiLearned;
            m.a = key.source;
            m.b = (std::int32_t) key.kind;
            m.i0 = key.channel;
            m.i1 = key.number;
            if (!toNrt_.try_push(m)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }
    if (keyed && current_->learnMap != nullptr) {   // khoá chính xác → mọi kênh → mọi thiết bị → cả hai
        const midi::MidiLearnMap& map = *current_->learnMap;
        const midi::LearnTarget* t = map.find(key);
        midi::LearnKey k2 = key;
        if (t == nullptr) {
            k2.channel = kAnyMidiChannel;
            t = map.find(k2);
        }
        if (t == nullptr) {
            k2 = key;
            k2.source = kAnyMidiSource;
            t = map.find(k2);
        }
        if (t == nullptr) {
            k2.channel = kAnyMidiChannel;
            t = map.find(k2);
        }
        if (t != nullptr) {
            applyMapping(*t, pm, e.offset);
            return;
        }
    }
    if (pm.kind == K::NoteOn && pm.value > 0) noteEvent(selectedTrack_, pm.number, (float) pm.value / 127.0f, true, e.offset);
    else if (pm.kind == K::NoteOn || pm.kind == K::NoteOff) noteEvent(selectedTrack_, pm.number, 0.0f, false, e.offset);
}

void RtEngine::notifyMapped(int kind, int track, int slot, int param, double value) noexcept [[clang::nonblocking]] {
    RtMessage m;
    m.kind = RtMessage::MappedChange;
    m.a = kind;
    m.b = track;
    m.i0 = slot;
    m.i1 = param;
    m.value = value;
    if (!toNrt_.try_push(m)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);
}

// [RT] Hành động của mapping. Nút (launch, transport, mute…): nốt nhấn hoặc CC ≥ 64 (Launchpad gửi 127 / 0).
// Liên tục (gain, tham số FX): giá trị CC (hoặc velocity) → [min, max]. Thay đổi mixer / FX báo main (MappedChange).
void RtEngine::applyMapping(const midi::LearnTarget& t, const midi::ParsedMidi& m, int blockOffset) noexcept
    [[clang::nonblocking]] {
    (void) blockOffset;   // hành động áp ở đầu block (như lệnh le_send)
    using K = midi::ParsedMidi::Kind;
    const bool isCc = m.kind == K::ControlChange;
    const bool press = isCc ? m.value >= 64 : (m.kind == K::NoteOn && m.value > 0);
    auto cmd = [this](std::uint16_t type, int track, int slot) {
        LeCommand c{};
        c.type = type;
        c.track = (std::int8_t) track;
        c.slot = (std::int8_t) slot;
        handleCommand(c);
    };
    using A = midi::LearnAction;
    switch (t.action) {
        case A::ClipLaunch:
            if (press) cmd(LE_CMD_CLIP_LAUNCH, t.track, t.slot);
            return;
        case A::SceneLaunch:
            if (press) cmd(LE_CMD_SCENE_LAUNCH, -1, t.slot);
            return;
        case A::TransportPlay:
            if (press) cmd(LE_CMD_TRANSPORT_PLAY, -1, -1);
            return;
        case A::TransportStop:
            if (press) cmd(LE_CMD_TRANSPORT_STOP, -1, -1);
            return;
        case A::TransportToggle:
            if (press) cmd(transport_.playing() ? LE_CMD_TRANSPORT_STOP : LE_CMD_TRANSPORT_PLAY, -1, -1);
            return;
        case A::StopAll:
            if (press) cmd(LE_CMD_STOP_ALL, -1, -1);
            return;
        case A::TrackMute:
            if (press && t.track >= 0 && t.track < LE_MAX_TRACKS) {
                const bool mute = !mixer_.muted(t.track);
                mixer_.setMute(t.track, mute);
                notifyMapped(1, t.track, 0, 0, mute ? 1.0 : 0.0);
            }
            return;
        case A::TrackGain:
            if ((isCc || press) && t.track >= 0 && t.track < LE_MAX_TRACKS) {
                const float db = midi::mapLearnValue(t, m.value);
                mixer_.setGainDb(t.track, db);
                notifyMapped(0, t.track, 0, 0, db);
            }
            return;
        case A::FxParam:
            if (isCc || press) {
                const float v = midi::mapLearnValue(t, m.value);
                if (t.track < 0) {
                    if (t.slot == 0) masterEq_.setGainDb(t.paramId, v);
                    else mixer_.setLimiterParam(t.paramId, v);
                } else if (t.track < LE_MAX_TRACKS) {
                    if (fx_[t.track].setParamLatest(t.slot, t.paramId, v) == 0) return;   // slot trống
                }
                notifyMapped(2, t.track, t.slot, t.paramId, v);
            }
            return;
        case A::LoopButton:   // máy trạng thái chạy ở main (overdub cần vé main tạo, thu cần buffer main cấp)
            if (press) {
                RtMessage msg;
                msg.kind = RtMessage::LoopButton;
                msg.a = selectedTrack_;
                if (!toNrt_.try_push(msg)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        case A::TrackStop:   // track đang chọn: như CLIP_STOP (theo quantize)
            if (press) cmd(LE_CMD_CLIP_STOP, selectedTrack_, -1);
            return;
        case A::UndoOverdub:   // model (lớp undo) ở main
            if (press) {
                RtMessage msg;
                msg.kind = RtMessage::UndoOverdub;
                msg.a = selectedTrack_;
                if (!toNrt_.try_push(msg)) droppedEvents_.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        case A::None: return;
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
    const bool provisional = pedalTrack_ >= 0;   // vòng đầu pedal mode: với UI, transport chưa chạy (04 §2.5)
    s.playing = transport_.playing() && !provisional ? 1 : 0;
    s.beat = provisional ? 0.0 : blockStartBeat_;
    s.bpm = transport_.bpm();
    s.sampleRate = sampleRate_;
    s.bufferSize = numFrames;
    s.beatsPerBar = transport_.beatsPerBar();
    s.quantize = transport_.quantize();
    s.latencyRoundTripSamples = latencyRoundTrip_.load(std::memory_order_relaxed);
    s.cpuLoad = cpu_.average();
    s.cpuPeak = cpu_.peak();
    const auto dev = (std::uint32_t) std::max(0, deviceXruns_.load(std::memory_order_relaxed));
    s.xrunCount = std::max(xruns_.count(), dev);   // cùng 1 xrun thường được cả hai cách đếm → lấy max
    int voices = load_.activeVoices();
    for (const auto& t : tracks_) voices += t.activeVoices();
    s.activeVoices = voices;
    s.anyRecording = (spike_.isRecording() || scheduler_.anyRecording()) ? 1 : 0;
    s.inputPeak = inputMeter_.value();
    s.masterPeak[0] = mixer_.masterPeak(0);
    s.masterPeak[1] = mixer_.masterPeak(1);
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        for (int c = 0; c < LE_MAX_SCENES; ++c) s.clipState[t][c] = scheduler_.cellState(t, c);
        s.trackPlayingSlot[t] = (std::int8_t) scheduler_.playingSlot(t);
        s.trackClipProgress[t] = scheduler_.progress(transport_, t);
        s.trackPeak[t][0] = mixer_.trackPeak(t, 0);
        s.trackPeak[t][1] = mixer_.trackPeak(t, 1);
    }
    publisher_.publish(s);
}

} // namespace le::core
