#include "spike/measure/LoadGenerator.h"

#include <algorithm>
#include <cmath>

namespace le::spike {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr float  kSilence = 1.0e-4f;   // -80 dB: dưới mức này coi như envelope đã tắt

// Nội suy Hermite 4 điểm (Catmull-Rom). xm1, x0, x1, x2 là 4 sample liên tiếp, t ∈ [0, 1)
// là vị trí giữa x0 và x1. Đây là công thức sampler thật sẽ dùng (04 §4, §6.2).
inline float hermite4(float t, float xm1, float x0, float x1, float x2) noexcept [[clang::nonblocking]] {
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

float coefForTime(double seconds, double sr) {
    // Sau `seconds`, mức giảm còn e^-5 (~-43 dB): level *= coef mỗi sample.
    return static_cast<float>(std::exp(-5.0 / (seconds * sr)));
}
} // namespace

// [main]
void LoadGenerator::prepare(double sampleRate, int /*maxBlock*/, uint32_t seed) {
    sampleRate_ = sampleRate;
    length_ = static_cast<int>(std::lround(sampleRate));   // 1 giây

    // Buffer nguồn: 220 Hz + các hoạ âm (số chu kỳ nguyên trong 1 giây → loop liền mạch) + chút nhiễu,
    // giống một sample nhạc cụ thật hơn là sine trần.
    source_.assign(static_cast<size_t>(length_ + 3), 0.0f);
    uint32_t noise = seed | 1u;
    for (int k = 0; k < length_; ++k) {
        const double t = k / sampleRate;
        noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
        const double nz = (noise * (1.0 / 4294967296.0)) * 2.0 - 1.0;
        const double x = 0.50 * std::sin(2 * kPi * 220 * t)
                       + 0.25 * std::sin(2 * kPi * 440 * t + 0.3)
                       + 0.12 * std::sin(2 * kPi * 660 * t + 1.1)
                       + 0.03 * nz;
        source_[static_cast<size_t>(k + 1)] = static_cast<float>(x);
    }
    source_[0] = source_[static_cast<size_t>(length_)];                     // x[-1]  = x[len-1]
    source_[static_cast<size_t>(length_ + 1)] = source_[1];                // x[len] = x[0]
    source_[static_cast<size_t>(length_ + 2)] = source_[2];                // x[len+1] = x[1]

    attackStep_      = static_cast<float>(1.0 / (0.005 * sampleRate));
    decayCoef_       = coefForTime(0.200, sampleRate);
    releaseCoef_     = coefForTime(0.300, sampleRate);
    fastReleaseCoef_ = coefForTime(0.003, sampleRate);
    // 128 voice cộng lại vẫn không vượt 0 dBFS (mỗi voice ≤ 0.9 · 0.007 · 1 ≈ -44 dB).
    voiceGain_ = 0.9f / kMaxVoices;

    for (int s = 0; s < 25; ++s) pitchRatio_[static_cast<size_t>(s)] = std::pow(2.0, (s - 12) / 12.0);
    for (int p = 0; p < 16; ++p) {                         // pan luật -3 dB (04 §11)
        const double theta = (p / 15.0) * kPi * 0.5;
        panL_[static_cast<size_t>(p)] = static_cast<float>(std::cos(theta));
        panR_[static_cast<size_t>(p)] = static_cast<float>(std::sin(theta));
    }

    voices_.fill(Voice{});
    rng_ = seed | 1u;
    activeVoices_.store(0, std::memory_order_relaxed);
}

// [any] (kể cả [RT])
void LoadGenerator::setVoices(int n) noexcept [[clang::nonblocking]] {
    targetVoices_.store(std::clamp(n, 0, kMaxVoices), std::memory_order_relaxed);
}

// [RT] xorshift32: vài phép dịch bit, không cấp phát, không lock.
float LoadGenerator::nextRandom01() noexcept [[clang::nonblocking]] {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ >> 8) * (1.0f / 16777216.0f);
}

