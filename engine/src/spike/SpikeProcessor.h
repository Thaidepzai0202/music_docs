#pragma once
// Âm thanh cho spike P0 (xoá ở P1-37): sine, thu vào RAM, phát loop, passthrough.
// Tải giả lập (SPIKE_LOAD_VOICES) do spike/measure/LoadGenerator (agent 80) làm, RtEngine gọi trực tiếp.
// Mọi thứ trừ prepare() chạy trên audio thread.
#include <atomic>
#include <cstdint>

#include "le/engine_api.h"

namespace le::spike {

class SpikeProcessor {
public:
    static constexpr int kMaxRecordMs = 10000;

    // [main] Gọi khi audio chưa chạy. `recordBuf` do Engine sở hữu, cấp phát sẵn (≥ 10 giây).
    void prepare(double sampleRate, float* recordBuf, int recordCapacityFrames) noexcept;

    // [RT] LE_CMD_SPIKE_SINE / RECORD / PLAY_RECORD / PASSTHROUGH. Trả false nếu không phải lệnh của nó.
    bool handleCommand(const LeCommand& c) noexcept [[clang::nonblocking]];

    // [RT] Cộng tín hiệu vào outL/outR (đã được xoá trước). `in` là kênh mic 0, có thể nullptr.
    void process(const float* in, float* outL, float* outR, int numFrames) noexcept [[clang::nonblocking]];

    // [RT] true đúng 1 lần sau khi một lượt thu kết thúc.
    bool consumeRecordingFinished() noexcept [[clang::nonblocking]];

    bool isRecording() const noexcept [[clang::nonblocking]] { return recording_.load(std::memory_order_relaxed); }

    // [main] Số frame của lượt thu gần nhất (0 nếu đang thu hoặc chưa thu). Đọc được buffer
    // [0, recordedFrames) an toàn khi isRecording()==false, vì RT chỉ ghi vào buffer lúc đang thu.
    int recordedFrames() const noexcept { return recordedFrames_.load(std::memory_order_acquire); }
    const float* recordBuffer() const noexcept { return recordBuf_; }
    double sampleRate() const noexcept { return sampleRate_; }

private:
    static float approach(float current, float target, float step) noexcept [[clang::nonblocking]] {
        if (current < target) return current + step < target ? current + step : target;
        if (current > target) return current - step > target ? current - step : target;
        return current;
    }

    double sampleRate_ = 48000.0;
    float rampStep_ = 1.0f / 480.0f;   // 0→1 trong 10ms
    int fadeFrames_ = 144;             // 3ms ở hai đầu vòng loop

    // Sine
    double sinePhase_ = 0.0;
    double sineInc_ = 0.0;
    float sineGain_ = 0.0f, sineTarget_ = 0.0f;

    // Thu / phát
    float* recordBuf_ = nullptr;
    int recordCapacity_ = 0;
    int recordPos_ = 0;
    int recordTarget_ = 0;
    std::atomic<bool> recording_{false};
    std::atomic<int> recordedFrames_{0};
    bool recordFinished_ = false;
    int playPos_ = 0;
    float playGain_ = 0.0f, playTarget_ = 0.0f;

    // Passthrough mic → loa
    float passGain_ = 0.0f, passTarget_ = 0.0f;
};

} // namespace le::spike
