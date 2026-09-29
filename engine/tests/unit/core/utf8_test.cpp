// 05 §1: mọi chuỗi qua FFI là UTF-8 — round-trip tiếng Việt + emoji qua project.open, clip.setAudio / clip.info,
// le_get_peaks, SFZ có sample tên có dấu, capture, export, thông báo lỗi. So từng byte.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {
juce::File utf8File(const juce::File& parent, const std::string& name) { return parent.getChildFile(juce::String::fromUTF8(name.c_str())); }
std::string bytes(const juce::var& v) { return v.toString().toStdString(); }   // toStdString = UTF-8
} // namespace

TEST_CASE("UTF-8 round-trip: đường dẫn / id tiếng Việt + emoji chạy đúng và trả lại đúng từng byte", "[core][utf8]") {
    const juce::File root = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-utf8", "", false);
    const juce::File project = utf8File(root, "Bài hát mới 🥁.loopproj");
    const juce::File library = utf8File(root, "Thư viện ñ");
    REQUIRE(utf8File(project, "audio").createDirectory());
    REQUIRE(utf8File(library, "kit").createDirectory());
    const juce::File sine(juce::String(LE_TEST_FIXTURES_DIR) + "/clip_sine_4beats_120.wav");
    REQUIRE(sine.copyFileTo(utf8File(utf8File(project, "audio"), "Trống ñ.wav")));
    REQUIRE(sine.copyFileTo(utf8File(utf8File(library, "kit"), "Kích ñ.wav")));
    REQUIRE(utf8File(utf8File(library, "kit"), "bộ trống.sfz").replaceWithText(juce::String::fromUTF8("<region> sample=Kích ñ.wav key=36\n")));

    const std::string libPath = library.getFullPathName().toStdString();
    LeConfig cfg{};
    cfg.apiVersion = LE_API_VERSION;
    cfg.numInputChannels = 1;
    cfg.preferredBufferSize = 128;
    cfg.preferredSampleRate = 48000.0;
    cfg.libraryDir = libPath.c_str();
    auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
    auto* dev = d.get();
    Engine e(cfg, std::move(d));
    REQUIRE(e.audioStart() == LE_OK);
    auto call = [&](const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e.call(j.c_str()).c_str()), v);
        return v;
    };
    auto wait = [&](const juce::var& reply) {
        REQUIRE((bool) reply["ok"]);
        const std::string q = R"({"op":"job.result","jobId":)" + reply["result"]["jobId"].toString().toStdString() + "}";
        juce::var r;
        for (int i = 0; i < 2000; ++i) {
            r = call(q)["result"];
            if (r["status"].toString() != "running") break;
            e.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return r;
    };
    auto render = [&](int frames) {
        std::vector<float> in(128, 0.25f), L(128), R(128);
        for (int done = 0; done < frames; done += 128) {
            const float* ins[1] = {in.data()};
            float* outs[2] = {L.data(), R.data()};
            dev->render(ins, 1, outs, 2, 128);
            e.pump();
        }
    };

    const std::string dir = project.getFullPathName().toStdString();
    REQUIRE((bool) call(R"({"op":"project.open","dir":")" + dir + R"("})")["ok"]);
    REQUIRE(wait(call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"cú đấm 🎵","file":"audio/Trống ñ.wav","lengthBeats":4,"originalBpm":120})"))["status"].toString() == "done");
    const auto info = call(R"({"op":"clip.info","track":0,"slot":0})")["result"];
    REQUIRE(bytes(info["file"]) == "audio/Trống ñ.wav");
    REQUIRE(bytes(info["clipId"]) == "cú đấm 🎵");
    float peaks[32] = {};
    REQUIRE(e.getPeaks("cú đấm 🎵", 0, peaks, 16) > 0);

    const auto sfz = wait(call(R"({"op":"track.setInstrument","track":1,"instrument":{"kind":"sfz","path":"kit/bộ trống.sfz"}})"));
    REQUIRE(sfz["status"].toString() == "done");
    REQUIRE((int) sfz["result"]["samplesLoaded"] == 1);

    REQUIRE((bool) call(R"({"op":"capture.start","path":"instruments/giọng 🎤/source.caf","maxSeconds":1})")["ok"]);
    render(4800);
    const auto cap = call(R"({"op":"capture.stop"})")["result"];
    REQUIRE(bytes(cap["file"]) == "instruments/giọng 🎤/source.caf");
    REQUIRE(utf8File(utf8File(utf8File(project, "instruments"), "giọng 🎤"), "source.caf").existsAsFile());

    const auto ex = wait(call(R"({"op":"export.scene","scene":0,"bars":1,"path":"exports/Bản phối 🎶.wav"})"));
    REQUIRE(ex["status"].toString() == "done");
    REQUIRE(bytes(ex["result"]["file"]) == "exports/Bản phối 🎶.wav");
    REQUIRE(utf8File(utf8File(project, "exports"), "Bản phối 🎶.wav").existsAsFile());

    // Thông báo lỗi tiếng Việt ra JSON đúng UTF-8
    const auto err = call(R"({"op":"fx.set","track":0,"index":9,"type":"eq3"})");
    REQUIRE(bytes(err["error"]["message"]) == "index phải là 0..2");
    root.deleteRecursively();
}
