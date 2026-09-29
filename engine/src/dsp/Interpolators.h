// Interpolators — đọc sample ở vị trí lẻ (pos = 12.37). Dùng chung cho Sampler và AudioClipPlayer. [RT]
// Toàn bộ là toán số inline: không cấp phát, không lock → an toàn trên audio thread.
//
// Hermite 4 điểm (Catmull-Rom): dùng 4 sample liền nhau x[i−1], x[i], x[i+1], x[i+2] để vẽ một đường cong
// mượt đi qua x[i] và x[i+1]. So với nội suy tuyến tính (nối thẳng 2 điểm), ít làm tối tiếng và ít
// "răng cưa" khi đổi tốc độ phát (04 §4, §6.2). Tại t = 0 trả về đúng x[i] (không đổi dữ liệu gốc).
//
// Các hàm read*() nhận `ch` = AudioData::channel(c): nhờ vùng đệm kPadFrames frame 0 ở hai đầu,
// pos được phép nằm trong [−1, numFrames + 1] mà không cần kiểm tra biên.
#pragma once

#include <cstdint>

namespace le::dsp {

// t ∈ [0, 1): vị trí giữa x0 và x1.
inline float hermite4(float t, float xm1, float x0, float x1, float x2) noexcept [[clang::nonblocking]] {
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

inline float linear2(float t, float x0, float x1) noexcept [[clang::nonblocking]] {
    return x0 + (x1 - x0) * t;
}

// floor cho số có thể âm, trả int64 (static_cast cắt về 0 nên sai với số âm).
inline int64_t floorToInt(double x) noexcept [[clang::nonblocking]] {
    const auto i = static_cast<int64_t>(x);
    return (static_cast<double>(i) > x) ? i - 1 : i;
}

inline float readHermite(const float* ch, double pos) noexcept [[clang::nonblocking]] {
    const int64_t i = floorToInt(pos);
    const auto t = static_cast<float>(pos - static_cast<double>(i));
    return hermite4(t, ch[i - 1], ch[i], ch[i + 1], ch[i + 2]);
}

inline float readLinear(const float* ch, double pos) noexcept [[clang::nonblocking]] {
    const int64_t i = floorToInt(pos);
    const auto t = static_cast<float>(pos - static_cast<double>(i));
    return linear2(t, ch[i], ch[i + 1]);
}

// Đọc Hermite trong một vòng lặp [loopStart, loopEnd): các điểm lân cận vượt qua mép vòng được lấy
// vòng sang đầu kia → nối vòng liền mạch. pos phải nằm trong [loopStart, loopEnd). Vòng dài ≥ 4 frame.
inline float readHermiteLoop(const float* ch, double pos, int64_t loopStart, int64_t loopEnd) noexcept
    [[clang::nonblocking]] {
    const int64_t i = floorToInt(pos);
    const auto t = static_cast<float>(pos - static_cast<double>(i));
    if (i - 1 >= loopStart && i + 2 < loopEnd) return hermite4(t, ch[i - 1], ch[i], ch[i + 1], ch[i + 2]);
    const int64_t len = loopEnd - loopStart;
    auto wrap = [&](int64_t k) noexcept [[clang::nonblocking]] {
        if (k < loopStart) return k + len;
        if (k >= loopEnd) return k - len;
        return k;
    };
    return hermite4(t, ch[wrap(i - 1)], ch[wrap(i)], ch[wrap(i + 1)], ch[wrap(i + 2)]);
}

} // namespace le::dsp
