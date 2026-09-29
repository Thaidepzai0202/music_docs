// P0-07 — LatencyProbe: tín hiệu trễ đã biết + nhiễu → phải tìm đúng độ trễ ±1 sample.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (output phải đúng từng bit, không phải gần đúng).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spike/measure/LatencyProbe.h"

#include <cmath>
#include <cstdlib>
#include <random>
#include <vector>

using le::spike::LatencyProbe;
using le::spike::LatencyResult;

namespace {

double rms(const std::vector<float>& v) {
    double e = 0.0;
    for (float x : v) e += static_cast<double>(x) * x;
    return std::sqrt(e / static_cast<double>(v.size()));
}

// Giả lập "loa → mic": input tại thời điểm t = gain · output(t − delay) + nhiễu trắng.
// Chạy probe theo từng block như audio callback thật. Yêu cầu delay ≥ blockSize
// (luôn đúng trên máy thật: round-trip lớn hơn nhiều so với 1 buffer).
struct LoopbackSim {
    int    delay = 523;
    float  gain = 1.0f;       // âm = đảo pha
    double noiseRms = 0.0;
    uint32_t seed = 1234;

    LatencyResult run(LatencyProbe& probe, int blockSize) const {
        std::mt19937 rng(seed);
        std::normal_distribution<double> gauss(0.0, 1.0);
        std::vector<float> history;               // mọi sample đã phát ra (kênh 0)
        history.reserve(static_cast<size_t>(probe.totalSamples() + blockSize));
        std::vector<float> in(static_cast<size_t>(blockSize)), outL(in.size()), outR(in.size());
        float* outs[2] = {outL.data(), outR.data()};

        probe.start();
        int64_t t = 0;
        const int64_t limit = probe.totalSamples() + 10 * blockSize;
        while (!probe.isDone() && t < limit) {
            for (int i = 0; i < blockSize; ++i) {
                const int64_t src = t + i - delay;
                float x = (src >= 0) ? gain * history[static_cast<size_t>(src)] : 0.0f;
                x += static_cast<float>(noiseRms * gauss(rng));
                in[static_cast<size_t>(i)] = x;
            }
            probe.processRt(in.data(), outs, 2, blockSize);
            history.insert(history.end(), outL.begin(), outL.end());
            t += blockSize;
        }
        REQUIRE(probe.isDone());
        return probe.analyze();
    }
};

// RMS của chirp khi đang phát (để đặt nhiễu theo dB so với tín hiệu)
double chirpRms(const LatencyProbe& probe) {
    return rms(probe.chirp()) * LatencyProbe::Config{}.gain;
}

} // namespace

TEST_CASE("LatencyProbe::findLag tìm đúng độ trễ 523 sample với nhiễu -30 dB", "[spike][latency]") {
    LatencyProbe probe;
    probe.prepare(48000.0, 256);
    const auto& ref = probe.chirp();
    const int delay = 523;
    const int64_t start = 1000;

    std::vector<float> rec(20000, 0.0f);
    for (size_t k = 0; k < ref.size(); ++k) rec[static_cast<size_t>(start + delay) + k] = ref[k];

    std::mt19937 rng(7);
    std::normal_distribution<double> gauss(0.0, 1.0);
    const double nr = rms(ref) * std::pow(10.0, -30.0 / 20.0);
    for (auto& x : rec) x += static_cast<float>(nr * gauss(rng));

    float score = 0.0f;
    const int32_t lag = LatencyProbe::findLag(ref.data(), static_cast<int>(ref.size()), rec.data(),
                                              static_cast<int64_t>(rec.size()), start, 10000, &score);
    CHECK(std::abs(lag - delay) <= 1);
    CHECK(score > 0.9f);
}

