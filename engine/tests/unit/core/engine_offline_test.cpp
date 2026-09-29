// Engine + OfflineDeviceIO: launchLog.read (P1-17), media services reset (03 §8), LeState grid (P1-07).
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/AudioSession.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {
struct Offline {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    Offline() {
        LeConfig c{};
        c.apiVersion = LE_API_VERSION;
        c.preferredBufferSize = 128;
        c.preferredSampleRate = 48000.0;
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(c, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
    }
    juce::var call(const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e->call(j.c_str()).c_str()), v);
        return v;
    }
    void send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        REQUIRE(e->send(c));
    }
    void render(int frames) {
        std::vector<float> L(128), R(128);
        float* outs[2] = {L.data(), R.data()};
        for (int done = 0; done < frames; done += 128) dev->render(nullptr, 0, outs, 2, std::min(128, frames - done));
        e->pump();
    }
    LeState state() {
        LeState s{};
        globalStatePublisher().read(s);
        return s;
    }
};
} // namespace

TEST_CASE("launchLog.read: đọc tăng dần theo sinceIndex", "[core][launchlog][api]") {
    Offline o;
    REQUIRE((bool) o.call(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"a","lengthBeats":4,"notes":[]})")["ok"]);
    REQUIRE((bool) o.call(R"({"op":"clip.setMidi","track":1,"slot":0,"clipId":"b","lengthBeats":4,"notes":[]})")["ok"]);
    o.render(128);
    o.send(LE_CMD_SCENE_LAUNCH, -1, 0);
    o.render(128);
    auto r = o.call(R"({"op":"launchLog.read","sinceIndex":0})")["result"];
    REQUIRE(r["events"].size() == 3);   // scene, launch t0, launch t1
    REQUIRE(r["events"][0]["kind"].toString() == "scene");
    REQUIRE((int) r["events"][0]["slot"] == 0);
    REQUIRE(r["events"][1]["kind"].toString() == "launch");
    REQUIRE((double) r["events"][1]["beat"] == 0.0);
    REQUIRE((int) r["nextIndex"] == 3);

    o.send(LE_CMD_CLIP_STOP, 0);
    o.render(96000 + 256);
    r = o.call(R"({"op":"launchLog.read","sinceIndex":3})")["result"];
    REQUIRE(r["events"].size() == 1);
    REQUIRE(r["events"][0]["kind"].toString() == "stop");
    REQUIRE((int) r["events"][0]["track"] == 0);
    REQUIRE((double) r["events"][0]["beat"] == 4.0);
    REQUIRE((int) r["events"][0]["index"] == 3);
    REQUIRE(o.call(R"({"op":"launchLog.read","sinceIndex":-1})")["error"]["code"].toString() == "INVALID_ARG");
}

TEST_CASE("LeState: clipState / trackPlayingSlot / beat / bpm / quantize từ engine thật", "[core][state]") {
    Offline o;
    o.call(R"({"op":"clip.setMidi","track":3,"slot":2,"clipId":"x","lengthBeats":2,"notes":[]})");
    o.render(128);
    o.send(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_1_4);
    o.send(LE_CMD_CLIP_LAUNCH, 3, 2);
    o.render(24000 + 128);   // tới beat 1+
    const LeState s = o.state();
    REQUIRE(s.playing == 1);
    REQUIRE(s.bpm == 120.0);
    REQUIRE(s.quantize == LE_Q_1_4);
    REQUIRE(s.beatsPerBar == 4);
    REQUIRE(s.clipState[3][2] == LE_CLIP_PLAYING);
    REQUIRE(s.trackPlayingSlot[3] == 2);
    REQUIRE(s.trackPlayingSlot[0] == -1);
    REQUIRE(s.beat > 1.0);
    REQUIRE(s.trackClipProgress[3] > 0.5f);
}

