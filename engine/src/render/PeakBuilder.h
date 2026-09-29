// PeakBuilder — dữ liệu vẽ waveform cho UI (04 §5.7, P1-24). [worker] (cấp phát, file I/O).
// 68 làm le_get_peaks + cache RAM; file này tính peaks và đọc/ghi cache đĩa `cache/<clipId>.peaks`.
//
// 3 mức: 256 / 2048 / 16384 sample mỗi điểm. Mỗi điểm = cặp (min, max) float, GỘP mọi kênh
// (min của các min, max của các max). Điểm cuối có thể phủ ít sample hơn (phần lẻ cuối file).
// Mức 1 và 2 được gộp từ 8 điểm của mức dưới → nhanh, và CHÍNH XÁC như tính thẳng từ dữ liệu
// (min/max của hợp các đoạn = min/max của cả khối).
//
// ── Định dạng file .peaks, version 1 (little-endian, như mọi máy Apple) ──
//   offset  kiểu        nội dung
//   0       char[4]     magic "LEPK"
//   4       uint32      version = 1
//   8       uint32      headerBytes = 64 (dữ liệu bắt đầu tại đây)
//   12      uint32      numChannels của nguồn
//   16      float64     sampleRate của nguồn
//   24      int64       numFrames của nguồn
//   32      uint32      numLevels = 3
//   36      uint32[3]   samplesPerPoint = {256, 2048, 16384}
//   48      uint32[3]   numPoints mỗi mức = ceil(numFrames / samplesPerPoint)
//   60      uint32      reserved = 0
//   64      float32[]   mức 0: min0, max0, min1, max1, … rồi tới mức 1, mức 2
// Kích thước file = 64 + 8 · (numPoints0 + numPoints1 + numPoints2). Sai bất kỳ trường nào → coi như
// cache hỏng (đọc trả false) → caller tính lại. Ghi vào "<path>.tmp" rồi đổi tên → không bao giờ để lại
// file ghi dở.
#pragma once

#include "dsp/AudioData.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace le::render {

struct Peaks {
    static constexpr int kLevels = 3;
    static constexpr std::array<int, kLevels> kSamplesPerPoint = {256, 2048, 16384};

    int     numChannels = 0;
    double  sampleRate = 0.0;
    int64_t numFrames = 0;
    std::array<std::vector<float>, kLevels> levels;   // mỗi mức: [min0, max0, min1, max1, …]

    int64_t numPoints(int level) const noexcept {
        return (level >= 0 && level < kLevels) ? static_cast<int64_t>(levels[static_cast<size_t>(level)].size() / 2) : 0;
    }
    // Chép tối đa maxPairs cặp của `level` (từ điểm firstPoint) vào out[2·i], out[2·i+1].
    // Trả số cặp đã ghi, hoặc LE_ERR_INVALID_ARG (< 0) nếu level/tham số sai. Dùng cho le_get_peaks.
    int32_t copyTo(int level, float* out, int32_t maxPairs, int64_t firstPoint = 0) const noexcept;
};

// [worker] Tính 3 mức. cancel = true giữa chừng → trả Peaks rỗng (numFrames = 0).
Peaks buildPeaks(const dsp::AudioData& data, const std::atomic<bool>* cancel = nullptr);

// [worker] Ghi / đọc cache. readPeaksFile kiểm toàn bộ header. Nếu expectFrames ≥ 0 hoặc
// expectSampleRate > 0 thì còn kiểm cache có khớp file nguồn hiện tại không (file audio bị thay → false).
bool writePeaksFile(const std::string& path, const Peaks& peaks, std::string* error = nullptr);
bool readPeaksFile(const std::string& path, Peaks& out, std::string* error = nullptr,
                   int64_t expectFrames = -1, double expectSampleRate = 0.0);

} // namespace le::render
