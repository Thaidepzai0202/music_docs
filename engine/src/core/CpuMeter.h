#pragma once
// CpuMeter: tỷ lệ thời gian xử lý / thời lượng buffer (08 §2).
// Toàn bộ chạy trên audio thread, chỉ dùng biến thường (không ai khác đọc trực tiếp; giá trị ra ngoài
// qua StatePublisher).
#include <algorithm>
#include <cstdint>

#include "core/HostTime.h"

namespace le::core {

class CpuMeter {
public:
    // [main] Gọi trong prepare, khi audio chưa chạy.
    void prepare(double sampleRate, const HostClock& clock) noexcept {
        sampleRate_ = sampleRate > 0 ? sampleRate : 48000.0;
        clock_ = clock;
        reset();
    }

    void reset() noexcept {
        winBusyNs_ = winWallNs_ = winPeak_ = 0.0;
        avg_ = peak_ = 0.0f;
        hasWindow_ = false;
    }

    // [RT] Đầu callback.
    void begin() noexcept [[clang::nonblocking]] { startTicks_ = hostTicks(); }

    // [RT] Cuối callback. `numFrames` là số frame của block vừa xử lý.
    void end(int numFrames) noexcept [[clang::nonblocking]] {
        const double busyNs = clock_.ticksToNs(hostTicks() - startTicks_);
        const double wallNs = (double) numFrames * 1.0e9 / sampleRate_;
        if (wallNs <= 0.0) return;

        winBusyNs_ += busyNs;
        winWallNs_ += wallNs;
        winPeak_ = std::max(winPeak_, busyNs / wallNs);
        if (!hasWindow_) {                     // cửa sổ đầu tiên chưa đủ 1 giây: cho UI thấy số tạm
            avg_ = (float) (winBusyNs_ / winWallNs_);
            peak_ = (float) winPeak_;
        }
        if (winWallNs_ >= 1.0e9) {             // đủ 1 giây → chốt số, mở cửa sổ mới
            avg_ = (float) (winBusyNs_ / winWallNs_);
            peak_ = (float) winPeak_;
            hasWindow_ = true;
            winBusyNs_ = winWallNs_ = winPeak_ = 0.0;
        }
    }

    float average() const noexcept [[clang::nonblocking]] { return avg_; }   // 0..1 (có thể >1 khi quá tải)
    float peak() const noexcept [[clang::nonblocking]] { return peak_; }

private:
    double sampleRate_ = 48000.0;
    HostClock clock_{};
    std::uint64_t startTicks_ = 0;
    double winBusyNs_ = 0, winWallNs_ = 0, winPeak_ = 0;
    float avg_ = 0, peak_ = 0;
    bool hasWindow_ = false;
};

} // namespace le::core
