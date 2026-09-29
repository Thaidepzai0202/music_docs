// P1-06: GraphSnapshot + swap + ReleasePool + luật generation.
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {
LeConfig offlineConfig() {
    LeConfig c{};
    c.apiVersion = LE_API_VERSION;
    c.preferredBufferSize = 128;
    c.preferredSampleRate = 48000.0;
    c.numInputChannels = 0;
    return c;
}

juce::var parse(const std::string& s) {
    juce::var v;
    juce::JSON::parse(juce::String::fromUTF8(s.c_str()), v);
    return v;
}

// Engine offline + thread "audio" render liên tục bằng OfflineDeviceIO (đúng vai audio thread thật).
struct OfflineEngine {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> engine;
    std::atomic<bool> running{false};
    std::thread audio;

    OfflineEngine() {
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        engine = std::make_unique<Engine>(offlineConfig(), std::move(d));
        REQUIRE(engine->audioStart() == LE_OK);
    }
    ~OfflineEngine() {
        stopThread();
        engine.reset();
    }
    void renderBlocks(int blocks, int n = 128) {   // trên thread hiện tại
        std::vector<float> L((size_t) n), R((size_t) n);
        float* outs[2] = {L.data(), R.data()};
        for (int b = 0; b < blocks; ++b) dev->render(nullptr, 0, outs, 2, n);
    }
    void startThread() {
        running = true;
        audio = std::thread([this] {
            std::vector<float> L(128), R(128);
            float* outs[2] = {L.data(), R.data()};
            while (running.load(std::memory_order_acquire)) dev->render(nullptr, 0, outs, 2, 128);
        });
    }
    void stopThread() {
        if (!audio.joinable()) return;
        running = false;
        audio.join();
    }
    juce::var call(const std::string& json) { return parse(engine->call(json.c_str())); }
};

// Giả lập voice đang release: giữ generation cho tới khi test thả ra.
struct PinningUser final : GenerationUser {
    std::atomic<std::uint32_t> pinned{0};
    std::atomic<int> fastReleases{0};
    bool usesGeneration(std::uint32_t g) const noexcept [[clang::nonblocking]] override { return g != 0 && g == pinned.load(); }
    void fastReleaseGeneration(std::uint32_t g) noexcept [[clang::nonblocking]] override {
        if (g == pinned.load()) {
            fastReleases.fetch_add(1);
            pinned.store(0);
        }
    }
};

std::string setMidi(int track, int slot, double len, int nNotes = 4) {
    std::string notes;
    for (int i = 0; i < nNotes; ++i) notes += std::string(i ? "," : "") + R"({"p":)" + std::to_string(60 + i) +
                                              R"(,"v":100,"s":)" + std::to_string(i * 0.5) + R"(,"d":0.25})";
    return R"({"op":"clip.setMidi","track":)" + std::to_string(track) + R"(,"slot":)" + std::to_string(slot) +
           R"(,"clipId":"c_)" + std::to_string(track) + "_" + std::to_string(slot) + R"(","lengthBeats":)" +
           std::to_string(len) + R"(,"notes":[)" + notes + "]}";
}
} // namespace

TEST_CASE("Snapshot: lệnh cấu trúc → snapshot mới có đúng dữ liệu, bản cũ được thu hồi", "[core][snapshot]") {
    const int live0 = GraphSnapshot::liveCount().load();
    {
        OfflineEngine oe;
        const auto* first = oe.engine->rt().currentSnapshot();
        REQUIRE(first != nullptr);
        REQUIRE_FALSE(first->clips[2][3].present());

        REQUIRE((bool) oe.call(setMidi(2, 3, 4.0))["ok"]);
        oe.renderBlocks(1);   // RT nhận snapshot mới ở đầu block
        const auto* s = oe.engine->rt().currentSnapshot();
        REQUIRE(s != first);
        REQUIRE(s->generation > first->generation);
        REQUIRE(s->clips[2][3].kind == ClipKind::Midi);
        REQUIRE(s->clips[2][3].lengthBeats == 4.0);
        REQUIRE(s->clips[2][3].midi->notes.size() == 4);

        oe.engine->pump();    // ReleasePool (main)
        REQUIRE(oe.engine->releasedSnapshots() == 1);
        REQUIRE(oe.engine->rt().retiringCount() == 0);

        REQUIRE((bool) oe.call(R"({"op":"transport.setTimeSignature","num":3,"den":4})")["ok"]);
        oe.renderBlocks(1);
        REQUIRE(oe.engine->rt().transport().beatsPerBar() == 3);
    }
    REQUIRE(GraphSnapshot::liveCount().load() == live0);   // không leak
}

