// P3-18/19 export.scene (RtEngine offline riêng, đúng N bar ±0 sample, stems) · P3-17 export.jamStart / jamStop.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/AudioFileIO.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    juce::File dir;
    Rig() {
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.preferredBufferSize = 128;
        cfg.preferredSampleRate = 48000.0;
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(cfg, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-exp", "", false);
        dir.createDirectory();
        REQUIRE(ok(R"({"op":"project.open","dir":")" + dir.getFullPathName().toStdString() + R"("})"));
    }
    ~Rig() {
        e.reset();
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
        for (int i = 0; i < 6000; ++i) {
            const auto r = call(q)["result"];
            if (r["status"].toString() != "running") return r;
            e->pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        FAIL("job quá 30 s");
        return {};
    }
    void send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0, float f0 = 0.0f) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        c.f0 = f0;
        REQUIRE(e->send(c));
    }
    // Render live, trả kênh L và R nối liền.
    void render(int frames, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr) {
        std::vector<float> L(128), R(128);
        for (int done = 0; done < frames; done += 128) {
            const int n = std::min(128, frames - done);
            float* outs[2] = {L.data(), R.data()};
            dev->render(nullptr, 0, outs, 2, n);
            if (outL != nullptr) outL->insert(outL->end(), L.begin(), L.begin() + n);
            if (outR != nullptr) outR->insert(outR->end(), R.begin(), R.begin() + n);
            e->pump();
        }
    }
    void setAudio(int track, const char* file) {
        waitJob(call(std::string(R"({"op":"clip.setAudio","track":)") + std::to_string(track) +
                     R"(,"slot":0,"clipId":"c)" + std::to_string(track) + R"(","lengthBeats":4,"originalBpm":120,"file":")" LE_TEST_FIXTURES_DIR "/" +
                     file + R"("})"));
    }
    std::string path(const char* rel) const { return dir.getChildFile(rel).getFullPathName().toStdString(); }
};

} // namespace

TEST_CASE("export.scene: đúng N bar ±0 sample, giống hệt bản phát live (±1 LSB 24-bit), stems, FX live không bị đụng",
          "[core][export]") {
    Rig r;
    r.setAudio(0, "clip_sine_4beats_120.wav");
    r.setAudio(1, "clip_click_4beats_120.wav");
    r.send(LE_CMD_TRACK_GAIN, 1, -1, 0, -6.0f);
    r.send(LE_CMD_TRACK_PAN, 0, -1, 0, -0.5f);
    REQUIRE(r.ok(R"({"op":"fx.set","track":0,"index":0,"type":"filter","params":{"1":800}})"));
    r.send(LE_CMD_FX_PARAM, -1, 0, 1, 3.0f);   // EQ master mid +3 dB
    r.render(1024);                              // live nhận snapshot + lệnh (chưa phát gì)
    const auto liveFx = r.e->fxProcessor(0, 0).lock();

    const auto res = r.waitJob(r.call(R"({"op":"export.scene","scene":0,"bars":2,"path":"exports/s.wav","stems":true})"));
    REQUIRE(res["status"].toString() == "done");
    const auto& o = res["result"];
    REQUIRE(o["file"].toString() == "exports/s.wav");
    REQUIRE((juce::int64) o["frames"] == 192000);   // 2 bar × 4 beat × 24000 — đúng ±0 sample
    REQUIRE((double) o["seconds"] == 4.0);
    REQUIRE(o["stems"].size() == 2);
    REQUIRE(o["stems"][0].toString() == "exports/s_t1.wav");
    REQUIRE(o["stems"][1].toString() == "exports/s_t2.wav");
    REQUIRE(r.e->fxProcessor(0, 0).lock() == liveFx);   // instance live giữ nguyên (export dùng bản clone)

    const auto mix = le::io::decodeAudioFile(r.path("exports/s.wav"));
    REQUIRE(mix.error == LE_OK);
    REQUIRE(mix.data->numFrames() == 192000);
    for (const char* stem : {"exports/s_t1.wav", "exports/s_t2.wav"}) {
        const auto d = le::io::decodeAudioFile(r.path(stem));
        REQUIRE(d.error == LE_OK);
        REQUIRE(d.data->numFrames() == 192000);
    }
    REQUIRE_FALSE(juce::File(r.path("exports/s_t3.wav")).existsAsFile());   // track trống: không có stem

    // Bản live cùng scene (FX live chưa từng chạy → cùng state với bản clone)
    std::vector<float> L, R;
    r.send(LE_CMD_SCENE_LAUNCH, -1, 0);
    r.render(192000, &L, &R);
    double maxDiff = 0.0;
    for (std::int64_t i = 0; i < 192000; ++i) {
        maxDiff = std::max(maxDiff, (double) std::fabs(mix.data->channel(0)[i] - L[(size_t) i]));
        maxDiff = std::max(maxDiff, (double) std::fabs(mix.data->channel(1)[i] - R[(size_t) i]));
    }
    REQUIRE(maxDiff < 3.0 / 8388608.0);   // ≤ 1.5 LSB dither + làm tròn 24-bit

    REQUIRE(r.call(R"({"op":"export.scene","scene":8,"bars":2,"path":"x.wav"})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(r.call(R"({"op":"export.scene","scene":0,"bars":0,"path":"x.wav"})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(r.call(R"({"op":"export.scene","scene":0,"bars":1,"path":"x.wav","format":"mp3"})")["error"]["code"].toString() == "INVALID_ARG");
}

TEST_CASE("export.jamStart / jamStop: file = đúng tiếng master đã phát; stop khi chưa start → INVALID_ARG", "[core][export]") {
    Rig r;
    REQUIRE(r.call(R"({"op":"export.jamStop"})")["error"]["code"].toString() == "INVALID_ARG");
    r.setAudio(0, "clip_sine_4beats_120.wav");
    r.send(LE_CMD_CLIP_LAUNCH, 0, 0);
    r.render(4800);
    REQUIRE(r.ok(R"({"op":"export.jamStart","path":"exports/jam.wav"})"));
    REQUIRE(r.call(R"({"op":"export.jamStart","path":"exports/jam2.wav"})")["error"]["code"].toString() == "INVALID_ARG");
    std::vector<float> L, R;
    r.render(48000, &L, &R);
    const auto res = r.call(R"({"op":"export.jamStop"})")["result"];
    REQUIRE(res["file"].toString() == "exports/jam.wav");
    REQUIRE((double) res["seconds"] == 1.0);   // RT nhận ring ở block ngay sau jamStart
    REQUIRE((juce::int64) res["droppedFrames"] == 0);
    const auto d = le::io::decodeAudioFile(r.path("exports/jam.wav"));
    REQUIRE(d.error == LE_OK);
    REQUIRE(d.data->numFrames() == 48000);
    double maxDiff = 0.0;
    for (std::int64_t i = 0; i < 48000; ++i) maxDiff = std::max(maxDiff, (double) std::fabs(d.data->channel(0)[i] - L[(size_t) i]));
    REQUIRE(maxDiff < 3.0 / 8388608.0);
    REQUIRE(r.call(R"({"op":"export.jamStop"})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(r.ok(R"({"op":"export.jamStart","path":"exports/jam3.wav"})"));   // phiên mới
    r.render(1280);
    REQUIRE(std::fabs((double) r.call(R"({"op":"export.jamStop"})")["result"]["seconds"] - 1280.0 / 48000.0) < 1e-9);
}
