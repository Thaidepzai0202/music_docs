// AudioData — buffer âm thanh float (mono/stereo) dùng chung cho clip, sample của instrument, zone render.
// HỢP ĐỒNG NỘI BỘ 80 ↔ 68 (docs/11 §6): snapshot của 68 giữ đúng kiểu này.
//
// Vòng đời (03 §4):
//   [main/worker] tạo bằng std::make_shared<AudioData>(…), ghi dữ liệu qua writePointer()
//   → "đóng băng" bằng cách chuyển sang AudioDataPtr (shared_ptr<const AudioData>) rồi đưa vào snapshot.
//   [RT] chỉ nhận `const AudioData*` (con trỏ thô MƯỢN từ snapshot): không copy, không huỷ shared_ptr.
//   Huỷ luôn xảy ra ở main (ReleasePool), vì snapshot cũ giữ tham chiếu cho tới khi được xoá.
//
// Bố cục: mỗi kênh là một dải float liên tục, có kPadFrames frame 0 ở HAI đầu.
//   → Bộ nội suy (Hermite cần x[i-1], x[i+1], x[i+2]) đọc lố đầu/cuối vẫn an toàn, không cần if,
//     và lố ra ngoài thì là im lặng (nốt không loop tắt tự nhiên).
//   Chỉ số hợp lệ khi đọc channel(c)[i]: −kPadFrames ≤ i < numFrames() + kPadFrames.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace le::dsp {

class AudioData {
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kPadFrames = 4;

    // [main/worker] Cấp phát và xoá về 0. numChannels bị kẹp vào 1..2.
    AudioData(int numChannels, int64_t numFrames, double sampleRate)
        : numChannels_(std::clamp(numChannels, 1, kMaxChannels)),
          numFrames_(std::max<int64_t>(0, numFrames)),
          sampleRate_(sampleRate),
          stride_(static_cast<size_t>(numFrames_) + 2 * kPadFrames),
          data_(stride_ * static_cast<size_t>(numChannels_), 0.0f) {}

    AudioData(const AudioData&) = delete;             // tránh copy vô tình buffer lớn
    AudioData& operator=(const AudioData&) = delete;

    int     numChannels() const noexcept [[clang::nonblocking]] { return numChannels_; }
    int64_t numFrames() const noexcept [[clang::nonblocking]] { return numFrames_; }
    double  sampleRate() const noexcept [[clang::nonblocking]] { return sampleRate_; }
    bool    isStereo() const noexcept [[clang::nonblocking]] { return numChannels_ == 2; }
    double  durationSeconds() const noexcept [[clang::nonblocking]] {
        return sampleRate_ > 0.0 ? static_cast<double>(numFrames_) / sampleRate_ : 0.0;
    }

    // [any] Con trỏ tới frame 0 của kênh ch (0 ≤ ch < numChannels()).
    const float* channel(int ch) const noexcept [[clang::nonblocking]] {
        return data_.data() + stride_ * static_cast<size_t>(ch) + kPadFrames;
    }
    // [any] Như channel(), nhưng mono thì kênh 1 trả về kênh 0 → player luôn đọc được 2 kênh.
    const float* channelOrMono(int ch) const noexcept [[clang::nonblocking]] {
        return channel(std::min(ch, numChannels_ - 1));
    }
    // [any] Đọc có kiểm tra biên: ngoài [0, numFrames) trả 0. Dùng ở chỗ chỉ số có thể vượt xa vùng đệm.
    float sampleAt(int ch, int64_t i) const noexcept [[clang::nonblocking]] {
        return (i >= 0 && i < numFrames_) ? channel(ch)[i] : 0.0f;
    }

    // [main/worker] Ghi dữ liệu TRƯỚC khi đóng băng (hoặc bởi đúng 1 bên sở hữu bản không-const,
    // ví dụ buffer thu/overdub của 68). Không dùng sau khi đã đưa vào snapshot dạng const.
    float* writePointer(int ch) noexcept [[clang::nonblocking]] {
        return data_.data() + stride_ * static_cast<size_t>(ch) + kPadFrames;
    }

private:
    int     numChannels_;
    int64_t numFrames_;
    double  sampleRate_;
    size_t  stride_;              // numFrames + 2·kPadFrames
    std::vector<float> data_;     // cấp phát 1 lần trong constructor, không bao giờ đổi kích thước
};

using AudioDataPtr = std::shared_ptr<const AudioData>;   // [NRT] chủ sở hữu; RT chỉ dùng AudioDataPtr::get()

} // namespace le::dsp
