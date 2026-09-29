#pragma once
// DeviceIO: lớp trừu tượng thiết bị audio (03 §8). RtEngine không biết thiết bị đến từ đâu,
// nên Plan B (CoreAudioIO tự viết) chỉ cần hiện thực lại interface này.
#include <cstdint>
#include <string>

#include "io/AudioSession.h"

namespace le::io {

struct CallbackContext {
    std::uint64_t hostTimeNs = 0;   // thời điểm IO của block (0 = thiết bị không cung cấp)
};

// Thứ mà thiết bị gọi vào. RtEngine hiện thực interface này.
class AudioCallback {
public:
    virtual ~AudioCallback() = default;
    // [main] Trước khi callback đầu tiên chạy (và mỗi lần device restart). Được phép cấp phát.
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    // [RT]
    virtual void process(const float* const* in, int numIn, float* const* out, int numOut,
                         int numFrames, const CallbackContext& ctx) noexcept [[clang::nonblocking]] = 0;
    // [main hoặc thread của device] Device đã dừng.
    virtual void released() {}
};

struct DeviceConfig {
    double sampleRate = 48000.0;
    int bufferSize = 128;
    int numInputs = 1;
    int numOutputs = 2;
};

struct DeviceLatencies {
    int inputSamples = 0;
    int outputSamples = 0;
    int roundTrip() const { return inputSamples + outputSamples; }
};

// Mọi method gọi từ main thread.
class DeviceIO {
public:
    virtual ~DeviceIO() = default;
    virtual std::int32_t start(const DeviceConfig& cfg, AudioCallback* cb) = 0;   // LeError
    virtual void stop() = 0;
    virtual std::int32_t restart(const DeviceConfig& cfg) = 0;                   // đổi buffer/SR, LeError
    virtual bool isRunning() const = 0;
    virtual double sampleRate() const = 0;
    virtual int bufferSize() const = 0;
    virtual int numInputs() const = 0;
    virtual DeviceLatencies latencies() const = 0;
    virtual int xrunCount() const = 0;           // bộ đếm của driver, -1 nếu không có
    virtual std::string deviceName() const = 0;
    virtual std::string lastError() const = 0;

    // Chỉ iOS có ý nghĩa (AVAudioSession mode). Mặc định: không làm gì.
    virtual bool setSessionMode(session::Mode) { return true; }
    virtual session::Mode sessionMode() const { return session::Mode::Default; }
    // Số lần danh sách/route thiết bị đổi (macOS dùng để phát ROUTE_CHANGED).
    virtual std::uint32_t deviceChangeCount() const { return 0; }
};

} // namespace le::io
