// RtEngine chạy offline, không cần thiết bị: lệnh spike → âm thanh → state → event.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "core/RtEngine.h"
#include "core/XrunDetector.h"

using namespace le::core;
namespace spike = le::spike;

namespace {

struct OfflineRig {
    static constexpr double kRate = 48000.0;
    static constexpr int kBlock = 128;

    CommandQueue commands{kRtCommandCapacity};
    RtToNrtQueue toNrt{kRtToNrtCapacity};
    MidiQueue midiIn{kMidiToRtCapacity};
    StatePublisher publisher;
    std::vector<float> recordBuf = std::vector<float>(48000 * 10);
    RtEngine rt{commands, toNrt, midiIn, publisher};

    std::vector<float> inBuf = std::vector<float>(kBlock, 0.0f);
    std::vector<float> outL = std::vector<float>(kBlock), outR = std::vector<float>(kBlock);
    uint64_t hostNs = 1'000'000;

    OfflineRig() {
        rt.setRecordBuffer(recordBuf.data(), (int) recordBuf.size());
        rt.prepare(kRate, 1024);
    }

    void send(uint16_t type, int32_t i0 = 0, float f0 = 0, float f1 = 0) {
        LeCommand c{};
        c.type = type;
        c.track = c.slot = -1;
        c.i0 = i0;
        c.f0 = f0;
        c.f1 = f1;
        REQUIRE(commands.try_push(c));
    }

    // Chạy 1 block; host time tăng đúng thời lượng block (không xrun).
    void block(int n = kBlock, bool withInput = true) {
        const float* in[1] = {inBuf.data()};
        float* out[2] = {outL.data(), outR.data()};
        le::io::CallbackContext ctx;
        ctx.hostTimeNs = hostNs;
        rt.process(withInput ? in : nullptr, withInput ? 1 : 0, out, 2, n, ctx);
        hostNs += (uint64_t) ((double) n * 1e9 / kRate);
    }

    LeState state() {
        LeState s;
        publisher.read(s);
        return s;
    }

    float peak() const {
        float p = 0;
        for (float x : outL) p = std::max(p, std::fabs(x));
        return p;
    }
};

} // namespace

TEST_CASE("RtEngine: im lặng khi chưa có lệnh, state được publish", "[core][rt]") {
    OfflineRig rig;
    rig.block();
    REQUIRE(rig.peak() == 0.0f);
    const auto s = rig.state();
    REQUIRE(s.publishCounter == 1);
    REQUIRE(s.sampleRate == 48000.0);
    REQUIRE(s.bufferSize == OfflineRig::kBlock);
    REQUIRE(s.xrunCount == 0);
    REQUIRE(s.trackPlayingSlot[0] == -1);
    rig.block();
    REQUIRE(rig.state().publishCounter == 2);
}

TEST_CASE("RtEngine: SPIKE_SINE ra sine đúng biên độ và tần số, có ramp", "[core][rt][spike]") {
    OfflineRig rig;
    rig.send(LE_CMD_SPIKE_SINE, 0, 1000.0f, 0.5f);
    rig.block();
    REQUIRE(std::fabs(rig.outL[0]) < 0.01f);   // ramp 10ms: sample đầu gần 0, không click

    std::vector<float> all;
    for (int b = 0; b < 40; ++b) {   // ~107ms
        rig.block();
        all.insert(all.end(), rig.outL.begin(), rig.outL.end());
    }
    REQUIRE(rig.peak() == Catch::Approx(0.5f).margin(0.01f));
    REQUIRE(rig.outL == rig.outR);

    // Đếm số lần cắt 0 đi lên trong 20 block cuối (2560 sample = 53.3ms → ~53 chu kỳ ở 1 kHz)
    int crossings = 0;
    for (size_t i = all.size() - 2560 + 1; i < all.size(); ++i)
        if (all[i - 1] < 0.0f && all[i] >= 0.0f) ++crossings;
    REQUIRE(crossings >= 52);
    REQUIRE(crossings <= 54);

    rig.send(LE_CMD_SPIKE_SINE, 0, 1000.0f, 0.0f);   // gain 0 = tắt (ramp xuống)
    for (int b = 0; b < 10; ++b) rig.block();
    REQUIRE(rig.peak() == 0.0f);
}

TEST_CASE("RtEngine: SPIKE_RECORD thu đúng số sample, báo RECORDING_FINISHED, phát loop", "[core][rt][spike]") {
    OfflineRig rig;
    for (int i = 0; i < OfflineRig::kBlock; ++i) rig.inBuf[(size_t) i] = 0.25f;

    rig.send(LE_CMD_SPIKE_RECORD, 100);   // 100ms = 4800 sample
    rig.block();
    REQUIRE(rig.rt.spikeProcessor().isRecording());
    REQUIRE(rig.state().anyRecording == 1);
    REQUIRE(rig.toNrt.front() == nullptr);

    for (int b = 0; b < 40; ++b) rig.block();   // 41 × 128 = 5248 ≥ 4800
    REQUIRE_FALSE(rig.rt.spikeProcessor().isRecording());
    REQUIRE(rig.rt.spikeProcessor().recordedFrames() == 4800);
    REQUIRE(rig.recordBuf[0] == 0.25f);
    REQUIRE(rig.recordBuf[4799] == 0.25f);
    REQUIRE(rig.recordBuf[4800] == 0.0f);

    const RtMessage* m = rig.toNrt.front();
    REQUIRE(m != nullptr);
    REQUIRE(m->type == LE_EVT_RECORDING_FINISHED);
    REQUIRE(m->a == -1);
    REQUIRE(m->value == 4800.0);
    rig.toNrt.pop();
    REQUIRE(rig.toNrt.front() == nullptr);   // chỉ báo 1 lần

    // Phát loop (không có input để khỏi lẫn passthrough)
    rig.send(LE_CMD_SPIKE_PLAY_RECORD, 1);
    for (int b = 0; b < 10; ++b) rig.block(OfflineRig::kBlock, false);
    REQUIRE(rig.peak() == Catch::Approx(0.25f).margin(1e-4f));
    rig.send(LE_CMD_SPIKE_PLAY_RECORD, 0);
    for (int b = 0; b < 10; ++b) rig.block(OfflineRig::kBlock, false);
    REQUIRE(rig.peak() == 0.0f);
}

