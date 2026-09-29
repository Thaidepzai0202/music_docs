// P1-35 / P4-12: latency.calibrate (loopback giả trễ D sample) + latency.setOffset.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {
juce::var callJson(Engine& e, const std::string& j) {
    juce::var v;
    juce::JSON::parse(juce::String::fromUTF8(e.call(j.c_str()).c_str()), v);
    return v;
}
int stateLatency() {
    LeState s{};
    globalStatePublisher().read(s);
    return s.latencyRoundTripSamples;
}
} // namespace

TEST_CASE("latency.setOffset / calibrate: L = device báo + offset; loopback trễ D → đo đúng D và áp dụng ngay", "[core][latency]") {
    LeConfig cfg{};
    cfg.apiVersion = LE_API_VERSION;
    cfg.numInputChannels = 1;
    cfg.preferredBufferSize = 128;
    cfg.preferredSampleRate = 48000.0;
    auto d = std::make_unique<le::io::OfflineDeviceIO>(128);
    auto* dev = d.get();
    dev->setLatencies(100, 100);   // device báo 200
    Engine e(cfg, std::move(d));
    REQUIRE(e.audioStart() == LE_OK);

    constexpr int D = 1000;        // round-trip thật (loopback)
    std::vector<float> history;    // output L đã phát
    std::vector<float> in(128), L(128), R(128);
    auto block = [&] {
        const auto t = (std::int64_t) history.size();
        for (int i = 0; i < 128; ++i) in[(size_t) i] = t + i - D >= 0 ? 0.5f * history[(size_t) (t + i - D)] : 0.0f;
        const float* ins[1] = {in.data()};
        float* outs[2] = {L.data(), R.data()};
        dev->render(ins, 1, outs, 2, 128);
        history.insert(history.end(), L.begin(), L.end());
        e.pump();
    };
    block();
    REQUIRE(stateLatency() == 200);
    REQUIRE((bool) callJson(e, R"({"op":"latency.setOffset","samples":-50})")["ok"]);
    block();
    REQUIRE(stateLatency() == 150);
    REQUIRE(callJson(e, R"({"op":"latency.setOffset","samples":1e9})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(callJson(e, R"({"op":"latency.setOffset","samples":1.5})")["error"]["code"].toString() == "INVALID_ARG");

    const auto job = callJson(e, R"({"op":"latency.calibrate"})");
    REQUIRE((bool) job["ok"]);
    REQUIRE(callJson(e, R"({"op":"latency.calibrate"})")["error"]["code"].toString() == "INVALID_ARG");   // đang đo
    const std::string q = R"({"op":"job.result","jobId":)" + job["result"]["jobId"].toString().toStdString() + "}";
    juce::var res;
    for (int i = 0; i < 20000; ++i) {   // ≤ ~53 s audio
        block();
        res = callJson(e, q)["result"];
        if (res["status"].toString() != "running") break;
        if ((i & 63) == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(res["status"].toString() == "done");
    const auto& r = res["result"];
    REQUIRE((int) r["measuredSamples"] == D);
    REQUIRE((int) r["reportedSamples"] == 200);
    REQUIRE((int) r["offsetSamples"] == D - 200);
    REQUIRE((double) r["confidence"] > 0.5);
    block();
    REQUIRE(stateLatency() == D);   // áp dụng ngay (thay offset −50 chỉnh tay trước đó)
    REQUIRE((int) callJson(e, R"({"op":"engine.info"})")["result"]["latencyOffsetSamples"] == D - 200);
}
