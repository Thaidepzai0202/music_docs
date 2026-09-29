#pragma once
// R3 (rt-review 2026-09-29): flush-to-zero cho số subnormal trong lúc audio thread xử lý một block.
// Đuôi reverb/delay/IIR tắt dần sẽ rơi vào vùng subnormal (< 1e-38); trên nhiều CPU phép tính trên đó chậm hàng
// chục lần. FTZ đổi chúng thành 0 — khác biệt chỉ ở biên độ < 1e-38, không nghe được và không đổi golden.
// Tương đương juce::ScopedNoDenormals nhưng gắn [[clang::nonblocking]] (chỉ đọc/ghi thanh ghi trạng thái FPU).
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace le::core {

class ScopedFlushDenormals {
public:
    ScopedFlushDenormals() noexcept [[clang::nonblocking]] {
#if defined(__aarch64__)
        std::uint64_t v = 0;
        asm volatile("mrs %0, fpcr" : "=r"(v));
        prev_ = v;
        asm volatile("msr fpcr, %0" : : "r"(v | (1ull << 24)));   // FZ
#elif defined(__SSE__) || defined(_M_X64)
        prev_ = _mm_getcsr();
        _mm_setcsr((unsigned) prev_ | 0x8040u);   // FTZ | DAZ
#endif
    }
    ~ScopedFlushDenormals() noexcept [[clang::nonblocking]] {
#if defined(__aarch64__)
        asm volatile("msr fpcr, %0" : : "r"(prev_));
#elif defined(__SSE__) || defined(_M_X64)
        _mm_setcsr((unsigned) prev_);
#endif
    }
    ScopedFlushDenormals(const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator=(const ScopedFlushDenormals&) = delete;

private:
    std::uint64_t prev_ = 0;
};

} // namespace le::core
