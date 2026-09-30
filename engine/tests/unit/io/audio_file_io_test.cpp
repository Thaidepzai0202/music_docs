// P1-11: AudioFileIO + job clip.setAudio; P1-27 (phần engine): job track.setInstrument.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "core/Engine.h"
#include "io/AudioFileIO.h"
#include "io/OfflineDeviceIO.h"

using namespace le;

namespace {
struct TempDir {
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-audiofile", "", false);
    TempDir() { dir.createDirectory(); }
    ~TempDir() { dir.deleteRecursively(); }
    std::string path(const char* name) const { return dir.getChildFile(name).getFullPathName().toStdString(); }
};

// Ghi file bằng JUCE: kênh c = sine (200 + 100c Hz) biên độ 0.5, `frames` frame.
bool writeFile(juce::AudioFormat& fmt, const std::string& path, int channels, int frames, double sr, int bits) {
    juce::File f(juce::String::fromUTF8(path.c_str()));
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream>(f);
    auto w = fmt.createWriterFor(os, juce::AudioFormatWriterOptions{}.withSampleRate(sr).withNumChannels(channels).withBitsPerSample(bits));
    if (w == nullptr) return false;
    juce::AudioBuffer<float> b(channels, frames);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < frames; ++i) b.setSample(c, i, 0.5f * (float) std::sin(2.0 * M_PI * (200.0 + 100.0 * c) * i / sr));
    return w->writeFromAudioSampleBuffer(b, 0, frames);
}

struct Offline {
    io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<core::Engine> e;
    explicit Offline(const std::string& libraryDir = {}) {
        LeConfig c{};
        c.apiVersion = LE_API_VERSION;
        c.preferredSampleRate = 48000.0;
        c.preferredBufferSize = 128;
        c.libraryDir = libraryDir.empty() ? nullptr : libraryDir.c_str();
        auto d = std::make_unique<io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<core::Engine>(c, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
    }
    juce::var call(const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e->call(j.c_str()).c_str()), v);
        return v;
    }
    // Chờ job xong (pump như Timer 30Hz), trả job.result.result
    juce::var wait(juce::int64 id) {
        for (int i = 0; i < 500; ++i) {
            e->pump();
            auto r = call("{\"op\":\"job.result\",\"jobId\":" + std::to_string(id) + "}")["result"];
            if (r["status"].toString() != "running") return r;
            juce::Thread::sleep(10);
        }
        return {};
    }
    void render(int blocks = 1) {
        std::vector<float> L(128), R(128);
        float* o[2] = {L.data(), R.data()};
        for (int i = 0; i < blocks; ++i) dev->render(nullptr, 0, o, 2, 128);
    }
};

struct Events {
    static std::vector<std::pair<int32_t, int32_t>>& v() { static std::vector<std::pair<int32_t, int32_t>> x; return x; }
    static void cb(int32_t type, int32_t a, int32_t, int64_t, double) { v().push_back({type, a}); }
    Events() { v().clear(); core::setEventCallback(&cb); }
    ~Events() { core::setEventCallback(nullptr); }
};
} // namespace

TEST_CASE("decodeAudioFile: WAV/AIFF/FLAC, mono/stereo, giữ sample rate", "[io][audiofile]") {
    TempDir td;
    juce::WavAudioFormat wav;
    juce::AiffAudioFormat aiff;
    juce::FlacAudioFormat flac;
    struct Case { juce::AudioFormat* fmt; const char* name; int ch; double sr; int bits; };
    const Case cases[] = {{&wav, "m.wav", 1, 44100.0, 24}, {&wav, "s.wav", 2, 48000.0, 16}, {&aiff, "s.aiff", 2, 48000.0, 24},
                          {&flac, "m.flac", 1, 96000.0, 24}, {&flac, "s.flac", 2, 44100.0, 16}};
    for (const auto& c : cases) {
        INFO(c.name);
        REQUIRE(writeFile(*c.fmt, td.path(c.name), c.ch, 4410, c.sr, c.bits));
        const auto r = io::decodeAudioFile(td.path(c.name));
        REQUIRE(r.error == LE_OK);
        REQUIRE(r.data != nullptr);
        REQUIRE(r.data->numChannels() == c.ch);
        REQUIRE(r.data->numFrames() == 4410);
        REQUIRE(r.data->sampleRate() == c.sr);
        const float expect = 0.5f * (float) std::sin(2.0 * M_PI * 200.0 * 100 / c.sr);
        REQUIRE(r.data->channel(0)[100] == Catch::Approx(expect).margin(c.bits == 16 ? 1e-4 : 1e-6));
        REQUIRE(r.data->channel(0)[-1] == 0.0f);   // vùng đệm của AudioData
    }
}

