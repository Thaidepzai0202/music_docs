#pragma once
// Engine: facade mà C API gọi vào (03 §7). Mọi method chạy trên main thread (05 §1).
#include <cstdint>
#include <memory>
#include <string>

#include "core/JobSystem.h"
#include "core/RtEngine.h"
#include "le/engine_api.h"

namespace juce { class ScopedJuceInitialiser_GUI; }

namespace le::io { class DeviceIO; }

namespace le::core {

// [any] Publisher sống suốt đời process → le_read_state an toàn cả trước le_create / sau le_destroy.
StatePublisher& globalStatePublisher();

// [main] Callback event do Dart đăng ký. Lưu toàn cục vì Dart gọi le_set_event_callback TRƯỚC le_create.
void setEventCallback(LeEventCallback cb);
void emitEvent(std::int32_t type, std::int32_t a, std::int32_t b, std::int64_t jobId, double value);

class EventPump;

class Engine {
public:
    // `device` = nullptr → JuceDeviceIO (tạo lúc audioStart). Test / render offline truyền OfflineDeviceIO.
    explicit Engine(const LeConfig& cfg, std::unique_ptr<io::DeviceIO> device = nullptr);   // validate trước
    ~Engine();

    static std::int32_t validate(const LeConfig* cfg);   // LeError

    std::int32_t audioStart();
    void audioStop();
    bool send(const LeCommand& cmd);
    std::string call(const char* requestJson);   // luôn trả JSON envelope (05 §3)

    // [main] Timer 30Hz: xả rtToNrt, phát event, theo dõi interruption/route/xrun.
    void pump();

    std::uint32_t rejectedCommands() const noexcept { return rejectedCommands_; }
    io::DeviceIO* device() noexcept { return device_.get(); }

private:
    std::string handleCall(const std::string& op, const void* request);   // request: const juce::var*
    std::string startLatencyJob();
    std::string startStretchJob(const void* request);
    void refreshDeviceInfo();

    // Thứ tự khai báo = thứ tự khởi tạo; huỷ theo thứ tự ngược lại. JUCE phải sống lâu nhất.
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juce_;

    LeConfig cfg_{};
    std::string dataDir_, libraryDir_;
    int preferredBuffer_ = 128;
    double preferredRate_ = 48000.0;
    int numInputs_ = 1;

    CommandQueue commands_{kRtCommandCapacity};
    RtToNrtQueue toNrt_{kRtToNrtCapacity};
    std::unique_ptr<float[]> recordBuf_;
    int recordCapacity_ = 0;
    RtEngine rt_;
    std::unique_ptr<JobSystem> jobs_;   // sau rt_: job tham chiếu LatencyProbe trong rt_ → phải huỷ trước
    std::unique_ptr<io::DeviceIO> device_;
    std::unique_ptr<EventPump> pump_;

    std::uint32_t rejectedCommands_ = 0;
    std::uint32_t lastXruns_ = 0;
    std::uint32_t seenInterruptBegan_ = 0, seenInterruptEnded_ = 0, seenRoute_ = 0, seenDeviceChanges_ = 0;
    int pumpTicks_ = 0;
    int startGraceTicks_ = 0;
};

} // namespace le::core
