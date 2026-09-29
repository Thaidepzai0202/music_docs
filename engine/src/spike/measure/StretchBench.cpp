#include "spike/measure/StretchBench.h"

#include "spike/measure/WavIO.h"

#include "signalsmith-stretch/signalsmith-stretch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>

namespace le::spike {

namespace {
using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

float rmsDb(const float* x, int64_t n) {
    double e = 0.0;
    for (int64_t i = 0; i < n; ++i) e += static_cast<double>(x[i]) * x[i];
    return static_cast<float>(10.0 * std::log10(e / static_cast<double>(n) + 1e-20));
}

// Seed cố định → output lặp lại được (P3-04 cần golden). Mặc định thư viện seed bằng
// std::random_device; số ngẫu nhiên chỉ dùng khi time-stretch > 2×, nhưng cứ cố định cho chắc.
constexpr long kSeed = 0x5eed;

using Stretch = signalsmith::stretch::SignalsmithStretch<float>;

// Render một zone: output dài đúng n sample, thẳng hàng với input.
//   padded = input + inputLatency() sample 0 ở cuối (dựng sẵn bên ngoài).
//   tmp    = buffer n + outputLatency().
// Theo README của Signalsmith (mục "Seeking and starting" / "Ending"):
//   1) seek() inputLatency() sample đầu → "thời điểm xử lý" trùng với sample 0 của input
//   2) process() phần còn lại + inputLatency() sample 0 → thời điểm xử lý tới cuối input
//   3) flush() outputLatency() sample → lấy nốt phần đuôi
//   Output có outputLatency() sample pre-roll ở đầu → bỏ đi.
void renderZone(Stretch& st, const std::vector<float>& padded, int64_t n, std::vector<float>& tmp, float* out) {
    st.reset();
    const int inLat = st.inputLatency();
    const int outLat = st.outputLatency();

    const float* seekIn[1] = {padded.data()};
    st.seek(seekIn, inLat, 1.0);

    const float* procIn[1] = {padded.data() + inLat};
    float* procOut[1] = {tmp.data()};
    st.process(procIn, static_cast<int>(n), procOut, static_cast<int>(n));

    float* flushOut[1] = {tmp.data() + n};
    // playbackRate = 1: phần đuôi tiếp tục "trôi" theo thời gian thật (không time-stretch).
    // Mặc định 0 = đứng yên thời gian → đuôi bị kéo dài/nhoè (đã thử, xem notes).
    st.flush(flushOut, outLat, 1.0f);

    std::copy(tmp.begin() + outLat, tmp.begin() + outLat + n, out);
}
} // namespace

std::vector<int> StretchBench::zoneSemitones(const Config& cfg) {
    if (!cfg.semitones.empty()) return cfg.semitones;
    std::vector<int> list;
    if (cfg.semitoneStep <= 0 || cfg.semitoneMax < cfg.semitoneMin) return list;
    for (int k = cfg.semitoneMin; k <= cfg.semitoneMax; k += cfg.semitoneStep) list.push_back(k);
    return list;
}

std::string StretchBench::zoneFileName(int index, int semitones, bool formant) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "z%02d_%+03dst_%s.wav", index, semitones, formant ? "formant" : "plain");
    return buf;
}

