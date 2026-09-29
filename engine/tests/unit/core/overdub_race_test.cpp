// R1 (rt-review 2026-09-29 của 80, S1): bật lại overdub trong lúc RT còn kết thúc lượt trước và main CHƯA pump.
// App thật pump 30 Hz (~12 block @128) → ở đây render nhiều block giữa hai lần pump (ScenarioRunner / sim.advance
// pump sau MỖI block nên không bao giờ lộ race). Chạy dưới ASan + TSan: không use-after-free, không data race.
// Kèm R6: không vào được overdub (Re-Pitch khác tempo) → LE_EVT_ERROR(LE_ERR_OVERDUB_UNSUPPORTED), vẫn PLAYING.
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

struct Ev {
    int type, a, b;
    double value;
};
std::vector<Ev>& evs() {
    static std::vector<Ev> v;
    return v;
}
void onEvent(int32_t type, int32_t a, int32_t b, int64_t, double value) { evs().push_back({type, a, b, value}); }

int count(int type) {
    int n = 0;
    for (const auto& e : evs()) n += e.type == type ? 1 : 0;
    return n;
}

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    std::vector<float> in = std::vector<float>(128, 0.25f), L = std::vector<float>(128), R = std::vector<float>(128);
    juce::File dir;

    explicit Rig(double originalBpm = 120.0) {
        evs().clear();
        setEventCallback(&onEvent);
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.numInputChannels = 1;
        cfg.preferredBufferSize = 128;
        cfg.preferredSampleRate = 48000.0;
        auto d = std::make_unique<le::io::OfflineDeviceIO>(128);
        dev = d.get();
        dev->setLatencies(1000, 1000);   // L = 2000 sample ≈ 16 block
        e = std::make_unique<Engine>(cfg, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-odrace", "", false);
        dir.createDirectory();
        REQUIRE(call(R"({"op":"project.open","dir":")" + dir.getFullPathName().toStdString() + R"("})"));
        REQUIRE(call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"c0","lengthBeats":4,"originalBpm":)" +
                     std::to_string((int) originalBpm) + R"(,"file":")" LE_TEST_FIXTURES_DIR R"(/clip_silence_4beats_120.wav"})"));
        for (int i = 0; i < 400 && !loaded(); ++i) wait();
        REQUIRE(loaded());
        send(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_NONE);
        send(LE_CMD_CLIP_LAUNCH, 0, 0);
        render(4);
        e->pump();
    }
    ~Rig() {
        e.reset();
        setEventCallback(nullptr);
        dir.deleteRecursively();
    }
    bool loaded() const { return e->model().clips[0][0].has_value() && e->model().clips[0][0]->audio != nullptr; }
    bool call(const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e->call(j.c_str()).c_str()), v);
        return (bool) v["ok"];
    }
    void send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0, double d0 = 0.0) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        c.d0 = d0;
        REQUIRE(e->send(c));
    }
    void render(int blocks) {   // KHÔNG pump
        const float* ins[1] = {in.data()};
        float* outs[2] = {L.data(), R.data()};
        for (int i = 0; i < blocks; ++i) dev->render(ins, 1, outs, 2, 128);
    }
    void wait() {
        e->pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    int cell() const {
        LeState s{};
        globalStatePublisher().read(s);
        return s.clipState[0][0];
    }
    std::weak_ptr<const le::dsp::AudioData> clipAudio() const { return e->model().clips[0][0]->audio; }
    static double sum(const std::weak_ptr<const le::dsp::AudioData>& w) {
        const auto a = w.lock();
        if (a == nullptr) return -1.0;
        double s = 0.0;
        for (std::int64_t i = 0; i < a->numFrames(); ++i) s += a->channel(0)[i];
        return s;
    }
};

} // namespace