// 06 §4: sample thư viện là FLAC 24-bit / 48 kHz. registerBasicFormats() có FLAC vì JUCE_USE_FLAC mặc định 1 (CMake
// không tắt). Round-trip: MỌI mẫu khớp trong 1 LSB 24-bit; sine + nhiễu để predictor FLAC làm việc thật.
#if !JUCE_USE_FLAC
#error "JUCE_USE_FLAC phải bật: thư viện nhạc cụ dùng FLAC (06 §4)"
#endif
TEST_CASE("FLAC 24-bit/48k stereo: round-trip từng mẫu ≤ 1 LSB; kit SFZ dùng FLAC nạp được; đo thời gian decode", "[io][audiofile][flac]") {
    TempDir td;
    juce::FlacAudioFormat flac;
    auto write = [&](const std::string& path, int frames, std::uint32_t seed, juce::AudioBuffer<float>* keep) {
        juce::File f(juce::String::fromUTF8(path.c_str()));
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream>(f);
        auto w = flac.createWriterFor(os, juce::AudioFormatWriterOptions{}.withSampleRate(48000.0).withNumChannels(2).withBitsPerSample(24));
        REQUIRE(w != nullptr);
        juce::AudioBuffer<float> b(2, frames);
        std::uint32_t x = seed;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < frames; ++i) {
                x = x * 1664525u + 1013904223u;
                const float noise = ((float) (x >> 8) / 16777216.0f - 0.5f) * 0.2f;
                b.setSample(c, i, 0.6f * (float) std::sin(2.0 * M_PI * (110.0 + 55.0 * c) * i / 48000.0) + noise);
            }
        REQUIRE(w->writeFromAudioSampleBuffer(b, 0, frames));
        if (keep != nullptr) *keep = b;
    };

    const int frames = 48000 * 10;
    juce::AudioBuffer<float> src;
    write(td.path("long.flac"), frames, 1u, &src);
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const auto r = io::decodeAudioFile(td.path("long.flac"));
    const double decodeMs = juce::Time::getMillisecondCounterHiRes() - t0;
    REQUIRE(r.error == LE_OK);
    REQUIRE(r.data->numChannels() == 2);
    REQUIRE(r.data->numFrames() == frames);
    REQUIRE(r.data->sampleRate() == 48000.0);
    float worst = 0.0f;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < frames; ++i) worst = std::max(worst, std::fabs(r.data->channel(c)[i] - src.getSample(c, i)));
    CHECK(worst <= 1.0f / 8388608.0f * 1.01f);   // 1 LSB 24-bit (lượng tử hoá lúc ghi)

    // Kit 16 pad FLAC (0.5 s mỗi pad) qua track.setInstrument — đúng đường nạp của thư viện
    std::string sfz = "<group> loop_mode=one_shot\n";
    for (int k = 0; k < 16; ++k) {
        write(td.path(("pad" + std::to_string(k) + ".flac").c_str()), 24000, 100u + (std::uint32_t) k, nullptr);
        sfz += "<region> key=" + std::to_string(36 + k) + " sample=pad" + std::to_string(k) + ".flac\n";
    }
    juce::File(juce::String(td.path("kit.sfz"))).replaceWithText(sfz);
    Offline o(td.dir.getFullPathName().toStdString());
    const double t1 = juce::Time::getMillisecondCounterHiRes();
    const auto job = o.wait((juce::int64) o.call(R"({"op":"track.setInstrument","track":0,"instrument":{"kind":"sfz","path":"kit.sfz"}})")["result"]["jobId"]);
    const double kitMs = juce::Time::getMillisecondCounterHiRes() - t1;
    REQUIRE(job["status"].toString() == "done");
    REQUIRE((int) job["result"]["samplesLoaded"] == 16);
    const auto& inst = o.e->model().tracks[0].instrument;
    REQUIRE(inst != nullptr);
    REQUIRE(inst->zones.size() == 16);
    REQUIRE(inst->zones[0].data->numFrames() == 24000);

    std::printf("[flac] decode 10 s stereo 24-bit/48k: %.1f ms (%.0f× realtime); kit 16 pad × 0.5 s qua job: %.1f ms\n",
                decodeMs, 10000.0 / std::max(decodeMs, 0.001), kitMs);
}

