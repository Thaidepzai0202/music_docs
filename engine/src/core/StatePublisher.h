#pragma once
// StatePublisher: audio thread ghi 1 bản LeState mỗi block, thread bất kỳ đọc bản mới nhất.
// Seqlock theo H. Boehm, "Can Seqlocks Get Along With Programming Language Memory Models?" (2012).
//
// Vì sao an toàn (tóm tắt cho người mới học):
// - Chỉ có 1 người ghi (audio thread), nên người ghi không bao giờ phải chờ ai → không lock, RT-safe.
// - Bộ đếm `seq_` lẻ nghĩa là "đang ghi dở". Người đọc đọc seq trước và sau khi copy;
//   nếu seq lẻ hoặc hai lần khác nhau thì bản vừa copy có thể bị rách → đọc lại.
// - Dữ liệu được lưu thành mảng std::atomic<uint64_t> (đọc/ghi relaxed). Như vậy về mặt C++
//   không có data race (TSan sạch). Trên ARM64, load/store relaxed 64-bit chỉ là lệnh ldr/str thường.
// - fence(release) ở người ghi và fence(acquire) ở người đọc bảo đảm: nếu người đọc thấy seq không
//   đổi thì mọi word nó đọc được đều thuộc cùng một lần publish.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <type_traits>

namespace le::core {

template <typename T>
class SeqLockPublisher {
    static_assert(std::is_trivially_copyable_v<T>, "chỉ publish được struct POD");
    static constexpr std::size_t kWords = (sizeof(T) + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

public:
    SeqLockPublisher() noexcept {
        for (auto& w : words_) w.store(0, std::memory_order_relaxed);
    }

    // [RT] Chỉ được gọi từ MỘT thread (người ghi duy nhất).
    void publish(const T& value) noexcept [[clang::nonblocking]] {
        std::uint64_t buf[kWords] = {};
        std::memcpy(buf, &value, sizeof(T));

        const std::uint32_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);          // lẻ: đang ghi
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t i = 0; i < kWords; ++i)
            words_[i].store(buf[i], std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);          // chẵn: xong
    }

    // [any] Thử đọc 1 lần. false = đụng lúc đang ghi, gọi lại.
    bool tryRead(T& out) const noexcept {
        std::uint64_t buf[kWords];
        const std::uint32_t s1 = seq_.load(std::memory_order_acquire);
        if (s1 & 1u) return false;
        for (std::size_t i = 0; i < kWords; ++i)
            buf[i] = words_[i].load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        const std::uint32_t s2 = seq_.load(std::memory_order_relaxed);
        if (s1 != s2) return false;
        std::memcpy(&out, buf, sizeof(T));
        return true;
    }

    // [any, không phải RT] Đọc bản mới nhất. Người ghi chỉ giữ trạng thái "lẻ" trong ~100ns
    // mỗi 2.7ms nên vòng lặp gần như luôn thành công ngay lần đầu.
    void read(T& out) const noexcept {
        for (int attempt = 0; !tryRead(out); ++attempt)
            if (attempt >= 16) std::this_thread::yield();
    }

    // Số lần publish đã hoàn tất (để test / debug).
    std::uint32_t publishCount() const noexcept { return seq_.load(std::memory_order_acquire) / 2; }

private:
    alignas(64) std::atomic<std::uint32_t> seq_{0};
    alignas(64) std::array<std::atomic<std::uint64_t>, kWords> words_;
};

} // namespace le::core
