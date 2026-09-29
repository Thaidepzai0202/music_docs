#include "dsp/fx/Eq3Fx.h"

#include <algorithm>

namespace le::dsp {

namespace {
constexpr int kChunk = 16;
}

Eq3Fx::Eq3Fx() {
    for (const ParamInfo& p : paramInfo(FxType::EQ3)) params_[static_cast<size_t>(p.id)] = p.defaultValue;
}

// [main]
void Eq3Fx::prepare(double sampleRate, int /*maxBlock*/) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    for (auto& g : gain_) g.prepare(sampleRate_);
    for (int id = 0; id < 3; ++id) setParam(id, params_[static_cast<size_t>(id)]);
    reset();
}

void Eq3Fx::reset() noexcept [[clang::nonblocking]] {
    for (int b = 0; b < 3; ++b) {
        gain_[static_cast<size_t>(b)].snap();
        updateCoeffs(b);
    }
    for (auto& ch : st_)
        for (auto& s : ch) s.reset();
}

void Eq3Fx::setParam(int id, float value) noexcept [[clang::nonblocking]] {
    if (id < 0 || id > 2) return;
    const float v = clampParam(FxType::EQ3, id, value);
    params_[static_cast<size_t>(id)] = v;
    gain_[static_cast<size_t>(id)].setTarget(v);
}

float Eq3Fx::getParam(int id) const noexcept [[clang::nonblocking]] {
    return (id >= 0 && id <= 2) ? params_[static_cast<size_t>(id)] : 0.0f;
}

void Eq3Fx::updateCoeffs(int band) noexcept [[clang::nonblocking]] {
    const double g = gain_[static_cast<size_t>(band)].current();
    auto& k = k_[static_cast<size_t>(band)];
    switch (band) {
        case 0: k = BiquadCoeffs::lowShelf(kLowHz, g, sampleRate_); break;
        case 1: k = BiquadCoeffs::peaking(kMidHz, kMidQ, g, sampleRate_); break;
        default: k = BiquadCoeffs::highShelf(kHighHz, g, sampleRate_); break;
    }
}

// [RT]
void Eq3Fx::process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] {
    const int n = ctx.numFrames;
    const int nc = std::clamp(numCh, 1, 2);
    for (int s0 = 0; s0 < n; s0 += kChunk) {
        const int m = std::min(kChunk, n - s0);
        for (int b = 0; b < 3; ++b) {
            LinearSmoother& g = gain_[static_cast<size_t>(b)];
            if (g.isSmoothing()) {
                updateCoeffs(b);
                g.skip(m);
            }
        }
        for (int c = 0; c < nc; ++c) {
            auto& st = st_[static_cast<size_t>(c)];
            float* x = io[c] + s0;
            for (int i = 0; i < m; ++i) {
                float y = st[0].process(k_[0], x[i]);
                y = st[1].process(k_[1], y);
                x[i] = st[2].process(k_[2], y);
            }
        }
    }
    guardOutput(io, numCh, ctx.numFrames);   // NaN / Inf → 0 + reset (Processor.h)
}

} // namespace le::dsp
