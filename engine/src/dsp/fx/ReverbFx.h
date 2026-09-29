// ReverbFx — reverb Freeverb (juce::Reverb) có mix dry/wet làm mượt (04 §9, P3-14). [RT] trừ prepare().
// Tham số: 0 size 0–1 · 1 damping 0–1 · 2 width 0–1 · 3 mix 0–1.
//   - juce::Reverb cấp phát buffer comb/allpass trong setSampleRate() (gọi ở prepare, [main]). Sau đó
//     processStereo/processMono/setParameters chỉ là toán số trên buffer có sẵn (tự làm mượt 10 ms bên trong).
//     JUCE không gắn [[clang::nonblocking]] nên các lời gọi này được tắt cảnh báo -Wfunction-effects tại chỗ;
//     RTSan (mac-rtsan) kiểm lúc chạy.
//   - juce::Reverb chạy chỉ-wet (dryLevel = 0); tự trộn out = dry·(1 − mix) + wet·mix bằng LinearSmoother.
#pragma once

#include "dsp/Processor.h"
#include "dsp/Smoother.h"

#include <array>
#include <memory>
#include <vector>

namespace juce { class Reverb; }

namespace le::dsp {

class ReverbFx final : public Processor {
public:
    ReverbFx();
    ~ReverbFx() override;
    FxType type() const noexcept override { return FxType::Reverb; }
    void prepare(double sampleRate, int maxBlock) override;
    void reset() noexcept [[clang::nonblocking]] override;
    void process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] override;
    void setParam(int id, float value) noexcept [[clang::nonblocking]] override;
    float getParam(int id) const noexcept [[clang::nonblocking]] override;

private:
    void applyParameters() noexcept [[clang::nonblocking]];

    std::unique_ptr<juce::Reverb> reverb_;    // tạo ở constructor ([main]); không bao giờ huỷ trên RT
    std::array<float, 4> params_{};
    std::array<std::vector<float>, 2> dry_;   // maxBlock mỗi kênh, cấp phát trong prepare
    LinearSmoother mix_;
    bool dirty_ = true;
    int  maxBlock_ = 0;
};

} // namespace le::dsp
