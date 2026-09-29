#pragma once
// AudioFileIO (P1-11): decode file âm thanh thành dsp::AudioData float. [worker] KHÔNG gọi trên audio thread
// (đọc file, cấp phát, JUCE AudioFormatManager).
// Định dạng: WAV / AIFF / CAF / FLAC. File > 2 kênh được trộn về 2. Giữ nguyên sample rate của file
// (Sampler / AudioClipPlayer tự nhân tỉ lệ fileSR / engineSR).
#include <cstdint>
#include <functional>
#include <string>

#include "dsp/AudioData.h"
#include "le/engine_api.h"

namespace le::io {

struct DecodeResult {
    std::int32_t error = LE_OK;   // LE_OK | LE_ERR_FILE_NOT_FOUND | LE_ERR_FILE_FORMAT | LE_ERR_OUT_OF_MEMORY
    std::string message;
    dsp::AudioDataPtr data;       // khác nullptr khi error == LE_OK
};

// [worker] Đường dẫn tuyệt đối, UTF-8.
DecodeResult decodeAudioFile(const std::string& absolutePath);

// Kiểu hàm nạp sample truyền vào SfzLoader (80) → engine truyền thẳng &decodeAudioFile.
using SampleLoader = std::function<DecodeResult(const std::string& absolutePath)>;

} // namespace le::io