TEST_CASE("RtEngine: SPIKE_PASSTHROUGH đưa mic ra loa", "[core][rt][spike]") {
    OfflineRig rig;
    for (int i = 0; i < OfflineRig::kBlock; ++i) rig.inBuf[(size_t) i] = 0.1f;
    rig.block();
    REQUIRE(rig.peak() == 0.0f);
    rig.send(LE_CMD_SPIKE_PASSTHROUGH, 1);
    for (int b = 0; b < 10; ++b) rig.block();
    REQUIRE(rig.peak() == Catch::Approx(0.1f).margin(1e-5f));
    REQUIRE(rig.state().inputPeak == Catch::Approx(0.1f).margin(1e-3f));
}

TEST_CASE("RtEngine: SPIKE_LOAD_VOICES báo activeVoices", "[core][rt][spike]") {
    OfflineRig rig;
    rig.send(LE_CMD_SPIKE_LOAD_VOICES, 64);
    rig.block();
    REQUIRE(rig.state().activeVoices == 64);
    rig.send(LE_CMD_SPIKE_LOAD_VOICES, 500);   // kẹp về 128
    rig.block();
    REQUIRE(rig.state().activeVoices == 128);
}

TEST_CASE("RtEngine: block lớn hơn maxBlock được chia nhỏ", "[core][rt]") {
    OfflineRig rig;
    rig.rt.prepare(OfflineRig::kRate, 64);
    rig.send(LE_CMD_SPIKE_SINE, 0, 440.0f, 0.5f);
    rig.block(OfflineRig::kBlock);   // 128 > 64
    REQUIRE(rig.state().bufferSize == OfflineRig::kBlock);
}

TEST_CASE("RtEngine: phát hiện xrun theo host time", "[core][rt]") {
    OfflineRig rig;
    rig.block();
    rig.block();
    REQUIRE(rig.state().xrunCount == 0);
    rig.hostNs += 10'000'000;   // lỡ 10ms
    rig.block();
    REQUIRE(rig.state().xrunCount == 1);
    rig.block();
    REQUIRE(rig.state().xrunCount == 1);

    rig.rt.setDeviceXruns(5);   // driver đếm nhiều hơn → lấy max
    rig.block();
    REQUIRE(rig.state().xrunCount == 5);
}

TEST_CASE("XrunDetector: ngưỡng 1.5 × thời lượng block", "[core]") {
    XrunDetector d;
    d.prepare(48000.0);
    const uint64_t blockNs = 2'666'667;   // 128 frame
    uint64_t t = 1'000'000'000;
    REQUIRE_FALSE(d.onCallback(t, 128));                 // callback đầu: không so sánh
    REQUIRE_FALSE(d.onCallback(t += blockNs, 128));
    REQUIRE_FALSE(d.onCallback(t += blockNs * 14 / 10, 128));   // 1.4× → chưa tính
    REQUIRE(d.onCallback(t += blockNs * 16 / 10, 128));        // 1.6× → xrun
    REQUIRE_FALSE(d.onCallback(0, 128));                 // không có host time → bỏ qua
    REQUIRE(d.count() == 1);
}

TEST_CASE("RtEngine: LatencyProbe đo đúng loopback giả lập, chirp ghi đè sine", "[core][rt][spike][latency]") {
    OfflineRig rig;
    constexpr int kDelay = 523;   // in[t] = out[t - kDelay] (kDelay ≥ block nên luôn là output đã tính)
    std::vector<float> history;   // mọi sample output đã phát

    rig.send(LE_CMD_SPIKE_SINE, 0, 440.0f, 0.3f);
    auto& probe = rig.rt.latencyProbe();
    probe.start();
    REQUIRE(probe.isRunning());

    const int maxBlocks = (int) (probe.totalSamples() / OfflineRig::kBlock) + 20;
    int blocks = 0;
    float peakWhileMeasuring = 0.0f;
    while (!probe.isDone() && blocks < maxBlocks) {
        const auto t0 = (long) history.size();
        for (int i = 0; i < OfflineRig::kBlock; ++i) {
            const long src = t0 + i - kDelay;
            rig.inBuf[(size_t) i] = src >= 0 ? history[(size_t) src] : 0.0f;
        }
        rig.block();
        history.insert(history.end(), rig.outL.begin(), rig.outL.end());
        if (blocks > 2 && blocks < 30) peakWhileMeasuring = std::max(peakWhileMeasuring, rig.peak());   // pre-roll: chỉ im lặng
        ++blocks;
    }
    REQUIRE(probe.isDone());
    REQUIRE(peakWhileMeasuring < 1e-6f);   // lúc đo, sine bị thay bằng im lặng/chirp

    const auto r = probe.analyze();
    REQUIRE(r.ok);
    REQUIRE(r.validRuns == spike::LatencyResult::kRuns);
    REQUIRE(std::abs(r.measuredSamples - kDelay) <= 1);

    for (int b = 0; b < 10; ++b) rig.block();   // đo xong: sine trở lại
    REQUIRE(rig.peak() == Catch::Approx(0.3f).margin(0.01f));
}
