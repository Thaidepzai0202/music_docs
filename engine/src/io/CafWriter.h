#pragma once
// Ghi file CAF float32 (P1-21, 04 §5.6: audio/<clipId>.caf). [worker]
// JUCE chỉ ĐỌC được CAF (CoreAudioFormat), nên ghi tay theo đặc tả Apple Core Audio Format:
//   'caff' v1 → chunk 'desc' (AudioStreamBasicDescription, big-endian) → chunk 'data' (edit count + PCM).
// PCM: float32 little-endian, interleaved. Đọc lại bằng decodeAudioFile() giống từng bit.
#include <string>

#include "dsp/AudioData.h"

namespace le::io {

bool writeCafFloat32(const std::string& absolutePath, const dsp::AudioData& data, std::string* error = nullptr);

} // namespace le::io
