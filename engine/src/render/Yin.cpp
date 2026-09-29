#include "render/Yin.h"

#include <algorithm>
#include <cmath>

namespace le {

namespace {
// Median của v (làm xáo trộn thứ tự v). v không rỗng.
float medianInPlace(std::vector<float>& v) {
    const size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
    const float hi = v[mid];
    if (v.size() % 2 == 1) return hi;
    const float lo = *std::max_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid));
    return 0.5f * (lo + hi);
}
} // namespace

float Yin::hzToMidi(float hz, float a4Hz) {
    return 69.0f + 12.0f * std::log2(hz / a4Hz);
}

float Yin::midiToHz(float midi, float a4Hz) {
    return a4Hz * std::exp2((midi - 69.0f) / 12.0f);
}

Yin::Yin(const YinConfig& cfg) : cfg_(cfg) {
    cfg_.frameSize = std::max(64, cfg_.frameSize);
    cfg_.hopSize = std::max(1, cfg_.hopSize);
    const size_t half = static_cast<size_t>(cfg_.frameSize / 2);
    diff_.assign(half, 0.0f);
    cmnd_.assign(half, 0.0f);
    pad_.assign(static_cast<size_t>(cfg_.frameSize), 0.0f);
}

// [worker]
YinFrame Yin::analyzeFrame(const float* x, double sampleRate) {
    YinFrame out;
    const int N = cfg_.frameSize;
    const int W = N / 2;                          // cửa sổ tích phân
    // Cần x[j + τ + 1] với j < W (τ + 1 cho nội suy) → τ ≤ N − W − 2
    const int tauMax = std::min(N - W - 2, static_cast<int>(std::ceil(sampleRate / cfg_.minHz)));
    const int tauMin = std::max(2, static_cast<int>(std::floor(sampleRate / cfg_.maxHz)));
    if (tauMax <= tauMin + 1) return out;

    double energy = 0.0;
    for (int i = 0; i < N; ++i) energy += static_cast<double>(x[i]) * x[i];
    out.rmsDb = static_cast<float>(10.0 * std::log10(energy / N + 1e-30));
    if (out.rmsDb < cfg_.silenceDb) {
        out.silent = true;
        return out;
    }

    // Bước 1: hàm hiệu d(τ) = Σ_{j<W} (x[j] − x[j+τ])²
    float* d = diff_.data();
    d[0] = 0.0f;
    for (int tau = 1; tau <= tauMax + 1; ++tau) {
        const float* y = x + tau;
        float acc = 0.0f;
        {
            // Cho phép đổi thứ tự cộng để compiler vector hoá (NEON). Sai số float không đáng kể.
#pragma clang fp reassociate(on)
            for (int j = 0; j < W; ++j) {
                const float e = x[j] - y[j];
                acc += e * e;
            }
        }
        d[tau] = acc;
    }

    // Bước 2: chuẩn hoá tích luỹ d'(τ) = d(τ) · τ / Σ_{k=1..τ} d(k)
    float* c = cmnd_.data();
    c[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax + 1; ++tau) {
        running += d[tau];
        c[tau] = running > 0.0 ? static_cast<float>(static_cast<double>(d[tau]) * tau / running) : 1.0f;
    }

    // Bước 3: τ đầu tiên dưới ngưỡng, rồi đi xuống tới đáy của hố đó.
    int tau = -1;
    for (int t = tauMin; t <= tauMax; ++t) {
        if (c[t] < cfg_.threshold) {
            while (t + 1 <= tauMax && c[t + 1] < c[t]) ++t;
            tau = t;
            break;
        }
    }
    if (tau < 0) {
        // Không frame nào dưới ngưỡng → không voiced. Vẫn trả aperiodicity nhỏ nhất để tính confidence.
        float best = 1e9f;
        for (int t = tauMin; t <= tauMax; ++t) best = std::min(best, c[t]);
        out.aperiodicity = best;
        return out;
    }

    // Bước 4: nội suy parabol trên d(τ) thô (d' bị lệch nhẹ do mẫu số thay đổi theo τ).
    double shift = 0.0;
    const double a = d[tau - 1], b = d[tau], cc = d[tau + 1];
    const double denom = a - 2.0 * b + cc;
    if (denom > 0.0) shift = std::clamp(0.5 * (a - cc) / denom, -1.0, 1.0);
    const double period = tau + shift;
    const double hz = sampleRate / period;

    out.aperiodicity = c[tau];
    if (hz < cfg_.minHz || hz > cfg_.maxHz) return out;
    out.hz = static_cast<float>(hz);
    out.voiced = true;
    return out;
}

// [worker]
PitchEstimate Yin::analyze(const float* mono, int64_t numSamples, double sampleRate) {
    PitchEstimate p;
    if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0) return p;

    midi_.clear();
    aper_.clear();
    const int N = cfg_.frameSize;
    auto runFrame = [&](const float* frame) {
        const YinFrame f = analyzeFrame(frame, sampleRate);
        ++p.totalFrames;
        if (f.silent) return;
        ++p.analyzedFrames;
        if (!f.voiced) return;
        ++p.voicedFrames;
        midi_.push_back(hzToMidi(f.hz, cfg_.a4Hz));
        aper_.push_back(f.aperiodicity);
    };

    if (numSamples < N) {
        std::fill(pad_.begin(), pad_.end(), 0.0f);
        std::copy(mono, mono + numSamples, pad_.begin());
        runFrame(pad_.data());
    } else {
        for (int64_t start = 0; start + N <= numSamples; start += cfg_.hopSize) runFrame(mono + start);
    }
    if (p.voicedFrames == 0 || p.analyzedFrames == 0) return p;

    // Median thô → giữ frame trong ±1 nửa cung (loại lỗi quãng tám, nốt lướt) → median lần 2.
    tmp_ = midi_;
    const float rough = medianInPlace(tmp_);
    tmp_.clear();
    std::vector<float> stableAper;
    stableAper.reserve(aper_.size());
    for (size_t i = 0; i < midi_.size(); ++i) {
        if (std::fabs(midi_[i] - rough) <= 1.0f) {
            tmp_.push_back(midi_[i]);
            stableAper.push_back(aper_[i]);
        }
    }
    p.stableFrames = static_cast<int>(tmp_.size());
    if (p.stableFrames == 0) return p;

    const float m = medianInPlace(tmp_);
    const float medAper = medianInPlace(stableAper);
    p.hz = midiToHz(m, cfg_.a4Hz);
    p.rootNote = std::clamp(static_cast<int>(std::lround(m)), 0, 127);
    p.cents = (m - static_cast<float>(p.rootNote)) * 100.0f;
    const float ratio = static_cast<float>(p.stableFrames) / static_cast<float>(p.analyzedFrames);
    p.confidence = std::clamp(ratio * (1.0f - medAper), 0.0f, 1.0f);
    p.ok = true;
    return p;
}

} // namespace le
