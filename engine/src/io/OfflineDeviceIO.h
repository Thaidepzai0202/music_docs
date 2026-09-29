#pragma once
// OfflineDeviceIO: "thiết bị" không có phần cứng. Không có thread audio: người gọi (test, scenario runner,
// le-harness render) tự gọi render() trên thread của mình, nên kết quả hoàn toàn deterministic.
// Host time được giả lập tăng đúng theo số frame (không bao giờ có xrun).
#include <algorithm>

#include "io/DeviceIO.h"
#include "le/engine_api.h"

namespace le::io {

class OfflineDeviceIO final : public DeviceIO {
public:
    // maxBlock: block lớn nhất sẽ render (RtEngine cấp phát theo số này).
    explicit OfflineDeviceIO(int maxBlock = 1024) : maxBlock_(std::max(1, maxBlock)) {}

    std::int32_t start(const DeviceConfig& cfg, AudioCallback* cb) override {
        cfg_ = cfg;
        cb_ = cb;
        cb_->prepare(cfg.sampleRate, std::max(maxBlock_, cfg.bufferSize));
        hostNs_ = 1'000'000'000ull;
        running_ = true;
        return LE_OK;
    }
    void stop() override {
        if (running_ && cb_ != nullptr) cb_->released();
        running_ = false;
    }
    std::int32_t restart(const DeviceConfig& cfg) override {
        AudioCallback* cb = cb_;
        stop();
        return cb != nullptr ? start(cfg, cb) : LE_OK;
    }

    // [thread của người gọi, đóng vai audio thread] n ≤ maxBlock.
    void render(const float* const* in, int numIn, float* const* out, int numOut, int n) noexcept {
        if (!running_ || cb_ == nullptr || n <= 0) return;
        CallbackContext ctx;
        ctx.hostTimeNs = hostNs_;
        cb_->process(in, numIn, out, numOut, n, ctx);
        hostNs_ += (std::uint64_t) ((double) n * 1.0e9 / cfg_.sampleRate + 0.5);
    }

    bool isRunning() const override { return running_; }
    double sampleRate() const override { return cfg_.sampleRate; }
    int bufferSize() const override { return cfg_.bufferSize; }
    int numInputs() const override { return cfg_.numInputs; }
    DeviceLatencies latencies() const override { return {}; }
    int xrunCount() const override { return 0; }
    std::string deviceName() const override { return "Offline"; }
    std::string lastError() const override { return {}; }
    int maxBlock() const { return maxBlock_; }

private:
    DeviceConfig cfg_{};
    AudioCallback* cb_ = nullptr;
    int maxBlock_;
    std::uint64_t hostNs_ = 0;
    bool running_ = false;
};

} // namespace le::io
