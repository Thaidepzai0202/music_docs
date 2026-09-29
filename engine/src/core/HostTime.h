#pragma once
// Đồng hồ đơn điệu dùng được trên audio thread (không syscall, không cấp phát).
#include <cstdint>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <chrono>
#endif

namespace le::core {

// [any] Tick thô của máy. Trên Apple là mach_absolute_time (đọc từ commpage, không syscall).
inline std::uint64_t hostTicks() noexcept [[clang::nonblocking]] {
#if defined(__APPLE__)
    // mach_absolute_time không được SDK đánh dấu nonblocking, nhưng chỉ đọc bộ đếm phần cứng qua commpage.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfunction-effects"
    return mach_absolute_time();
#pragma clang diagnostic pop
#else
    return (std::uint64_t) std::chrono::steady_clock::now().time_since_epoch().count();
#endif
}

// Hệ số đổi tick → ns. Gọi `init()` 1 lần trên main (mach_timebase_info là syscall).
struct HostClock {
    double nsPerTick = 1.0;

    void init() noexcept {
#if defined(__APPLE__)
        mach_timebase_info_data_t tb{};
        mach_timebase_info(&tb);
        nsPerTick = tb.denom != 0 ? (double) tb.numer / (double) tb.denom : 1.0;
#else
        nsPerTick = 1.0;
#endif
    }

    // [RT]
    double ticksToNs(std::uint64_t ticks) const noexcept [[clang::nonblocking]] { return (double) ticks * nsPerTick; }
    std::uint64_t nowNs() const noexcept [[clang::nonblocking]] { return (std::uint64_t) ticksToNs(hostTicks()); }
};

} // namespace le::core
