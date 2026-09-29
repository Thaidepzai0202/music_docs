// P1-39: LE_CMD_LOOP_BUTTON (07 §3.1b) ở chế độ fixed và pedal (04 §2.5) · transport.setTempoMode / tempoState ·
// về "chưa có tempo" khi transport dừng và hết clip · target MIDI {kind:"loopButton"}.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {

std::vector<double>& tempoEvents() {
    static std::vector<double> v;
    return v;
}
void onEvent(int32_t type, int32_t, int32_t, int64_t, double value) {
    if (type == LE_EVT_TEMPO_CHANGED) tempoEvents().push_back(value);
}

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    std::int64_t t = 0;
    std::vector<float> in = std::vector<float>(128), L = std::vector<float>(128), R = std::vector<float>(128);
    Rig() {
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.numInputChannels = 1;
        cfg.preferredBufferSize = 128;
        cfg.preferredSampleRate = 48000.0;
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(cfg, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
    }
    juce::var call(const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e->call(j.c_str()).c_str()), v);
        return v;
    }
    bool send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        return e->send(c);
    }
    void loop(int track = 0, int slot = -1) { REQUIRE(send(LE_CMD_LOOP_BUTTON, track, slot)); }
    void render(int frames) {
        for (int done = 0; done < frames; done += 128) {
            for (int i = 0; i < 128; ++i) in[(size_t) i] = 0.3f * (float) std::sin(2.0 * 3.14159265 * 220.0 * (double) (t + i) / 48000.0);
            const float* ins[1] = {in.data()};
            float* outs[2] = {L.data(), R.data()};
            dev->render(ins, 1, outs, 2, 128);
            t += 128;
            e->pump();
        }
    }
    LeState state() const {
        LeState s{};
        globalStatePublisher().read(s);
        return s;
    }
    int cell(int tr, int s) const { return state().clipState[tr][s]; }
    juce::var tempoState() { return call(R"({"op":"engine.info"})")["result"]["tempoState"]; }
};

} // namespace

TEST_CASE("LOOP_BUTTON (fixed): trống → thu → chốt → overdub → phát; dừng → launch; slot cụ thể", "[core][loop]") {
    Rig r;
    REQUIRE(r.tempoState()["mode"].toString() == "fixed");
    REQUIRE((bool) r.tempoState()["hasTempo"]);
    REQUIRE_FALSE(r.send(LE_CMD_LOOP_BUTTON, 8, -1));   // track sai
    REQUIRE_FALSE(r.send(LE_CMD_LOOP_BUTTON, 0, 9));

    r.loop();   // ô trống đầu tiên (slot 0) → thu tự do
    r.render(256);
    REQUIRE(r.cell(0, 0) == LE_CLIP_RECORDING);
    r.render(48000);
    r.loop();   // chốt → take tự do làm tròn lên 1 bar
    r.render(96000);
    REQUIRE(r.cell(0, 0) == LE_CLIP_PLAYING);
    r.render(2048);   // take đã vào model (vé overdub cần clip trong model)
    r.loop();   // overdub bật
    r.render(256);
    REQUIRE(r.cell(0, 0) == LE_CLIP_OVERDUBBING);
    r.loop();   // overdub tắt
    r.render(4096);
    REQUIRE(r.cell(0, 0) == LE_CLIP_PLAYING);
    REQUIRE(r.send(LE_CMD_CLIP_STOP, 0));
    r.render(96000 + 256);
    REQUIRE(r.cell(0, 0) == LE_CLIP_STOPPED);
    r.loop(0, 0);   // slot cụ thể: stopped → launch
    r.render(96000 + 256);
    REQUIRE(r.cell(0, 0) == LE_CLIP_PLAYING);
    r.loop(0, 0);   // đang phát → overdub
    r.render(256);
    REQUIRE(r.cell(0, 0) == LE_CLIP_OVERDUBBING);
    r.loop(0, 0);
    r.render(4096);
    REQUIRE(r.cell(0, 0) == LE_CLIP_PLAYING);

    r.loop(0, 3);   // slot cụ thể: ô trống 3 → thu (ô 0 đang phát → xếp dừng)
    r.render(96000 + 256);
    REQUIRE(r.cell(0, 3) == LE_CLIP_RECORDING);
    REQUIRE(r.send(LE_CMD_TRANSPORT_STOP));   // Recording → bỏ take
    r.render(256);
    r.loop();   // slot −1, không ô nào chạy → ô trống ĐẦU TIÊN (1), không phải ô Stopped 0 (07 §3.1b)
    r.render(256);
    REQUIRE(r.cell(0, 1) == LE_CLIP_RECORDING);
    REQUIRE(r.cell(0, 0) == LE_CLIP_STOPPED);
}

