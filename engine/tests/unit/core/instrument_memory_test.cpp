// 06 §4 nạp lười: chỉ nhạc cụ đang gán cho track nằm trong RAM. Đổi nhạc cụ → bản cũ được nhả khi snapshot cũ hết người
// dùng (voice đang kêu fade xong) — không cache ngầm nào giữ lại. Đổi qua lại 10 nhạc cụ nhiều vòng → RAM không tăng dần
// (engine.info.memoryMB = phys_footprint).
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {

#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
constexpr bool kSanitizer = true;   // ASan giữ bộ nhớ đã free (quarantine) → không đo footprint
#else
constexpr bool kSanitizer = false;
#endif
#else
constexpr bool kSanitizer = false;
#endif

constexpr int kInstruments = 10;
constexpr int kFrames = 48000 * 4;   // 4 s stereo → 1.5 MB float mỗi nhạc cụ

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    juce::File dir;
    std::string lib;
    std::vector<float> L = std::vector<float>(128), R = std::vector<float>(128);
    Rig() {
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-instmem", "", false);
        dir.createDirectory();
        juce::WavAudioFormat wav;
        for (int k = 0; k < kInstruments; ++k) {
            const juce::File f = dir.getChildFile("s" + juce::String(k) + ".wav");
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream>(f);
            auto w = wav.createWriterFor(os, juce::AudioFormatWriterOptions{}.withSampleRate(48000.0).withNumChannels(2).withBitsPerSample(16));
            REQUIRE(w != nullptr);
            juce::AudioBuffer<float> b(2, kFrames);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < kFrames; ++i) b.setSample(c, i, 0.3f * (float) std::sin(2.0 * M_PI * (200.0 + 20.0 * k) * i / 48000.0));
            REQUIRE(w->writeFromAudioSampleBuffer(b, 0, kFrames));
            w.reset();
            dir.getChildFile("i" + juce::String(k) + ".sfz")
                .replaceWithText("<region> sample=s" + juce::String(k) + ".wav lokey=0 hikey=127 pitch_keycenter=60 ampeg_release=0.01\n");
        }
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
        dir.deleteRecursively();
    }
    juce::var call(const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e->call(j.c_str()).c_str()), v);
        return v;
    }
    void waitJob(const juce::var& reply) {
        REQUIRE((bool) reply["ok"]);
        const std::string q = R"({"op":"job.result","jobId":)" + reply["result"]["jobId"].toString().toStdString() + "}";
        for (int i = 0; i < 2000 && call(q)["result"]["status"].toString() == "running"; ++i) {
            e->pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        REQUIRE(call(q)["result"]["status"].toString() == "done");
    }
    void blocks(int n) {
        for (int i = 0; i < n; ++i) {
            float* outs[2] = {L.data(), R.data()};
            dev->render(nullptr, 0, outs, 2, 128);
            e->pump();
        }
    }
    void note(uint16_t type, int n) {
        LeCommand c{};
        c.type = type;
        c.track = 0;
        c.i0 = n;
        c.f0 = 0.8f;
        REQUIRE(e->send(c));
    }
    double memoryMB() { return (double) call(R"({"op":"engine.info"})")["result"]["memoryMB"]; }
};

} // namespace

TEST_CASE("Nạp lười: đổi qua lại 10 nhạc cụ (nốt đang kêu) → bản cũ được nhả, RAM không tăng dần", "[core][memory][instrument]") {
    Rig r;
    std::weak_ptr<const le::dsp::Instrument> prev;
    double afterFirstRound = 0.0;
    for (int round = 0; round < 3; ++round) {
        for (int k = 0; k < kInstruments; ++k) {
            r.waitJob(r.call(R"({"op":"track.setInstrument","track":0,"instrument":{"kind":"sfz","path":"i)" + std::to_string(k) +
                             R"(.sfz"}})"));
            r.blocks(8);   // voice của nhạc cụ trước fade 5 ms → snapshot cũ retire → main delete (ReleasePool)
            INFO("vòng " << round << ", nhạc cụ " << k);
            CHECK(prev.expired());
            prev = r.e->model().tracks[0].instrument;
            REQUIRE_FALSE(prev.expired());
            r.note(LE_CMD_NOTE_ON, 60);   // nốt còn giữ khi đổi nhạc cụ lần sau
            r.blocks(2);
            CHECK(r.e->rt().track(0).activeVoices() == 1);
        }
        if (round == 0) afterFirstRound = r.memoryMB();
    }
    const double afterLast = r.memoryMB();
    INFO("memoryMB sau vòng 1 = " << afterFirstRound << ", sau vòng 3 = " << afterLast);
    std::printf("[instrument-memory] memoryMB sau vòng 1 = %.1f, sau vòng 3 = %.1f (mỗi nhạc cụ ≈ 1.5 MB)\n", afterFirstRound, afterLast);
    if (!kSanitizer && afterFirstRound > 0.0) CHECK(afterLast - afterFirstRound < 6.0);   // giữ lại hết = +30 MB

    // Track về audio → nhạc cụ được nhả
    r.note(LE_CMD_NOTE_OFF, 60);
    REQUIRE((bool) r.call(R"({"op":"track.configure","track":0,"kind":"audio"})")["ok"]);
    r.blocks(8);
    CHECK(prev.expired());
}