// [RT] "Bấm một nốt": chọn cao độ, vị trí bắt đầu, pan, velocity, độ dài nốt.
void LoadGenerator::noteOn(Voice& v) noexcept [[clang::nonblocking]] {
    const int semi = std::min(24, static_cast<int>(nextRandom01() * 25.0f));
    const int pan  = std::min(15, static_cast<int>(nextRandom01() * 16.0f));
    const float vel = 0.5f + 0.5f * nextRandom01();
    v.inc = pitchRatio_[static_cast<size_t>(semi)];
    v.pos = static_cast<double>(nextRandom01()) * (length_ - 1);
    v.gainL = voiceGain_ * vel * panL_[static_cast<size_t>(pan)];
    v.gainR = voiceGain_ * vel * panR_[static_cast<size_t>(pan)];
    v.samplesToRelease = static_cast<int32_t>((0.3f + 0.9f * nextRandom01()) * static_cast<float>(sampleRate_));
    v.level = 0.0f;
    v.stage = Stage::Attack;
    v.fastRelease = false;
}

// [RT]
void LoadGenerator::processRt(float* const* out, int numCh, int n) noexcept [[clang::nonblocking]] {
    if (length_ == 0 || out == nullptr || numCh <= 0) return;
    const int target = targetVoices_.load(std::memory_order_relaxed);

    // Áp số voice mong muốn ở đầu block: bật voice còn thiếu, cho voice thừa fade nhanh.
    for (int i = 0; i < kMaxVoices; ++i) {
        Voice& v = voices_[static_cast<size_t>(i)];
        if (i < target) {
            if (v.stage == Stage::Off) noteOn(v);
        } else if (v.stage != Stage::Off) {
            v.fastRelease = true;
            v.stage = Stage::Release;
        }
    }

    float* L = out[0];
    float* R = numCh >= 2 ? out[1] : nullptr;
    const float* src = source_.data();
    const double len = static_cast<double>(length_);
    int active = 0;

    for (int vi = 0; vi < kMaxVoices; ++vi) {
        Voice& v = voices_[static_cast<size_t>(vi)];
        if (v.stage == Stage::Off) continue;

        for (int s = 0; s < n; ++s) {
            // 1) Envelope
            switch (v.stage) {
                case Stage::Attack:
                    v.level += attackStep_;
                    if (v.level >= 1.0f) { v.level = 1.0f; v.stage = Stage::Decay; }
                    break;
                case Stage::Decay:
                    v.level = sustain_ + (v.level - sustain_) * decayCoef_;
                    if (v.level - sustain_ < kSilence) { v.level = sustain_; v.stage = Stage::Sustain; }
                    break;
                case Stage::Sustain:
                    break;
                case Stage::Release:
                    v.level *= v.fastRelease ? fastReleaseCoef_ : releaseCoef_;
                    break;
                case Stage::Off:
                    break;
            }
            if (v.stage != Stage::Release && --v.samplesToRelease <= 0) v.stage = Stage::Release;

            // 2) Đọc buffer với bước phân số + Hermite. source_[i] là x[i-1] (có 1 sample đệm đầu).
            const int   i = static_cast<int>(v.pos);
            const float t = static_cast<float>(v.pos - i);
            const float* p = src + i;
            const float y = hermite4(t, p[0], p[1], p[2], p[3]) * v.level;

            // 3) Pan + cộng dồn stereo
            L[s] += y * v.gainL;
            if (R != nullptr) R[s] += y * v.gainR;

            v.pos += v.inc;
            if (v.pos >= len) v.pos -= len;   // buffer 1 giây phát vòng

            // 4) Hết envelope → nốt mới (nếu voice này vẫn được yêu cầu) hoặc tắt hẳn
            if (v.stage == Stage::Release && v.level < kSilence) {
                if (!v.fastRelease && vi < target) {
                    noteOn(v);
                } else {
                    v.stage = Stage::Off;
                    v.level = 0.0f;
                    break;
                }
            }
        }
        if (v.stage != Stage::Off) ++active;
    }
    activeVoices_.store(active, std::memory_order_relaxed);
}

} // namespace le::spike
