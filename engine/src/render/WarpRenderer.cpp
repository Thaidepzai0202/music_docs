#include "render/WarpRenderer.h"

#include "io/AudioFileIO.h"
#include "io/CafWriter.h"
#include "le/engine_api.h"
#include "render/Yin.h"

#include "signalsmith-stretch/signalsmith-stretch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace le::render {

namespace {
using Stretch = signalsmith::stretch::SignalsmithStretch<float>;
constexpr long kSeed = 0x5eed;
constexpr int64_t kChunkOut = 8192;
} // namespace

int64_t warpedLength(int64_t inFrames, double originalBpm, double newBpm) noexcept {
    if (inFrames <= 0) return 0;
    if (!(originalBpm > 0.0) || !(newBpm > 0.0)) return inFrames;
    return std::max<int64_t>(1, std::llround(static_cast<double>(inFrames) * originalBpm / newBpm));
}

namespace {
// Nén pre-echo trước mỗi onset (preset percussive, xem WarpRenderer.h). rate = sample nguồn / sample output.
int suppressPreEcho(dsp::AudioData& out, const dsp::AudioData& src, const std::vector<int64_t>& onsets, double rate) {
    const double sr = out.sampleRate();
    const int nc = out.numChannels();
    const int64_t nOut = out.numFrames(), nIn = src.numFrames();
    const auto fw = std::max<int64_t>(8, std::llround(0.0025 * sr));     // khung 2.5 ms
    const auto span = std::max<int64_t>(fw, std::llround(0.025 * sr));   // 25 ms trước onset
    const auto guard = std::max<int64_t>(1, std::llround(0.001 * sr));    // chừa 1 ms sát onset
    auto rmsOf = [](const dsp::AudioData& d, int64_t a, int64_t b) {
        a = std::max<int64_t>(0, a);
        b = std::min(d.numFrames(), b);
        if (b <= a) return 0.0;
        double e = 0.0;
        for (int c = 0; c < d.numChannels(); ++c)
            for (int64_t i = a; i < b; ++i) e += static_cast<double>(d.channel(c)[i]) * d.channel(c)[i];
        return std::sqrt(e / static_cast<double>((b - a) * d.numChannels()));
    };
    int touched = 0;
    for (const int64_t os : onsets) {
        const auto to = static_cast<int64_t>(std::llround(static_cast<double>(os) / rate));
        const int64_t end = to - guard, start = to - span;
        if (start < 0 || end <= start || to >= nOut) continue;
        std::vector<double> gain;
        for (int64_t a = start; a < end; a += fw) {
            const int64_t b = std::min(end, a + fw);
            const double srcRms = rmsOf(src, static_cast<int64_t>(std::floor(static_cast<double>(a) * rate)),
                                        std::min(nIn, static_cast<int64_t>(std::ceil(static_cast<double>(b) * rate))));
            const double outRms = rmsOf(out, a, b);
            const double target = 2.0 * srcRms + 1e-6;   // +6 dB so với nguồn, sàn −120 dB
            gain.push_back(outRms > target ? target / outRms : 1.0);
        }
        bool any = false;
        for (const double g : gain) any = any || g < 1.0;
        if (!any) continue;
        ++touched;
        // Gain từng sample: nội suy tuyến tính giữa tâm các khung; 1 ở mép ngoài (start), về 1 dần trong đoạn guard.
        const int nf = static_cast<int>(gain.size());
        for (int64_t i = start; i < to; ++i) {
            double g;
            if (i >= end) {
                const double last = gain.back();
                g = last + (1.0 - last) * static_cast<double>(i - end) / static_cast<double>(to - end);
            } else {
                const double pos = static_cast<double>(i - start) / static_cast<double>(fw) - 0.5;   // theo tâm khung
                if (pos <= 0.0) g = 1.0 + (gain[0] - 1.0) * std::max(0.0, pos + 0.5) * 2.0;
                else {
                    const int k = std::min(nf - 1, static_cast<int>(pos));
                    const double f = pos - k;
                    g = k + 1 < nf ? gain[static_cast<size_t>(k)] * (1.0 - f) + gain[static_cast<size_t>(k + 1)] * f : gain.back();
                }
            }
            for (int c = 0; c < nc; ++c) out.writePointer(c)[i] = static_cast<float>(out.channel(c)[i] * g);
        }
    }
    return touched;
}
} // namespace

