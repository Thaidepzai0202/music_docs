// P4-17 bộ nhớ thấp (memory.pressure) · P4-19 an toàn khi bị kill (CAF nguyên tử, dọn .tmp) · R7 buffer thu theo SR.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
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
    int type, a;
    double value;
};
std::vector<Ev>& evs() {
    static std::vector<Ev> v;
    return v;
}
void onEvent(int32_t type, int32_t a, int32_t, int64_t, double value) { evs().push_back({type, a, value}); }

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    juce::File dir;
    Rig() {
        evs().clear();
        setEventCallback(&onEvent);
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.numInputChannels = 1;
        cfg.preferredBufferSize = 128;
        cfg.preferredSampleRate = 48000.0;
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(cfg, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-robust", "", false);
        dir.createDirectory();
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
    juce::var waitJob(const juce::var& reply) {
        REQUIRE((bool) reply["ok"]);
        const std::string q = R"({"op":"job.result","jobId":)" + reply["result"]["jobId"].toString().toStdString() + "}";
        juce::var r;
        for (int i = 0; i < 4000; ++i) {
            r = call(q)["result"];
            if (r["status"].toString() != "running") break;
            e->pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return r;
    }
    void render(int frames) {
        std::vector<float> in(128), L(128), R(128);
        for (int done = 0; done < frames; done += 128) {
            for (int i = 0; i < 128; ++i) in[(size_t) i] = 0.4f * (float) std::sin(2.0 * 3.14159265 * 220.0 * (done + i) / 48000.0);
            const float* ins[1] = {in.data()};
            float* outs[2] = {L.data(), R.data()};
            dev->render(ins, 1, outs, 2, 128);
            e->pump();
        }
    }
    void open() { REQUIRE(ok(R"({"op":"project.open","dir":")" + dir.getFullPathName().toStdString() + R"("})")); }
};
} // namespace

TEST_CASE("P4-19: CAF ghi nguyên tử (.tmp → tên thật); project.open dọn .tmp còn sót sau khi bị kill", "[core][robust]") {
    Rig r;
    r.dir.getChildFile("audio").createDirectory();
    r.dir.getChildFile("exports").createDirectory();
    r.dir.getChildFile("audio/rec_1.caf.tmp").replaceWithText("dở");
    r.dir.getChildFile("exports/mix.wav.tmp").replaceWithText("dở");
    r.dir.getChildFile("cache/stretched").createDirectory();
    r.dir.getChildFile("cache/stretched/c@100.00.caf.tmp").replaceWithText("dở");
    r.open();
    REQUIRE_FALSE(r.dir.getChildFile("audio/rec_1.caf.tmp").exists());
    REQUIRE_FALSE(r.dir.getChildFile("exports/mix.wav.tmp").exists());
    REQUIRE_FALSE(r.dir.getChildFile("cache/stretched/c@100.00.caf.tmp").exists());

    le::dsp::AudioData d(2, 4800, 48000.0);
    for (int i = 0; i < 4800; ++i) d.writePointer(0)[i] = d.writePointer(1)[i] = 0.001f * (float) i;
    const std::string path = r.dir.getChildFile("audio/ok.caf").getFullPathName().toStdString();
    REQUIRE(le::io::writeCafFloat32(path, d, nullptr));
    REQUIRE_FALSE(r.dir.getChildFile("audio/ok.caf.tmp").exists());
    const auto back = le::io::decodeAudioFile(path);
    REQUIRE(back.error == LE_OK);
    REQUIRE(back.data->numFrames() == 4800);
    REQUIRE(back.data->channel(1)[4799] == d.channel(1)[4799]);
}

TEST_CASE("P4-17: memory.pressure nhả stretched của clip không phát + nhạc cụ không dùng (gán lại → tạo lại từ cache)",
          "[core][robust][memory]") {
    Rig r;
    r.open();
    r.waitJob(r.call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"a","file":")" LE_TEST_FIXTURES_DIR
                     R"(/clip_click_4beats_100.wav","lengthBeats":4,"originalBpm":100,"warp":"stretch"})"));
    r.waitJob(r.call(R"({"op":"clip.setAudio","track":1,"slot":0,"clipId":"b","file":")" LE_TEST_FIXTURES_DIR
                     R"(/clip_click_4beats_100.wav","lengthBeats":4,"originalBpm":100,"warp":"stretch"})"));
    r.e->pump();
    REQUIRE((double) r.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["stretchedBpm"] == 120.0);
    REQUIRE((double) r.call(R"({"op":"clip.info","track":1,"slot":0})")["result"]["stretchedBpm"] == 120.0);
    LeCommand c{};
    c.type = LE_CMD_CLIP_LAUNCH;
    c.track = 1;
    c.slot = 0;
    REQUIRE(r.e->send(c));
    r.render(1024);

    // Nhạc cụ tự thu, không track nào dùng
    REQUIRE(r.ok(R"({"op":"capture.start","path":"instruments/v/source.caf","maxSeconds":2})"));
    r.render(48000);
    REQUIRE(r.ok(R"({"op":"capture.stop"})"));
    REQUIRE(r.waitJob(r.call(R"({"op":"instrument.createFromRecording","instrumentId":"v","file":"instruments/v/source.caf","rootNote":57})"))["status"].toString() == "done");

    REQUIRE(r.call(R"({"op":"memory.pressure","level":"x"})")["error"]["code"].toString() == "INVALID_ARG");
    const auto m = r.call(R"({"op":"memory.pressure","level":"warning"})")["result"];
    REQUIRE((double) m["freedMB"] > 0.1);
    REQUIRE((double) m["usedMB"] > 0.0);
    REQUIRE(evs().back().type == LE_EVT_MEMORY_WARNING);
    REQUIRE(evs().back().value > 0.0);
    REQUIRE((double) r.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["stretchedBpm"] == 0.0);     // không phát → nhả
    REQUIRE((double) r.call(R"({"op":"clip.info","track":1,"slot":0})")["result"]["stretchedBpm"] == 120.0);   // đang phát → giữ

    // Gán nhạc cụ đã bị nhả → tự tạo lại (từ cache zone), track có tiếng trở lại
    r.waitJob(r.call(R"({"op":"track.setInstrument","track":2,"instrument":{"kind":"user","id":"v"}})"));
    for (int i = 0; i < 400 && r.e->model().tracks[2].instrument == nullptr; ++i) {
        r.e->pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(r.e->model().tracks[2].instrument != nullptr);
    REQUIRE(r.e->model().tracks[2].instrument->zones.size() == 13);
    REQUIRE((double) r.call(R"({"op":"engine.info"})")["result"]["memoryMB"] > 0.0);
}

TEST_CASE("R7: device chạy lại ở SR cao hơn → buffer thu được cấp lại đủ 66 s", "[core][robust]") {
    Rig r;
    LeCommand c{};
    c.type = LE_CMD_TRACK_ARM;
    c.track = 0;
    c.i0 = 1;
    REQUIRE(r.e->send(c));
    REQUIRE(r.e->recordBufferFrames(0) == 66 * 48000);
    REQUIRE(r.ok(R"({"op":"sim.offline","enabled":true,"sampleRate":96000,"blockSize":128})"));   // device mới @96k
    REQUIRE(r.e->recordBufferFrames(0) == 66 * 96000);
}
