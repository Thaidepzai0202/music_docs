#include "dsp/fx/FilterFx.h"

#include <algorithm>
#include <cmath>

namespace le::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kChunk = 16;   // tính lại hệ số mỗi 16 sample khi tham số đang trượt
}

FilterFx::FilterFx() {
    for (const ParamInfo& p : paramInfo(FxType::Filter)) params_[static_cast<size_t>(p.id)] = p.defaultValue;
}

// [main]
void FilterFx::prepare(double sampleRate, int /*maxBlock*/) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    logCutoff_.prepare(sampleRate_);
    q_.prepare(sampleRate_);
    modeFadeLen_ = std::max(1, static_cast<int>(0.005 * sampleRate_ + 0.5));
    for (int id = 0; id < 3; ++id) setParam(id, params_[static_cast<size_t>(id)]);
    reset();
}

void FilterFx::reset() noexcept [[clang::nonblocking]] {
    logCutoff_.snap();
    q_.snap();
    prevMode_ = mode_;
    modeFadeLeft_ = 0;
    ch_.fill(Channel{});
    updateCoeffs();
}

void FilterFx::setParam(int id, float value) noexcept [[clang::nonblocking]] {
    if (id < 0 || id > 2) return;
    const float v = clampParam(FxType::Filter, id, value);
    params_[static_cast<size_t>(id)] = v;
    if (id == 0) {
        const int m = std::clamp(static_cast<int>(v + 0.5f), 0, 2);
        if (m != mode_) {
            prevMode_ = mode_;
            mode_ = m;
            modeFadeLeft_ = modeFadeLen_;
        }
    } else if (id == 1) {
        logCutoff_.setTarget(std::log2(v));
    } else {
        q_.setTarget(v);
    }
}

float FilterFx::getParam(int id) const noexcept [[clang::nonblocking]] {
    return (id >= 0 && id <= 2) ? params_[static_cast<size_t>(id)] : 0.0f;
}

// Hệ số SVF TPT (Cytomic): g = tan(π·fc/sr), k = 1/Q.
void FilterFx::updateCoeffs() noexcept [[clang::nonblocking]] {
    const double fc = std::min(std::exp2(static_cast<double>(logCutoff_.current())), 0.45 * sampleRate_);
    const double g = std::tan(kPi * fc / sampleRate_);
    const double k = 1.0 / std::max(0.1, static_cast<double>(q_.current()));
    const double a1 = 1.0 / (1.0 + g * (g + k));
    k_ = static_cast<float>(k);
    a1_ = static_cast<float>(a1);
    a2_ = static_cast<float>(g * a1);
    a3_ = static_cast<float>(g * g * a1);
}

// v1 = đầu ra "band", v2 = đầu ra "low" của SVF.
float FilterFx::select(int mode, float x, float v1, float v2) const noexcept [[clang::nonblocking]] {
    switch (mode) {
        case 1: return x - k_ * v1 - v2;   // high-pass
        case 2: return k_ * v1;             // band-pass, đỉnh 0 dB
        default: return v2;                 // low-pass
    }
}

// [RT]
void FilterFx::process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] {
    const int n = ctx.numFrames;
    const int nc = std::clamp(numCh, 1, 2);
    for (int s0 = 0; s0 < n; s0 += kChunk) {
        const int m = std::min(kChunk, n - s0);
        if (logCutoff_.isSmoothing() || q_.isSmoothing()) {
            updateCoeffs();
            logCutoff_.skip(m);
            q_.skip(m);
        }
        const int fadeStart = modeFadeLeft_;
        for (int c = 0; c < nc; ++c) {
            Channel& st = ch_[static_cast<size_t>(c)];
            float* x = io[c] + s0;
            int fade = fadeStart;
            for (int i = 0; i < m; ++i) {
                const float in = x[i];
                const float v3 = in - st.ic2;
                const float v1 = a1_ * st.ic1 + a2_ * v3;
                const float v2 = st.ic2 + a2_ * st.ic1 + a3_ * v3;
                st.ic1 = 2.0f * v1 - st.ic1;
                st.ic2 = 2.0f * v2 - st.ic2;
                float y = select(mode_, in, v1, v2);
                if (fade > 0) {   // crossfade 5 ms từ mode cũ sang mode mới
                    const float t = static_cast<float>(fade) / static_cast<float>(modeFadeLen_);
                    y = y * (1.0f - t) + select(prevMode_, in, v1, v2) * t;
                    --fade;
                }
                x[i] = y;
            }
        }
        modeFadeLeft_ = std::max(0, modeFadeLeft_ - m);
    }
    guardOutput(io, numCh, ctx.numFrames);   // NaN / Inf → 0 + reset (Processor.h)
}

} // namespace le::dsp
