// LinearSmoother — làm mượt tham số FX (mặc định 20 ms, 04 §9). [RT] trừ prepare().
// setTarget() chỉ đặt đích; next()/skip() trượt tuyến tính tới đó trong đúng N sample (đếm, không so float)
// → đổi knob không nghe tiếng "zipper". Toàn bộ là toán số trên vài biến → an toàn RT.
#pragma once

#include <algorithm>
#include <cstdint>

namespace le::dsp {

class LinearSmoother {
public:
    // [main]
    void prepare(double sampleRate, double seconds = 0.020) {
        steps_ = std::max<int32_t>(1, static_cast<int32_t>(sampleRate * seconds + 0.5));
        left_ = 0;
        current_ = target_;
    }
    void setTarget(float t) noexcept [[clang::nonblocking]] {
        target_ = t;
        left_ = steps_;
        step_ = (target_ - current_) / static_cast<float>(steps_);
    }
    // Nhảy ngay tới đích (reset(), tạo mới trên main).
    void snap() noexcept [[clang::nonblocking]] {
        current_ = target_;
        left_ = 0;
    }
    float next() noexcept [[clang::nonblocking]] {
        if (left_ > 0) {
            current_ += step_;
            if (--left_ == 0) current_ = target_;
        }
        return current_;
    }
    // Tiến n sample một lúc (dùng khi chỉ cập nhật hệ số mỗi khúc 16 sample).
    float skip(int n) noexcept [[clang::nonblocking]] {
        if (left_ > 0) {
            if (n >= left_) {
                current_ = target_;
                left_ = 0;
            } else {
                current_ += step_ * static_cast<float>(n);
                left_ -= n;
            }
        }
        return current_;
    }
    bool  isSmoothing() const noexcept [[clang::nonblocking]] { return left_ > 0; }
    float current() const noexcept [[clang::nonblocking]] { return current_; }
    float target() const noexcept [[clang::nonblocking]] { return target_; }

private:
    float   current_ = 0.0f, target_ = 0.0f, step_ = 0.0f;
    int32_t steps_ = 960, left_ = 0;
};

} // namespace le::dsp
