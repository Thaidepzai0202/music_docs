#include "dsp/fx/CompressorFx.h"

#include <algorithm>
#include <cmath>

namespace le::dsp {

CompressorFx::CompressorFx() {
    for (const ParamInfo& p : paramInfo(FxType::Compressor)) params_[static_cast<size_t>(p.id)] = p.defaultValue;
}

// [main]
void CompressorFx::prepare(double sampleRate, int /*maxBlock*/) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    threshold_.prepare(sampleRate_);
    ratio_.prepare(sampleRate_);
    makeup_.prepare(sampleRate_);
    for (int id = 0; id < 5; ++id) setParam(id, params_[static_cast<size_t>(id)]);
    reset();
}

void CompressorFx::reset() noexcept [[clang::nonblocking]] {
    threshold_.snap();
    ratio_.snap();
    makeup_.snap();
    updateTimeCoeffs();
    envDb_ = 0.0f;
}

void CompressorFx::setParam(int id, float value) noexcept [[clang::nonblocking]] {
    if (id < 0 || id > 4) return;
    const float v = clampParam(FxType::Compressor, id, value);
    params_[static_cast<size_t>(id)] = v;
    switch (id) {
        case 0: threshold_.setTarget(v); break;
        case 1: ratio_.setTarget(v); break;
        case 4: makeup_.setTarget(v); break;
        default: timeDirty_ = true; break;   // attack / release: tính lại hệ số ở đầu block kế tiếp
    }
}

float CompressorFx::getParam(int id) const noexcept [[clang::nonblocking]] {
    return (id >= 0 && id <= 4) ? params_[static_cast<size_t>(id)] : 0.0f;
}

// Hệ số làm mượt 1 cực: sau `t` giây đi được 63 % quãng đường (hằng số thời gian).
void CompressorFx::updateTimeCoeffs() noexcept [[clang::nonblocking]] {
    const double a = std::max(0.0001, static_cast<double>(params_[2]) / 1000.0);
    const double r = std::max(0.001, static_cast<double>(params_[3]) / 1000.0);
    attackCoef_ = static_cast<float>(std::exp(-1.0 / (a * sampleRate_)));
    releaseCoef_ = static_cast<float>(std::exp(-1.0 / (r * sampleRate_)));
    timeDirty_ = false;
}

float CompressorFx::computeReductionDb(float levelDb, float thresholdDb, float ratio) noexcept [[clang::nonblocking]] {
    const float slope = 1.0f - 1.0f / std::max(1.0f, ratio);
    const float over = levelDb - thresholdDb;
    if (2.0f * over <= -kKneeDb) return 0.0f;                    // dưới knee: không nén
    if (2.0f * over >= kKneeDb) return slope * over;               // trên knee: nén đủ tỉ lệ
    const float x = over + kKneeDb * 0.5f;                         // trong knee: chuyển mượt bậc 2
    return slope * x * x / (2.0f * kKneeDb);
}

// [RT]
void CompressorFx::process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] {
    if (timeDirty_) updateTimeCoeffs();
    const bool stereo = numCh >= 2;
    float* L = io[0];
    float* R = stereo ? io[1] : nullptr;
    constexpr float kDbPerLog2 = 6.0205999f;                      // 20·log10(2)
    for (int s = 0; s < ctx.numFrames; ++s) {
        const float t = threshold_.next();
        const float ratio = ratio_.next();
        const float makeup = makeup_.next();
        const float level = std::max(std::fabs(L[s]), stereo ? std::fabs(R[s]) : 0.0f);
        const float levelDb = kDbPerLog2 * std::log2(std::max(level, 1.0e-9f));
        const float target = computeReductionDb(levelDb, t, ratio);
        const float coef = target > envDb_ ? attackCoef_ : releaseCoef_;
        envDb_ = target + (envDb_ - target) * coef;
        const float gain = std::exp2((makeup - envDb_) / kDbPerLog2);
        L[s] *= gain;
        if (stereo) R[s] *= gain;
    }
    guardOutput(io, numCh, ctx.numFrames);   // NaN / Inf → 0 + reset (Processor.h)
}

} // namespace le::dsp
