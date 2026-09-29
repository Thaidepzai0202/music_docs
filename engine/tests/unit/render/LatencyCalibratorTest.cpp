// P1-35 / P4-12 (lõi) — LatencyCalibrator: loại ngoại lai bằng MAD, lý do thất bại, và chạy thật trên LatencyProbe
// với "device" giả lập ở thread riêng (loopback có nhiễu, trễ dao động từng chirp, một chirp hỏng, đổi SR giữa chừng).
// So sánh float bằng == là CÓ CHỦ ĐÍCH (giá trị đặt thẳng, không qua tính toán).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_test_macros.hpp>

#include "render/LatencyCalibrator.h"
#include "spike/measure/LatencyProbe.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace le::render;
using le::spike::LatencyProbe;

namespace {

std::vector<CalibrationRun> runsOf(std::initializer_list<int> lags, float score = 0.8f) {
    std::vector<CalibrationRun> v;
    for (int l : lags) v.push_back({l, l >= 0 ? score : 0.05f});
    return v;
}

// "Device" chạy ở thread riêng như audio thread: mỗi block 128 frame, output = probe (hoặc 0),
// input(t) = gain · output(t − d) + nhiễu. d đổi ở ĐẦU mỗi chirp (lúc đó mọi tiếng vọng trước đã về hết) theo `delays`.
// budget: số block được phép chạy (test dừng device ở đúng chỗ để prepare lại — prepare chỉ hợp lệ khi audio dừng).
struct SimDevice {
    LatencyProbe& probe;
    std::vector<int> delays{523};
    float gain = 0.7f;
    double noiseRms = 0.0;
    uint32_t seed = 7;

    std::atomic<long> budget{1L << 40};
    std::atomic<long> blocksDone{0};
    std::atomic<bool> stop{false};
    std::thread th;

    explicit SimDevice(LatencyProbe& p) : probe(p) {}
    ~SimDevice() { halt(); }

    void run() {
        th = std::thread([this] {
            constexpr int kBlock = 128;
            constexpr size_t kRing = 1u << 16;
            std::vector<float> ring(kRing, 0.0f);
            std::vector<float> in(kBlock), outL(kBlock), outR(kBlock);
            float* outs[2] = {outL.data(), outR.data()};
            std::mt19937 rng(seed);
            std::normal_distribution<double> gauss(0.0, 1.0);
            size_t idx = 0;
            int d = delays[0];
            long silent = 1 << 20;
            int64_t t = 0;
            while (!stop.load(std::memory_order_acquire)) {
                if (blocksDone.load(std::memory_order_relaxed) >= budget.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                    continue;
                }
                for (int i = 0; i < kBlock; ++i) {
                    const int64_t src = t + i - d;
                    float x = src >= 0 ? gain * ring[static_cast<size_t>(src) % kRing] : 0.0f;
                    x += static_cast<float>(noiseRms * gauss(rng));
                    in[static_cast<size_t>(i)] = x;
                }
                std::fill(outL.begin(), outL.end(), 0.0f);
                std::fill(outR.begin(), outR.end(), 0.0f);
                probe.processRt(in.data(), outs, 2, kBlock);
                for (int i = 0; i < kBlock; ++i) {
                    const float y = outL[static_cast<size_t>(i)];
                    if (y != 0.0f && silent > 2000) d = delays[idx++ % delays.size()];   // chirp mới bắt đầu
                    silent = y != 0.0f ? 0 : silent + 1;
                    ring[static_cast<size_t>(t + i) % kRing] = y;
                }
                t += kBlock;
                blocksDone.fetch_add(1, std::memory_order_release);
            }
        });
    }
    void halt() {
        stop.store(true, std::memory_order_release);
        if (th.joinable()) th.join();
    }
    void waitIdleAt(long blocks) const {   // device đã chạy đủ `blocks` block và đang đứng chờ
        while (blocksDone.load(std::memory_order_acquire) < blocks) std::this_thread::yield();
    }
};

CalibrateOptions fastOptions(int runs, int32_t reported) {
    CalibrateOptions o;
    o.runs = runs;
    o.reportedSamples = reported;
    o.pollMs = 1;
    o.timeoutMs = 20000.0;
    return o;
}

} // namespace