TEST_CASE("decodeAudioFile: > 2 kênh trộn về 2 (chẵn → trái, lẻ → phải)", "[io][audiofile]") {
    TempDir td;
    juce::WavAudioFormat wav;
    REQUIRE(writeFile(wav, td.path("quad.wav"), 4, 1000, 48000.0, 32));
    const auto r = io::decodeAudioFile(td.path("quad.wav"));
    REQUIRE(r.error == LE_OK);
    REQUIRE(r.data->numChannels() == 2);
    const int i = 123;
    const float c0 = 0.5f * (float) std::sin(2.0 * M_PI * 200.0 * i / 48000.0);
    const float c2 = 0.5f * (float) std::sin(2.0 * M_PI * 400.0 * i / 48000.0);
    REQUIRE(r.data->channel(0)[i] == Catch::Approx((c0 + c2) / 2).margin(1e-6));
}

TEST_CASE("decodeAudioFile: thiếu → FILE_NOT_FOUND, hỏng → FILE_FORMAT", "[io][audiofile]") {
    TempDir td;
    REQUIRE(io::decodeAudioFile(td.path("nope.wav")).error == LE_ERR_FILE_NOT_FOUND);
    REQUIRE(io::decodeAudioFile("relative.wav").error == LE_ERR_FILE_NOT_FOUND);
    juce::File(juce::String(td.path("junk.wav"))).replaceWithText("this is not audio at all, just text");
    const auto r = io::decodeAudioFile(td.path("junk.wav"));
    REQUIRE(r.error == LE_ERR_FILE_FORMAT);
    REQUIRE_FALSE(r.message.empty());
    REQUIRE(r.data == nullptr);
}

TEST_CASE("clip.setAudio: job decode → model + snapshot, JOB_DONE; lỗi đúng mã", "[io][audiofile][job]") {
    TempDir td;
    juce::WavAudioFormat wav;
    REQUIRE(writeFile(wav, td.path("loop.wav"), 2, 48000, 48000.0, 24));
    juce::File(juce::String(td.path("bad.wav"))).replaceWithText("garbage");
    Events ev;
    Offline o;

    // Tham số sai: đồng bộ
    REQUIRE(o.call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"a","file":"x.wav","lengthBeats":4,"originalBpm":120})")
                ["error"]["code"].toString() == "INVALID_ARG");   // tương đối, chưa project.open
    REQUIRE(o.call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"a","file":"/x.wav","lengthBeats":4,"originalBpm":500})")
                ["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(o.call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"a","file":"/x.wav","lengthBeats":4,"originalBpm":120,"warp":"fast"})")
                ["error"]["code"].toString() == "INVALID_ARG");

    // OK
    auto v = o.call(R"({"op":"clip.setAudio","track":1,"slot":2,"clipId":"loop","file":")" + td.path("loop.wav") +
                    R"(","lengthBeats":4,"originalBpm":120,"warp":"stretch","gainDb":-3})");
    REQUIRE((bool) v["ok"]);
    auto r = o.wait((juce::int64) v["result"]["jobId"]);
    REQUIRE(r["status"].toString() == "done");
    REQUIRE((int) r["result"]["frames"] == 48000);
    REQUIRE((int) r["result"]["channels"] == 2);
    const auto info = o.call(R"({"op":"clip.info","track":1,"slot":2})")["result"];
    REQUIRE(info["kind"].toString() == "audio");
    REQUIRE(info["warp"].toString() == "stretch");
    REQUIRE((double) info["gainDb"] == -3.0);
    o.render();
    const auto* snap = o.e->rt().currentSnapshot();
    REQUIRE(snap->clips[1][2].kind == core::ClipKind::Audio);
    REQUIRE(snap->clips[1][2].audio->numFrames() == 48000);
    bool done = false;
    for (auto& e : Events::v()) done |= e.first == LE_EVT_JOB_DONE;
    REQUIRE(done);

    // Thiếu file / file hỏng: job failed với đúng mã + JOB_FAILED(a = mã)
    v = o.call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"m","file":")" + td.path("missing.wav") + R"(","lengthBeats":4,"originalBpm":120})");
    r = o.wait((juce::int64) v["result"]["jobId"]);
    REQUIRE(r["status"].toString() == "failed");
    REQUIRE(r["error"]["code"].toString() == "FILE_NOT_FOUND");
    v = o.call(R"({"op":"clip.setAudio","track":0,"slot":1,"clipId":"b","file":")" + td.path("bad.wav") + R"(","lengthBeats":4,"originalBpm":120})");
    r = o.wait((juce::int64) v["result"]["jobId"]);
    REQUIRE(r["error"]["code"].toString() == "FILE_FORMAT");
    bool failedNotFound = false, failedFormat = false;
    for (auto& e : Events::v()) {
        failedNotFound |= e.first == LE_EVT_JOB_FAILED && e.second == LE_ERR_FILE_NOT_FOUND;
        failedFormat |= e.first == LE_EVT_JOB_FAILED && e.second == LE_ERR_FILE_FORMAT;
    }
    REQUIRE(failedNotFound);
    REQUIRE(failedFormat);
    REQUIRE(o.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["kind"].toString() == "empty");
}

