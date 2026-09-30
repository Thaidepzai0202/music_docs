// WavIO — đọc/ghi WAV (công cụ đo P0, CLI trong engine/tools/) và ghi FLAC 24-bit cho thư viện (P2-35). [main] / [worker]
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

// Ghi FLAC 24-bit (06 §4: định dạng thư viện trong bundle). Mỗi mẫu làm tròn về lưới 24-bit k / 2^23 (gần nhất,
// kẹp [−1, 1 − 2^−23]) rồi đưa thẳng số nguyên cho encoder → không phụ thuộc cách JUCE đổi float → int. Buffer đã
// nằm trên lưới (render::kits::quantize24) thì decode lại (io::decodeAudioFile / readAudioMono) ra ĐÚNG từng bit.
// compression: mức nén FLAC 1..8 (lossless ở mọi mức). Mặc định 5 (mặc định của libFLAC): đã đo trên 220 file thư
// viện, file giống hệt TỪNG BYTE giữa build debug và release. Mức 8 nhỏ hơn ~7 % nhưng byte phụ thuộc cờ build của
// libFLAC (âm thanh decode ra vẫn y hệt) → sinh lại ở preset khác sẽ tạo diff LFS vô ích.
bool writeFlac24(const std::string& path, const float* const* channels, int numChannels, int64_t numSamples,
                 double sampleRate, int compression = 5, std::string* error = nullptr);

inline bool writeFlac24Mono(const std::string& path, const float* data, int64_t numSamples, double sampleRate,
                            std::string* error = nullptr) {
    const float* ch[1] = {data};
    return writeFlac24(path, ch, 1, numSamples, sampleRate, 5, error);
}

// Đọc WAV/AIFF/FLAC… (các format cơ bản của JUCE), trộn mọi kênh thành mono.
bool readAudioMono(const std::string& path, std::vector<float>& out, double& sampleRate,
                   std::string* error = nullptr);

} // namespace le::spike
