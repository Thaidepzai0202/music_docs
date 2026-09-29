// Sanitize — chặn NaN / ±Inf trong đường audio (R2 của engine/tools/docs/rt-review-2026-09-29.md). [RT]
//
// Vì sao cần: một phép tính hỏng (tham số lạ, dữ liệu file hỏng, mic trả rác) tạo NaN. NaN "dính": NaN + x = NaN,
// nên một mẫu NaN lọt vào state của filter IIR / delay line / reverb sẽ làm FX đó ra NaN MÃI MÃI, và Limiter
// không chặn được (std::max / std::clamp với NaN trả lại NaN). Ra tới loa thì nghe tiếng bụp hoặc mất tiếng.
//
// Kiểm rẻ, 1 lượt mỗi block: x·0 = 0 với mọi số hữu hạn, nhưng NaN·0 = NaN và Inf·0 = NaN. Cộng dồn các x·0
// → tổng là NaN khi và chỉ khi block có mẫu không hữu hạn. Vòng lặp chỉ có nhân + cộng nên compiler vector hoá
// (NEON 4 float / lệnh). Không dùng -ffast-math ở đâu trong engine, nên x·0 không bị "tối ưu" thành 0.
#pragma once

#include <cmath>

namespace le::dsp {

// true nếu x[0..n) toàn số hữu hạn.
inline bool allFinite(const float* x, int n) noexcept [[clang::nonblocking]] {
    float acc = 0.0f;
    {
#pragma clang fp reassociate(on)
        for (int i = 0; i < n; ++i) acc += x[i] * 0.0f;
    }
    return !std::isnan(acc);
}

// Thay mẫu không hữu hạn bằng 0 (giữ nguyên mẫu tốt). Trả số mẫu đã thay.
inline int sanitize(float* x, int n) noexcept [[clang::nonblocking]] {
    int bad = 0;
    for (int i = 0; i < n; ++i)
        if (!std::isfinite(x[i])) {
            x[i] = 0.0f;
            ++bad;
        }
    return bad;
}

// Kiểm + sửa một vùng stereo (R có thể nullptr). Trả true nếu đã phải sửa.
inline bool sanitizeIfNeeded(float* L, float* R, int n) noexcept [[clang::nonblocking]] {
    const bool okL = allFinite(L, n);
    const bool okR = R == nullptr || allFinite(R, n);
    if (okL && okR) return false;
    if (!okL) sanitize(L, n);
    if (!okR) sanitize(R, n);
    return true;
}

} // namespace le::dsp
