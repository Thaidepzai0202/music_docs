// sim.offline / sim.advance (05 §3): chạy engine thật qua C API không cần thiết bị — đúng cách 77 dùng qua dylib.
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <vector>
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
bool send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0) {
    LeCommand c{};
    c.type = type;
    c.track = (int8_t) track;
    c.slot = (int8_t) slot;
    c.i0 = i0;
    return le_send(&c);
}
struct Scope {
    Scope() {
        LeConfig c{};
        c.apiVersion = LE_API_VERSION;
        c.preferredBufferSize = 128;
        c.preferredSampleRate = 48000.0;
        REQUIRE(le_create(&c) == LE_OK);
    }
    ~Scope() { le_destroy(); }
};
} // namespace

TEST_CASE("sim.advance cần sim.offline trước; tham số sai → INVALID_ARG", "[sim][api]") {
    Scope s;
    REQUIRE(call(R"({"op":"sim.advance","frames":128})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(call(R"({"op":"sim.offline","blockSize":0})")["error"]["code"].toString() == "INVALID_ARG");
    auto v = call(R"({"op":"sim.offline","enabled":true,"sampleRate":48000,"blockSize":64})");
    REQUIRE((bool) v["ok"]);
    REQUIRE((int) v["result"]["blockSize"] == 64);
    REQUIRE((bool) call(R"({"op":"engine.info"})")["result"]["running"]);
    REQUIRE(call(R"({"op":"sim.advance"})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(call(R"({"op":"sim.advance","frames":1.5})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE((bool) call(R"({"op":"sim.offline","enabled":false})")["ok"]);
    REQUIRE_FALSE((bool) call(R"({"op":"engine.info"})")["result"]["running"]);
}

TEST_CASE("sim.offline + sim.advance: launch quantized qua C API, LeState đúng từng sample", "[sim][api]") {
    Scope s;
    REQUIRE((bool) call(R"({"op":"sim.offline","enabled":true,"sampleRate":48000,"blockSize":128})")["ok"]);
    REQUIRE((bool) call(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"c","lengthBeats":4,"notes":[]})")["ok"]);
    REQUIRE(send(LE_CMD_TRANSPORT_PLAY));
    auto a = call(R"({"op":"sim.advance","frames":31200})")["result"];   // tới beat 1.3
    REQUIRE((int) a["frames"] == 31200);
    REQUIRE((double) a["beat"] == 1.3);
    REQUIRE(send(LE_CMD_CLIP_LAUNCH, 0, 0));
    call(R"({"op":"sim.advance","frames":64800})");   // tới đúng 96000
    LeState st{};
    le_read_state(&st);
    REQUIRE(st.clipState[0][0] == LE_CLIP_QUEUED_PLAY);
    call(R"({"op":"sim.advance","frames":1})");
    le_read_state(&st);
    REQUIRE(st.clipState[0][0] == LE_CLIP_PLAYING);
    REQUIRE(st.trackPlayingSlot[0] == 0);

    a = call(R"({"op":"sim.advance","beats":2})")["result"];
    REQUIRE((int) a["frames"] == 48000);                                     // 2 beat @ 120 BPM
    REQUIRE(std::abs((double) a["beat"] - 144001.0 / 24000.0) < 1e-9);       // 96001 + 48000 frame
    const auto log = call(R"({"op":"launchLog.read","sinceIndex":0})")["result"]["events"];
    REQUIRE(log.size() == 1);
    REQUIRE((double) log[0]["beat"] == 4.0);
}

TEST_CASE("sim.offline từ chối khi audio thật đang chạy", "[sim][api]") {
    // Không mở thiết bị thật trong unit test: chỉ kiểm nhánh offline → offline (được phép đổi cấu hình).
    Scope s;
    REQUIRE((bool) call(R"({"op":"sim.offline","sampleRate":44100,"blockSize":256})")["ok"]);
    REQUIRE((bool) call(R"({"op":"sim.offline","sampleRate":48000,"blockSize":128})")["ok"]);   // đang offline: đổi được
    REQUIRE((double) call(R"({"op":"engine.info"})")["result"]["sampleRate"] == 48000.0);
}

namespace {
std::vector<std::array<int32_t, 3>>& simEvents() {
    static std::vector<std::array<int32_t, 3>> v;
    return v;
}
void onSimEvent(int32_t type, int32_t a, int32_t b, int64_t, double) { simEvents().push_back({type, a, b}); }
} // namespace

TEST_CASE("sim: luồng hợp đồng của 77 — thu 1 bar → clip.info audio + RECORDING_FINISHED(track, slot)", "[sim][api][recorder]") {
    simEvents().clear();
    le_set_event_callback(&onSimEvent);
    const juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-sim-rec", "", false);
    {
        Scope s;
        REQUIRE((bool) call(R"({"op":"project.open","dir":")" + dir.getFullPathName().toStdString() + R"("})")["ok"]);
        REQUIRE((bool) call(R"({"op":"sim.offline","enabled":true,"sampleRate":48000,"blockSize":128})")["ok"]);
        REQUIRE(send(LE_CMD_CLIP_RECORD, 1, 0, 1));   // transport dừng, chưa arm
        call(R"({"op":"sim.advance","frames":1})");
        LeState st{};
        le_read_state(&st);
        REQUIRE(st.clipState[1][0] == LE_CLIP_RECORDING);
        call(R"({"op":"sim.advance","beats":4.1})");
        le_read_state(&st);
        REQUIRE(st.clipState[1][0] == LE_CLIP_PLAYING);
        const auto info = call(R"({"op":"clip.info","track":1,"slot":0})")["result"];
        REQUIRE(info["kind"].toString() == "audio");
        REQUIRE((double) info["lengthBeats"] == 4.0);
        REQUIRE(info["file"].toString().startsWith("audio/"));

        bool finished = false;
        for (int i = 0; i < 200 && !finished; ++i) {   // job ghi file chạy trên worker; sim.advance pump event
            call(R"({"op":"sim.advance","frames":128})");
            for (const auto& e : simEvents()) finished |= e[0] == LE_EVT_RECORDING_FINISHED && e[1] == 1 && e[2] == 0;
            juce::Thread::sleep(5);
        }
        REQUIRE(finished);
        REQUIRE(dir.getChildFile(info["file"].toString()).existsAsFile());
    }
    le_set_event_callback(nullptr);
    dir.deleteRecursively();
}
