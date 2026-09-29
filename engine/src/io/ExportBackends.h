// Nội bộ của ExportWriter: giao diện backend WAV / AAC. [worker]
#pragma once

#include "io/ExportWriter.h"

#include <cstdint>
#include <memory>
#include <string>

namespace le::io {

struct ExportWriter::Backend {
    virtual ~Backend() = default;
    // Ghi n frame planar. clipped += số sample bị kẹp. Trả LeError.
    virtual int32_t write(const float* const* channels, int n, int64_t& clipped, std::string* error) = 0;
    // Hoàn tất và đóng file (.tmp). Trả LeError.
    virtual int32_t close(std::string* error) = 0;
    // Đóng không cần hoàn tất (lỗi / abort). Không ném lỗi.
    virtual void closeQuietly() noexcept = 0;
};

std::unique_ptr<ExportWriter::Backend> makeWav24Backend(const std::string& tmpPath, double sampleRate, int numChannels,
                                                        uint32_t ditherSeed, int32_t& code, std::string* error);
// Chỉ Apple có hiện thực thật (ExportWriterAac.cpp); nơi khác trả nullptr + LE_ERR_NOT_IMPLEMENTED.
std::unique_ptr<ExportWriter::Backend> makeAacBackend(const std::string& tmpPath, double sampleRate, int numChannels,
                                                      int requestedBitrate, int& actualBitrate, int32_t& code,
                                                      std::string* error);

} // namespace le::io