TEST_CASE("LatencyProbe đo loopback 523 sample + nhiễu -30 dB ở nhiều block size", "[spike][latency]") {
    for (int block : {64, 128, 256, 480}) {
        CAPTURE(block);
        LatencyProbe probe;
        probe.prepare(48000.0, block);
        LoopbackSim sim;
        sim.delay = 523;
        sim.gain = 0.25f;                                           // mic thu nhỏ hơn loa 12 dB
        sim.noiseRms = chirpRms(probe) * 0.25 * std::pow(10.0, -30.0 / 20.0);
        const LatencyResult r = sim.run(probe, block);

        REQUIRE(r.ok);
        CHECK(r.validRuns == LatencyResult::kRuns);
        CHECK(std::abs(r.measuredSamples - 523) <= 1);
        CHECK(r.spreadSamples <= 1);
        for (int run = 0; run < LatencyResult::kRuns; ++run) {
            CAPTURE(run);
            CHECK(std::abs(r.runs[static_cast<size_t>(run)] - 523) <= 1);
        }
        CHECK(r.measuredMs == Catch::Approx(523.0 / 48.0).margin(0.05));
    }
}

TEST_CASE("LatencyProbe chịu được đảo pha, nhiễu 0 dB và độ trễ lớn", "[spike][latency]") {
    struct Case { int delay; float gain; double snrDb; double sr; };
    for (const Case c : {Case{2400, -0.5f, 30.0, 48000.0},     // 50 ms, đảo pha
                         Case{12000, 0.1f, 0.0, 48000.0},      // 250 ms, nhiễu to bằng tín hiệu
                         Case{700, 0.3f, 20.0, 44100.0}}) {    // sample rate 44.1 kHz
        CAPTURE(c.delay, c.gain, c.snrDb, c.sr);
        LatencyProbe probe;
        probe.prepare(c.sr, 256);
        LoopbackSim sim;
        sim.delay = c.delay;
        sim.gain = c.gain;
        sim.noiseRms = chirpRms(probe) * std::fabs(c.gain) * std::pow(10.0, -c.snrDb / 20.0);
        const LatencyResult r = sim.run(probe, 256);
        REQUIRE(r.ok);
        CHECK(std::abs(r.measuredSamples - c.delay) <= 1);
    }
}

TEST_CASE("LatencyProbe báo không thấy tín hiệu khi mic im lặng", "[spike][latency]") {
    LatencyProbe probe;
    probe.prepare(48000.0, 128);
    LoopbackSim sim;
    sim.gain = 0.0f;
    sim.noiseRms = 1e-4;   // chỉ có nhiễu nền
    const LatencyResult r = sim.run(probe, 128);
    CHECK_FALSE(r.ok);
    CHECK(r.validRuns == 0);
    CHECK(r.measuredSamples == -1);
}

TEST_CASE("LatencyProbe: vòng đời start / isDone / không đụng output khi rảnh", "[spike][latency]") {
    LatencyProbe probe;
    probe.prepare(48000.0, 128);
    CHECK_FALSE(probe.isDone());
    CHECK_FALSE(probe.isRunning());

    std::vector<float> in(128, 0.0f), outL(128, 7.0f), outR(128, 7.0f);
    float* outs[2] = {outL.data(), outR.data()};

    // Chưa start: processRt không được ghi vào output
    probe.processRt(in.data(), outs, 2, 128);
    CHECK(outL[0] == 7.0f);
    CHECK(outR[127] == 7.0f);

    probe.start();
    CHECK(probe.isRunning());
    CHECK_FALSE(probe.isDone());           // chưa có block nào chạy → chưa xong

    // Chạy hết một lượt, kiểm tra chirp xuất hiện đúng vị trí emitStart(0) trên cả 2 kênh
    std::vector<float> played;
    while (!probe.isDone()) {
        probe.processRt(nullptr, outs, 2, 128);   // in = nullptr (không có mic) phải an toàn
        played.insert(played.end(), outL.begin(), outL.end());
        CHECK(outL == outR);
    }
    CHECK_FALSE(probe.isRunning());
    const auto& ch = probe.chirp();
    const int64_t e0 = probe.emitStart(0);
    const float g = LatencyProbe::Config{}.gain;
    for (size_t k = 0; k < ch.size(); k += 97)
        CHECK(played[static_cast<size_t>(e0) + k] == g * ch[k]);
    CHECK(played[static_cast<size_t>(e0) - 1] == 0.0f);

    // start lần 2: isDone phải về false cho tới khi lượt mới xong
    probe.start();
    CHECK_FALSE(probe.isDone());
    while (!probe.isDone()) probe.processRt(in.data(), outs, 2, 128);
    CHECK(probe.isDone());
}
