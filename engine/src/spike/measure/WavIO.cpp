#include "spike/measure/WavIO.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <limits>
#include <memory>

namespace le::spike {

namespace {
juce::File toFile(const std::string& path) {
    // juce::File chỉ nhận đường dẫn tuyệt đối; getChildFile xử lý được cả hai kiểu.
    return juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(path.c_str()));
}

bool fail(std::string* error, const std::string& msg) {
    if (error != nullptr) *error = msg;
    return false;
}
} // namespace

// [main] / [worker]
bool writeWavFloat(const std::string& path, const float* const* channels, int numChannels,
                   int64_t numSamples, double sampleRate, std::string* error) {
    if (numChannels <= 0 || numSamples < 0 || sampleRate <= 0.0)
        return fail(error, "tham số không hợp lệ");

    const juce::File file = toFile(path);
    if (!file.getParentDirectory().createDirectory())
        return fail(error, "không tạo được thư mục " + file.getParentDirectory().getFullPathName().toStdString());
    file.deleteFile();

    auto fileStream = std::make_unique<juce::FileOutputStream>(file);
    if (!fileStream->openedOk())
        return fail(error, "không mở được file để ghi: " + file.getFullPathName().toStdString());
    std::unique_ptr<juce::OutputStream> stream = std::move(fileStream);

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate(sampleRate)
                             .withNumChannels(numChannels)
                             .withBitsPerSample(32)
                             .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    std::unique_ptr<juce::AudioFormatWriter> writer = wav.createWriterFor(stream, options);
    if (writer == nullptr) return fail(error, "JUCE không tạo được WAV writer");

    // writeFromFloatArrays nhận số sample kiểu int → ghi theo từng đoạn.
    constexpr int64_t kChunk = 1 << 20;
    std::vector<const float*> ptrs(static_cast<size_t>(numChannels));
    for (int64_t done = 0; done < numSamples; done += kChunk) {
        const int count = static_cast<int>(std::min(kChunk, numSamples - done));
        for (int c = 0; c < numChannels; ++c) ptrs[static_cast<size_t>(c)] = channels[c] + done;
        if (!writer->writeFromFloatArrays(ptrs.data(), numChannels, count))
            return fail(error, "ghi dữ liệu thất bại: " + file.getFullPathName().toStdString());
    }
    writer.reset();   // flush + đóng file
    return true;
}

// [main] / [worker]
bool readAudioMono(const std::string& path, std::vector<float>& out, double& sampleRate, std::string* error) {
    const juce::File file = toFile(path);
    if (!file.existsAsFile()) return fail(error, "không thấy file: " + file.getFullPathName().toStdString());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr) return fail(error, "không đọc được định dạng audio: " + path);

    const int64_t len = reader->lengthInSamples;
    const int numCh = static_cast<int>(reader->numChannels);
    if (len <= 0 || numCh <= 0) return fail(error, "file rỗng: " + path);
    if (len > std::numeric_limits<int>::max()) return fail(error, "file quá dài: " + path);

    juce::AudioBuffer<float> buf(numCh, static_cast<int>(len));
    if (!reader->read(&buf, 0, static_cast<int>(len), 0, true, true))
        return fail(error, "đọc dữ liệu thất bại: " + path);

    out.assign(static_cast<size_t>(len), 0.0f);
    const float scale = 1.0f / static_cast<float>(numCh);
    for (int c = 0; c < numCh; ++c) {
        const float* src = buf.getReadPointer(c);
        for (int64_t i = 0; i < len; ++i) out[static_cast<size_t>(i)] += src[i] * scale;
    }
    sampleRate = reader->sampleRate;
    return true;
}

} // namespace le::spike
