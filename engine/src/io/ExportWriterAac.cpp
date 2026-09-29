// Backend AAC/M4A của ExportWriter qua ExtAudioFile (AudioToolbox C API, Apple). [worker]
// Nền tảng khác: trả LE_ERR_NOT_IMPLEMENTED.
#include "io/ExportBackends.h"

#include "le/engine_api.h"

#include <algorithm>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace le::io {

#if defined(__APPLE__)

namespace {
std::string osStatus(OSStatus s) { return "OSStatus " + std::to_string(static_cast<long>(s)); }

class AacBackend final : public ExportWriter::Backend {
public:
    AacBackend(ExtAudioFileRef f, int nc) : file_(f), nc_(nc) {}
    ~AacBackend() override { closeQuietly(); }

    int32_t write(const float* const* ch, int n, int64_t& /*clipped*/, std::string* error) override {
        // Client format: float32 interleaved; ExtAudioFile tự chuyển sang AAC
        interleaved_.resize(static_cast<size_t>(n) * static_cast<size_t>(nc_));
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < nc_; ++c) interleaved_[static_cast<size_t>(i * nc_ + c)] = ch[c][i];
        AudioBufferList abl;
        abl.mNumberBuffers = 1;
        abl.mBuffers[0].mNumberChannels = static_cast<UInt32>(nc_);
        abl.mBuffers[0].mDataByteSize = static_cast<UInt32>(interleaved_.size() * sizeof(float));
        abl.mBuffers[0].mData = interleaved_.data();
        const OSStatus s = ExtAudioFileWrite(file_, static_cast<UInt32>(n), &abl);
        if (s != noErr) {
            if (error != nullptr) *error = "ExtAudioFileWrite lỗi: " + osStatus(s);
            return LE_ERR_DISK_FULL;
        }
        return LE_OK;
    }

    int32_t close(std::string* error) override {
        if (file_ == nullptr) return LE_OK;
        const OSStatus s = ExtAudioFileDispose(file_);   // flush encoder + ghi header/priming
        file_ = nullptr;
        if (s != noErr) {
            if (error != nullptr) *error = "ExtAudioFileDispose lỗi: " + osStatus(s);
            return LE_ERR_DISK_FULL;
        }
        return LE_OK;
    }
    void closeQuietly() noexcept override {
        if (file_ != nullptr) ExtAudioFileDispose(file_);
        file_ = nullptr;
    }

private:
    ExtAudioFileRef file_ = nullptr;
    int nc_;
    std::vector<float> interleaved_;
};

// Chọn bitrate cao nhất encoder cho phép mà ≤ yêu cầu (VD mono không nhận 256 kbps).
UInt32 pickBitrate(AudioConverterRef conv, UInt32 requested) {
    UInt32 size = 0;
    if (AudioConverterGetPropertyInfo(conv, kAudioConverterApplicableEncodeBitRates, &size, nullptr) != noErr || size == 0)
        return requested;
    std::vector<AudioValueRange> ranges(size / sizeof(AudioValueRange));
    if (AudioConverterGetProperty(conv, kAudioConverterApplicableEncodeBitRates, &size, ranges.data()) != noErr)
        return requested;
    UInt32 best = 0, lowest = 0xFFFFFFFFu;
    for (const auto& r : ranges) {
        const auto lo = static_cast<UInt32>(r.mMinimum), hi = static_cast<UInt32>(r.mMaximum);
        lowest = std::min(lowest, lo);
        if (lo <= requested) best = std::max(best, std::min(hi, requested));
    }
    return best > 0 ? best : lowest;
}
} // namespace

std::unique_ptr<ExportWriter::Backend> makeAacBackend(const std::string& tmpPath, double sampleRate, int numChannels,
                                                      int requestedBitrate, int& actualBitrate, int32_t& code,
                                                      std::string* error) {
    code = LE_ERR_FILE_FORMAT;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(tmpPath.c_str()),
                                                           static_cast<CFIndex>(tmpPath.size()), false);
    if (url == nullptr) {
        if (error != nullptr) *error = "đường dẫn không hợp lệ: " + tmpPath;
        return nullptr;
    }
    AudioStreamBasicDescription dst{};
    dst.mSampleRate = sampleRate;
    dst.mFormatID = kAudioFormatMPEG4AAC;
    dst.mChannelsPerFrame = static_cast<UInt32>(numChannels);
    dst.mFramesPerPacket = 1024;
    ExtAudioFileRef file = nullptr;
    OSStatus s = ExtAudioFileCreateWithURL(url, kAudioFileM4AType, &dst, nullptr, kAudioFileFlags_EraseFile, &file);
    CFRelease(url);
    if (s != noErr || file == nullptr) {
        if (error != nullptr) *error = "không tạo được file M4A (sample rate " + std::to_string(sampleRate) + "): " + osStatus(s);
        return nullptr;
    }
    auto backend = std::make_unique<AacBackend>(file, numChannels);

    AudioStreamBasicDescription client{};
    client.mSampleRate = sampleRate;
    client.mFormatID = kAudioFormatLinearPCM;
    client.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    client.mBitsPerChannel = 32;
    client.mChannelsPerFrame = static_cast<UInt32>(numChannels);
    client.mFramesPerPacket = 1;
    client.mBytesPerFrame = 4 * static_cast<UInt32>(numChannels);
    client.mBytesPerPacket = client.mBytesPerFrame;
    s = ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof(client), &client);
    if (s != noErr) {
        if (error != nullptr) *error = "không đặt được client format: " + osStatus(s);
        return nullptr;
    }

    // Bitrate: đặt trên AudioConverter bên trong rồi báo ExtAudioFile áp lại cấu hình (ConverterConfig = NULL)
    AudioConverterRef conv = nullptr;
    UInt32 size = sizeof(conv);
    if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_AudioConverter, &size, &conv) == noErr && conv != nullptr) {
        UInt32 br = pickBitrate(conv, static_cast<UInt32>(std::max(8000, requestedBitrate)));
        if (AudioConverterSetProperty(conv, kAudioConverterEncodeBitRate, sizeof(br), &br) == noErr) {
            CFArrayRef config = nullptr;
            ExtAudioFileSetProperty(file, kExtAudioFileProperty_ConverterConfig, sizeof(config), &config);
        }
        UInt32 actual = 0;
        size = sizeof(actual);
        if (AudioConverterGetProperty(conv, kAudioConverterEncodeBitRate, &size, &actual) == noErr) actualBitrate = static_cast<int>(actual);
    }
    code = LE_OK;
    return backend;
}

#else   // không phải Apple

std::unique_ptr<ExportWriter::Backend> makeAacBackend(const std::string&, double, int, int, int&, int32_t& code,
                                                      std::string* error) {
    code = LE_ERR_NOT_IMPLEMENTED;
    if (error != nullptr) *error = "M4A/AAC chỉ hỗ trợ trên Apple (ExtAudioFile)";
    return nullptr;
}

#endif

} // namespace le::io