TEST_CASE("R1: tắt rồi bật lại overdub trước khi main pump → lượt 2 chờ vé mới, RT không ghi vào target cũ, cả hai lượt được lưu",
          "[core][recorder][overdub][race]") {
    Rig r;
    r.send(LE_CMD_OVERDUB_TOGGLE, 0);   // BẬT lượt 1 → vé T1
    r.render(24);
    r.e->pump();
    REQUIRE(r.cell() == LE_CLIP_OVERDUBBING);
    const auto T1 = r.clipAudio();
    REQUIRE(Rig::sum(T1) > 1.0);

    r.send(LE_CMD_OVERDUB_TOGGLE, 0);   // TẮT
    r.render(2);                         // < L: RT còn ghi phần bù latency vào T1
    r.send(LE_CMD_OVERDUB_TOGGLE, 0);   // BẬT lại ngay (double-tap) — RT còn giữ vé T1 → main HOÃN
    r.render(24);                        // RT xong lượt 1 (trả vé), KHÔNG mở lượt 2 trên T1
    REQUIRE(r.cell() == LE_CLIP_PLAYING);
    const double t1Done = Rig::sum(T1);
    r.e->pump();                         // nhận vé T1 → lưu lượt 1 → bật lượt hoãn với vé T2
    r.render(24);
    r.e->pump();
    REQUIRE(r.cell() == LE_CLIP_OVERDUBBING);
    const auto T2 = r.clipAudio();
    REQUIRE(T2.lock().get() != T1.lock().get());
    const double t2a = Rig::sum(T2);
    r.render(24);
    r.e->pump();
    REQUIRE(Rig::sum(T2) > t2a + 1.0);   // lượt 2 ghi vào T2…
    REQUIRE(Rig::sum(T1) == t1Done);     // …T1 không đổi nữa

    r.send(LE_CMD_OVERDUB_TOGGLE, 0);   // TẮT lượt 2
    r.render(40);
    for (int i = 0; i < 200 && count(LE_EVT_RECORDING_FINISHED) < 2; ++i) r.wait();
    REQUIRE(count(LE_EVT_RECORDING_FINISHED) == 2);   // CẢ HAI lượt đã ghi file
    REQUIRE(r.cell() == LE_CLIP_PLAYING);
    REQUIRE(count(LE_EVT_ERROR) == 0);
    REQUIRE((bool) r.call(R"({"op":"clip.undoOverdub","track":0,"slot":0})"));   // undo về trước lượt 2
    r.render(2);
    r.e->pump();
    REQUIRE(r.clipAudio().lock().get() == T1.lock().get());
}

TEST_CASE("R1: clip.clear trong lúc RT còn ghi lượt overdub → target sống tới khi RT trả vé (ASan: không use-after-free)",
          "[core][recorder][overdub][race]") {
    Rig r;
    r.send(LE_CMD_OVERDUB_TOGGLE, 0);
    r.render(24);
    r.e->pump();
    r.send(LE_CMD_OVERDUB_TOGGLE, 0);   // TẮT: RT còn ghi phần bù L…
    r.render(2);
    const auto T = r.clipAudio();
    REQUIRE((bool) r.call(R"({"op":"clip.clear","track":0,"slot":0})"));   // …trong lúc model + snapshot thả target
    for (int k = 0; k < 20; ++k) {
        r.render(2);
        r.e->pump();   // snapshot cũ được thu hồi, RT vẫn ghi vào target (vé main đang giữ)
    }
    REQUIRE(T.expired());   // RT đã trả vé → main nhả target
    REQUIRE(r.cell() == LE_CLIP_EMPTY);

    // Bật overdub trên clip MIDI / ô trống không làm hỏng gì
    r.send(LE_CMD_OVERDUB_TOGGLE, 0);
    r.render(4);
    r.e->pump();
    REQUIRE(r.cell() == LE_CLIP_EMPTY);
}

TEST_CASE("R6: overdub clip Re-Pitch khác tempo → LE_EVT_ERROR(LE_ERR_OVERDUB_UNSUPPORTED), vẫn PLAYING; hợp tempo thì bật được",
          "[core][recorder][overdub]") {
    Rig r(100.0);   // clip 100 BPM, transport 120 → Re-Pitch
    r.send(LE_CMD_OVERDUB_TOGGLE, 0);
    r.render(4);
    r.e->pump();
    REQUIRE(r.cell() == LE_CLIP_PLAYING);
    REQUIRE(count(LE_EVT_ERROR) == 1);
    REQUIRE(evs().back().a == LE_ERR_OVERDUB_UNSUPPORTED);
    REQUIRE(evs().back().b == 0);

    r.send(LE_CMD_SET_BPM, -1, -1, 0, 100.0);   // giờ phát 1:1
    r.render(4);
    r.e->pump();
    r.send(LE_CMD_OVERDUB_TOGGLE, 0);           // main đã biết lượt trước bị trả vé → đây là BẬT
    r.render(8);
    r.e->pump();
    REQUIRE(r.cell() == LE_CLIP_OVERDUBBING);
    r.send(LE_CMD_OVERDUB_TOGGLE, 0);
    r.render(40);
    r.e->pump();
    REQUIRE(r.cell() == LE_CLIP_PLAYING);
    REQUIRE(count(LE_EVT_ERROR) == 1);
}
