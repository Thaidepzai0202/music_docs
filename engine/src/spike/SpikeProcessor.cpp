#include "spike/SpikeProcessor.h"

#include <algorithm>
#include <cmath>

namespace le::spike {

namespace {
constexpr double kTwoPi = 6.283185307179586;
}

void SpikeProcessor::prepare(double sampleRate, float* recordBuf, int recordCapacityFrames) noexcept {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000.0;
    rampStep_ = (float) (1.0 / (0.010 * sampleRate_));
    fadeFrames_ = std::max(1, (int) (0.003 * sampleRate_));
    recordBuf_ = recordBuf;
    recordCapacity_ = recordBuf != nullptr ? recordCapacityFrames : 0;
    recording_.store(false, std::memory_order_relaxed);
    recordedFrames_.store(0, std::memory_order_release);
    recordPos_ = recordTarget_ = playPos_ = 0;
    recordFinished_ = false;
}

bool SpikeProcessor::handleCommand(const LeCommand& c) noexcept [[clang::nonblocking]] {
    switch (c.type) {
        case LE_CMD_SPIKE_SINE: {
            const double hz = std::clamp((double) c.f0, 20.0, 20000.0);
            sineInc_ = kTwoPi * hz / sampleRate_;
            sineTarget_ = std::clamp(c.f1, 0.0f, 1.0f);
            return true;
        }
        case LE_CMD_SPIKE_RECORD: {
            if (c.i0 <= 0) {                    // i0 = 0: dừng sớm, giữ phần đã thu
                if (recording_.load(std::memory_order_relaxed)) recordTarget_ = recordPos_;
                return true;
            }
            const int ms = std::min((int) c.i0, kMaxRecordMs);
            recordTarget_ = std::min(recordCapacity_, (int) ((double) ms * sampleRate_ / 1000.0));
            recordPos_ = 0;
            playTarget_ = 0.0f;                 // không phát trong lúc ghi đè buffer
            playGain_ = 0.0f;
            recordedFrames_.store(0, std::memory_order_relaxed);
            recording_.store(recordTarget_ > 0, std::memory_order_relaxed);
            return true;
        }
        case LE_CMD_SPIKE_PLAY_RECORD:
            if (c.i0 != 0 && playTarget_ == 0.0f) playPos_ = 0;
            playTarget_ = c.i0 != 0 ? 1.0f : 0.0f;
            return true;
        case LE_CMD_SPIKE_PASSTHROUGH:
            passTarget_ = c.i0 != 0 ? 1.0f : 0.0f;
            return true;
        default:
            return false;
    }
}

void SpikeProcessor::process(const float* in, float* outL, float* outR, int numFrames) noexcept
    [[clang::nonblocking]] {
    // 1) Thu mic vào buffer cấp phát sẵn.
    if (recording_.load(std::memory_order_relaxed)) {
        const int n = std::min(numFrames, recordTarget_ - recordPos_);
        if (n > 0) {
            if (in != nullptr) std::copy(in, in + n, recordBuf_ + recordPos_);
            else std::fill(recordBuf_ + recordPos_, recordBuf_ + recordPos_ + n, 0.0f);
            recordPos_ += n;
        }
        if (recordPos_ >= recordTarget_) {
            recording_.store(false, std::memory_order_relaxed);
            // release: main thấy recordedFrames_ ≠ 0 thì cũng thấy toàn bộ sample đã ghi.
            recordedFrames_.store(recordPos_, std::memory_order_release);
            recordFinished_ = true;
        }
    }

    const int recorded = recordedFrames_.load(std::memory_order_relaxed);
    const float fade = (float) fadeFrames_;

    for (int i = 0; i < numFrames; ++i) {
        float mono = 0.0f;

        // 2) Sine
        sineGain_ = approach(sineGain_, sineTarget_, rampStep_);
        if (sineGain_ > 0.0f || sineTarget_ > 0.0f) {
            mono += sineGain_ * (float) std::sin(sinePhase_);
            sinePhase_ += sineInc_;
            if (sinePhase_ >= kTwoPi) sinePhase_ -= kTwoPi;
        }

        // 3) Phát loop phần đã thu, fade 3ms ở hai đầu vòng để không click.
        playGain_ = approach(playGain_, recorded > 0 ? playTarget_ : 0.0f, rampStep_);
        if (playGain_ > 0.0f && recorded > 0) {
            if (playPos_ >= recorded) playPos_ = 0;
            const float edge = (float) std::min(playPos_, recorded - 1 - playPos_);
            const float env = edge < fade ? edge / fade : 1.0f;
            mono += playGain_ * env * recordBuf_[playPos_];
            ++playPos_;
        }

        // 4) Passthrough
        passGain_ = approach(passGain_, passTarget_, rampStep_);
        if (passGain_ > 0.0f && in != nullptr) mono += passGain_ * in[i];

        outL[i] += mono;
        outR[i] += mono;
    }
}

bool SpikeProcessor::consumeRecordingFinished() noexcept [[clang::nonblocking]] {
    const bool f = recordFinished_;
    recordFinished_ = false;
    return f;
}

} // namespace le::spike
