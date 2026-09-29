// le-latency-selftest — P0-07 trên Mac. [main] + [RT] (chỉ ở chế độ --device)
//
//   le-latency-selftest            # loopback GIẢ LẬP: nhiều độ trễ × block size × mức nhiễu, không cần loa/mic
//   le-latency-selftest --device   # đo THẬT: loa Mac → mic Mac, 3 lần (cần cho phép Terminal dùng micro)
//
// Exit code 0 = mọi trường hợp giả lập tìm đúng độ trễ ±1 sample (hoặc --device đo được).
#include "spike/measure/LatencyProbe.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

using le::spike::LatencyProbe;
using le::spike::LatencyResult;

namespace {

// Giả lập "loa → mic": input(t) = gain · output(t − delay) + nhiễu. Chạy theo block như callback thật.
LatencyResult simulate(double sr, int block, int delay, float gain, double snrDb) {
    LatencyProbe probe;
    probe.prepare(sr, block);
    double chirpE = 0.0;
    for (float v : probe.chirp()) chirpE += static_cast<double>(v) * v;
    const double chirpRms = std::sqrt(chirpE / static_cast<double>(probe.chirp().size())) * LatencyProbe::Config{}.gain;
    const double noiseRms = chirpRms * std::fabs(gain) * std::pow(10.0, -snrDb / 20.0);

    std::mt19937 rng(static_cast<uint32_t>(delay * 31 + block));
    std::normal_distribution<double> gauss(0.0, 1.0);
    std::vector<float> history, in(static_cast<size_t>(block)), outL(in.size()), outR(in.size());
    float* outs[2] = {outL.data(), outR.data()};
    probe.start();
    for (int64_t t = 0; !probe.isDone(); t += block) {
        for (int i = 0; i < block; ++i) {
            const int64_t src = t + i - delay;
            const float x = src >= 0 ? gain * history[static_cast<size_t>(src)] : 0.0f;
            in[static_cast<size_t>(i)] = x + static_cast<float>(noiseRms * gauss(rng));
        }
        probe.processRt(in.data(), outs, 2, block);
        history.insert(history.end(), outL.begin(), outL.end());
    }
    return probe.analyze();
}

int runSynthetic() {
    std::printf("Loopback giả lập (48 kHz): độ trễ × block × SNR\n");
    std::printf("  delay  block  SNR(dB)  đo được  lệch  hợp lệ  score min\n");
    int failures = 0;
    for (int delay : {523, 1024, 2400, 12000}) {
        for (int block : {64, 128, 256}) {
            for (double snr : {30.0, 0.0}) {
                const LatencyResult r = simulate(48000.0, block, delay, 0.25f, snr);
                float minScore = 1.0f;
                for (float s : r.score) minScore = std::min(minScore, s);
                const int err = r.measuredSamples - delay;
                const bool pass = r.ok && std::abs(err) <= 1;
                if (!pass) ++failures;
                std::printf("  %5d  %5d  %7.0f  %7d  %+4d   %d/5    %.3f  %s\n", delay, block, snr,
                            r.measuredSamples, err, r.validRuns, static_cast<double>(minScore), pass ? "OK" : "FAIL");
            }
        }
    }
    std::printf(failures == 0 ? "\nTất cả OK (±1 sample)\n" : "\n%d trường hợp FAIL\n", failures);
    return failures == 0 ? 0 : 1;
}

// ─────────────────────────── Đo thật trên thiết bị của Mac ───────────────────────────

class ProbeCallback final : public juce::AudioIODeviceCallback {
public:
    LatencyProbe probe;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        probe.prepare(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());  // [main] trước khi callback chạy
    }
    void audioDeviceStopped() override {}

    // [RT] Chỉ gọi probe.processRt (nonblocking) + xoá kênh thừa.
    void audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut,
                                          int n, const juce::AudioIODeviceCallbackContext&) override {
        for (int c = 0; c < numOut; ++c)
            if (out[c] != nullptr) std::memset(out[c], 0, sizeof(float) * static_cast<size_t>(n));
        probe.processRt(numIn > 0 ? in[0] : nullptr, out, numOut, n);
    }
};

int runDevice() {
    juce::ScopedJuceInitialiser_GUI juceInit;   // AudioDeviceManager cần MessageManager
    juce::AudioDeviceManager dm;
    const juce::String err = dm.initialiseWithDefaultDevices(1, 2);
    if (err.isNotEmpty()) {
        std::fprintf(stderr, "không mở được thiết bị audio: %s\n", err.toRawUTF8());
        return 2;
    }
    juce::AudioIODevice* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr) {
        std::fprintf(stderr, "không có thiết bị audio\n");
        return 2;
    }
    const int reported = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
    std::printf("Thiết bị: %s  %.0f Hz, buffer %d, latency device báo: in %d + out %d = %d sample (%.2f ms)\n",
                dev->getName().toRawUTF8(), dev->getCurrentSampleRate(), dev->getCurrentBufferSizeSamples(),
                dev->getInputLatencyInSamples(), dev->getOutputLatencyInSamples(), reported,
                reported * 1000.0 / dev->getCurrentSampleRate());
    std::printf("Đang đo 3 lần (mỗi lần ~2.7 s). Giữ phòng yên tĩnh, bật loa vừa đủ nghe.\n");

    ProbeCallback cb;
    dm.addAudioCallback(&cb);   // gọi audioDeviceAboutToStart → prepare
    int okCount = 0;
    for (int run = 0; run < 3; ++run) {
        cb.probe.start();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!cb.probe.isDone() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (!cb.probe.isDone()) {
            std::printf("  lần %d: hết giờ (callback không chạy?)\n", run + 1);
            continue;
        }
        const LatencyResult r = cb.probe.analyze();
        std::printf("  lần %d: %s  %d sample = %.2f ms  (5 lần: %d %d %d %d %d, lệch %d, peak mic %.3f)\n", run + 1,
                    r.ok ? "OK  " : "FAIL", r.measuredSamples, r.measuredMs, r.runs[0], r.runs[1], r.runs[2],
                    r.runs[3], r.runs[4], r.spreadSamples, static_cast<double>(r.inputPeak));
        if (r.ok) ++okCount;
    }
    dm.removeAudioCallback(&cb);
    dm.closeAudioDevice();
    if (okCount == 0) std::printf("Không đo được: kiểm tra quyền micro (System Settings → Privacy → Microphone) và âm lượng loa.\n");
    return okCount > 0 ? 0 : 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--device") == 0) return runDevice();
    if (argc >= 2) {
        std::fprintf(stderr, "usage: le-latency-selftest [--device]\n");
        return 1;
    }
    return runSynthetic();
}
