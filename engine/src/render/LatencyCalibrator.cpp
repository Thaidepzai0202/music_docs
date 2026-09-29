#include "render/LatencyCalibrator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include "spike/measure/LatencyProbe.h"

namespace le::render {

namespace {

// Median của dãy đã sắp xếp; số phần tử chẵn → trung bình hai phần tử giữa.
double sortedMedian(const std::vector<double>& v) {
    const size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return sortedMedian(v);
}

} // namespace

const char* calibrationErrorName(CalibrationError e) noexcept {
    switch (e) {
        case CalibrationError::None: return "NONE";
        case CalibrationError::NoSignal: return "NO_SIGNAL";
        case CalibrationError::TooNoisy: return "TOO_NOISY";
        case CalibrationError::Inconsistent: return "INCONSISTENT";
        case CalibrationError::DeviceChanged: return "DEVICE_CHANGED";
        case CalibrationError::Timeout: return "TIMEOUT";
        case CalibrationError::Cancelled: return "CANCELLED";
        case CalibrationError::NotPrepared: return "NOT_PREPARED";
    }
    return "UNKNOWN";
}

CalibrationResult evaluateCalibration(const std::vector<CalibrationRun>& runs, float inputPeak, float noiseRms,
                                      int32_t reportedSamples, double sampleRate, const CalibrationCriteria& c) {
    CalibrationResult r;
    r.runs = runs;
    r.inlier.assign(runs.size(), 0);
    r.totalRuns = static_cast<int32_t>(runs.size());
    r.reportedSamples = reportedSamples;
    r.inputPeak = inputPeak;
    r.noiseRms = noiseRms;
    r.sampleRate = sampleRate;
    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    const int minValid = std::max(1, c.minValidRuns);

    // 1) Lần nào nghe thấy chirp
    std::vector<size_t> heard;
    for (size_t i = 0; i < runs.size(); ++i)
        if (runs[i].lagSamples >= 0 && runs[i].score >= c.minScore) heard.push_back(i);
    r.heardRuns = static_cast<int32_t>(heard.size());

    if (!(inputPeak >= c.minInputPeak)) {   // mic không thu được gì (kể cả NaN)
        r.error = CalibrationError::NoSignal;
        return r;
    }
    if (r.heardRuns < minValid) {
        r.error = noiseRms > c.maxNoiseRms ? CalibrationError::TooNoisy : CalibrationError::NoSignal;
        return r;
    }

    // 2) Loại ngoại lai bằng MAD
    std::vector<double> lags;
    lags.reserve(heard.size());
    for (size_t i : heard) lags.push_back(static_cast<double>(runs[i].lagSamples));
    const double med = median(lags);
    std::vector<double> dev;
    dev.reserve(lags.size());
    for (double x : lags) dev.push_back(std::fabs(x - med));
    const double mad = median(dev);
    const double floorSamples = std::max(2.0, c.outlierFloorMs * sr / 1000.0);
    const double limit = std::max(c.madK * 1.4826 * mad, floorSamples);

    std::vector<double> good;
    double scoreSum = 0.0;
    for (size_t k = 0; k < heard.size(); ++k) {
        if (dev[k] > limit) continue;
        r.inlier[heard[k]] = 1;
        good.push_back(lags[k]);
        scoreSum += runs[heard[k]].score;
    }
    r.validRuns = static_cast<int32_t>(good.size());
    if (good.empty()) {   // không thể xảy ra (median luôn là inlier) nhưng giữ cho chắc
        r.error = CalibrationError::Inconsistent;
        return r;
    }
    std::sort(good.begin(), good.end());
    r.roundTripSamples = static_cast<int32_t>(std::llround(sortedMedian(good)));
    r.spreadSamples = static_cast<int32_t>(good.back() - good.front());

    // 3) Đủ số lần và đủ chụm?
    const double maxSpread = c.maxSpreadMs * sr / 1000.0;
    if (r.validRuns < minValid || static_cast<double>(r.spreadSamples) > maxSpread) {
        r.error = CalibrationError::Inconsistent;
        return r;
    }

    // 4) Độ tin cậy: tỉ lệ lần hợp lệ × chất lượng tương quan (score 0.5 trở lên = đủ tốt) × độ chụm
    //    (spread 0 → 1, spread = ngưỡng → 0.5).
    const double ratio = static_cast<double>(r.validRuns) / static_cast<double>(std::max(1, r.totalRuns));
    const double scoreFactor = std::clamp(scoreSum / static_cast<double>(r.validRuns) / 0.5, 0.0, 1.0);
    const double spreadFactor = 1.0 - 0.5 * std::clamp(static_cast<double>(r.spreadSamples) / std::max(1.0, maxSpread), 0.0, 1.0);
    r.confidence = static_cast<float>(ratio * scoreFactor * spreadFactor);
    r.offsetSamples = r.roundTripSamples - reportedSamples;
    r.ok = true;
    return r;
}

CalibrationResult runCalibration(spike::LatencyProbe& probe, const CalibrateOptions& opt, const std::atomic<bool>* cancel,
                                 std::atomic<float>* progress) {
    using Clock = std::chrono::steady_clock;
    auto fail = [&](CalibrationError e) {
        CalibrationResult r;
        r.error = e;
        r.reportedSamples = opt.reportedSamples;
        r.sampleRate = probe.sampleRate();
        return r;
    };
    auto cancelled = [&] { return cancel != nullptr && cancel->load(std::memory_order_relaxed); };

    const uint32_t epoch0 = probe.epoch();
    if (epoch0 == 0 || probe.totalSamples() <= 0 || !(probe.sampleRate() > 0.0)) return fail(CalibrationError::NotPrepared);

    constexpr int kPerSession = spike::LatencyResult::kRuns;
    const int sessions = std::max(1, (std::max(1, opt.runs) + kPerSession - 1) / kPerSession);
    const double sessionMs = static_cast<double>(probe.totalSamples()) * 1000.0 / probe.sampleRate();
    const double timeoutMs = opt.timeoutMs > 0.0 ? opt.timeoutMs : sessionMs + 3000.0;

    std::vector<CalibrationRun> runs;
    runs.reserve(static_cast<size_t>(sessions * kPerSession));
    float peak = 0.0f, noise = 0.0f;
    double sr = probe.sampleRate();

    for (int s = 0; s < sessions; ++s) {
        if (cancelled()) return fail(CalibrationError::Cancelled);
        probe.start();
        const auto t0 = Clock::now();
        while (!probe.isDone()) {
            if (cancelled()) return fail(CalibrationError::Cancelled);
            const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
            if (elapsed > timeoutMs) return fail(CalibrationError::Timeout);
            if (progress != nullptr)
                progress->store(static_cast<float>((s + std::min(0.95, elapsed / sessionMs)) / sessions), std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(std::max(1, opt.pollMs)));
        }
        const spike::LatencyResult lr = probe.analyze();
        if (lr.epoch != epoch0 || probe.epoch() != epoch0) return fail(CalibrationError::DeviceChanged);
        for (int k = 0; k < kPerSession; ++k)
            runs.push_back({lr.runs[static_cast<size_t>(k)], lr.score[static_cast<size_t>(k)]});
        peak = std::max(peak, lr.inputPeak);
        noise = std::max(noise, lr.noiseRms);
        sr = lr.sampleRate;
    }
    if (progress != nullptr) progress->store(1.0f, std::memory_order_relaxed);
    return evaluateCalibration(runs, peak, noise, opt.reportedSamples, sr, opt.criteria);
}

} // namespace le::render
