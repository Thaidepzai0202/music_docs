// DelayFx — delay đồng bộ tempo, feedback có lọc thông thấp, ping-pong (04 §9, P3-13). [RT] trừ prepare().
// Tham số: 0 time = chỉ số nhịp (xem kNoteBeats) · 1 feedback 0–0.95 · 2 mix 0–1 · 3 ping-pong 0/1.
//   - Thời gian delay = số beat của nhịp · 60/bpm · sr, tính lại mỗi block từ ctx.bpm. Đổi BPM/nhịp → thời
//     gian trượt trong 100 ms (nghe như băng chạy nhanh/chậm thoáng qua, không click).
//   - Buffer 4 s mỗi kênh cấp phát trong prepare(); thời gian dài hơn 4 s (BPM rất chậm) bị kẹp ở 4 s.
//   - Đọc vị trí lẻ bằng Hermite 4 điểm. Lọc thông thấp 1 cực (~6 kHz) nằm TRONG vòng feedback → mỗi
//     tiếng vọng tối dần như delay analog; tiếng vọng đầu tiên vẫn nguyên vẹn.
//   - Ping-pong (cần 2 kênh): input trộn mono đi vào kênh trái, vọng qua lại trái → phải → trái…
//   - mix: out = dry·(1 − mix) + wet·mix.
#pragma once

#include "dsp/Processor.h"
#include "dsp/Smoother.h"

#include <array>
#include <vector>

namespace le::dsp {

class DelayFx final : public Processor {
public:
    // Độ dài (beat, nốt đen = 1) của 11 lựa chọn: 1/16T 1/16 1/16. 1/8T 1/8 1/8. 1/4T 1/4 1/4. 1/2T 1/2
    static constexpr std::array<double, 11> kNoteBeats = {0.25 * 2.0 / 3.0, 0.25, 0.375, 0.5 * 2.0 / 3.0, 0.5, 0.75,
                                                         2.0 / 3.0,        1.0,  1.5,   4.0 / 3.0,        2.0};
    static constexpr double kMaxSeconds = 4.0;

    DelayFx();
    FxType type() const noexcept override { return FxType::Delay; }
    void prepare(double sampleRate, int maxBlock) override;
    void reset() noexcept [[clang::nonblocking]] override;
    void process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] override;
    void setParam(int id, float value) noexcept [[clang::nonblocking]] override;
    float getParam(int id) const noexcept [[clang::nonblocking]] override;

    // [any] Số sample delay ứng với nhịp đang chọn ở bpm (đã kẹp) — cho test.
    double delaySamplesFor(double bpm) const noexcept [[clang::nonblocking]];

private:
    float readTap(const float* buf, double delay) const noexcept [[clang::nonblocking]];

    double sampleRate_ = 48000.0;
    std::array<float, 4> params_{};
    std::array<std::vector<float>, 2> buf_;   // cấp phát trong prepare, không đổi kích thước sau đó
    int64_t len_ = 0, write_ = 0;
    LinearSmoother time_, feedback_, mix_;
    double lastTarget_ = -1.0;
    bool   needSnapTime_ = true;              // lần đầu (sau reset) nhảy thẳng tới thời gian đúng
    float  lpCoef_ = 0.5f;
    std::array<float, 2> lpState_{};
};

} // namespace le::dsp
