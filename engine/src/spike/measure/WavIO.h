// WavIO — đọc/ghi WAV cho công cụ đo P0 (StretchBench, CLI trong engine/tools/). [main] / [worker]
// Bọc juce_audio_formats. Không bao giờ gọi từ audio thread (có file I/O và cấp phát).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace le::spike {

// Ghi WAV 32-bit float (không clip khi tín hiệu > 1.0). Tạo thư mục cha nếu chưa có.
// path tương đối được tính từ thư mục hiện tại.
bool writeWavFloat(const std::string& path, const float* const* channels, int numChannels,
                   int64_t numSamples, double sampleRate, std::string* error = nullptr);

inline bool writeWavMono(const std::string& path, const float* data, int64_t numSamples,
                         double sampleRate, std::string* error = nullptr) {
    const float* ch[1] = {data};
    return writeWavFloat(path, ch, 1, numSamples, sampleRate, error);
}

// Đọc WAV/AIFF/FLAC… (các format cơ bản của JUCE), trộn mọi kênh thành mono.
bool readAudioMono(const std::string& path, std::vector<float>& out, double& sampleRate,
                   std::string* error = nullptr);

} // namespace le::spike
