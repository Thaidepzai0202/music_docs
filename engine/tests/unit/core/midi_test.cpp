// P4-01/02 MIDI input → track đang chọn (đúng sample theo host time) · P4-04 learn / setMappings (wildcard) ·
// mapping đổi mixer / FX → model khớp · project.open xoá mapping. Nguồn 0 ("virtual") = cùng đường với thiết bị thật.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"
#include "midi/LaunchpadMap.h"

using namespace le::core;

namespace {

struct Ev {
    int type, a, b;
};
std::vector<Ev>& evs() {
    static std::vector<Ev> v;
    return v;
}
void onEvent(int32_t type, int32_t a, int32_t b, int64_t, double) { evs().push_back({type, a, b}); }

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    juce::File dir;
    std::vector<float> L = std::vector<float>(128), R = std::vector<float>(128);
    Rig() {
        evs().clear();
        setEventCallback(&onEvent);
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-midi", "", false);
        dir.getChildFile("kit").createDirectory();
        juce::File(juce::String(LE_TEST_FIXTURES_DIR) + "/clip_sine_4beats_120.wav").copyFileTo(dir.getChildFile("kit/tone.wav"));
        dir.getChildFile("kit/tone.sfz").replaceWithText("<region> sample=tone.wav lokey=0 hikey=127 pitch_keycenter=60\n");
        lib = dir.getFullPathName().toStdString();
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.preferredBufferSize = 128;
        cfg.preferredSampleRate = 48000.0;
        cfg.libraryDir = lib.c_str();
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(cfg, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
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
    void waitJob(const juce::var& reply) {
        REQUIRE((bool) reply["ok"]);
        const std::string q = R"({"op":"job.result","jobId":)" + reply["result"]["jobId"].toString().toStdString() + "}";
        for (int i = 0; i < 2000 && call(q)["result"]["status"].toString() == "running"; ++i) {
            e->pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        REQUIRE(call(q)["result"]["status"].toString() == "done");
    }
    void send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        REQUIRE(e->send(c));
    }
    void midi(std::uint8_t s, std::uint8_t d1, std::uint8_t d2, std::int64_t hostNs = 0) {
        const std::uint8_t b[3] = {s, d1, d2};
        REQUIRE(e->injectMidi(b, 3, hostNs));
    }
    void block() {
        float* outs[2] = {L.data(), R.data()};
        dev->render(nullptr, 0, outs, 2, 128);
        e->pump();
    }
    int firstNonZero() const {
        for (int i = 0; i < 128; ++i)
            if (L[(size_t) i] != 0.0f) return i;
        return -1;
    }
    LeState state() const {
        LeState s{};
        globalStatePublisher().read(s);
        return s;
    }
    std::string lib;
};

} // namespace

TEST_CASE("P4-02: nốt MIDI tới track đang chọn, rơi ĐÚNG sample theo host time của block", "[core][midi]") {
    Rig r;
    r.waitJob(r.call(R"({"op":"track.setInstrument","track":3,"instrument":{"kind":"sfz","path":"kit/tone.sfz"}})"));
    r.send(LE_CMD_SELECT_TRACK, 3);
    r.block();
    r.block();
    REQUIRE(r.firstNonZero() == -1);
    const std::uint64_t t0 = r.dev->nextHostTimeNs();
    r.midi(0x90, 60, 100, (std::int64_t) t0 + (std::int64_t) std::llround(37 * 1e9 / 48000.0));   // offset 37
    r.block();
    const int first = r.firstNonZero();
    REQUIRE(first >= 37);
    REQUIRE(first <= 38);   // sample đầu của attack có thể = 0
    for (int i = 0; i < 37; ++i) REQUIRE(r.L[(size_t) i] == 0.0f);
    r.midi(0x80, 60, 0);   // note-off (host time 0 → đầu block kế)
    for (int i = 0; i < 40; ++i) r.block();
    REQUIRE(r.firstNonZero() == -1);   // đã release hết

    // Không phải track đang chọn → im
    r.send(LE_CMD_SELECT_TRACK, 0);
    r.block();
    r.midi(0x90, 64, 100);
    r.block();
    r.block();
    REQUIRE(r.firstNonZero() == -1);
}

TEST_CASE("P4-04: learn → mapping, learnResult, learnCancel; message lúc learn không phát tiếng", "[core][midi]") {
    Rig r;
    REQUIRE(r.ok(R"({"op":"clip.setMidi","track":0,"slot":2,"clipId":"m","lengthBeats":4,"notes":[]})"));
    r.send(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_NONE);
    REQUIRE(r.call(R"({"op":"midi.learnResult"})")["error"]["code"].toString() == "INVALID_ARG");   // chưa learn
    REQUIRE(r.call(R"({"op":"midi.learnStart","target":{"kind":"clip","track":9,"slot":0}})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(r.ok(R"({"op":"midi.learnStart","target":{"kind":"clip","track":0,"slot":2}})"));
    r.midi(0xB3, 20, 100);   // CC 20 kênh 4
    r.block();
    REQUIRE(evs().size() >= 1);
    REQUIRE(evs().back().type == LE_EVT_MIDI_LEARNED);
    REQUIRE(evs().back().a == 1);    // cc
    REQUIRE(evs().back().b == 20);
    const auto lr = r.call(R"({"op":"midi.learnResult"})")["result"];
    REQUIRE(lr["kind"].toString() == "cc");
    REQUIRE((int) lr["channel"] == 3);
    REQUIRE((int) lr["number"] == 20);
    REQUIRE(lr["deviceName"].toString() == "virtual");
    REQUIRE(r.state().clipState[0][2] == LE_CLIP_STOPPED);   // lúc learn: không kích hoạt

    r.midi(0xB3, 20, 127);   // nhấn → launch (0, 2)
    r.block();
    r.block();
    REQUIRE(r.state().clipState[0][2] == LE_CLIP_PLAYING);

    auto learned = [] {
        int n = 0;
        for (const auto& x : evs()) n += x.type == LE_EVT_MIDI_LEARNED ? 1 : 0;
        return n;
    };
    const int before = learned();
    REQUIRE(r.ok(R"({"op":"midi.learnStart","target":{"kind":"stopAll"}})"));
    REQUIRE(r.ok(R"({"op":"midi.learnCancel"})"));
    r.midi(0x90, 50, 90);
    r.block();
    REQUIRE(learned() == before);   // đã huỷ → không learn
    REQUIRE(r.state().clipState[0][2] == LE_CLIP_PLAYING);   // và không stopAll
}

TEST_CASE("P4-04: setMappings (device \"\" / channel −1 = mọi), transport / gain / FX qua CC → model khớp; project.open xoá",
          "[core][midi]") {
    Rig r;
    REQUIRE(r.ok(R"({"op":"fx.set","track":1,"index":0,"type":"filter"})"));
    REQUIRE(r.ok(R"({"op":"midi.setMappings","mappings":[
        {"src":{"device":"","kind":"note","channel":-1,"number":36},"target":{"kind":"transport","action":"toggle"}},
        {"src":{"device":"","kind":"cc","channel":0,"number":7},"target":{"kind":"trackGain","track":2,"minDb":-40,"maxDb":0}},
        {"src":{"device":"virtual","kind":"cc","channel":-1,"number":74},"target":{"kind":"fx","track":1,"slot":0,"param":1}},
        {"src":{"device":"Launchpad X","kind":"note","channel":0,"number":11},"target":{"kind":"stopAll"}}]})"));
    r.midi(0x99, 36, 100);   // kênh 10
    r.block();
    REQUIRE(r.state().playing == 1);
    r.midi(0x89, 36, 0);     // nhả: không làm gì
    r.midi(0x90, 36, 100);   // kênh 1: cũng khớp (channel −1)
    r.block();
    REQUIRE(r.state().playing == 0);

    r.midi(0xB0, 7, 127);
    r.block();
    REQUIRE(r.e->rt().mixer().gainDb(2) == 0.0f);
    REQUIRE(r.e->model().mixer[2].gainDb == 0.0f);   // MappedChange → model (export / lưu project)
    r.midi(0xB0, 7, 0);
    r.block();
    REQUIRE(r.e->rt().mixer().gainDb(2) == -40.0f);
    REQUIRE(r.e->model().mixer[2].gainDb == -40.0f);
    r.midi(0xB1, 7, 127);    // kênh 2: mapping chỉ kênh 1 → không khớp
    r.block();
    REQUIRE(r.e->model().mixer[2].gainDb == -40.0f);

    r.midi(0xB5, 74, 127);   // dải mặc định = ParamInfo của filter cutoff (20..20000)
    r.block();
    REQUIRE(r.e->model().tracks[1].fx[0]->params[1] == 20000.0f);

    REQUIRE(r.call(R"({"op":"midi.setMappings","mappings":[{"src":{"device":"","kind":"pc","channel":0,"number":1},"target":{"kind":"stopAll"}}]})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(r.call(R"({"op":"midi.setMappings","mappings":[{"src":{"device":"","kind":"cc","channel":16,"number":1},"target":{"kind":"stopAll"}}]})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(r.call(R"({"op":"midi.setMappings","mappings":[{"src":{"device":"","kind":"cc","channel":0,"number":1},"target":{"kind":"fx","track":-1,"slot":1,"param":2}}]})")["error"]["code"].toString() == "INVALID_ARG");

    const std::string dir = r.dir.getChildFile("proj").getFullPathName().toStdString();
    REQUIRE(r.ok(R"({"op":"project.open","dir":")" + dir + R"("})"));   // mapping là state project
    r.block();
    r.midi(0x90, 36, 100);
    r.block();
    REQUIRE(r.state().playing == 0);
}

TEST_CASE("P4-01: midi.listDevices / enableDevice; sim.midiIn", "[core][midi]") {
    Rig r;
    const auto l = r.call(R"({"op":"midi.listDevices"})")["result"];
    REQUIRE(l["inputs"].isArray());
    REQUIRE(l["outputs"].isArray());
    REQUIRE(r.ok(R"({"op":"midi.enableDevice","id":"khong-co-thiet-bi","enabled":true})"));   // nhớ, cắm vào tự mở
    REQUIRE(r.call(R"({"op":"midi.enableDevice","id":"x"})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(r.ok(R"({"op":"midi.setMappings","mappings":[{"src":{"device":"","kind":"note","channel":-1,"number":40},"target":{"kind":"transport","action":"play"}}]})"));
    REQUIRE(r.ok(R"({"op":"sim.midiIn","bytes":[144,40,100]})"));
    r.block();
    REQUIRE(r.state().playing == 1);
    REQUIRE(r.call(R"({"op":"sim.midiIn","bytes":[240,1,2]})")["error"]["code"].toString() == "INVALID_ARG");   // SysEx: bị lọc
    REQUIRE(r.call(R"({"op":"sim.midiIn","bytes":[300]})")["error"]["code"].toString() == "INVALID_ARG");
}

TEST_CASE("P1-39: sửa clip MIDI đang phát (clip.setMidi) giữ pha — nốt bị xoá tắt ngay, nốt đã qua kêu từ vòng sau, "
          "không treo / không trùng, mọi block size", "[core][midi][edit]") {
    for (const int block : {128, 1000}) {
        Rig r;
        r.dir.getChildFile("kit/hold.sfz").replaceWithText("<region> sample=tone.wav lokey=0 hikey=127 pitch_keycenter=60 ampeg_release=0.001\n");
        r.waitJob(r.call(R"({"op":"track.setInstrument","track":0,"instrument":{"kind":"sfz","path":"kit/hold.sfz"}})"));
        REQUIRE(r.ok(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"m","lengthBeats":4,
                         "notes":[{"p":60,"v":100,"s":0,"d":3},{"p":64,"v":100,"s":1,"d":2}]})"));
        r.send(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_NONE);
        r.send(LE_CMD_CLIP_LAUNCH, 0, 0);
        std::vector<float> L((size_t) block), R((size_t) block);
        auto render = [&](int frames) {
            for (int done = 0; done < frames; done += block) {
                float* outs[2] = {L.data(), R.data()};
                r.dev->render(nullptr, 0, outs, 2, std::min(block, frames - done));
                r.e->pump();
            }
        };
        auto voices = [&] { return r.e->rt().track(0).activeVoices(); };
        render(36000);   // beat 1.5: 60 + 64 đang kêu
        REQUIRE(voices() == 2);
        // Piano roll: xoá 64, thêm 67 ở beat 0.5 (đã qua trong vòng này)
        REQUIRE(r.ok(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"m","lengthBeats":4,
                         "notes":[{"p":60,"v":100,"s":0,"d":3},{"p":67,"v":100,"s":0.5,"d":3}]})"));
        render(2400);    // +50 ms: 64 đã tắt (release 1 ms), 67 CHƯA kêu (vòng sau)
        REQUIRE(voices() == 1);
        render(96000 - 38400 + 14400);   // qua điểm loop tới beat 4.6: 60 (beat 0) + 67 (beat 0.5) kêu lại
        REQUIRE(voices() == 2);
        render(96000);   // thêm một vòng: không nốt treo / trùng
        REQUIRE(voices() == 2);
        r.send(LE_CMD_CLIP_STOP, 0);
        render(96000 + 4800);
        REQUIRE(voices() == 0);
    }
}

TEST_CASE("track.configure color \"#RRGGBB\" (Launchpad dùng palette gần nhất)", "[core][midi]") {
    Rig r;
    REQUIRE(r.ok(R"({"op":"track.configure","track":2,"kind":"audio","name":"Trống","color":"#FF0000"})"));
    REQUIRE(r.e->model().tracks[2].hasColor);
    REQUIRE(r.e->model().tracks[2].color == 0xFF0000u);
    REQUIRE(le::midi::nearestPaletteColor(r.e->model().tracks[2].color) == 5);
    REQUIRE(r.ok(R"({"op":"track.configure","track":3,"kind":"audio","color":"#1e88e5"})"));
    REQUIRE(r.e->model().tracks[3].color == 0x1E88E5u);
    for (const char* bad : {"red", "#FF00", "FF0000", "#GG0000", "#FF00001"})
        REQUIRE(r.call(std::string(R"({"op":"track.configure","track":2,"kind":"audio","color":")") + bad + R"("})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE_FALSE(r.e->model().tracks[4].hasColor);   // chưa đặt → bảng mặc định theo chỉ số track
    r.block();   // pump tính màu LED (không có Launchpad thật: không gửi gì)
}