TEST_CASE("clip.setAudio: clip.clear trong lúc decode → kết quả bị bỏ (JOB_CANCELLED)", "[io][audiofile][job]") {
    TempDir td;
    juce::WavAudioFormat wav;
    REQUIRE(writeFile(wav, td.path("a.wav"), 1, 480000, 48000.0, 24));
    Offline o;
    auto v = o.call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"a","file":")" + td.path("a.wav") + R"(","lengthBeats":4,"originalBpm":120})");
    o.call(R"({"op":"clip.clear","track":0,"slot":0})");   // trước khi pump áp kết quả
    const auto r = o.wait((juce::int64) v["result"]["jobId"]);
    REQUIRE(r["status"].toString() == "failed");
    REQUIRE(r["error"]["code"].toString() == "JOB_CANCELLED");
    REQUIRE(o.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["kind"].toString() == "empty");
}

TEST_CASE("track.setInstrument sfz: SfzLoader + decodeAudioFile qua job → snapshot có Instrument", "[io][sfz][job]") {
    TempDir td;
    juce::WavAudioFormat wav;
    td.dir.getChildFile("kit").createDirectory();
    REQUIRE(writeFile(wav, td.dir.getChildFile("kit/kick.wav").getFullPathName().toStdString(), 1, 4800, 44100.0, 24));
    REQUIRE(writeFile(wav, td.dir.getChildFile("kit/snare.wav").getFullPathName().toStdString(), 1, 4800, 44100.0, 24));
    td.dir.getChildFile("kit/kit.sfz").replaceWithText(
        "<group> loop_mode=one_shot\n<region> sample=kick.wav key=36\n<region> sample=snare.wav key=38\n");
    td.dir.getChildFile("kit/broken.sfz").replaceWithText("<region> sample=missing.wav key=40\n");

    Offline o(td.dir.getFullPathName().toStdString());   // libraryDir → path tương đối
    // kind "user" (P3-05): id chưa đăng ký bằng instrument.createFromRecording → INVALID_ARG (capture_test kiểm đường đúng)
    REQUIRE(o.call(R"({"op":"track.setInstrument","track":2,"instrument":{"kind":"user","id":"x"}})")["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(o.call(R"({"op":"track.setInstrument","track":2,"instrument":{"kind":"sfz"}})")["error"]["code"].toString() == "INVALID_ARG");

    auto v = o.call(R"({"op":"track.setInstrument","track":2,"instrument":{"kind":"sfz","path":"kit/kit.sfz"}})");
    REQUIRE((bool) v["ok"]);
    auto r = o.wait((juce::int64) v["result"]["jobId"]);
    REQUIRE(r["status"].toString() == "done");
    REQUIRE((int) r["result"]["regions"] == 2);
    REQUIRE((int) r["result"]["samplesLoaded"] == 2);
    REQUIRE(o.e->model().tracks[2].kind == core::TrackKind::Instrument);
    o.render();
    const auto* inst = o.e->rt().currentSnapshot()->tracks[2].instrument.get();
    REQUIRE(inst != nullptr);
    REQUIRE(inst->zones.size() == 2);
    REQUIRE(inst->findZone(38, 100) != nullptr);
    REQUIRE(inst->findZone(38, 100)->data->sampleRate() == 44100.0);

    v = o.call(R"({"op":"track.setInstrument","track":3,"instrument":{"kind":"sfz","path":"kit/broken.sfz"}})");
    r = o.wait((juce::int64) v["result"]["jobId"]);
    REQUIRE(r["status"].toString() == "failed");
    REQUIRE(r["error"]["code"].toString() == "FILE_NOT_FOUND");
    v = o.call(R"({"op":"track.setInstrument","track":3,"instrument":{"kind":"sfz","path":"kit/none.sfz"}})");
    REQUIRE(o.wait((juce::int64) v["result"]["jobId"])["error"]["code"].toString() == "FILE_NOT_FOUND");
}

TEST_CASE("Đường dẫn kiểu app thật: libraryDir = flutter_assets/assets/library, clip.setAudio audio/<id>.wav theo project",
          "[io][sfz][paths]") {
    TempDir td;
    // Giống bundle iOS: <bundle>/Frameworks/App.framework/flutter_assets/assets/library (06 §1)
    const juce::File lib = td.dir.getChildFile("Runner.app/Frameworks/App.framework/flutter_assets/assets/library");
    const juce::File src(juce::String(LE_TEST_FIXTURES_DIR) + "/kit_synth");
    REQUIRE(src.isDirectory());
    REQUIRE(src.copyDirectoryTo(lib.getChildFile("kits/kit_synth")));
    const juce::File project = td.dir.getChildFile("Documents/projects/p1");
    project.getChildFile("audio").createDirectory();
    juce::WavAudioFormat wav;
    REQUIRE(writeFile(wav, project.getChildFile("audio/c_42.wav").getFullPathName().toStdString(), 2, 48000, 48000.0, 32));

    Offline o(lib.getFullPathName().toStdString());
    REQUIRE((bool) o.call(R"({"op":"project.open","dir":")" + project.getFullPathName().toStdString() + R"("})")["ok"]);
    auto v = o.call(R"({"op":"track.setInstrument","track":0,"instrument":{"kind":"sfz","path":"kits/kit_synth/kit_synth.sfz"}})");
    auto r = o.wait((juce::int64) v["result"]["jobId"]);
    REQUIRE(r["status"].toString() == "done");
    REQUIRE((int) r["result"]["regions"] == 4);
    REQUIRE(r["result"]["warnings"].size() == 0);

    v = o.call(R"({"op":"clip.setAudio","track":1,"slot":0,"clipId":"c_42","file":"audio/c_42.wav","lengthBeats":4,"originalBpm":120})");
    r = o.wait((juce::int64) v["result"]["jobId"]);
    REQUIRE(r["status"].toString() == "done");
    REQUIRE((int) r["result"]["channels"] == 2);
    REQUIRE(o.call(R"({"op":"clip.info","track":1,"slot":0})")["result"]["file"].toString() == "audio/c_42.wav");
}

TEST_CASE("clip.setParams trên clip audio: snapshot giữ CÙNG AudioData (không decode lại), gain/warp đổi", "[io][audiofile][ops]") {
    TempDir td;
    juce::WavAudioFormat wav;
    REQUIRE(writeFile(wav, td.path("g.wav"), 1, 4800, 48000.0, 24));
    Offline o;
    auto v = o.call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"g","file":")" + td.path("g.wav") + R"(","lengthBeats":1,"originalBpm":120})");
    REQUIRE(o.wait((juce::int64) v["result"]["jobId"])["status"].toString() == "done");
    o.render();
    const auto* before = o.e->rt().currentSnapshot()->clips[0][0].audio.get();
    REQUIRE((bool) o.call(R"({"op":"clip.setParams","track":0,"slot":0,"gainDb":-6,"warp":"stretch"})")["ok"]);
    o.render();
    const auto& c = o.e->rt().currentSnapshot()->clips[0][0];
    REQUIRE(c.audio.get() == before);
    REQUIRE(c.gainDb == -6.0f);
    REQUIRE(c.warp == core::WarpMode::Stretch);
}