TEST_CASE("evaluateCalibration: 5 lần khớp → L = median, offset = L − reported, confidence cao", "[render][latency]") {
    const auto r = evaluateCalibration(runsOf({480, 481, 480, 479, 480}, 0.9f), 0.3f, 0.001f, 400, 48000.0);
    REQUIRE(r.ok);
    CHECK(r.error == CalibrationError::None);
    CHECK(r.roundTripSamples == 480);
    CHECK(r.offsetSamples == 80);
    CHECK(r.reportedSamples == 400);
    CHECK(r.spreadSamples == 2);
    CHECK(r.validRuns == 5);
    CHECK(r.heardRuns == 5);
    CHECK(r.totalRuns == 5);
    CHECK(r.confidence > 0.9f);
    CHECK(r.confidence <= 1.0f);
}

TEST_CASE("evaluateCalibration: MAD loại 1 lần hỏng (+220 sample), L không bị kéo", "[render][latency]") {
    const auto r = evaluateCalibration(runsOf({480, 481, 480, 700, 479}), 0.3f, 0.001f, 480, 48000.0);
    REQUIRE(r.ok);
    CHECK(r.roundTripSamples == 480);
    CHECK(r.offsetSamples == 0);
    CHECK(r.validRuns == 4);
    CHECK(r.heardRuns == 5);
    CHECK(r.inlier == std::vector<uint8_t>{1, 1, 1, 0, 1});
    CHECK(r.spreadSamples == 2);
    // Số chẵn lần hợp lệ → trung bình 2 giá trị giữa, làm tròn
    const auto e = evaluateCalibration(runsOf({480, 482, 481, 483, 5000, -1}), 0.3f, 0.001f, 0, 48000.0);
    REQUIRE(e.ok);
    CHECK(e.validRuns == 4);
    CHECK(e.roundTripSamples == 482);   // (481 + 482) / 2 = 481.5 → 482
    CHECK(e.totalRuns == 6);
    CHECK(e.confidence < r.confidence);   // 4/6 hợp lệ < 4/5
}

TEST_CASE("evaluateCalibration: sàn ngưỡng giữ jitter ±1 sample khi MAD = 0", "[render][latency]") {
    // 4 lần trùng nhau → MAD = 0; không có sàn thì 481 bị loại oan
    const auto r = evaluateCalibration(runsOf({480, 480, 480, 480, 481}), 0.3f, 0.001f, 0, 48000.0);
    REQUIRE(r.ok);
    CHECK(r.validRuns == 5);
}

