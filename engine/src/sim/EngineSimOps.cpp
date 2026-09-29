// Op cho test (05 §3): sim.offline / sim.advance. Chỉ biên dịch khi LE_ENABLE_SIM (preset Mac, libLoopCore.dylib).
// Dùng để chạy test hợp đồng Dart (77) với engine thật mà không cần thiết bị audio: [main] render đồng bộ.
#include "core/Engine.h"

#if LE_ENABLE_SIM
#include <algorithm>
#include <cmath>
#include <vector>

#include "io/OfflineDeviceIO.h"
#endif

namespace le::core {

#if LE_ENABLE_SIM

void Engine::registerSimOps() {
    ops_.add("sim.offline", [this](const juce::var& r) { return opSimOffline(r); });
    ops_.add("sim.advance", [this](const juce::var& r) { return opSimAdvance(r); });
    ops_.add("sim.midiIn", [this](const juce::var& r) { return opSimMidiIn(r); });
}

// {enabled, sampleRate, blockSize}: thay device bằng OfflineDeviceIO (và start luôn), chỉ khi audio chưa chạy.
// enabled:false → bỏ offline, le_audio_start sau đó dùng lại thiết bị thật.
Reply Engine::opSimOffline(const juce::var& req) {
    bool enabled = true;
    if (req.hasProperty("enabled") && !args::getBool(req, "enabled", enabled))
        return Reply::fail(LE_ERR_INVALID_ARG, "enabled phải là bool");
    const bool offlineNow = dynamic_cast<io::OfflineDeviceIO*>(device_.get()) != nullptr;
    if (device_ != nullptr && device_->isRunning() && !offlineNow)
        return Reply::fail(LE_ERR_INVALID_ARG, "audio thật đang chạy: gọi le_audio_stop trước");
    if (!enabled) {
        if (offlineNow) {
            device_->stop();
            device_.reset();
        }
        return Reply::ok();
    }
    double sr = 48000.0;
    int block = 128;
    if (req.hasProperty("sampleRate") && !args::getDouble(req, "sampleRate", sr, 8000.0, 192000.0))
        return Reply::fail(LE_ERR_INVALID_ARG, "sampleRate phải trong 8000..192000");
    if (req.hasProperty("blockSize") && !args::getInt(req, "blockSize", block, 1, 4096))
        return Reply::fail(LE_ERR_INVALID_ARG, "blockSize phải trong 1..4096");
    if (device_ != nullptr) device_->stop();
    device_ = std::make_unique<io::OfflineDeviceIO>(std::max(block, 1024));
    preferredRate_ = sr;
    preferredBuffer_ = block;
    const std::int32_t err = audioStart();
    if (err != LE_OK) return Reply::fail(err, "không start được OfflineDeviceIO");
    auto* o = new juce::DynamicObject();
    o->setProperty("sampleRate", sr);
    o->setProperty("blockSize", block);
    return Reply::ok(juce::var(o));
}

// {frames} | {beats} → render đồng bộ trên main theo blockSize, bỏ output, pump sau mỗi block
// (event + ReleasePool vẫn chạy). Trả {beat, frames}.
Reply Engine::opSimAdvance(const juce::var& req) {
    auto* dev = dynamic_cast<io::OfflineDeviceIO*>(device_.get());
    if (dev == nullptr || !dev->isRunning()) return Reply::fail(LE_ERR_INVALID_ARG, "chưa bật sim.offline");
    const double sr = dev->sampleRate();
    std::int64_t frames = 0;
    if (req.hasProperty("frames")) {
        double f = 0;
        if (!args::getDouble(req, "frames", f, 0.0, sr * 600.0) || f != std::floor(f))
            return Reply::fail(LE_ERR_INVALID_ARG, "frames phải là số nguyên 0..10 phút");
        frames = (std::int64_t) f;
    } else if (req.hasProperty("beats")) {
        double beats = 0;
        if (!args::getDouble(req, "beats", beats, 0.0, 100000.0)) return Reply::fail(LE_ERR_INVALID_ARG, "beats phải ≥ 0");
        // Audio không chạy trên thread khác (offline) → đọc Transport từ main là an toàn.
        const Transport& tr = rt_.transport();
        frames = tr.playing() ? tr.sampleAtBeat(tr.beatNow() + beats) - tr.samplePos()
                              : (std::int64_t) std::llround(beats * 60.0 * sr / tr.bpm());
        if (frames > (std::int64_t) (sr * 600.0)) return Reply::fail(LE_ERR_INVALID_ARG, "quá 10 phút");
    } else {
        return Reply::fail(LE_ERR_INVALID_ARG, "cần frames hoặc beats");
    }

    const int block = std::max(1, preferredBuffer_);
    std::vector<float> L((size_t) block), R((size_t) block);
    float* outs[2] = {L.data(), R.data()};
    for (std::int64_t done = 0; done < frames;) {
        const int n = (int) std::min<std::int64_t>(block, frames - done);
        dev->render(nullptr, 0, outs, 2, n);
        pump();
        done += n;
    }
    LeState s{};
    globalStatePublisher().read(s);
    auto* o = new juce::DynamicObject();
    o->setProperty("beat", rt_.transport().beatNow());
    o->setProperty("frames", (juce::int64) frames);
    return Reply::ok(juce::var(o));
}

// {bytes:[status, data1, data2]} → {} (P4, cho test hợp đồng Dart): message MIDI vào nguồn 0 ("virtual"), phát ở block
// kế tiếp — cùng đường với thiết bị thật (learn, mapping, nốt tới track đang chọn).
Reply Engine::opSimMidiIn(const juce::var& req) {
    const auto* arr = req["bytes"].getArray();
    if (arr == nullptr || arr->isEmpty() || arr->size() > 3) return Reply::fail(LE_ERR_INVALID_ARG, "bytes phải là mảng 1..3 số 0..255");
    std::uint8_t b[3] = {};
    for (int i = 0; i < arr->size(); ++i) {
        const juce::var& v = arr->getReference(i);
        const double d = args::isNumber(v) ? (double) v : -1.0;
        if (!(d >= 0.0 && d <= 255.0) || d != std::floor(d)) return Reply::fail(LE_ERR_INVALID_ARG, "bytes phải là số nguyên 0..255");
        b[i] = (std::uint8_t) d;
    }
    if (!injectMidi(b, arr->size(), 0)) return Reply::fail(LE_ERR_INVALID_ARG, "message không phải nốt / CC / message kênh (hoặc queue đầy)");
    return Reply::ok();
}

#else

void Engine::registerSimOps() {}
Reply Engine::opSimMidiIn(const juce::var&) { return Reply::fail(LE_ERR_NOT_IMPLEMENTED, "sim.* chỉ có trên Mac"); }
Reply Engine::opSimOffline(const juce::var&) { return Reply::fail(LE_ERR_NOT_IMPLEMENTED, "sim.* chỉ có trên Mac"); }
Reply Engine::opSimAdvance(const juce::var&) { return Reply::fail(LE_ERR_NOT_IMPLEMENTED, "sim.* chỉ có trên Mac"); }

#endif

} // namespace le::core