TEST_CASE("Snapshot: publish 2 lần trước khi RT lấy → bản chưa dùng huỷ trên main", "[core][snapshot]") {
    const int live0 = GraphSnapshot::liveCount().load();
    {
        OfflineEngine oe;
        oe.call(setMidi(0, 0, 4.0));
        oe.call(setMidi(0, 1, 8.0));   // bản đầu bị thay khi còn pending → huỷ ngay trên main
        REQUIRE(GraphSnapshot::liveCount().load() == live0 + 2);   // current + 1 pending
        oe.renderBlocks(1);
        const auto* s = oe.engine->rt().currentSnapshot();
        REQUIRE(s->clips[0][0].present());
        REQUIRE(s->clips[0][1].lengthBeats == 8.0);
    }
    REQUIRE(GraphSnapshot::liveCount().load() == live0);
}

TEST_CASE("Snapshot: 10^4 lần swap trong lúc render trên thread khác (ASan/RTSan/TSan)", "[core][snapshot][stress]") {
    const int live0 = GraphSnapshot::liveCount().load();
    {
        OfflineEngine oe;
        oe.startThread();
        for (int i = 0; i < 10000; ++i) {
            const int t = i % LE_MAX_TRACKS, c = (i / 8) % LE_MAX_SCENES;
            const auto r = (i % 3 == 2) ? oe.call(R"({"op":"clip.clear","track":)" + std::to_string(t) + R"(,"slot":)" +
                                                  std::to_string(c) + "}")
                                        : oe.call(setMidi(t, c, 1.0 + (i % 16), 1 + i % 8));
            REQUIRE((bool) r["ok"]);
            if (i % 16 == 0) oe.engine->pump();   // ReleasePool chạy thưa như Timer 30Hz
        }
        juce::Thread::sleep(20);
        oe.stopThread();
        oe.engine->pump();
        INFO("đã thu hồi " << oe.engine->releasedSnapshots());
        REQUIRE(oe.engine->releasedSnapshots() > 0);
        REQUIRE(oe.engine->rt().retiringCount() == 0);
    }
    REQUIRE(GraphSnapshot::liveCount().load() == live0);
}

TEST_CASE("Snapshot: generation còn người dùng → KHÔNG thu hồi cho tới khi voice cuối tắt", "[core][snapshot][generation]") {
    const int live0 = GraphSnapshot::liveCount().load();
    {
        OfflineEngine oe;
        PinningUser voice;
        // Đăng ký phải làm khi audio chưa chạy: dừng, đăng ký, chạy lại.
        oe.engine->audioStop();
        oe.engine->rt().addGenerationUser(&voice);
        REQUIRE(oe.engine->audioStart() == LE_OK);

        const std::uint32_t oldGen = oe.engine->rt().currentSnapshot()->generation;
        voice.pinned = oldGen;   // "voice đang release" còn đọc instrument của snapshot cũ

        oe.call(setMidi(1, 1, 4.0));
        oe.renderBlocks(4);
        oe.engine->pump();
        REQUIRE(oe.engine->rt().retiringCount() == 1);   // bị giữ lại
        REQUIRE(oe.engine->releasedSnapshots() == 0);

        voice.pinned = 0;   // voice cuối tắt
        oe.renderBlocks(1);
        oe.engine->pump();
        REQUIRE(oe.engine->rt().retiringCount() == 0);
        REQUIRE(oe.engine->releasedSnapshots() == 1);
        oe.engine->audioStop();
    }
    REQUIRE(GraphSnapshot::liveCount().load() == live0);
}

TEST_CASE("Snapshot: retiring[4] đầy → fade nhanh voice cũ nhất, chưa nhận bản mới", "[core][snapshot][generation]") {
    const int live0 = GraphSnapshot::liveCount().load();
    {
        OfflineEngine oe;
        std::vector<std::unique_ptr<PinningUser>> voices;
        oe.engine->audioStop();
        for (int i = 0; i < 6; ++i) {
            voices.push_back(std::make_unique<PinningUser>());
            oe.engine->rt().addGenerationUser(voices.back().get());
        }
        REQUIRE(oe.engine->audioStart() == LE_OK);

        // 4 lần đổi, mỗi snapshot cũ bị 1 voice giữ → retiring đầy
        for (int i = 0; i < 4; ++i) {
            voices[(size_t) i]->pinned = oe.engine->rt().currentSnapshot()->generation;
            oe.call(setMidi(0, i, 4.0));
            oe.renderBlocks(1);
        }
        REQUIRE(oe.engine->rt().retiringCount() == 4);
        const auto* before = oe.engine->rt().currentSnapshot();

        oe.call(setMidi(0, 5, 4.0));
        oe.renderBlocks(1);   // đầy: không đổi snapshot, fade nhanh bản cũ nhất
        REQUIRE(oe.engine->rt().currentSnapshot() == before);
        REQUIRE(voices[0]->fastReleases.load() == 1);

        oe.renderBlocks(2);   // bản cũ nhất được thả → còn chỗ → nhận bản mới
        REQUIRE(oe.engine->rt().currentSnapshot() != before);
        REQUIRE(oe.engine->rt().currentSnapshot()->clips[0][5].present());
        for (auto& v : voices) v->pinned = 0;
        oe.renderBlocks(2);
        oe.engine->pump();
        REQUIRE(oe.engine->rt().retiringCount() == 0);
        oe.engine->audioStop();
    }
    REQUIRE(GraphSnapshot::liveCount().load() == live0);
}

