#pragma once
// DeviceIO dùng juce::AudioDeviceManager (CoreAudio trên Mac, RemoteIO + AVAudioSession trên iOS).
#include <memory>

#include "io/AudioSession.h"
#include "io/DeviceIO.h"

namespace le::io {

class JuceDeviceIOImpl;

class JuceDeviceIO final : public DeviceIO {
public:
    JuceDeviceIO();            // [main] cần MessageManager đã khởi tạo (ScopedJuceInitialiser_GUI)
    ~JuceDeviceIO() override;

    std::int32_t start(const DeviceConfig& cfg, AudioCallback* cb) override;
    void stop() override;
    std::int32_t restart(const DeviceConfig& cfg) override;
    bool isRunning() const override;
    double sampleRate() const override;
    int bufferSize() const override;
    int numInputs() const override;
    DeviceLatencies latencies() const override;
    int xrunCount() const override;
    std::string deviceName() const override;
    std::string lastError() const override;

    // [main] Chỉ iOS: đổi mode AVAudioSession (default ↔ measurement), áp dụng ngay nếu đang chạy.
    bool setSessionMode(session::Mode mode) override;
    session::Mode sessionMode() const override;

    // [main] Số lần AudioDeviceManager báo thay đổi (danh sách device đổi).
    std::uint32_t deviceChangeCount() const override;

private:
    std::unique_ptr<JuceDeviceIOImpl> impl_;
};

} // namespace le::io
