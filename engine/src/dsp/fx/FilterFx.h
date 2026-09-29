// FilterFx — lọc LP / HP / BP kiểu State Variable, dạng TPT (topology-preserving, Zavalishin; công thức
// của Cytomic, cùng thuật toán với juce::dsp::StateVariableTPTFilter). [RT] trừ prepare().
// Tham số: 0 mode {0 LP, 1 HP, 2 BP} · 1 cutoff 20–20000 Hz · 2 resonance Q 0.5–10.
//   - TPT tính trước độ cong (g = tan(π·fc/sr)) → biên độ tại cutoff ĐÚNG như bộ lọc analog:
//     LP/HP ở Q 0.707 đúng −3 dB; BP chuẩn hoá đỉnh 0 dB.
//   - Cutoff trượt theo thang log (quãng tám) trong 20 ms; hệ số chỉ tính lại mỗi khúc 16 sample khi đang trượt.
//   - Đổi mode: crossfade 5 ms giữa output cũ và mới (SVF cho cả 3 output cùng lúc) → không click.
#pragma once

#include "dsp/Processor.h"
#include "dsp/Smoother.h"

#include <array>

namespace le::dsp {

class FilterFx final : public Processor {
public:
    FilterFx();
    FxType type() const noexcept override { return FxType::Filter; }
    void prepare(double sampleRate, int maxBlock) override;
    void reset() noexcept [[clang::nonblocking]] override;
    void process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] override;
    void setParam(int id, float value) noexcept [[clang::nonblocking]] override;
    float getParam(int id) const noexcept [[clang::nonblocking]] override;

private:
    struct Channel { float ic1 = 0.0f, ic2 = 0.0f; };
    void updateCoeffs() noexcept [[clang::nonblocking]];
    float select(int mode, float x, float v1, float v2) const noexcept [[clang::nonblocking]];

    double sampleRate_ = 48000.0;
    std::array<float, 3> params_{};
    LinearSmoother logCutoff_, q_;
    float k_ = 1.414f, a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    int mode_ = 0, prevMode_ = 0, modeFadeLeft_ = 0, modeFadeLen_ = 240;
    std::array<Channel, 2> ch_{};
};

} // namespace le::dsp