TEST_CASE("Media services reset → Engine stop + start lại device trên main", "[core][session]") {
    Offline o;
    REQUIRE(o.dev->startCount() == 1);
    le::io::session::counters().mediaServicesReset.fetch_add(1);   // giả lập notification của iOS
    o.e->pump();
    REQUIRE(o.dev->startCount() == 2);
    REQUIRE(o.dev->isRunning());
    REQUIRE(o.e->mediaServicesResets() == 1);
    o.e->pump();   // không có reset mới → không restart
    REQUIRE(o.dev->startCount() == 2);
}

TEST_CASE("P1-07: le_read_state từ thread khác khi engine render — counter đơn điệu, không rách (TSan)", "[core][state][stress]") {
    Offline o;
    o.call(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"x","lengthBeats":1,"notes":[]})");
    o.send(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_NONE);
    o.send(LE_CMD_CLIP_LAUNCH, 0, 0);

    o.render(128);   // publish block đầu của engine NÀY trước khi đọc (LeState là toàn cục, test trước để lại bản cũ)
    std::atomic<bool> running{true};
    std::thread audio([&] {   // vai audio thread: render liên tục
        std::vector<float> L(128), R(128);
        float* outs[2] = {L.data(), R.data()};
        for (int i = 0; i < 20000 && running.load(); ++i) o.dev->render(nullptr, 0, outs, 2, 128);
        running = false;
    });
    uint32_t last = 0, reads = 0, bad = 0;
    while (running.load()) {   // vai UI (Ticker) ở thread khác
        LeState s{};
        globalStatePublisher().read(s);
        if (s.publishCounter < last) ++bad;
        // bất biến của một bản KHÔNG rách: mọi trường cùng một block
        if (s.publishCounter > 0 && (s.bufferSize != 128 || s.sampleRate != 48000.0 || s.bpm != 120.0)) ++bad;
        for (int t = 0; t < LE_MAX_TRACKS; ++t)
            for (int c = 0; c < LE_MAX_SCENES; ++c)
                if (s.clipState[t][c] > LE_CLIP_OVERDUBBING) ++bad;
        last = s.publishCounter;
        ++reads;
    }
    audio.join();
    REQUIRE(bad == 0);
    REQUIRE(reads > 0);
    LeState s{};
    globalStatePublisher().read(s);
    REQUIRE(s.clipState[0][0] == LE_CLIP_PLAYING);
    REQUIRE(s.publishCounter >= 20000);
}