TEST_CASE("evaluateCalibration: lý do thất bại NO_SIGNAL / TOO_NOISY / INCONSISTENT", "[render][latency]") {
    // Mic im lặng
    auto r = evaluateCalibration(runsOf({-1, -1, -1, -1, -1}), 1e-5f, 1e-6f, 400, 48000.0);
    CHECK_FALSE(r.ok);
    CHECK(r.error == CalibrationError::NoSignal);
    CHECK(r.offsetSamples == 0);
    CHECK(r.confidence == 0.0f);
    // Mic có tiếng nhỏ (phòng yên tĩnh) nhưng không nghe thấy chirp (cắm tai nghe)
    r = evaluateCalibration(runsOf({-1, 480, -1, -1, -1}), 0.01f, 0.002f, 400, 48000.0);
    CHECK(r.error == CalibrationError::NoSignal);
    CHECK(r.heardRuns == 1);
    // Phòng ồn: nhiễu nền −20 dBFS, chỉ nghe thấy 2/5
    r = evaluateCalibration(runsOf({480, -1, 481, -1, -1}), 0.5f, 0.1f, 400, 48000.0);
    CHECK(r.error == CalibrationError::TooNoisy);
    // Nghe thấy đủ nhưng lệch nhau: 2 lần khớp + 1 lần xa → sau MAD còn 2 < 3
    r = evaluateCalibration(runsOf({480, 481, 2000, -1, -1}), 0.3f, 0.001f, 400, 48000.0);
    CHECK(r.error == CalibrationError::Inconsistent);
    CHECK(r.validRuns == 2);
    // Rải đều (MAD lớn, không loại được lần nào) → spread 320 sample > 1 ms
    r = evaluateCalibration(runsOf({400, 480, 560, 640, 720}), 0.3f, 0.001f, 400, 48000.0);
    CHECK(r.error == CalibrationError::Inconsistent);
    CHECK(r.validRuns == 5);
    CHECK(r.spreadSamples == 320);
    CHECK(r.roundTripSamples == 560);   // vẫn báo để debug
    CHECK_FALSE(r.ok);
    // Score dưới ngưỡng của calibrator (cao hơn của probe) → không tính là nghe thấy
    CalibrationCriteria strict;
    strict.minScore = 0.9f;
    r = evaluateCalibration(runsOf({480, 480, 480, 480, 480}, 0.5f), 0.3f, 0.001f, 400, 48000.0, strict);
    CHECK(r.heardRuns == 0);
    CHECK(r.error == CalibrationError::NoSignal);

    CHECK(std::string(calibrationErrorName(CalibrationError::NoSignal)) == "NO_SIGNAL");
    CHECK(std::string(calibrationErrorName(CalibrationError::TooNoisy)) == "TOO_NOISY");
    CHECK(std::string(calibrationErrorName(CalibrationError::Inconsistent)) == "INCONSISTENT");
    CHECK(std::string(calibrationErrorName(CalibrationError::DeviceChanged)) == "DEVICE_CHANGED");
    CHECK(std::string(calibrationErrorName(CalibrationError::None)) == "NONE");
}

TEST_CASE("runCalibration: loopback giả lập có nhiễu + trễ dao động ±2 sample → L đúng", "[render][latency]") {
    LatencyProbe probe;
    probe.prepare(48000.0, 128);
    SimDevice dev(probe);
    dev.delays = {523, 525, 521, 524, 522};
    dev.noiseRms = 0.01;   // ≈ −40 dBFS
    dev.run();
    std::atomic<float> progress{0.0f};
    const auto r = runCalibration(probe, fastOptions(5, 500), nullptr, &progress);
    dev.halt();
    INFO("error " << calibrationErrorName(r.error) << " L " << r.roundTripSamples);
    REQUIRE(r.ok);
    CHECK(r.totalRuns == 5);
    CHECK(r.validRuns == 5);
    CHECK(std::abs(r.roundTripSamples - 523) <= 1);
    CHECK(r.offsetSamples == r.roundTripSamples - 500);
    CHECK(r.spreadSamples >= 3);    // trễ dao động thật sự được đo ra
    CHECK(r.spreadSamples <= 5);
    CHECK(r.confidence > 0.5f);
    CHECK(r.sampleRate == 48000.0);
    CHECK(progress.load() == 1.0f);
}

TEST_CASE("runCalibration: 10 lần (2 lượt probe), 1 chirp hỏng +200 sample bị MAD loại", "[render][latency]") {
    LatencyProbe probe;
    probe.prepare(48000.0, 128);
    SimDevice dev(probe);
    dev.delays = {700, 701, 699, 700, 900, 700, 702, 700, 699, 701};
    dev.noiseRms = 0.005;
    dev.run();
    const auto r = runCalibration(probe, fastOptions(10, 650));
    dev.halt();
    REQUIRE(r.ok);
    CHECK(r.totalRuns == 10);
    CHECK(r.heardRuns == 10);
    CHECK(r.validRuns == 9);
    CHECK(r.inlier[4] == 0);
    CHECK(std::abs(r.roundTripSamples - 700) <= 1);
    CHECK(r.spreadSamples <= 4);
    // runs = 7 → làm tròn lên 10
    SimDevice dev2(probe);
    dev2.delays = {700};
    dev2.run();
    CHECK(runCalibration(probe, fastOptions(7, 0)).totalRuns == 10);
}

