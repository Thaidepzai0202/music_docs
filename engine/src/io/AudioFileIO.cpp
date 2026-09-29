// [worker] Decode file âm thanh bằng JUCE (WAV, AIFF, FLAC, CAF qua CoreAudioFormat trên Apple).
#include "io/AudioFileIO.h"

#include <algorithm>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>

namespace le::io {

namespace {
constexpr int kChunk = 65536;
constexpr std::int64_t kMaxBytes = 1LL << 30;   // 1 GB float: vượt thì coi như hết bộ nhớ (iPad 8 có 3 GB)

DecodeResult fail(std::int32_t err, std::string msg) {
    DecodeResult r;
    r.error = err;
    r.message = std::move(msg);
    return r;
}
} // namespace

DecodeResult decodeAudioFile(const std::string& absolutePath) {
    const juce::String p = juce::String::fromUTF8(absolutePath.c_str());
    if (!juce::File::isAbsolutePath(p)) return fail(LE_ERR_FILE_NOT_FOUND, "đường dẫn phải tuyệt đối: " + absolutePath);
    const juce::File f(p);
    if (!f.existsAsFile()) return fail(LE_ERR_FILE_NOT_FOUND, "không có file: " + absolutePath);

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
    if (reader == nullptr) return fail(LE_ERR_FILE_FORMAT, "không đọc được định dạng: " + absolutePath);

    const int inCh = (int) reader->numChannels;
    const std::int64_t len = reader->lengthInSamples;
    const double sr = reader->sampleRate;
    if (inCh < 1 || len <= 0 || !(sr >= 8000.0 && sr <= 384000.0))
        return fail(LE_ERR_FILE_FORMAT, "file rỗng hoặc thông số lạ (" + std::to_string(inCh) + " kênh, " +
                                            std::to_string(len) + " frame, " + std::to_string(sr) + " Hz): " + absolutePath);
    const int outCh = std::min(inCh, 2);
    if (len * outCh * (std::int64_t) sizeof(float) > kMaxBytes)
        return fail(LE_ERR_OUT_OF_MEMORY, "file quá dài: " + absolutePath);

    auto data = std::make_shared<dsp::AudioData>(outCh, len, sr);
    juce::AudioBuffer<float> tmp(inCh, (int) std::min<std::int64_t>(kChunk, len));
    for (std::int64_t pos = 0; pos < len;) {
        const int n = (int) std::min<std::int64_t>(kChunk, len - pos);
        if (!reader->read(&tmp, 0, n, pos, true, true)) return fail(LE_ERR_FILE_FORMAT, "lỗi đọc dữ liệu: " + absolutePath);
        if (inCh <= 2) {
            for (int c = 0; c < outCh; ++c) std::copy(tmp.getReadPointer(c), tmp.getReadPointer(c) + n, data->writePointer(c) + pos);
        } else {
            // > 2 kênh: kênh chẵn → trái, kênh lẻ → phải (trung bình)
            float* L = data->writePointer(0) + pos;
            float* R = data->writePointer(1) + pos;
            const int nl = (inCh + 1) / 2, nr = inCh / 2;
            for (int i = 0; i < n; ++i) {
                float l = 0.0f, r = 0.0f;
                for (int c = 0; c < inCh; ++c) (c % 2 == 0 ? l : r) += tmp.getSample(c, i);
                L[i] = l / (float) nl;
                R[i] = r / (float) nr;
            }
        }
        pos += n;
    }
    DecodeResult r;
    r.data = std::move(data);
    return r;
}

} // namespace le::io