TEST_CASE("P1-24 le_get_peaks: min/max đúng với file sine, maxPairs nhỏ → đúng số cặp, có cache .peaks", "[io][peaks]") {
    TempDir td;
    const juce::File project = td.dir.getChildFile("proj");
    project.getChildFile("audio").createDirectory();
    juce::WavAudioFormat wav;
    REQUIRE(writeFile(wav, project.getChildFile("audio/s.wav").getFullPathName().toStdString(), 1, 48000, 48000.0, 32));   // sine 200 Hz, 0.5
    Offline o;
    REQUIRE((bool) o.call(R"({"op":"project.open","dir":")" + project.getFullPathName().toStdString() + R"("})")["ok"]);
    auto v = o.call(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"s1","file":"audio/s.wav","lengthBeats":4,"originalBpm":120})");
    REQUIRE(o.wait((juce::int64) v["result"]["jobId"])["status"].toString() == "done");   // peaks sẵn sàng khi JOB_DONE

    std::vector<float> buf(2000, 99.0f);
    const int n0 = o.e->getPeaks("s1", 0, buf.data(), 1000);
    REQUIRE(n0 == 188);   // ceil(48000 / 256)
    for (int i = 0; i < n0 - 1; ++i) {   // chu kỳ 240 sample < 256 → mỗi điểm chứa trọn 1 chu kỳ
        REQUIRE(buf[(size_t) (2 * i)] == Catch::Approx(-0.5f).margin(2e-3));
        REQUIRE(buf[(size_t) (2 * i + 1)] == Catch::Approx(0.5f).margin(2e-3));
    }
    REQUIRE(o.e->getPeaks("s1", 0, buf.data(), 10) == 10);    // maxPairs nhỏ hơn dữ liệu
    REQUIRE(o.e->getPeaks("s1", 2, buf.data(), 100) == 3);    // ceil(48000 / 16384)
    REQUIRE(o.e->getPeaks("s1", 1, buf.data(), 0) == 0);
    REQUIRE(project.getChildFile("cache/s1.peaks").existsAsFile());
}

