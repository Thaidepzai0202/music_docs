#pragma once
// XrunDetector: phát hiện callback bị lỡ theo khoảng cách host time (08 §2).
// Xrun khi: hostTime(callback này) − hostTime(callback trước) > 1.5 × thời lượng block trước.
#include <cstdint>

namespace le::core {

class XrunDetector {
public:
    // [main] Gọi khi device (re)start. Callback đầu tiên sau đó không được so sánh.
    void prepare(double sampleRate) noexcept {
        sampleRate_ = sampleRate > 0 ? sampleRate : 48000.0;
        lastHostNs_ = 0;
        lastFrames_ = 0;
    }

    // [RT] Trả true nếu phát hiện xrun ở callback này. `hostNs` = 0 nghĩa là không biết.
    bool onCallback(std::uint64_t hostNs, int numFrames) noexcept [[clang::nonblocking]] {
        bool xrun = false;
        if (hostNs != 0 && lastHostNs_ != 0 && lastFrames_ > 0 && hostNs > lastHostNs_) {
            const double expectedNs = (double) lastFrames_ * 1.0e9 / sampleRate_;
            if ((double) (hostNs - lastHostNs_) > 1.5 * expectedNs) {
                ++count_;
                xrun = true;
            }
        }
        lastHostNs_ = hostNs;
        lastFrames_ = numFrames;
        return xrun;
    }

    std::uint32_t count() const noexcept [[clang::nonblocking]] { return count_; }

private:
    double sampleRate_ = 48000.0;
    std::uint64_t lastHostNs_ = 0;
    int lastFrames_ = 0;
    std::uint32_t count_ = 0;   // cộng dồn suốt đời engine (không reset khi restart device)
};

} // namespace le::core
