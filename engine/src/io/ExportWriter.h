// ExportWriter — ghi file export (04 §12, P3-18/19 lõi). [worker] (file I/O, cấp phát). KHÔNG gọi trên audio thread.
// 68 làm RtEngine offline + op export.* và gọi class này theo từng block render được.
//
// Định dạng:
//   Wav24  : WAV PCM 24-bit little-endian. float → 24-bit có TPDF dither (tổng 2 nhiễu đều ±½ LSB → tam giác
//            ±1 LSB) rồi làm tròn: bỏ méo lượng tử hoá, sai số |out − in| ≤ 1.5 LSB, trung bình 0, RMS ≈ 0.5 LSB.
//            Vượt ±1.0 → kẹp (đếm trong clippedSamples). Dither dùng xorshift có seed → export lặp lại được từng bit.
//   M4aAac : AAC trong M4A qua ExtAudioFile (AudioToolbox, chỉ Apple; nơi khác → LE_ERR_NOT_IMPLEMENTED).
//            Bitrate mặc định 256 kbps; encoder không nhận mức đó (VD mono) thì chọn mức cao nhất ≤ yêu cầu.
//            AAC có "priming" (~2112 sample) — ExtAudioFile ghi thông tin này nên khi đọc lại độ dài lệch ≤ 1 frame AAC.
// Ghi an toàn: mọi thứ ghi vào "<path>.tmp", finish() thành công mới đổi tên sang <path>. Lỗi / abort() /
// huỷ object khi chưa finish → xoá .tmp, không bao giờ để lại file hỏng mang tên thật.
#pragma once

#include "dsp/AudioData.h"

#include <cstdint>
#include <memory>
#include <string>

namespace le::io {

enum class ExportFormat : int32_t { Wav24 = 0, M4aAac = 1 };

struct ExportOptions {
    ExportFormat format = ExportFormat::Wav24;
    int      aacBitrate = 256000;
    uint32_t ditherSeed = 0x2545F491u;
};

const char* exportExtension(ExportFormat f) noexcept;   // "wav" / "m4a"

class ExportWriter {
public:
    ExportWriter();
    ~ExportWriter();                  // chưa finish → abort (xoá .tmp)
    ExportWriter(const ExportWriter&) = delete;
    ExportWriter& operator=(const ExportWriter&) = delete;

    // numChannels 1..2. Trả LeError (LE_OK khi thành công); error nhận mô tả.
    int32_t open(const std::string& path, double sampleRate, int numChannels, const ExportOptions& opt = {},
                 std::string* error = nullptr);
    // Ghi n frame, mỗi kênh một mảng float (planar).
    int32_t write(const float* const* channels, int numFrames, std::string* error = nullptr);
    // Đóng file, đổi .tmp → path.
    int32_t finish(std::string* error = nullptr);
    void abort() noexcept;

    int64_t framesWritten() const noexcept { return frames_; }
    int64_t clippedSamples() const noexcept { return clipped_; }
    int     bitrate() const noexcept { return bitrate_; }   // AAC: bitrate thực dùng
    bool    isOpen() const noexcept { return backend_ != nullptr; }

    struct Backend;                   // WAV / AAC — định nghĩa trong ExportWriter*.cpp

private:
    std::unique_ptr<Backend> backend_;
    std::string path_, tmpPath_;
    int64_t frames_ = 0, clipped_ = 0;
    int bitrate_ = 0;
};

struct ExportResult {
    bool ok = false;
    int32_t error = 0;
    std::string message;
    int64_t frames = 0;
    int64_t clippedSamples = 0;
    int bitrate = 0;
};

// [worker] Ghi cả buffer một lần.
ExportResult exportAudio(const std::string& path, const dsp::AudioData& data, const ExportOptions& opt = {});

} // namespace le::io
