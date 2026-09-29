#pragma once
// Metronome + count-in (P1-10, 04 §2.3). [RT] trừ prepare().
// Tiếng click tổng hợp SẴN trong prepare(): sine 1.5 kHz (phách mạnh) và 1 kHz (phách thường), dài 30 ms,
// có envelope. Mỗi beat nguyên k, click bắt đầu đúng tại sample Transport::sampleAtBeat(k). Sample đầu tiên
// của click khác 0, nên "click rơi đúng sample" kiểm được bằng phát hiện onset.
#include <cstdint>
#include <vector>

#include "core/BlockSplitter.h"
#include "core/Transport.h"

namespace le::core {

class Metronome {
public:
    enum Mode : int { Off = 0, On = 1, RecordOnly = 2 };   // LE_CMD_METRONOME i0
    static constexpr float kDefaultVolume = 0.8f;

    // [main] Cấp phát và tổng hợp 2 buffer click.
    void prepare(double sampleRate);

    // [RT]
    void setMode(int mode, float volume) noexcept [[clang::nonblocking]];
    void setCountingIn(bool on) noexcept [[clang::nonblocking]] { countingIn_ = on; }   // count-in luôn kêu
    void setRecording(bool on) noexcept [[clang::nonblocking]] { recording_ = on; }      // cho chế độ RecordOnly
    void reset() noexcept [[clang::nonblocking]] { clickPos_ = -1; }                    // cắt đuôi click (transport stop)

    // [RT] Cộng click vào L/R trong phạm vi segment (L/R là con trỏ tới frame 0 của block).
    void render(const Transport& tr, const Segment& seg, float* L, float* R) noexcept [[clang::nonblocking]];

    int mode() const noexcept [[clang::nonblocking]] { return mode_; }
    float volume() const noexcept [[clang::nonblocking]] { return volume_; }
    bool audible() const noexcept [[clang::nonblocking]] {
        return mode_ == On || countingIn_ || (mode_ == RecordOnly && recording_);
    }
    int clickLength() const noexcept { return (int) accent_.size(); }
    const std::vector<float>& accentClick() const noexcept { return accent_; }
    const std::vector<float>& normalClick() const noexcept { return normal_; }

private:
    std::vector<float> accent_, normal_;   // [main] tạo trong prepare, RT chỉ đọc
    int mode_ = Off;
    float volume_ = kDefaultVolume;
    bool countingIn_ = false, recording_ = false;
    const float* click_ = nullptr;         // click đang phát
    int clickPos_ = -1;                     // -1 = không có
};

} // namespace le::core
