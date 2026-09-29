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
        cb_->prepare(cfg.sampleRate, maxBlock_);   // render(n > maxBlock) → RtEngine tự chia khối (04 §1)
        hostNs_ = 1'000'000'000ull;
        running_ = true;
        ++startCount_;
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

    // [thread của người gọi, đóng vai audio thread] n bất kỳ (n > maxBlock: RtEngine chia khối).
    void render(const float* const* in, int numIn, float* const* out, int numOut, int n) noexcept {
        if (!running_ || cb_ == nullptr || n <= 0) return;
        CallbackContext ctx;
        ctx.hostTimeNs = hostNs_;
        cb_->process(in, numIn, out, numOut, n, ctx);
        hostNs_ += (std::uint64_t) ((double) n * 1.0e9 / cfg_.sampleRate + 0.5);
        renderedFrames_ += n;
    }

    bool isRunning() const override { return running_; }
    double sampleRate() const override { return cfg_.sampleRate; }
    int bufferSize() const override { return cfg_.bufferSize; }
    int numInputs() const override { return cfg_.numInputs; }
    DeviceLatencies latencies() const override { return latencies_; }
    // [main] Giả lập latency thiết bị báo (scenario "latencySamples", P1-20).
    void setLatencies(int inputSamples, int outputSamples) { latencies_ = {inputSamples, outputSamples}; }
    // [main] Giả lập route (scenario "headphones", P1-23).
    void setRoute(session::RouteInfo r) { route_ = r; }
    session::RouteInfo route() const override { return route_; }
    int xrunCount() const override { return 0; }
    std::string deviceName() const override { return "Offline"; }
    std::string lastError() const override { return {}; }
    int maxBlock() const { return maxBlock_; }
    int startCount() const { return startCount_; }
    std::uint64_t nextHostTimeNs() const { return hostNs_; }   // [test] host time của lần render kế tiếp
    // [main] Tổng số frame đã render (mọi lần start) — đồng hồ audio cho debounce tất định (P3-08).
    std::int64_t renderedFrames() const { return renderedFrames_; }

private:
    DeviceConfig cfg_{};
    AudioCallback* cb_ = nullptr;
    int maxBlock_;
    std::uint64_t hostNs_ = 0;
    bool running_ = false;
    int startCount_ = 0;
    std::int64_t renderedFrames_ = 0;
    DeviceLatencies latencies_{};
    session::RouteInfo route_{};
};

} // namespace le::io
