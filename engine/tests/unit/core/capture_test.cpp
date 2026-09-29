// P3-02 capture.start/stop/analyze · P3-05 instrument.createFromRecording (+ cache zone) · P3-06/07 setMode /
// setEnvelope · track.setInstrument {kind:"user"} · P1-30 RECORDING_FINISHED khi lượt overdub MIDI kết thúc.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/AudioFileIO.h"
#include "io/CafWriter.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {

struct Ev {
    int type, a, b;
    double value;
};
std::vector<Ev>& evs() {
    static std::vector<Ev> v;
    return v;
}
void onEvent(int32_t type, int32_t a, int32_t b, int64_t, double value) { evs().push_back({type, a, b, value}); }

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    juce::File dir;
    std::int64_t t = 0;      // sample input đã phát
    double hz = 220.0;       // input: sine (0 = im lặng)
    std::vector<float> L = std::vector<float>(128), R = std::vector<float>(128), in = std::vector<float>(128);

    explicit Rig(int inputs = 1) {
        evs().clear();
        setEventCallback(&onEvent);
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.numInputChannels = inputs;
        cfg.preferredBufferSize = 128;
        cfg.preferredSampleRate = 48000.0;
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(cfg, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-cap", "", false);
        dir.createDirectory();
        REQUIRE(ok(R"({"op":"project.open","dir":")" + dir.getFullPathName().toStdString() + R"("})"));
    }
    ~Rig() {
        e.reset();
        setEventCallback(nullptr);
        dir.deleteRecursively();
    }
    juce::var call(const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e->call(j.c_str()).c_str()), v);
        return v;
    }
    bool ok(const std::string& j) { return (bool) call(j)["ok"]; }
    std::string err(const std::string& j) { return call(j)["error"]["code"].toString().toStdString(); }
    void send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0, float f0 = 0.0f) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        c.f0 = f0;
        REQUIRE(e->send(c));
    }
    float input(std::int64_t i) const { return hz > 0.0 ? 0.5f * (float) std::sin(2.0 * 3.14159265358979 * hz * (double) i / 48000.0) : 0.0f; }
    float peak = 0.0f;   // đỉnh output của lần render gần nhất
    void render(int frames) {
        peak = 0.0f;
        for (int done = 0; done < frames; done += 128) {
            const int n = std::min(128, frames - done);
            for (int i = 0; i < n; ++i) in[(size_t) i] = input(t + i);
            const float* ins[1] = {in.data()};
            float* outs[2] = {L.data(), R.data()};
            dev->render(ins, 1, outs, 2, n);
            for (int i = 0; i < n; ++i) peak = std::max(peak, std::fabs(L[(size_t) i]));
            t += n;
            e->pump();
        }
    }
    juce::var waitJob(const juce::var& reply) {
        const std::string q = R"({"op":"job.result","jobId":)" + reply["result"]["jobId"].toString().toStdString() + "}";
        for (int i = 0; i < 6000; ++i) {
            const auto r = call(q)["result"];
            if (r["status"].toString() != "running") return r;
            e->pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        FAIL("job quá 30 s");
        return {};
    }
    int count(int type) const {
        int n = 0;
        for (const auto& x : evs()) n += x.type == type ? 1 : 0;
        return n;
    }
    std::string path(const char* rel) const { return dir.getChildFile(rel).getFullPathName().toStdString(); }
};

} // namespace

TEST_CASE("capture.start / stop: thu input ra file, stop idempotent, kiểm tham số", "[core][capture]") {
    Rig r;
    REQUIRE(r.err(R"({"op":"capture.stop"})") == "INVALID_ARG");   // chưa start
    REQUIRE(r.err(R"({"op":"capture.start","maxSeconds":1})") == "INVALID_ARG");
    REQUIRE(r.err(R"({"op":"capture.start","path":"a.caf","maxSeconds":500})") == "INVALID_ARG");
    r.render(1000);   // input trước capture.start không được thu
    const std::int64_t t0 = r.t;
    REQUIRE(r.ok(R"({"op":"capture.start","path":"instruments/i1/source.caf","maxSeconds":2})"));
    r.render(12000);
    const auto res = r.call(R"({"op":"capture.stop"})")["result"];
    REQUIRE(res["file"].toString() == "instruments/i1/source.caf");
    const double seconds = (double) res["seconds"];
    REQUIRE(seconds > 0.2);
    REQUIRE(seconds <= 12000.0 / 48000.0 + 1e-9);
    const auto again = r.call(R"({"op":"capture.stop"})")["result"];   // idempotent
    REQUIRE((double) again["seconds"] == seconds);

    const auto d = le::io::decodeAudioFile(r.path("instruments/i1/source.caf"));
    REQUIRE(d.error == LE_OK);
    REQUIRE(d.data->numFrames() == (std::int64_t) std::llround(seconds * 48000.0));
    // Mẫu đầu tiên là input tại lúc RT nhận vé (đầu block sau capture.start) — đúng từng sample
    const float* x = d.data->channel(0);
    std::int64_t offset = -1;
    for (std::int64_t o = 0; o <= 256 && offset < 0; ++o) {
        bool same = true;
        for (int i = 0; i < 64 && same; ++i) same = x[i] == r.input(t0 + o + i);
        if (same) offset = o;
    }
    REQUIRE(offset >= 0);
    for (std::int64_t i = 0; i < d.data->numFrames(); ++i) REQUIRE(x[i] == r.input(t0 + offset + i));
}

