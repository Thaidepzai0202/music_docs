// P3-08 hook đổi tempo (debounce 300 ms, huỷ job cũ, cache stretched/) · P3-10 chuyển sang bản stretched ở ranh
// giới bar · P3-11 loop thư viện khác BPM được render ngay khi gán.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"
#include "render/WarpRenderer.h"

using namespace le::core;

namespace {

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    juce::File dir;
    std::vector<float> L = std::vector<float>(128), R = std::vector<float>(128);

    Rig() {
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.preferredBufferSize = 128;
        cfg.preferredSampleRate = 48000.0;
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(cfg, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-warp", "", false);
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
    void send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0, double d0 = 0.0) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        c.d0 = d0;
        REQUIRE(e->send(c));
    }
    void render(int frames) {   // pump sau mỗi block (như app: event, snapshot, job)
        float* outs[2] = {L.data(), R.data()};
        for (int done = 0; done < frames; done += 128) {
            dev->render(nullptr, 0, outs, 2, std::min(128, frames - done));
            e->pump();
        }
    }
    void setClip(const char* file, int origBpm, const char* warp = "stretch") {
        const auto r = call(std::string(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"c0","lengthBeats":4,"originalBpm":)") +
                            std::to_string(origBpm) + R"(,"warp":")" + warp + R"(","file":")" LE_TEST_FIXTURES_DIR "/" + file + R"("})");
        REQUIRE((bool) r["ok"]);
        const std::string q = R"({"op":"job.result","jobId":)" + r["result"]["jobId"].toString().toStdString() + "}";
        for (int i = 0; i < 400 && call(q)["result"]["status"].toString() != "done"; ++i) {   // done = onDone đã chạy
            e->pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        REQUIRE(call(q)["result"]["status"].toString() == "done");
        e->pump();   // onDone của job warp (offline: worker đã xong ngay trong lúc submit)
    }
    double stretchedBpm() { return (double) call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["stretchedBpm"]; }
    const GraphSnapshot* snap() const { return e->rt().currentSnapshot(); }
    juce::File cacheFile(double bpm) const {
        return juce::File(juce::String::fromUTF8(le::render::stretchedCachePath(dir.getFullPathName().toStdString(), "c0", bpm).c_str()));
    }
};

} // namespace

TEST_CASE("P3-11: loop 100 BPM gán vào project 120 BPM → render bản stretched ngay (không debounce), có cache", "[core][warp]") {
    Rig r;
    r.setClip("clip_click_4beats_100.wav", 100);
    REQUIRE(r.stretchedBpm() == 120.0);
    const auto& c = *r.e->model().clips[0][0];
    REQUIRE(c.stretched->numFrames() == le::render::warpedLength(c.audio->numFrames(), 100.0, 120.0));
    REQUIRE(r.cacheFile(120.0).existsAsFile());
    r.render(128);   // RT nhận snapshot
    REQUIRE(r.snap()->clips[0][0].stretched != nullptr);
    REQUIRE(r.snap()->clips[0][0].stretchedBpm == 120.0);

    // Re-Pitch → không có bản stretched trong snapshot; đổi lại stretch → dùng lại bản trong model (không render lại)
    REQUIRE(r.ok(R"({"op":"clip.setParams","track":0,"slot":0,"warp":"repitch"})"));
    REQUIRE(r.stretchedBpm() == 0.0);
    REQUIRE(r.snap() != nullptr);
    r.render(128);
    REQUIRE(r.snap()->clips[0][0].stretched == nullptr);
    REQUIRE(r.ok(R"({"op":"clip.setParams","track":0,"slot":0,"warp":"stretch"})"));
    REQUIRE(r.stretchedBpm() == 120.0);
}

TEST_CASE("P3-08: SET_BPM → debounce 300 ms (audio time khi offline) rồi mới render; đổi liên tục chỉ render BPM cuối",
          "[core][warp]") {
    Rig r;
    r.setClip("clip_sine_4beats_120.wav", 120);   // cùng BPM → không cần stretched
    REQUIRE(r.stretchedBpm() == 0.0);
    r.send(LE_CMD_CLIP_LAUNCH, 0, 0);
    r.render(1024);

    r.send(LE_CMD_SET_BPM, -1, -1, 0, 110.0);
    r.render(9600);    // 200 ms < 300 ms
    r.send(LE_CMD_SET_BPM, -1, -1, 0, 100.0);   // debounce bắt đầu lại
    r.render(12000);   // 250 ms
    REQUIRE(r.stretchedBpm() == 0.0);
    r.render(4800);    // > 300 ms kể từ lần đổi cuối → render @100
    r.render(256);
    REQUIRE(r.stretchedBpm() == 100.0);
    REQUIRE(r.cacheFile(100.0).existsAsFile());
    REQUIRE_FALSE(r.cacheFile(110.0).existsAsFile());   // BPM trung gian không bao giờ được render

    // P3-10: player chuyển sang bản stretched ở ranh giới bar kế tiếp
    REQUIRE_FALSE(r.e->rt().track(0).clipStretched());
    r.render(2 * 115200);   // 2 bar @100 BPM
    REQUIRE(r.e->rt().track(0).clipStretched());

    // Về lại BPM gốc → Re-Pitch (1:1), không render gì; bản @100 vẫn giữ (quay lại 100 dùng ngay)
    r.send(LE_CMD_SET_BPM, -1, -1, 0, 120.0);
    r.render(19200);
    REQUIRE_FALSE(r.e->rt().track(0).clipStretched());
    r.send(LE_CMD_SET_BPM, -1, -1, 0, 100.0);
    r.render(19200);
    REQUIRE(r.stretchedBpm() == 100.0);
}

TEST_CASE("P3-08: audio của clip đổi (overdub / undo / setAudio mới) → bản stretched cũ không còn hợp lệ", "[core][warp]") {
    Rig r;
    r.setClip("clip_click_4beats_100.wav", 100);
    REQUIRE(r.stretchedBpm() == 120.0);
    r.setClip("clip_click_4beats_120.wav", 100);   // nguồn khác (vẫn khai 100 BPM) → render lại cho nguồn mới
    REQUIRE(r.stretchedBpm() == 120.0);
    const auto& c = *r.e->model().clips[0][0];
    REQUIRE(c.stretchedSource.lock() == c.audio);
    REQUIRE(r.ok(R"({"op":"clip.clear","track":0,"slot":0})"));
    REQUIRE(r.stretchedBpm() == 0.0);
}
