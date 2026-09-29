#pragma once
// RtEngine: xử lý audio 1 block (04 §1). Không phụ thuộc thiết bị: DeviceIO gọi vào qua AudioCallback,
// test và render offline gọi thẳng process().
#include <atomic>
#include <cstdint>
#include <memory>

#include "core/CpuMeter.h"
#include "core/HostTime.h"
#include "core/RtQueues.h"
#include "core/StatePublisher.h"
#include "core/XrunDetector.h"
#include "io/DeviceIO.h"
#include "le/engine_api.h"
#include "spike/SpikeProcessor.h"
#include "spike/measure/LatencyProbe.h"
#include "spike/measure/LoadGenerator.h"

namespace le::core {

using StatePublisher = SeqLockPublisher<LeState>;

class RtEngine final : public io::AudioCallback {
public:
    static constexpr int kMaxCommandsPerBlock = 64;

    // Các queue và publisher do Engine sở hữu, sống lâu hơn RtEngine.
    RtEngine(CommandQueue& commands, RtToNrtQueue& toNrt, StatePublisher& publisher);

    // [main] Buffer thu cho spike (Engine sở hữu). Gọi trước prepare.
    void setRecordBuffer(float* data, int capacityFrames) noexcept;

    // [main] Khi audio chưa chạy. Cấp phát mọi thứ process() cần.
    void prepare(double sampleRate, int maxBlockSize) override;

    // [RT]
    void process(const float* const* in, int numIn, float* const* out, int numOut, int numFrames,
                 const io::CallbackContext& ctx) noexcept [[clang::nonblocking]] override;

    // [main → RT] Giá trị do main cập nhật định kỳ, audio thread đọc relaxed.
    void setLatencyRoundTrip(int samples) noexcept { latencyRoundTrip_.store(samples, std::memory_order_relaxed); }
    void setDeviceXruns(int count) noexcept { deviceXruns_.store(count, std::memory_order_relaxed); }

    // [main] Cho spike.* trong le_call (P0-06/P0-09).
    const spike::SpikeProcessor& spikeProcessor() const noexcept { return spike_; }
    // [main] start() / [worker] isDone(), analyze() — luật thread: spike/measure/LatencyProbe.h
    spike::LatencyProbe& latencyProbe() noexcept { return probe_; }
    const spike::LoadGenerator& loadGenerator() const noexcept { return load_; }

    double sampleRate() const noexcept { return sampleRate_; }
    int maxBlockSize() const noexcept { return maxBlock_; }
    std::uint32_t droppedEvents() const noexcept { return droppedEvents_.load(std::memory_order_relaxed); }

private:
    void processChunk(const float* in0, float* outL, float* outR, int numFrames) noexcept [[clang::nonblocking]];
    void drainCommands() noexcept [[clang::nonblocking]];
    void pushEvent(std::int32_t type, std::int32_t a, std::int32_t b, double value) noexcept [[clang::nonblocking]];
    void publishState(int numFrames) noexcept [[clang::nonblocking]];

    CommandQueue& commands_;
    RtToNrtQueue& toNrt_;
    StatePublisher& publisher_;

    double sampleRate_ = 48000.0;
    int maxBlock_ = 0;
    std::unique_ptr<float[]> scratch_;   // [L | R], mỗi kênh maxBlock_ frame (dùng khi device có < 2 output)

    HostClock clock_{};
    CpuMeter cpu_;
    XrunDetector xruns_;
    spike::SpikeProcessor spike_;
    spike::LoadGenerator load_;        // P0-08 (agent 80)
    spike::LatencyProbe probe_;        // P0-07 (agent 80)
    double probeRate_ = 0.0;           // [main] SR lúc prepare probe gần nhất
    float* recordBuf_ = nullptr;
    int recordCapacity_ = 0;

    // Meter peak có decay (~300ms), cập nhật mỗi sample.
    float inputPeak_ = 0.0f;
    float masterPeak_[2] = {0.0f, 0.0f};
    float peakDecay_ = 0.9999f;
    int lastBlock_ = 0;
    std::uint32_t publishCounter_ = 0;

    std::atomic<int> latencyRoundTrip_{0};
    std::atomic<int> deviceXruns_{0};
    std::atomic<std::uint32_t> droppedEvents_{0};
};

} // namespace le::core