// [worker]
TransientStats analyzeTransients(const dsp::AudioData& src) {
    TransientStats st;
    const int64_t n = src.numFrames();
    const double sr = src.sampleRate();
    const int nc = src.numChannels();
    if (n <= 0 || !(sr > 0.0) || nc <= 0) return st;
    auto mono = [&](int64_t i) {
        float m = 0.0f;
        for (int c = 0; c < nc; ++c) m += src.channel(c)[i];
        return m / static_cast<float>(nc);
    };

    // 1) Onset theo năng lượng
    const int win = std::max(16, static_cast<int>(std::lround(0.005 * sr)));
    const int hop = std::max(8, win / 2);
    std::vector<double> db;
    for (int64_t s0 = 0; s0 + win <= n; s0 += hop) {
        double e = 0.0;
        for (int i = 0; i < win; ++i) {
            const double x = mono(s0 + i);
            e += x * x;
        }
        db.push_back(10.0 * std::log10(e / win + 1e-20));
    }
    if (!db.empty()) {
        const double peak = *std::max_element(db.begin(), db.end());
        const int look = std::max(1, static_cast<int>(std::lround(0.010 * sr / hop)));
        const int refractory = std::max(1, static_cast<int>(std::lround(0.050 * sr / hop)));
        int last = -refractory;
        for (int i = 0; i < static_cast<int>(db.size()); ++i) {
            double lo = i == 0 ? -200.0 : db[static_cast<size_t>(i - 1)];   // trước sample 0 coi như im lặng
            for (int k = std::max(0, i - look); k < i; ++k) lo = std::min(lo, db[static_cast<size_t>(k)]);
            if (db[static_cast<size_t>(i)] - lo >= 10.0 && db[static_cast<size_t>(i)] >= peak - 50.0 && i - last >= refractory) {
                ++st.onsets;
                last = i;
                // attack = mẫu đầu tiên ≥ 10 % đỉnh trong khung onset (khung dài 5 ms → vị trí chính xác hơn đầu khung)
                const int64_t s0 = static_cast<int64_t>(i) * hop;
                float pk = 0.0f;
                for (int k = 0; k < win; ++k) pk = std::max(pk, std::fabs(mono(s0 + k)));
                int64_t at = s0;
                for (int k = 0; k < win; ++k)
                    if (std::fabs(mono(s0 + k)) >= 0.1f * pk) {
                        at = s0 + k;
                        break;
                    }
                st.onsetSamples.push_back(at);
            }
        }
    }
    st.onsetsPerSecond = static_cast<double>(st.onsets) / (static_cast<double>(n) / sr);

    // 2) Có cao độ không (Yin trên tối đa 24 khung rải đều)
    le::Yin yin;
    const int frame = yin.config().frameSize;
    std::vector<float> buf(static_cast<size_t>(frame));
    constexpr int kFrames = 24;
    int nonSilent = 0, voiced = 0;
    for (int k = 0; k < kFrames; ++k) {
        const int64_t span = std::max<int64_t>(0, n - frame);
        const int64_t pos = span * k / (kFrames - 1);
        for (int i = 0; i < frame; ++i) buf[static_cast<size_t>(i)] = pos + i < n ? mono(pos + i) : 0.0f;
        const YinFrame f = yin.analyzeFrame(buf.data(), sr);
        if (f.silent) continue;
        ++nonSilent;
        if (f.voiced) ++voiced;
    }
    st.periodicFraction = nonSilent > 0 ? static_cast<float>(voiced) / static_cast<float>(nonSilent) : 0.0f;
    st.dense = st.onsetsPerSecond >= 1.0;
    st.percussive = st.dense && st.periodicFraction < 0.5f;
    return st;
}