TEST_CASE("Ops cấu trúc: kiểm tham số + clip.info / clip.getMidi", "[core][snapshot][ops]") {
    OfflineEngine oe;
    auto code = [&](const std::string& j) { return oe.call(j)["error"]["code"].toString().toStdString(); };
    REQUIRE(code(R"({"op":"clip.setMidi","track":8,"slot":0,"clipId":"x","lengthBeats":4,"notes":[]})") == "INVALID_ARG");
    REQUIRE(code(R"({"op":"clip.setMidi","track":0,"slot":0,"lengthBeats":4,"notes":[]})") == "INVALID_ARG");   // thiếu clipId
    REQUIRE(code(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"x","lengthBeats":0,"notes":[]})") == "INVALID_ARG");
    REQUIRE(code(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"x","lengthBeats":4,"notes":[{"p":60,"v":100,"s":4,"d":1}]})") == "INVALID_ARG");
    REQUIRE(code(R"({"op":"transport.setTimeSignature","num":7,"den":5})") == "INVALID_ARG");
    REQUIRE(code(R"({"op":"track.configure","track":0,"kind":"drums"})") == "INVALID_ARG");
    REQUIRE(code(R"({"op":"project.open","dir":"relative"})") == "INVALID_ARG");
    REQUIRE(code(R"({"op":"clip.getMidi","track":0,"slot":0})") == "INVALID_ARG");   // ô trống

    REQUIRE(oe.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["kind"].toString() == "empty");
    REQUIRE((bool) oe.call(R"({"op":"clip.setMidi","track":0,"slot":0,"clipId":"m1","lengthBeats":4,
                               "notes":[{"p":64,"v":90,"s":2,"d":1},{"p":60,"v":100,"s":0,"d":0.5}]})")["ok"]);
    const auto info = oe.call(R"({"op":"clip.info","track":0,"slot":0})")["result"];
    REQUIRE(info["kind"].toString() == "midi");
    REQUIRE(info["clipId"].toString() == "m1");
    REQUIRE((int) info["noteCount"] == 2);
    const auto notes = oe.call(R"({"op":"clip.getMidi","track":0,"slot":0})")["result"]["notes"];
    REQUIRE(notes.size() == 2);
    REQUIRE((int) notes[0]["p"] == 60);   // đã sắp theo startBeat
    REQUIRE((double) notes[1]["s"] == 2.0);

    REQUIRE((bool) oe.call(R"({"op":"clip.clear","track":0,"slot":0})")["ok"]);
    REQUIRE(oe.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["kind"].toString() == "empty");
    REQUIRE((bool) oe.call(R"({"op":"project.open","dir":"/tmp/le-proj"})")["ok"]);
    REQUIRE((bool) oe.call(R"({"op":"track.configure","track":3,"kind":"instrument","name":"Keys"})")["ok"]);
    REQUIRE(oe.engine->model().tracks[3].kind == TrackKind::Instrument);
    REQUIRE(oe.engine->model().tracks[3].name == "Keys");
    REQUIRE((bool) oe.call(R"({"op":"project.close"})")["ok"]);
    REQUIRE(oe.engine->model().tracks[3].kind == TrackKind::Audio);
}

TEST_CASE("clip.setParams: đổi gainDb / warp không decode lại (cùng AudioData trong snapshot)", "[core][snapshot][ops]") {
    OfflineEngine oe;
    REQUIRE(oe.call(R"({"op":"clip.setParams","track":0,"slot":0,"gainDb":-3})")["error"]["code"].toString() == "INVALID_ARG");   // ô trống
    oe.call(setMidi(0, 0, 4.0));
    REQUIRE(oe.call(R"({"op":"clip.setParams","track":0,"slot":0,"warp":"stretch"})")["error"]["code"].toString() == "INVALID_ARG");   // MIDI
    REQUIRE((bool) oe.call(R"({"op":"clip.setParams","track":0,"slot":0,"gainDb":-6})")["ok"]);
    REQUIRE(oe.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["gainDb"].toString() == "-6.0");
    REQUIRE(oe.call(R"({"op":"clip.setParams","track":0,"slot":0})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(oe.call(R"({"op":"clip.setParams","track":0,"slot":0,"gainDb":30})")["error"]["code"].toString() == "INVALID_ARG");
}
