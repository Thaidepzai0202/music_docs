// Biquad — bộ lọc bậc 2, hệ số TỰ TÍNH theo RBJ Audio EQ Cookbook (04 §9: KHÔNG dùng
// juce::dsp::IIR::Coefficients::make*, vì chúng cấp phát). Tính hệ số chỉ là toán số → gọi được trên RT.
// Dạng Transposed Direct Form II, hệ số và state bằng double (ổn định ở tần số thấp như shelf 200 Hz).
#pragma once

#include <cmath>

namespace le::dsp {

struct BiquadCoeffs {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;   // đã chia a0

    // RBJ: shelf với slope S = 1, peaking theo Q. gainDb = độ tăng/giảm ở dải phẳng (shelf) hoặc đỉnh (peak).
    static BiquadCoeffs lowShelf(double f0, double gainDb, double sr) noexcept [[clang::nonblocking]] {
        const double A = std::pow(10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / sr, c = std::cos(w);
        const double alpha = std::sin(w) / 2.0 * std::sqrt(2.0), sa = 2.0 * std::sqrt(A) * alpha;
        return norm(A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                    (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    static BiquadCoeffs highShelf(double f0, double gainDb, double sr) noexcept [[clang::nonblocking]] {
        const double A = std::pow(10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / sr, c = std::cos(w);
        const double alpha = std::sin(w) / 2.0 * std::sqrt(2.0), sa = 2.0 * std::sqrt(A) * alpha;
        return norm(A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                    (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
    static BiquadCoeffs peaking(double f0, double q, double gainDb, double sr) noexcept [[clang::nonblocking]] {
        const double A = std::pow(10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / sr, c = std::cos(w);
        const double alpha = std::sin(w) / (2.0 * q);
        return norm(1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    static BiquadCoeffs norm(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
        [[clang::nonblocking]] {
        return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
    }
};

struct BiquadState {
    double z1 = 0, z2 = 0;
    float process(const BiquadCoeffs& k, float xf) noexcept [[clang::nonblocking]] {
        const double x = xf;
        const double y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2;
        z2 = k.b2 * x - k.a2 * y;
        return static_cast<float>(y);
    }
    void reset() noexcept [[clang::nonblocking]] { z1 = z2 = 0; }
};

} // namespace le::dsp