TEST_CASE("runCalibration: mic tắt → NO_SIGNAL, nhiễu lớn → TOO_NOISY", "[render][latency]") {
    LatencyProbe probe;
    probe.prepare(48000.0, 128);
    {
        SimDevice dev(probe);
        dev.gain = 0.0f;
        dev.run();
        const auto r = runCalibration(probe, fastOptions(5, 400));
        CHECK(r.error == CalibrationError::NoSignal);
        CHECK(r.heardRuns == 0);
    }
    {
        SimDevice dev(probe);
        dev.noiseRms = 3.0;   // nhiễu to hơn chirp ~22 dB
        dev.run();
        const auto r = runCalibration(probe, fastOptions(5, 400));
        INFO("heard " << r.heardRuns << " noise " << r.noiseRms);
        CHECK(r.error == CalibrationError::TooNoisy);
        CHECK(r.noiseRms > 1.0f);
    }
}

TEST_CASE("runCalibration: device prepare lại (đổi SR) giữa hai lượt → DEVICE_CHANGED, không crash", "[render][latency]") {
    LatencyProbe probe;
    probe.prepare(48000.0, 128);
    const long session1 = static_cast<long>((probe.totalSamples() + 127) / 128);
    SimDevice dev(probe);
    dev.budget.store(0);
    dev.run();

    CalibrationResult r;
    std::thread worker([&] { r = runCalibration(probe, fastOptions(10, 400)); });
    while (!probe.isRunning()) std::this_thread::yield();   // lượt 1 đã được yêu cầu
    dev.budget.store(session1 + 2, std::memory_order_release);
    dev.waitIdleAt(session1 + 2);                            // device dừng sau lượt 1
    while (!probe.isRunning()) std::this_thread::yield();   // worker đã analyze lượt 1 và yêu cầu lượt 2
    probe.prepare(44100.0, 128);                             // "đổi route": audio đang dừng → hợp lệ
    CHECK(probe.epoch() == 2);
    dev.budget.store(1L << 40, std::memory_order_release);
    worker.join();
    dev.halt();
    CHECK(r.error == CalibrationError::DeviceChanged);
    CHECK_FALSE(r.ok);
}

TEST_CASE("runCalibration: chưa prepare / timeout / huỷ", "[render][latency]") {
    LatencyProbe fresh;
    CHECK(runCalibration(fresh, fastOptions(5, 0)).error == CalibrationError::NotPrepared);

    LatencyProbe probe;
    probe.prepare(48000.0, 128);
    auto o = fastOptions(5, 0);
    o.timeoutMs = 30.0;                                      // không có device chạy → hết giờ
    CHECK(runCalibration(probe, o).error == CalibrationError::Timeout);

    std::atomic<bool> cancel{false};
    CalibrationResult r;
    std::thread worker([&] { r = runCalibration(probe, fastOptions(5, 0), &cancel); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    cancel.store(true);
    worker.join();
    CHECK(r.error == CalibrationError::Cancelled);
}

TEST_CASE("LatencyProbe: prepare lúc lượt đo đang chờ → đo lại với bảng mới, analyze giữ bảng cũ an toàn", "[spike][latency]") {
    LatencyProbe probe;
    probe.prepare(48000.0, 128);
    CHECK(probe.epoch() == 1);
    probe.start();
    std::vector<float> in(128, 0.0f), outL(128), outR(128);
    float* outs[2] = {outL.data(), outR.data()};
    for (int i = 0; i < 10; ++i) probe.processRt(in.data(), outs, 2, 128);   // lượt đang dở
    probe.prepare(44100.0, 128);
    CHECK(probe.epoch() == 2);
    CHECK(probe.isRunning());                  // KHÔNG bị báo xong với buffer rỗng
    CHECK_FALSE(probe.isDone());
    CHECK(probe.sampleRate() == 44100.0);
    while (!probe.isDone()) probe.processRt(in.data(), outs, 2, 128);
    const auto r = probe.analyze();
    CHECK(r.epoch == 2);
    CHECK(r.sampleRate == 44100.0);
    CHECK(r.noiseRms == 0.0f);                 // mic im lặng
}