// [worker]
WarpResult renderWarp(const dsp::AudioData& src, const WarpConfig& cfg) {
    const auto t0 = std::chrono::steady_clock::now();
    WarpResult r;
    const int64_t nIn = src.numFrames();
    if (nIn <= 0 || !(cfg.originalBpm > 0.0) || !(cfg.newBpm > 0.0) || !(src.sampleRate() > 0.0)) {
        r.error = LE_ERR_INVALID_ARG;
        r.message = "clip rỗng hoặc BPM không hợp lệ";
        return r;
    }
    auto isCancelled = [&] { return cfg.cancel != nullptr && cfg.cancel->load(std::memory_order_relaxed); };
    if (isCancelled()) {
        r.error = LE_ERR_JOB_CANCELLED;
        r.message = "đã huỷ";
        return r;
    }

    const int nc = src.numChannels();
    const double sr = src.sampleRate();
    const int64_t nOut = warpedLength(nIn, cfg.originalBpm, cfg.newBpm);
    r.ratio = cfg.originalBpm / cfg.newBpm;
    const double rate = static_cast<double>(nIn) / static_cast<double>(nOut);   // sample vào / sample ra

    Stretch st(kSeed);
    double blockMs = cfg.blockMs, intervalMs = cfg.intervalMs;
    TransientStats ts;
    bool suppress = false;
    if (!(blockMs > 0.0 && intervalMs > 0.0)) {   // không ghi đè → theo preset
        bool percussive = cfg.preset == WarpPreset::Percussive;
        if (cfg.preset != WarpPreset::Tonal) {   // Auto: quyết định; Percussive: cần vị trí onset để nén pre-echo
            ts = analyzeTransients(src);
            r.onsetsPerSecond = ts.onsetsPerSecond;
            r.periodicFraction = ts.periodicFraction;
            if (cfg.preset == WarpPreset::Auto) percussive = ts.percussive;
            suppress = percussive || ts.dense;
        }
        r.percussive = percussive;
        blockMs = percussive ? kPercussiveBlockMs : 0.0;
        intervalMs = percussive ? kPercussiveIntervalMs : 0.0;
    }
    if (blockMs > 0.0 && intervalMs > 0.0) {   // kẹp trước khi đổi sang int (số lớn / Inf)
        blockMs = std::min(blockMs, 500.0);
        intervalMs = std::min(intervalMs, blockMs);
        const int block = std::max(64, static_cast<int>(std::lround(blockMs * sr / 1000.0)));
        st.configure(nc, block, std::clamp(static_cast<int>(std::lround(intervalMs * sr / 1000.0)), 16, block));
    } else {
        st.presetDefault(nc, static_cast<float>(sr));
    }
    r.blockSamples = st.blockSamples();
    r.intervalSamples = st.intervalSamples();
    const int inLat = st.inputLatency(), outLat = st.outputLatency();

    // Input đệm thêm inLat sample 0 ở cuối (để "thời điểm xử lý" chạy tới hết input, notes §3)
    std::vector<std::vector<float>> padded(static_cast<size_t>(nc));
    std::vector<std::vector<float>> tmp(static_cast<size_t>(nc));
    for (int c = 0; c < nc; ++c) {
        padded[static_cast<size_t>(c)].assign(static_cast<size_t>(nIn + inLat), 0.0f);
        std::copy(src.channel(c), src.channel(c) + nIn, padded[static_cast<size_t>(c)].begin());
        tmp[static_cast<size_t>(c)].assign(static_cast<size_t>(nOut + outLat), 0.0f);
    }
    std::vector<const float*> inPtr(static_cast<size_t>(nc));
    std::vector<float*> outPtr(static_cast<size_t>(nc));

    for (int c = 0; c < nc; ++c) inPtr[static_cast<size_t>(c)] = padded[static_cast<size_t>(c)].data();
    st.seek(inPtr.data(), inLat, rate);

    // process: tổng input = nIn, tổng output = nOut, chia khúc theo TỔNG TÍCH LUỸ → không lệch dù làm tròn
    int64_t inDone = 0;
    for (int64_t outDone = 0; outDone < nOut;) {
        if (isCancelled()) {
            r.error = LE_ERR_JOB_CANCELLED;
            r.message = "đã huỷ";
            return r;
        }
        const int64_t outEnd = std::min(nOut, outDone + kChunkOut);
        const int64_t inEnd = outEnd == nOut ? nIn : std::llround(static_cast<double>(outEnd) * rate);
        for (int c = 0; c < nc; ++c) {
            inPtr[static_cast<size_t>(c)] = padded[static_cast<size_t>(c)].data() + inLat + inDone;
            outPtr[static_cast<size_t>(c)] = tmp[static_cast<size_t>(c)].data() + outDone;
        }
        st.process(inPtr.data(), static_cast<int>(inEnd - inDone), outPtr.data(), static_cast<int>(outEnd - outDone));
        inDone = inEnd;
        outDone = outEnd;
        if (cfg.progress != nullptr)
            cfg.progress->store(static_cast<float>(outDone) / static_cast<float>(nOut), std::memory_order_relaxed);
    }
    for (int c = 0; c < nc; ++c) outPtr[static_cast<size_t>(c)] = tmp[static_cast<size_t>(c)].data() + nOut;
    st.flush(outPtr.data(), outLat, static_cast<float>(rate));   // playbackRate = tỉ lệ thật (không đứng yên)

    auto out = std::make_shared<dsp::AudioData>(nc, nOut, sr);
    for (int c = 0; c < nc; ++c)
        std::copy(tmp[static_cast<size_t>(c)].begin() + outLat, tmp[static_cast<size_t>(c)].begin() + outLat + nOut,
                  out->writePointer(c));
    if (suppress) r.suppressedOnsets = suppressPreEcho(*out, src, ts.onsetSamples, rate);
    r.data = std::move(out);
    r.ok = true;
    r.error = LE_OK;
    r.msTotal = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

std::string stretchedCacheFileName(const std::string& clipId, double bpm) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "@%.2f.caf", bpm);
    return clipId + buf;
}

std::string stretchedCachePath(const std::string& projectDir, const std::string& clipId, double bpm) {
    return (std::filesystem::path(projectDir) / "cache" / "stretched" / stretchedCacheFileName(clipId, bpm)).string();
}

bool writeStretchedCache(const std::string& path, const dsp::AudioData& data, std::string* error) {
    return io::writeCafFloat32(path, data, error);
}

dsp::AudioDataPtr readStretchedCache(const std::string& path, int64_t expectFrames, double expectSampleRate,
                                     std::string* error) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (error != nullptr) *error = "không có cache";
        return nullptr;
    }
    io::DecodeResult d = io::decodeAudioFile(path);
    if (d.error != LE_OK || d.data == nullptr) {
        if (error != nullptr) *error = "cache hỏng: " + d.message;
        return nullptr;
    }
    if (d.data->numFrames() != expectFrames || std::fabs(d.data->sampleRate() - expectSampleRate) > 1e-6) {
        if (error != nullptr) *error = "cache không khớp nguồn";
        return nullptr;
    }
    return d.data;
}

} // namespace le::render