TEST_CASE("P1-24: take vừa thu có peaks khi RECORDING_FINISHED", "[io][peaks][recorder]") {
    TempDir td;
    Offline o;
    REQUIRE((bool) o.call(R"({"op":"project.open","dir":")" + td.dir.getFullPathName().toStdString() + R"("})")["ok"]);
    LeCommand c{};
    c.type = LE_CMD_CLIP_RECORD;
    c.track = 0;
    c.slot = 0;
    c.i0 = 1;
    REQUIRE(o.e->send(c));
    o.render(96000 / 128 + 8);
    std::string id;
    for (int i = 0; i < 200 && id.empty(); ++i) {
        o.e->pump();
        const auto info = o.call(R"({"op":"clip.info","track":0,"slot":0})")["result"];
        if (info["kind"].toString() == "audio") id = info["clipId"].toString().toStdString();
        juce::Thread::sleep(5);
    }
    REQUIRE_FALSE(id.empty());
    std::vector<float> buf(1000);
    int n = 0;
    for (int i = 0; i < 400 && n == 0; ++i) {   // job ghi file + peaks chạy trên worker
        o.e->pump();
        n = o.e->getPeaks(id.c_str(), 0, buf.data(), 500);
        juce::Thread::sleep(5);
    }
    REQUIRE(n == (96000 + 144 + 255) / 256);   // take = 1 bar + đuôi 3 ms
}
