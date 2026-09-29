#include "render/PitchRenderer.h"

#include "le/engine_api.h"
#include "render/Yin.h"

#include "signalsmith-stretch/signalsmith-stretch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

namespace le::render {

namespace {
using Stretch = signalsmith::stretch::SignalsmithStretch<float>;
constexpr long kSeed = 0x5eed;           // deterministic (P0-09 notes §6)
constexpr int64_t kChunk = 8192;         // khúc render: kiểm cancel giữa các khúc

bool cancelled(const PitchRenderConfig& c) {
    return c.cancel != nullptr && c.cancel->load(std::memory_order_relaxed);
}

double rms(const float* x, int64_t n) {
    double e = 0.0;
    for (int64_t i = 0; i < n; ++i) e += static_cast<double>(x[i]) * x[i];
    return n > 0 ? std::sqrt(e / static_cast<double>(n)) : 0.0;
}

// Render 1 zone offline: output dài đúng n, thẳng hàng input (seek → process → flush, notes §3).
// Trả false nếu bị huỷ giữa chừng.
bool renderZone(Stretch& st, const std::vector<float>& padded, int64_t n, std::vector<float>& tmp, float* out,
                const PitchRenderConfig& cfg, int zoneIndex, int numZones) {
    st.reset();
    const int inLat = st.inputLatency(), outLat = st.outputLatency();
    const float* seekIn[1] = {padded.data()};
    st.seek(seekIn, inLat, 1.0);
    for (int64_t done = 0; done < n; done += kChunk) {
        if (cancelled(cfg)) return false;
        const auto m = static_cast<int>(std::min(kChunk, n - done));
        const float* in[1] = {padded.data() + inLat + done};
        float* o[1] = {tmp.data() + done};
        st.process(in, m, o, m);
        if (cfg.progress != nullptr)
            cfg.progress->store((static_cast<float>(zoneIndex) + static_cast<float>(done + m) / static_cast<float>(n)) /
                                    static_cast<float>(numZones),
                                std::memory_order_relaxed);
    }
    float* f[1] = {tmp.data() + n};
    st.flush(f, outLat, 1.0f);   // playbackRate = 1 (mặc định 0 làm nhoè đuôi, notes §3)
    std::copy(tmp.begin() + outLat, tmp.begin() + outLat + n, out);
    return true;
}

// Yin trên đoạn giữa (tối đa 1 s) — đủ để đo cao độ, rẻ hơn cả buffer.
PitchEstimate measure(Yin& yin, const float* x, int64_t n, double sr) {
    const auto len = std::min<int64_t>(n, static_cast<int64_t>(sr));
    return yin.analyze(x + (n - len) / 2, len, sr);
}
} // namespace

// [worker]
PitchRenderResult renderPitchInstrument(const float* mono, int64_t n, double sr, const PitchRenderConfig& cfg) {
    const auto t0 = std::chrono::steady_clock::now();
    PitchRenderResult r;
    auto failWith = [&](int32_t code, std::string msg) {
        r.ok = false;
        r.error = code;
        r.message = std::move(msg);
        r.instrument.reset();
        return r;
    };
    if (mono == nullptr || n <= 0 || !(sr > 0.0) || cfg.semitones.empty())
        return failWith(LE_ERR_INVALID_ARG, "input rỗng hoặc tham số không hợp lệ");
    if (cancelled(cfg)) return failWith(LE_ERR_JOB_CANCELLED, "đã huỷ");

    // 1) Nốt gốc
    Yin yin;
    const PitchEstimate p = yin.analyze(mono, n, sr);
    r.confidence = p.confidence;
    r.detectedHz = p.ok ? p.hz : 0.0f;
    if (cfg.rootNote >= 0 && cfg.rootNote <= 127) {
        r.rootNote = cfg.rootNote;
        r.rootCents = cfg.rootCents;
        r.rootFromUser = true;
    } else if (p.ok && p.confidence >= cfg.minConfidence) {
        r.rootNote = p.rootNote;
        r.rootCents = p.cents;
    } else {
        return failWith(LE_ERR_PITCH_NOT_DETECTED,
                        "Không chắc nốt gốc (confidence " + std::to_string(p.confidence) + "): cần người dùng chọn");
    }

    std::vector<int> ks = cfg.semitones;
    std::sort(ks.begin(), ks.end());
    ks.erase(std::unique(ks.begin(), ks.end()), ks.end());
    const int numZones = static_cast<int>(ks.size());

    // 2) Render các zone
    Stretch st(kSeed);
    if (cfg.blockMs > 0.0 && cfg.intervalMs > 0.0) {
        const int block = std::max(64, static_cast<int>(std::lround(cfg.blockMs * sr / 1000.0)));
        st.configure(1, block, std::clamp(static_cast<int>(std::lround(cfg.intervalMs * sr / 1000.0)), 16, block));
    } else {
        st.presetDefault(1, static_cast<float>(sr));
    }
    std::vector<float> padded(static_cast<size_t>(n + st.inputLatency()), 0.0f);
    std::copy(mono, mono + n, padded.begin());
    std::vector<float> tmp(static_cast<size_t>(n + st.outputLatency()), 0.0f);
    // f0 cho phân tích formant: cao độ Yin nếu có, không thì cao độ của nốt gốc
    const double baseHz = r.detectedHz > 0.0f ? static_cast<double>(r.detectedHz)
                                               : 440.0 * std::pow(2.0, (r.rootNote + r.rootCents / 100.0 - 69.0) / 12.0);

    auto inst = std::make_shared<dsp::Instrument>();
    inst->name = cfg.name;
    std::vector<double> zoneRms(static_cast<size_t>(numZones), 0.0);
    int zeroIndex = -1;
    for (int z = 0; z < numZones; ++z) {
        const int k = ks[static_cast<size_t>(z)];
        if (k == 0) zeroIndex = z;
        const double shift = k - r.rootCents / 100.0;   // zone ra đúng nốt tuyệt đối root + k
        st.setTransposeSemitones(static_cast<float>(shift));
        st.setFormantFactor(1.0f, cfg.formant);
        st.setFormantBase(static_cast<float>(baseHz / sr));
        auto data = std::make_shared<dsp::AudioData>(1, n, sr);
        if (!renderZone(st, padded, n, tmp, data->writePointer(0), cfg, z, numZones))
            return failWith(LE_ERR_JOB_CANCELLED, "đã huỷ khi render zone " + std::to_string(k));

        // 3) Đo lại cao độ → tuneCents (so với cao độ THẬT của input dịch đi `shift`)
        float tune = 0.0f, err = 0.0f;
        if (cfg.measureTune && r.detectedHz > 0.0f) {
            const PitchEstimate q = measure(yin, data->channel(0), n, sr);
            if (q.ok && q.confidence >= 0.5f) {
                const double target = static_cast<double>(r.detectedHz) * std::pow(2.0, shift / 12.0);
                err = static_cast<float>(1200.0 * std::log2(static_cast<double>(q.hz) / target));
                if (std::fabs(err) <= 50.0f) tune = -err;   // lệch > 50 cent: nghi Yin sai quãng, không bù
            }
        }
        zoneRms[static_cast<size_t>(z)] = rms(data->channel(0), n);

        dsp::Zone zone;
        zone.rootKey = static_cast<int16_t>(std::clamp(r.rootNote + k, 0, 127));
        zone.loKey = static_cast<int16_t>(std::clamp(r.rootNote + k - 1, 0, 127));
        zone.hiKey = static_cast<int16_t>(std::clamp(r.rootNote + k + 1, 0, 127));
        zone.tuneCents = tune;
        zone.loopMode = dsp::LoopMode::NoLoop;
        zone.env = cfg.env;
        zone.data = data.get();
        inst->samples.push_back(std::move(data));
        inst->zones.push_back(zone);
        r.semitones.push_back(k);
        r.zoneTuneCents.push_back(tune);
        r.zoneMeasuredErrorCents.push_back(err);
    }
    if (cancelled(cfg)) return failWith(LE_ERR_JOB_CANCELLED, "đã huỷ");

    // Zone ngoài cùng phủ nốt tới mép bàn phím (04 §8: phím ngoài dải dùng zone gần nhất)
    inst->zones.front().loKey = 0;
    inst->zones.back().hiKey = 127;
    // Khoảng cách zone > 3 nửa cung (danh sách tuỳ chỉnh) → kéo hiKey cho liền, không để hở phím
    for (size_t z = 0; z + 1 < inst->zones.size(); ++z)
        inst->zones[z].hiKey = static_cast<int16_t>(std::max<int>(inst->zones[z].hiKey, inst->zones[z + 1].loKey - 1));

    // 4) Chuẩn hoá RMS về zone gốc (k = 0, hoặc zone gần 0 nhất)
    if (zeroIndex < 0) {
        zeroIndex = 0;
        for (int z = 1; z < numZones; ++z)
            if (std::abs(ks[static_cast<size_t>(z)]) < std::abs(ks[static_cast<size_t>(zeroIndex)])) zeroIndex = z;
    }
    const double refRms = zoneRms[static_cast<size_t>(zeroIndex)];
    for (int z = 0; z < numZones; ++z) {
        float g = 0.0f;
        const double zr = zoneRms[static_cast<size_t>(z)];
        if (cfg.normalizeRms && refRms > 0.0 && zr > 0.0)
            g = std::clamp(static_cast<float>(20.0 * std::log10(refRms / zr)), -cfg.maxGainDb, cfg.maxGainDb);
        inst->zones[static_cast<size_t>(z)].gainDb = g;
        r.zoneGainDb.push_back(g);
    }

    inst->mode = dsp::Instrument::Mode::Natural;
    inst->classicZone = zeroIndex;
    if (cfg.progress != nullptr) cfg.progress->store(1.0f, std::memory_order_relaxed);
    r.instrument = std::move(inst);
    r.ok = true;
    r.error = LE_OK;
    r.msTotal = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

} // namespace le::render