// [worker]
StretchBench::Result StretchBench::run(const float* mono, int64_t numSamples, double sampleRate, const Config& cfg) {
    Result r;
    r.numSamples = numSamples;
    if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0) {
        r.error = "input rỗng hoặc sample rate không hợp lệ";
        return r;
    }
    if (numSamples > std::numeric_limits<int>::max() / 2) {
        r.error = "input quá dài";
        return r;
    }
    const std::vector<int> zones = zoneSemitones(cfg);
    if (zones.empty()) {
        r.error = "danh sách nửa cung rỗng hoặc dải không hợp lệ";
        return r;
    }
    for (int k : zones) {
        if (k < -48 || k > 48) {   // ±4 quãng tám: ngoài dải này Signalsmith không còn ý nghĩa
            r.error = "nửa cung ngoài dải ±48";
            return r;
        }
    }

    const auto tSetup = Clock::now();
    Stretch st(kSeed);
    if (cfg.blockMs > 0.0 && cfg.intervalMs > 0.0) {   // NaN → không vào đây (preset); số lớn / Inf → kẹp trước khi đổi sang int
        const double blockMs = std::min(cfg.blockMs, 500.0), intervalMs = std::min(cfg.intervalMs, blockMs);
        const int block = std::max(64, static_cast<int>(std::lround(blockMs * sampleRate / 1000.0)));
        const int interval = std::clamp(static_cast<int>(std::lround(intervalMs * sampleRate / 1000.0)), 16, block);
        st.configure(1, block, interval);
    } else if (cfg.cheaper) {
        st.presetCheaper(1, static_cast<float>(sampleRate));
    } else {
        st.presetDefault(1, static_cast<float>(sampleRate));
    }
    r.inputLatency = st.inputLatency();
    r.outputLatency = st.outputLatency();
    r.blockSamples = st.blockSamples();
    r.intervalSamples = st.intervalSamples();

    std::vector<float> padded(static_cast<size_t>(numSamples + r.inputLatency), 0.0f);
    std::copy(mono, mono + numSamples, padded.begin());
    std::vector<float> tmp(static_cast<size_t>(numSamples + r.outputLatency), 0.0f);
    std::vector<float> out(static_cast<size_t>(numSamples), 0.0f);
    r.msSetup = msSince(tSetup);
    r.inputRmsDb = rmsDb(mono, numSamples);

    // Tần số chuẩn hoá (Hz / sr) kẹp về [0, 0.5]: Inf / số lớn từ JSON không được lọt vào Signalsmith
    const float tonality = cfg.tonalityLimitHz > 0.0 ? static_cast<float>(std::min(cfg.tonalityLimitHz / sampleRate, 0.5)) : 0.0f;
    const float formantBase = cfg.formantBaseHz > 0.0 ? static_cast<float>(std::min(cfg.formantBaseHz / sampleRate, 0.5)) : 0.0f;
    const int numZones = static_cast<int>(zones.size());

    for (int z = 0; z < numZones; ++z) {
        if (cfg.cancel != nullptr && cfg.cancel->load(std::memory_order_relaxed)) {
            r.cancelled = true;
            r.error = "đã huỷ";
            return r;
        }
        const int semi = zones[static_cast<size_t>(z)];

        const auto t0 = Clock::now();
        st.setTransposeSemitones(static_cast<float>(semi), tonality);
        // Giữ formant = "bù lại" phần dịch cao độ khi tính đường bao phổ (compensatePitch = true)
        // với hệ số formant 1 (không dịch formant thêm). Tắt: formant trượt theo cao độ ("chipmunk").
        st.setFormantFactor(1.0f, cfg.formant);
        st.setFormantBase(formantBase);
        renderZone(st, padded, numSamples, tmp, out.data());
        const double ms = msSince(t0);

        float peak = 0.0f;
        for (float v : out) peak = std::max(peak, std::fabs(v));
        r.semitones.push_back(semi);
        r.msPerZone.push_back(ms);
        r.peakPerZone.push_back(peak);
        r.rmsDbPerZone.push_back(rmsDb(out.data(), numSamples));
        r.msTotal += ms;

        if (!cfg.outDir.empty()) {
            const auto tw = Clock::now();
            std::string path = cfg.outDir;
            if (path.back() != '/') path += '/';
            path += zoneFileName(z, semi, cfg.formant);
            std::string err;
            if (!writeWavMono(path, out.data(), numSamples, sampleRate, &err)) {
                r.error = err;
                return r;
            }
            r.files.push_back(path);
            r.msWrite += msSince(tw);
        }
        if (cfg.keepAudio) r.audio.push_back(out);
        if (cfg.progress != nullptr)
            cfg.progress->store(static_cast<float>(z + 1) / static_cast<float>(numZones), std::memory_order_relaxed);
    }
    r.ok = true;
    return r;
}

} // namespace le::spike
