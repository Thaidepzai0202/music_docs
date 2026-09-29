// P1-35 / P4-12: latency.calibrate (LatencyCalibrator của 80) · latency.setOffset. [main]
// L RT dùng để bù thu âm = inputLatency + outputLatency (device báo) + offset (04 §5.3). Offset là thiết lập TOÀN CỤC:
// project.open không đổi; engine không lưu qua các lần mở app (app gửi lại bằng latency.setOffset khi khởi động).
#include <algorithm>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "render/LatencyCalibrator.h"

namespace le::core {

// {samples} → {}. Dương = bù trễ nhiều hơn (take dời sớm lên thêm).
Reply Engine::opLatencySetOffset(const juce::var& req) {
    int samples = 0;
    if (!args::getInt(req, "samples", samples, -kMaxLatencyOffset, kMaxLatencyOffset))
        return Reply::fail(LE_ERR_INVALID_ARG, "samples phải là số nguyên trong ±" + std::to_string(kMaxLatencyOffset));
    latencyOffset_ = samples;
    refreshDeviceInfo();
    return Reply::ok();
}

// – → jobId → {measuredSamples, reportedSamples, offsetSamples = measured − reported, spreadSamples, confidence, validRuns}.
// Thành công → áp dụng offset NGAY. Thất bại → job failed, message = mã của calibrator (NO_SIGNAL, TOO_NOISY…).
Reply Engine::opLatencyCalibrate(const juce::var&) {
    if (device_ == nullptr || !device_->isRunning()) return Reply::fail(LE_ERR_AUDIO_DEVICE, "audio chưa chạy (le_audio_start)");
    if (device_->numInputs() <= 0) return Reply::fail(LE_ERR_MIC_PERMISSION, "không có input (quyền mic / audio.setInputEnabled)");
    if (jobs_->anyRunning("latency")) return Reply::fail(LE_ERR_INVALID_ARG, "đang đo latency");
    render::CalibrateOptions opt;
    opt.reportedSamples = device_->latencies().roundTrip();
    spike::LatencyProbe& probe = rt_.latencyProbe();
    auto result = std::make_shared<render::CalibrationResult>();
    const auto id = jobs_->submit(
        "latency",   // cùng tên với spike.latencyLoopback → loại trừ nhau (anyRunning)
        [&probe, opt, result](JobSystem::Context& ctx) {   // [worker]
            *result = render::runCalibration(probe, opt, &ctx.cancel, &ctx.progress);
            if (result->error == render::CalibrationError::Cancelled) return JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
            if (!result->ok) return JobOutcome::fail(LE_ERR_AUDIO_DEVICE, render::calibrationErrorName(result->error));
            auto* r = new juce::DynamicObject();
            r->setProperty("measuredSamples", result->roundTripSamples);
            r->setProperty("reportedSamples", result->reportedSamples);
            r->setProperty("offsetSamples", result->offsetSamples);
            r->setProperty("spreadSamples", result->spreadSamples);
            r->setProperty("confidence", result->confidence);
            r->setProperty("validRuns", result->validRuns);
            return JobOutcome::ok(juce::var(r));
        },
        [this, result](JobOutcome& o) {   // [main] áp dụng ngay khi Dart nhận JOB_DONE
            if (o.error != LE_OK) return;
            latencyOffset_ = std::clamp(result->offsetSamples, -kMaxLatencyOffset, kMaxLatencyOffset);
            refreshDeviceInfo();
        });
    return Reply::job(id);
}

} // namespace le::core