TEST_CASE("capture: tới maxSeconds tự dừng → RECORDING_FINISHED(-2, -2, frames), stop sau đó trả đúng kết quả", "[core][capture]") {
    Rig r;
    REQUIRE(r.ok(R"({"op":"capture.start","path":"cap.caf","maxSeconds":0.2})"));
    r.render(24000);
    REQUIRE(r.count(LE_EVT_RECORDING_FINISHED) == 1);
    REQUIRE(evs().back().a == -2);
    REQUIRE(evs().back().b == -2);
    REQUIRE(evs().back().value == 9600.0);
    const auto res = r.call(R"({"op":"capture.stop"})")["result"];
    REQUIRE((double) res["seconds"] == 0.2);
    REQUIRE(juce::File(r.path("cap.caf")).existsAsFile());
}

TEST_CASE("capture: input đang tắt (audio.setInputEnabled false) → MIC_PERMISSION", "[core][capture]") {
    Rig r;
    REQUIRE(r.ok(R"({"op":"audio.setInputEnabled","enabled":false})"));
    REQUIRE(r.err(R"({"op":"capture.start","path":"x.caf","maxSeconds":1})") == "MIC_PERMISSION");
}

TEST_CASE("capture.analyze + createFromRecording: nốt gốc, trim, 13 zone, cache, gán track, envelope / mode", "[core][capture][instrument]") {
    Rig r;
    r.hz = 0.0;
    REQUIRE(r.ok(R"({"op":"capture.start","path":"instruments/v/source.caf","maxSeconds":3})"));
    r.render(9600);    // 0.2 s im lặng
    r.hz = 220.0;      // A3
    r.render(48000);   // 1 s
    r.hz = 0.0;
    r.render(9600);
    REQUIRE(r.ok(R"({"op":"capture.stop"})"));

    const auto a = r.waitJob(r.call(R"({"op":"capture.analyze","file":"instruments/v/source.caf"})"));
    REQUIRE(a["status"].toString() == "done");
    const auto& ar = a["result"];
    REQUIRE((int) ar["rootNote"] == 57);
    REQUIRE((double) ar["confidence"] > 0.8);
    REQUIRE(std::fabs((double) ar["cents"]) < 5.0);
    REQUIRE((int) ar["trimStartSample"] > 9000);   // bỏ khoảng lặng đầu (giữ 5 ms pre-roll)
    REQUIRE((int) ar["trimStartSample"] < 10000);
    REQUIRE((int) ar["trimEndSample"] > 57000);
    REQUIRE((int) ar["trimEndSample"] < 58500);
    REQUIRE(ar["peaks"].size() == 1024);

    // Gán track TRƯỚC khi job xong (đăng ký id ngay) + envelope gửi trước
    const auto job = r.call(R"({"op":"instrument.createFromRecording","instrumentId":"v","file":"instruments/v/source.caf","mode":"natural"})");
    REQUIRE((bool) job["ok"]);
    REQUIRE(r.ok(R"({"op":"instrument.setEnvelope","instrumentId":"v","a":0.01,"d":0.1,"s":0.5,"r":0.2})"));
    REQUIRE((bool) r.waitJob(r.call(R"({"op":"track.setInstrument","track":1,"instrument":{"kind":"user","id":"v"}})"))["status"].toString().isNotEmpty());
    const auto c = r.waitJob(job);
    REQUIRE(c["status"].toString() == "done");
    REQUIRE((int) c["result"]["rootNote"] == 57);
    REQUIRE((int) c["result"]["zones"] == 13);
    REQUIRE_FALSE((bool) c["result"]["cached"]);
    REQUIRE(juce::File(r.path("instruments/v/zones/meta.json")).existsAsFile());
    r.render(256);
    const auto* inst = r.e->rt().currentSnapshot()->tracks[1].instrument.get();
    REQUIRE(inst != nullptr);
    REQUIRE(inst->zones.size() == 13);
    REQUIRE(inst->zones[0].env.sustain == 0.5f);   // envelope gửi trước khi render xong vẫn được áp

    // Phát nốt → có tiếng
    r.hz = 0.0;
    r.send(LE_CMD_NOTE_ON, 1, -1, 57, 0.8f);
    r.render(4800);
    REQUIRE(r.peak > 0.01f);
    r.send(LE_CMD_NOTE_OFF, 1, -1, 57);
    r.render(24000);

    REQUIRE(r.ok(R"({"op":"instrument.setMode","instrumentId":"v","mode":"classic"})"));
    r.render(256);
    REQUIRE(r.e->rt().currentSnapshot()->tracks[1].instrument->mode == le::dsp::Instrument::Mode::Classic);
    REQUIRE(r.err(R"({"op":"instrument.setMode","instrumentId":"v","mode":"x"})") == "INVALID_ARG");
    REQUIRE(r.err(R"({"op":"instrument.setMode","instrumentId":"none","mode":"classic"})") == "INVALID_ARG");
    REQUIRE(r.err(R"({"op":"instrument.setEnvelope","instrumentId":"v","a":0,"d":0,"s":2,"r":0})") == "INVALID_ARG");

    // P3-05: tạo lại (mở lại project) → dùng zone đã cache; xoá cache → render lại
    const auto c2 = r.waitJob(r.call(R"({"op":"instrument.createFromRecording","instrumentId":"v","file":"instruments/v/source.caf"})"));
    REQUIRE((bool) c2["result"]["cached"]);
    REQUIRE((int) c2["result"]["zones"] == 13);
    juce::File(r.path("instruments/v/zones")).deleteRecursively();
    const auto c3 = r.waitJob(r.call(R"({"op":"instrument.createFromRecording","instrumentId":"v","file":"instruments/v/source.caf"})"));
    REQUIRE_FALSE((bool) c3["result"]["cached"]);
}

