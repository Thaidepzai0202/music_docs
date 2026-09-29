// Eq3Fx — EQ 3 băng: low shelf 200 Hz, mid peak 1 kHz (Q 0.7), high shelf 5 kHz, mỗi băng ±15 dB
// (04 §9, P3-14; dùng cả cho master P3-15). [RT] trừ prepare().
// Hệ số biquad TỰ TÍNH theo RBJ Audio EQ Cookbook (dsp/Biquad.h) — không dùng IIR::Coefficients::make*
// (cấp phát). Gain trượt 20 ms; khi đang trượt, hệ số tính lại mỗi khúc 16 sample.
#pragma once

#include "dsp/Biquad.h"
#include "dsp/Processor.h"
#include "dsp/Smoother.h"

#include <array>

namespace le::dsp {

class Eq3Fx final : public Processor {
public:
    static constexpr double kLowHz = 200.0, kMidHz = 1000.0, kMidQ = 0.7, kHighHz = 5000.0;

    Eq3Fx();
    FxType type() const noexcept override { return FxType::EQ3; }
    void prepare(double sampleRate, int maxBlock) override;
    void reset() noexcept [[clang::nonblocking]] override;
    void process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] override;
    void setParam(int id, float value) noexcept [[clang::nonblocking]] override;
    float getParam(int id) const noexcept [[clang::nonblocking]] override;

private:
    void updateCoeffs(int band) noexcept [[clang::nonblocking]];

    double sampleRate_ = 48000.0;
    std::array<float, 3> params_{};
    std::array<LinearSmoother, 3> gain_;
    std::array<BiquadCoeffs, 3> k_{};
    std::array<std::array<BiquadState, 3>, 2> st_{};   // [kênh][băng]
};

} // namespace le::dsp