TEST_CASE("project.open / close: reset MỌI state RT của project, giữ thiết lập toàn cục", "[core][api][project]") {
    Offline o;
    auto sendF = [&](uint16_t type, int track, float f0, int32_t i0 = 0, double d0 = 0.0) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = -1;
        c.i0 = i0;
        c.f0 = f0;
        c.d0 = d0;
        REQUIRE(o.e->send(c));
    };
    REQUIRE((bool) o.call(R"({"op":"midi.setRecordQuantize","grid":0.25})")["ok"]);   // toàn cục
    REQUIRE((bool) o.call(R"({"op":"transport.setTimeSignature","num":3,"den":4})")["ok"]);
    sendF(LE_CMD_SET_BPM, -1, 0.0f, 0, 90.0);
    sendF(LE_CMD_SET_QUANTIZE, -1, 0.0f, LE_Q_1_4);
    sendF(LE_CMD_METRONOME, -1, 0.3f, 1);
    sendF(LE_CMD_SET_COUNT_IN, -1, 0.0f, 2);
    sendF(LE_CMD_TRACK_GAIN, 2, -6.0f);
    sendF(LE_CMD_TRACK_PAN, 2, 0.5f);
    sendF(LE_CMD_TRACK_MUTE, 3, 0.0f, 1);
    sendF(LE_CMD_TRACK_SOLO, 4, 0.0f, 1);
    sendF(LE_CMD_TRACK_ARM, 5, 0.0f, 1);
    sendF(LE_CMD_TRACK_MONITOR, 5, 0.0f, 2);
    sendF(LE_CMD_MASTER_GAIN, -1, -4.0f);
    sendF(LE_CMD_TRANSPORT_PLAY, -1, 0.0f);
    o.render(1024);
    const RtEngine& rt = o.e->rt();
    REQUIRE(o.state().playing == 1);
    REQUIRE(o.state().bpm == 90.0);
    REQUIRE(o.state().beatsPerBar == 3);
    REQUIRE(rt.mixer().gainDb(2) == -6.0f);
    REQUIRE(rt.armed(5));

    const std::string dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("le_reset").getFullPathName().toStdString();
    REQUIRE((bool) o.call(R"({"op":"project.open","dir":")" + dir + R"("})")["ok"]);
    o.render(1024);   // ~60 lệnh: RT xả 64 lệnh / block
    const LeState s = o.state();
    REQUIRE(s.playing == 0);
    REQUIRE(s.bpm == 120.0);
    REQUIRE(s.beatsPerBar == 4);
    REQUIRE(s.quantize == LE_Q_1_BAR);
    REQUIRE(rt.metronome().mode() == 0);
    REQUIRE(rt.scheduler().countInBars() == 0);
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        REQUIRE(rt.mixer().gainDb(t) == 0.0f);
        REQUIRE(rt.mixer().pan(t) == 0.0f);
        REQUIRE_FALSE(rt.mixer().muted(t));
        REQUIRE_FALSE(rt.mixer().soloed(t));
        REQUIRE_FALSE(rt.armed(t));
        REQUIRE(rt.monitorMode(t) == 0);
    }
    REQUIRE(rt.mixer().masterGainDb() == 0.0f);
    REQUIRE(rt.masterEq().gainDb(1) == 0.0f);

    // project.close cũng vậy
    sendF(LE_CMD_SET_BPM, -1, 0.0f, 0, 140.0);
    o.render(256);
    REQUIRE((bool) o.call(R"({"op":"project.close"})")["ok"]);
    o.render(256);
    REQUIRE(o.state().bpm == 120.0);
}

TEST_CASE("audio.setInputEnabled: restart device có / không có input, project.open không đổi", "[core][api][audio]") {
    Offline o;   // LeConfig.numInputChannels = 0 → bật lại dùng 1 kênh
    const int starts = o.dev->startCount();
    auto r = o.call(R"({"op":"audio.setInputEnabled","enabled":false})");
    REQUIRE((bool) r["ok"]);
    REQUIRE((int) r["result"]["inputChannels"] == 0);
    REQUIRE((int) o.call(R"({"op":"engine.info"})")["result"]["inputChannels"] == 0);
    REQUIRE(o.dev->startCount() == starts);   // đã là 0 kênh: không restart

    r = o.call(R"({"op":"audio.setInputEnabled","enabled":true})");
    REQUIRE((bool) r["ok"]);
    REQUIRE((int) r["result"]["inputChannels"] == 1);
    REQUIRE(o.dev->startCount() == starts + 1);
    REQUIRE(o.dev->numInputs() == 1);
    o.render(512);   // RtEngine đã prepare lại, vẫn chạy

    const std::string dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("le_input").getFullPathName().toStdString();
    REQUIRE((bool) o.call(R"({"op":"project.open","dir":")" + dir + R"("})")["ok"]);
    REQUIRE(o.dev->numInputs() == 1);   // thiết lập toàn cục

    r = o.call(R"({"op":"audio.setInputEnabled","enabled":false})");
    REQUIRE((int) r["result"]["inputChannels"] == 0);
    REQUIRE(o.dev->startCount() == starts + 2);
    REQUIRE(o.call(R"({"op":"audio.setInputEnabled"})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(o.call(R"({"op":"audio.setInputEnabled","enabled":1})")["error"]["code"].toString() == "INVALID_ARG");
}
