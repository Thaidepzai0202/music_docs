// CompressorFx — nén dải động feed-forward, liên kết stereo (04 §9, P3-14). [RT] trừ prepare().
// Tham số: 0 threshold −60–0 dB · 1 ratio 1–20 · 2 attack 0.1–100 ms · 3 release 10–1000 ms · 4 makeup 0–24 dB.
// Mỗi sample:
//   mức = max(|L|, |R|) theo dB → "gain computer" (knee mềm 6 dB): vượt ngưỡng x dB thì giảm x·(1 − 1/ratio) dB
//   → làm mượt lượng giảm: tăng theo attack, nhả theo release (hệ số mũ tính sẵn khi đổi tham số)
//   → gain = 10^((makeup − lượng giảm)/20), nhân vào cả 2 kênh (giữ ảnh stereo).
// Ví dụ: tín hiệu −6 dBFS, ngưỡng −18, ratio 4 → vượt 12 dB → giảm 9 dB → ra −15 dBFS.
#pragma once

#include "dsp/Processor.h"
#include "dsp/Smoother.h"

#include <array>

namespace le::dsp {

class CompressorFx final : public Processor {
public:
    static constexpr float kKneeDb = 6.0f;

    CompressorFx();
    FxType type() const noexcept override { return FxType::Compressor; }
    void prepare(double sampleRate, int maxBlock) override;
    void reset() noexcept [[clang::nonblocking]] override;
    void process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] override;
    void setParam(int id, float value) noexcept [[clang::nonblocking]] override;
    float getParam(int id) const noexcept [[clang::nonblocking]] override;

    // [RT] Lượng giảm hiện tại (dB, ≥ 0) — để meter "GR" trên UI nếu cần.
    float gainReductionDb() const noexcept [[clang::nonblocking]] { return envDb_; }

    // Gain computer tĩnh (dB vào → dB cần giảm), tách ra để test.
    static float computeReductionDb(float levelDb, float thresholdDb, float ratio) noexcept [[clang::nonblocking]];

private:
    void updateTimeCoeffs() noexcept [[clang::nonblocking]];

    double sampleRate_ = 48000.0;
    std::array<float, 5> params_{};
    LinearSmoother threshold_, ratio_, makeup_;
    float attackCoef_ = 0.0f, releaseCoef_ = 0.0f;
    bool  timeDirty_ = true;
    float envDb_ = 0.0f;
};

} // namespace le::dsp