TEST_CASE("LOOP_BUTTON (pedal): vòng đầu thu ngay, chốt suy ra tempo; về chưa có tempo khi dừng và hết clip", "[core][loop][pedal]") {
    Rig r;
    REQUIRE(r.call(R"({"op":"transport.setTempoMode","mode":"x"})")["error"]["code"].toString() == "INVALID_ARG");
    const auto set = r.call(R"({"op":"transport.setTempoMode","mode":"firstLoop"})");
    REQUIRE_FALSE((bool) set["result"]["hasTempo"]);
    REQUIRE(r.tempoState()["mode"].toString() == "firstLoop");

    r.loop();   // vòng đầu: thu NGAY, với UI transport chưa chạy
    r.render(128);
    REQUIRE(r.cell(0, 0) == LE_CLIP_RECORDING);
    REQUIRE(r.state().playing == 0);
    r.render(96000 - 128);
    r.loop();   // chốt sau đúng 2.0 s → 4 beat @120
    r.render(128);
    REQUIRE(r.cell(0, 0) == LE_CLIP_PLAYING);
    REQUIRE(r.state().playing == 1);
    REQUIRE(std::fabs(r.state().bpm - 120.0) < 1e-9);
    REQUIRE((bool) r.tempoState()["hasTempo"]);
    r.render(4096);
    REQUIRE((double) r.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["lengthBeats"] == 4.0);
    REQUIRE((double) r.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["originalBpm"] == r.state().bpm);   // BPM lúc chốt
    // Đổi BPM tay sau khi có tempo → đi đường warp bình thường, KHÔNG về "chưa có tempo" (04 §2.5)
    LeCommand bpm{};
    bpm.type = LE_CMD_SET_BPM;
    bpm.d0 = 100.0;
    REQUIRE(r.e->send(bpm));
    r.render(256);
    REQUIRE(r.state().bpm == 100.0);
    REQUIRE((bool) r.tempoState()["hasTempo"]);

    // Dừng transport nhưng còn clip → vẫn có tempo
    REQUIRE(r.send(LE_CMD_TRANSPORT_STOP));
    r.render(256);
    REQUIRE((bool) r.tempoState()["hasTempo"]);
    // Xoá hết clip → chưa có tempo (+ TEMPO_CHANGED(0)) → vòng đầu mới lại suy ra tempo
    tempoEvents().clear();
    setEventCallback(&onEvent);
    REQUIRE((bool) r.call(R"({"op":"clip.clear","track":0,"slot":0})")["ok"]);
    r.render(256);
    REQUIRE_FALSE((bool) r.tempoState()["hasTempo"]);
    REQUIRE(tempoEvents().size() == 1);
    REQUIRE(tempoEvents().back() == 0.0);
    setEventCallback(nullptr);
    r.loop(1);
    const std::int64_t t0 = r.t;   // block đầu thu = sample đầu của take
    r.render(148800);   // ≈ 3.1 s (render theo khối 128 → 148864)
    const double T = (double) (r.t - t0);
    r.loop(1);
    r.render(128);
    REQUIRE(std::fabs(r.state().bpm - 60.0 * 8 * 48000.0 / T) < 1e-6);   // nb = 8 (4 beat → < 80 BPM)
}

TEST_CASE("MIDI learn target loopButton → LOOP_BUTTON cho track đang chọn; project.open giữ firstLoop theo model", "[core][loop][midi]") {
    Rig r;
    REQUIRE((bool) r.call(R"({"op":"midi.setMappings","mappings":[{"src":{"device":"","kind":"note","channel":-1,"number":60},"target":{"kind":"loopButton"}}]})")["ok"]);
    REQUIRE(r.send(LE_CMD_SELECT_TRACK, 2));
    r.render(128);
    const std::uint8_t on[3] = {0x90, 60, 100};
    REQUIRE(r.e->injectMidi(on, 3));
    r.render(256);
    REQUIRE(r.cell(2, 0) == LE_CLIP_RECORDING);
    const std::uint8_t off[3] = {0x80, 60, 0};   // nhả: không làm gì
    REQUIRE(r.e->injectMidi(off, 3));
    r.render(256);
    REQUIRE(r.cell(2, 0) == LE_CLIP_RECORDING);
}

