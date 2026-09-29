// P1-30: thu MIDI trên track instrument, quantize khi thu, overdub MIDI, midiClip.quantize — qua C API + sim.*.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

#include <juce_core/juce_core.h>

#include "le/engine_api.h"

namespace {
juce::var call(const std::string& j) {
    char* r = le_call(j.c_str());
    juce::var v;
    juce::JSON::parse(juce::String::fromUTF8(r), v);
    le_free_string(r);
    return v;
}
void send(uint16_t type, int track, int slot = -1, int32_t i0 = 0, float f0 = 0.0f) {
    LeCommand c{};
    c.type = type;
    c.track = (int8_t) track;
    c.slot = (int8_t) slot;
    c.i0 = i0;
    c.f0 = f0;
    REQUIRE(le_send(&c));
}
// Tới đúng beat b (tuyệt đối, 120 BPM @48k → 24000 frame/beat) rồi mới gửi lệnh: lệnh xử lý ở đầu block kế.
void advanceTo(double beat) {
    const auto st = call(R"({"op":"sim.advance","frames":0})")["result"];
    const double now = (double) st["beat"];
    const auto frames = (juce::int64) std::llround((beat - now) * 24000.0);
    if (frames > 0) call("{\"op\":\"sim.advance\",\"frames\":" + std::to_string(frames) + "}");
}
struct Scope {
    Scope() {
        LeConfig c{};
        c.apiVersion = LE_API_VERSION;
        c.preferredBufferSize = 128;
        c.preferredSampleRate = 48000.0;
        REQUIRE(le_create(&c) == LE_OK);
        REQUIRE((bool) call(R"({"op":"sim.offline","enabled":true,"sampleRate":48000,"blockSize":128})")["ok"]);
        REQUIRE((bool) call(R"({"op":"track.configure","track":2,"kind":"instrument"})")["ok"]);
    }
    ~Scope() { le_destroy(); }
};
bool near(double a, double b) { return std::fabs(a - b) <= 0.01; }
} // namespace

TEST_CASE("P1-30: thu MIDI 1 bar — nốt đúng vị trí, velocity = round(f0·127), phím còn giữ kéo tới cuối take",
          "[sim][midi][recorder]") {
    Scope s;
    send(LE_CMD_CLIP_RECORD, 2, 0, 1);   // transport dừng → play beat 0, thu 1 bar
    advanceTo(0.5);
    send(LE_CMD_NOTE_ON, 2, -1, 60, 0.8f);
    advanceTo(1.25);
    send(LE_CMD_NOTE_OFF, 2, -1, 60);
    advanceTo(3.0);
    send(LE_CMD_NOTE_ON, 2, -1, 64, 1.0f);   // giữ qua cuối take
    advanceTo(4.2);
    const auto info = call(R"({"op":"clip.info","track":2,"slot":0})")["result"];
    REQUIRE(info["kind"].toString() == "midi");
    REQUIRE((double) info["lengthBeats"] == 4.0);
    const auto notes = call(R"({"op":"clip.getMidi","track":2,"slot":0})")["result"]["notes"];
    REQUIRE(notes.size() == 2);
    REQUIRE((int) notes[0]["p"] == 60);
    REQUIRE((int) notes[0]["v"] == 102);   // round(0.8·127)
    REQUIRE(near((double) notes[0]["s"], 0.5));
    REQUIRE(near((double) notes[0]["d"], 0.75));
    REQUIRE((int) notes[1]["p"] == 64);
    REQUIRE((int) notes[1]["v"] == 127);
    REQUIRE(near((double) notes[1]["s"], 3.0));
    REQUIRE(near((double) notes[1]["d"], 1.0));   // tới cuối take
    send(LE_CMD_NOTE_OFF, 2, -1, 64);
}

TEST_CASE("P1-30: nốt trước khi take bắt đầu (count-in) không được ghi; track audio thu ra audio", "[sim][midi][recorder]") {
    Scope s;
    send(LE_CMD_SET_COUNT_IN, -1, -1, 1);
    send(LE_CMD_CLIP_RECORD, 2, 1, 1);   // count-in 1 bar → thu từ beat 4
    advanceTo(1.0);
    send(LE_CMD_NOTE_ON, 2, -1, 50, 0.5f);
    advanceTo(1.5);
    send(LE_CMD_NOTE_OFF, 2, -1, 50);
    advanceTo(5.0);
    send(LE_CMD_NOTE_ON, 2, -1, 55, 0.5f);
    advanceTo(5.5);
    send(LE_CMD_NOTE_OFF, 2, -1, 55);
    advanceTo(8.3);
    const auto notes = call(R"({"op":"clip.getMidi","track":2,"slot":1})")["result"]["notes"];
    REQUIRE(notes.size() == 1);
    REQUIRE((int) notes[0]["p"] == 55);
    REQUIRE(near((double) notes[0]["s"], 1.0));   // beat 5 − bắt đầu take (beat 4)
}

