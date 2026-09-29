// SilenceTrimmer — cắt khoảng lặng đầu/cuối của tiếng thu trước khi dò cao độ (04 §8, P3-02 lõi). [worker]
//   - "Có tiếng" = |sample| ≥ ngưỡng (mặc định −45 dBFS) ở BẤT KỲ kênh nào.
//   - Giữ lại pre-roll (mặc định 5 ms) trước sample có tiếng đầu tiên và post-roll (5 ms) sau sample có tiếng
//     cuối cùng, không vượt ra ngoài buffer.
//   - fades = true: fade-in tuyến tính trên đoạn pre-roll và fade-out trên đoạn post-roll (đoạn này vốn dưới
//     ngưỡng) → chỗ cắt không có bậc, sampler không nghe "tách".
//   - Không có sample nào đạt ngưỡng → range.silent = true, trimSilence trả nullptr (caller báo "không nghe thấy tiếng").
#pragma once

#include "dsp/AudioData.h"

#include <cstdint>

namespace le::render {

struct TrimConfig {
    float  thresholdDb = -45.0f;
    double preRollMs = 5.0;
    double postRollMs = 5.0;
    bool   trimStart = true;
    bool   trimEnd = true;
    bool   fades = true;
};

struct TrimRange {
    int64_t start = 0;      // frame đầu giữ lại
    int64_t end = 0;        // frame sau frame cuối giữ lại (end − start = độ dài)
    bool    silent = true;
};

// [any] Chỉ tìm khoảng, không cấp phát.
TrimRange findTrimRange(const dsp::AudioData& data, const TrimConfig& cfg = {}) noexcept;

// [worker] Copy phần [start, end) thành AudioData mới (cùng số kênh, sample rate). Im lặng → nullptr.
dsp::AudioDataPtr trimSilence(const dsp::AudioData& data, const TrimConfig& cfg = {}, TrimRange* range = nullptr);

} // namespace le::render