TEST_CASE("createFromRecording: im lặng → INVALID_ARG; nhiễu không có rootNote → PITCH_NOT_DETECTED; có rootNote thì được",
          "[core][capture][instrument]") {
    Rig r;
    r.hz = 0.0;
    REQUIRE(r.ok(R"({"op":"capture.start","path":"s.caf","maxSeconds":1})"));
    r.render(24000);
    REQUIRE(r.ok(R"({"op":"capture.stop"})"));
    auto j = r.waitJob(r.call(R"({"op":"instrument.createFromRecording","instrumentId":"s","file":"s.caf"})"));
    REQUIRE(j["status"].toString() == "failed");
    REQUIRE(j["error"]["code"].toString() == "INVALID_ARG");

    // Nhiễu trắng (LCG cố định)
    le::dsp::AudioData noise(1, 48000, 48000.0);
    std::uint32_t seed = 12345;
    for (std::int64_t i = 0; i < 48000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        noise.writePointer(0)[i] = 0.3f * ((float) (seed >> 8) / 8388608.0f - 1.0f);
    }
    REQUIRE(le::io::writeCafFloat32(r.path("n.caf"), noise, nullptr));
    j = r.waitJob(r.call(R"({"op":"instrument.createFromRecording","instrumentId":"n","file":"n.caf"})"));
    REQUIRE(j["status"].toString() == "failed");
    REQUIRE(j["error"]["code"].toString() == "PITCH_NOT_DETECTED");
    j = r.waitJob(r.call(R"({"op":"instrument.createFromRecording","instrumentId":"n","file":"n.caf","rootNote":60})"));
    REQUIRE(j["status"].toString() == "done");
    REQUIRE((int) j["result"]["rootNote"] == 60);
    REQUIRE(r.err(R"({"op":"track.setInstrument","track":0,"instrument":{"kind":"user","id":"khong-co"}})") == "INVALID_ARG");
}

TEST_CASE("P1-30: lượt overdub MIDI kết thúc (tắt / TRANSPORT_STOP) → RECORDING_FINISHED(track, slot, số nốt), nốt đang giữ được đóng",
          "[core][midi][overdub]") {
    Rig r;
    r.hz = 0.0;
    REQUIRE(r.ok(R"({"op":"clip.setMidi","track":2,"slot":1,"clipId":"m","lengthBeats":4,"notes":[{"p":48,"v":100,"s":0,"d":0.5}]})"));
    r.send(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_NONE);
    r.send(LE_CMD_CLIP_LAUNCH, 2, 1);
    r.render(2400);
    r.send(LE_CMD_OVERDUB_TOGGLE, 2);
    r.render(2400);
    r.send(LE_CMD_NOTE_ON, 2, -1, 64, 0.5f);
    r.render(6000);
    r.send(LE_CMD_NOTE_OFF, 2, -1, 64);
    r.render(2400);
    REQUIRE(r.count(LE_EVT_RECORDING_FINISHED) == 0);   // đang overdub: chưa báo
    r.send(LE_CMD_OVERDUB_TOGGLE, 2);   // tắt
    r.render(1024);
    REQUIRE(r.count(LE_EVT_RECORDING_FINISHED) == 1);
    REQUIRE(evs().back().a == 2);
    REQUIRE(evs().back().b == 1);
    REQUIRE(evs().back().value == 1.0);

    // Lượt 2: giữ phím lúc TRANSPORT_STOP → nốt được đóng tại điểm dừng
    r.send(LE_CMD_OVERDUB_TOGGLE, 2);
    r.render(2400);
    r.send(LE_CMD_NOTE_ON, 2, -1, 67, 0.5f);
    r.render(4800);
    r.send(LE_CMD_TRANSPORT_STOP);
    r.render(1024);
    REQUIRE(r.count(LE_EVT_RECORDING_FINISHED) == 2);
    REQUIRE(evs().back().value == 1.0);
    const auto notes = r.call(R"({"op":"clip.getMidi","track":2,"slot":1})")["result"]["notes"];
    REQUIRE(notes.size() == 3);
}