TEST_CASE("P1-30: midi.setRecordQuantize 0.25 → điểm bắt đầu về lưới, giữ độ dài", "[sim][midi][recorder]") {
    Scope s;
    REQUIRE(call(R"({"op":"midi.setRecordQuantize","grid":0.3})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE((bool) call(R"({"op":"midi.setRecordQuantize","grid":0.25})")["ok"]);
    send(LE_CMD_CLIP_RECORD, 2, 0, 1);
    advanceTo(1.1);   // → 1.0
    send(LE_CMD_NOTE_ON, 2, -1, 60, 0.5f);
    advanceTo(1.6);
    send(LE_CMD_NOTE_OFF, 2, -1, 60);
    advanceTo(3.9);   // → 4.0 = cuối vòng → quay về 0
    send(LE_CMD_NOTE_ON, 2, -1, 62, 0.5f);
    advanceTo(3.95);
    send(LE_CMD_NOTE_OFF, 2, -1, 62);
    advanceTo(4.2);
    const auto notes = call(R"({"op":"clip.getMidi","track":2,"slot":0})")["result"]["notes"];
    REQUIRE(notes.size() == 2);
    REQUIRE((int) notes[0]["p"] == 62);
    REQUIRE(near((double) notes[0]["s"], 0.0));
    REQUIRE((int) notes[1]["p"] == 60);
    REQUIRE(near((double) notes[1]["s"], 1.0));
    REQUIRE(near((double) notes[1]["d"], 0.5));
}

TEST_CASE("P1-30: overdub MIDI trộn nốt mới vào clip đang phát", "[sim][midi][recorder]") {
    Scope s;
    REQUIRE((bool) call(R"({"op":"clip.setMidi","track":2,"slot":3,"clipId":"m","lengthBeats":4,
                            "notes":[{"p":48,"v":100,"s":0,"d":0.5}]})")["ok"]);
    send(LE_CMD_CLIP_LAUNCH, 2, 3);
    advanceTo(0.1);
    send(LE_CMD_OVERDUB_TOGGLE, 2);
    advanceTo(6.0);   // vòng 2, vị trí 2.0 trong clip
    send(LE_CMD_NOTE_ON, 2, -1, 67, 0.5f);
    advanceTo(6.5);
    send(LE_CMD_NOTE_OFF, 2, -1, 67);
    advanceTo(7.8);   // nốt vắt qua điểm loop: 3.8 → 0.2
    send(LE_CMD_NOTE_ON, 2, -1, 69, 0.5f);
    advanceTo(8.2);
    send(LE_CMD_NOTE_OFF, 2, -1, 69);
    advanceTo(8.3);
    const auto notes = call(R"({"op":"clip.getMidi","track":2,"slot":3})")["result"]["notes"];
    REQUIRE(notes.size() == 3);
    REQUIRE((int) notes[0]["p"] == 48);
    REQUIRE((int) notes[1]["p"] == 67);
    REQUIRE(near((double) notes[1]["s"], 2.0));
    REQUIRE(near((double) notes[1]["d"], 0.5));
    REQUIRE((int) notes[2]["p"] == 69);
    REQUIRE(near((double) notes[2]["s"], 3.8));
    REQUIRE(near((double) notes[2]["d"], 0.4));   // vắt qua điểm loop
}

TEST_CASE("midiClip.quantize: điểm bắt đầu về lưới gần nhất, giữ độ dài", "[sim][midi]") {
    Scope s;
    REQUIRE((bool) call(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"q","lengthBeats":4,
        "notes":[{"p":60,"v":100,"s":0.13,"d":0.3},{"p":62,"v":100,"s":1.37,"d":0.2},{"p":64,"v":100,"s":3.9,"d":0.1}]})")["ok"]);
    REQUIRE(call(R"({"op":"midiClip.quantize","track":0,"slot":0,"grid":0})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE((bool) call(R"({"op":"midiClip.quantize","track":0,"slot":0,"grid":0.25})")["ok"]);
    const auto notes = call(R"({"op":"clip.getMidi","track":0,"slot":0})")["result"]["notes"];
    REQUIRE(notes.size() == 3);
    REQUIRE((int) notes[0]["p"] == 64);   // 3.9 → 4.0 → về đầu vòng 0
    REQUIRE((double) notes[0]["s"] == 0.0);
    REQUIRE((double) notes[1]["s"] == 0.25);
    REQUIRE((double) notes[1]["d"] == 0.3);
    REQUIRE((double) notes[2]["s"] == 1.25);
}