TEST_CASE("MIDI learn trackStop / undoOverdub: áp lên track đang chọn; không có lớp undo thì bỏ qua, không xoá clip",
          "[core][loop][midi]") {
    Rig r;
    REQUIRE((bool) r.call(R"({"op":"midi.setMappings","mappings":[
        {"src":{"device":"","kind":"note","channel":-1,"number":62},"target":{"kind":"trackStop"}},
        {"src":{"device":"","kind":"note","channel":-1,"number":63},"target":{"kind":"undoOverdub"}}]})")["ok"]);
    REQUIRE(r.send(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_NONE));
    REQUIRE(r.send(LE_CMD_SELECT_TRACK, 1));
    auto press = [&](std::uint8_t note) {
        const std::uint8_t on[3] = {0x90, note, 100};
        REQUIRE(r.e->injectMidi(on, 3));
        r.render(256);
    };
    r.loop(1);   // thu
    r.render(48000);
    r.loop(1);   // chốt: take tự do tối thiểu 1 bar (04 §5.2) → kết thúc ở beat 4 = 96000
    r.render(48000 + 4096);
    REQUIRE(r.cell(1, 0) == LE_CLIP_PLAYING);
    const auto* before = r.e->model().clips[1][0]->audio.get();
    press(63);   // chưa overdub: không có lớp undo → bỏ qua, clip còn nguyên
    REQUIRE(r.e->model().clips[1][0].has_value());
    REQUIRE(r.e->model().clips[1][0]->audio.get() == before);

    r.loop(1);   // overdub bật
    r.render(24000);
    r.loop(1);   // tắt
    r.render(8192);
    REQUIRE((bool) r.call(R"({"op":"clip.info","track":1,"slot":0})")["result"]["hasUndo"]);
    REQUIRE(r.e->model().clips[1][0]->audio.get() != before);
    press(63);   // hoàn tác → đúng bản trước overdub
    REQUIRE(r.e->model().clips[1][0]->audio.get() == before);
    REQUIRE_FALSE((bool) r.call(R"({"op":"clip.info","track":1,"slot":0})")["result"]["hasUndo"]);
    press(63);   // hết lớp undo → bỏ qua
    REQUIRE(r.e->model().clips[1][0].has_value());

    press(62);   // dừng track đang chọn
    REQUIRE(r.cell(1, 0) == LE_CLIP_STOPPED);
}

TEST_CASE("setTempoMode firstLoopBeats: giữ độ dài vòng đầu qua mở lại project → vòng sau làm tròn theo bội số đó",
          "[core][loop][pedal]") {
    Rig r;
    REQUIRE(r.call(R"({"op":"transport.setTempoMode","mode":"firstLoop","firstLoopBeats":-1})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(r.call(R"({"op":"transport.setTempoMode","mode":"firstLoop","firstLoopBeats":"x"})")["error"]["code"].toString() == "INVALID_ARG");
    // Như replay của project đã có vòng đầu 6 beat + một clip
    const auto res = r.call(R"({"op":"transport.setTempoMode","mode":"firstLoop","firstLoopBeats":6})")["result"];
    REQUIRE((double) res["firstLoopBeats"] == 6.0);
    REQUIRE((bool) r.call(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"m","lengthBeats":6,"notes":[]})")["ok"]);
    REQUIRE((bool) r.tempoState()["hasTempo"]);
    REQUIRE((double) r.tempoState()["firstLoopBeats"] == 6.0);
    r.loop(1);   // thu tự do (transport dừng → play tại beat 0, thu ngay)
    r.render(128);
    REQUIRE(r.cell(1, 0) == LE_CLIP_RECORDING);
    r.render(187136);   // ≈ 7.8 beat @120
    r.loop(1);   // → làm tròn lên bội số 6 = 12 beat
    r.render(24000 * 5);
    REQUIRE((double) r.call(R"({"op":"clip.info","track":1,"slot":0})")["result"]["lengthBeats"] == 12.0);
}

TEST_CASE("Chốt vòng đầu → engine.info.tempoState.firstLoopBeats", "[core][loop][pedal]") {
    Rig r;
    REQUIRE((double) r.call(R"({"op":"transport.setTempoMode","mode":"firstLoop"})")["result"]["firstLoopBeats"] == 0.0);
    r.loop();
    r.render(128);
    r.render(96000 - 128);
    r.loop();
    r.render(256);
    REQUIRE((double) r.tempoState()["firstLoopBeats"] == 4.0);
}
