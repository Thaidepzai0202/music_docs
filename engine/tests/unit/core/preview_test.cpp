// 05 §3 preview.play / preview.stop: kênh preview riêng (ngoài 8 track, sau master EQ, trước limiter, −6 dB) ·
// kit → groove K-H-S-H 1 bar (120 khi transport dừng, BPM đang chạy khi phát) · nhạc cụ → arpeggio C-E-G-C ·
// lượt mới / stop → fade (không click) · vé được nhả khi RT thôi dùng · cache LRU 3 · không đụng track / transport.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 128;

// Ghi WAV float 32 mono: x(i), `frames` frame.
template <typename F>
void writeWav(const juce::File& f, int frames, F x) {
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream>(f);
    auto w = wav.createWriterFor(os, juce::AudioFormatWriterOptions{}.withSampleRate(kSr).withNumChannels(1).withBitsPerSample(32));
    REQUIRE(w != nullptr);
    juce::AudioBuffer<float> b(1, frames);
    for (int i = 0; i < frames; ++i) b.setSample(0, i, x(i));
    REQUIRE(w->writeFromAudioSampleBuffer(b, 0, frames));
}

// Burst 50 ms (sine 1 kHz, fade 1 ms hai đầu) biên độ a → kick / snare / hat phân biệt theo biên độ.
float burst(int i, float a) {
    const int n = (int) (0.05 * kSr), f = (int) (0.001 * kSr);
    if (i >= n) return 0.0f;
    const float env = std::min({1.0f, (float) i / (float) f, (float) (n - 1 - i) / (float) f});
    return a * env * (float) std::sin(2.0 * M_PI * 1000.0 * i / kSr);
}

struct Rig {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    juce::File dir;
    std::string lib;
    std::vector<float> L = std::vector<float>(kBlock), R = std::vector<float>(kBlock);
    Rig() {
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-preview", "", false);
        dir.getChildFile("kit").createDirectory();
        writeWav(dir.getChildFile("kit/kick.wav"), 4800, [](int i) { return burst(i, 0.9f); });
        writeWav(dir.getChildFile("kit/snare.wav"), 4800, [](int i) { return burst(i, 0.3f); });
        writeWav(dir.getChildFile("kit/hat.wav"), 4800, [](int i) { return burst(i, 0.08f); });
        dir.getChildFile("kit/kit.sfz").replaceWithText("<group> loop_mode=one_shot\n"
                                                        "<region> key=36 sample=kick.wav\n"
                                                        "<region> key=38 sample=snare.wav\n"
                                                        "<region> key=42 sample=hat.wav\n");
        // C4 = 261.63 Hz ở phím 60, loop liên tục (nốt giữ bao lâu cũng kêu)
        writeWav(dir.getChildFile("tone.wav"), 48000, [](int i) { return 0.5f * (float) std::sin(2.0 * M_PI * 261.6256 * i / kSr); });
        dir.getChildFile("tone.sfz").replaceWithText(
            "<region> sample=tone.wav lokey=0 hikey=127 pitch_keycenter=60 ampeg_release=0.005 ampeg_attack=0.001\n");
        dir.getChildFile("tone2.sfz").replaceWithText("<region> sample=tone.wav lokey=0 hikey=127 pitch_keycenter=48\n");
        writeWav(dir.getChildFile("loop.wav"), 96000, [](int i) { return 0.8f * (float) std::sin(2.0 * M_PI * 500.0 * i / kSr); });
        lib = dir.getFullPathName().toStdString();
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.preferredBufferSize = kBlock;
        cfg.preferredSampleRate = kSr;
        cfg.libraryDir = lib.c_str();
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(cfg, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
        send(LE_CMD_METRONOME, -1, -1, 0);
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
    void send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0, double d0 = 0.0, float f0 = 0.0f) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        c.d0 = d0;
        c.f0 = f0;
        REQUIRE(e->send(c));
    }
    // preview.play; offline: worker đã xong → pump áp kết quả (đưa vé cho RT), block kế tiếp bắt đầu phát.
    void play(const std::string& args) {
        const juce::var r = call(R"({"op":"preview.play",)" + args + "}");
        REQUIRE((bool) r["ok"]);
        e->pump();
        REQUIRE(e->previewJob() == 0);
    }
    // Gọi op trả jobId, chờ job xong (pump như Timer 30Hz), trả job.result.
    juce::var job(const std::string& j) {
        const juce::var reply = call(j);
        REQUIRE((bool) reply["ok"]);
        const std::string q = R"({"op":"job.result","jobId":)" + reply["result"]["jobId"].toString().toStdString() + "}";
        for (int i = 0; i < 2000 && call(q)["result"]["status"].toString() == "running"; ++i) {
            e->pump();
            juce::Thread::sleep(2);
        }
        return call(q)["result"];
    }
    // Render `frames` frame (bội số block), trả kênh L.
    std::vector<float> render(int frames) {
        std::vector<float> out;
        for (int done = 0; done < frames; done += kBlock) {
            float* outs[2] = {L.data(), R.data()};
            dev->render(nullptr, 0, outs, 2, kBlock);
            out.insert(out.end(), L.begin(), L.end());
            e->pump();
        }
        return out;
    }
    LeState state() const {
        LeState s{};
        globalStatePublisher().read(s);
        return s;
    }
};

float peak(const std::vector<float>& x, double fromSec, double toSec) {
    float p = 0.0f;
    const auto a = (std::size_t) (fromSec * kSr), b = std::min(x.size(), (std::size_t) (toSec * kSr));
    for (std::size_t i = a; i < b; ++i) p = std::max(p, std::fabs(x[i]));
    return p;
}

float maxStep(const std::vector<float>& x, double fromSec, double toSec) {
    float p = 0.0f;
    const auto a = std::max<std::size_t>(1, (std::size_t) (fromSec * kSr)), b = std::min(x.size(), (std::size_t) (toSec * kSr));
    for (std::size_t i = a; i < b; ++i) p = std::max(p, std::fabs(x[i] - x[i - 1]));
    return p;
}

// Tần số theo số lần đổi dấu (−→+) trong [from, to)
double freq(const std::vector<float>& x, double fromSec, double toSec) {
    int ups = 0;
    const auto a = (std::size_t) (fromSec * kSr) + 1, b = std::min(x.size(), (std::size_t) (toSec * kSr));
    for (std::size_t i = a; i < b; ++i) ups += (x[i - 1] < 0.0f && x[i] >= 0.0f) ? 1 : 0;
    return ups / (toSec - fromSec);
}

} // namespace

TEST_CASE("preview audio: −6 dB, fade vào / ra không click, hết durationMs thì im và vé được nhả", "[core][preview]") {
    Rig r;
    r.play(R"("source":{"kind":"audio","file":"loop.wav"},"durationMs":500)");
    const auto x = r.render(48000);
    CHECK(std::fabs(peak(x, 0.1, 0.4) - 0.4f) < 0.01f);   // 0.8 × kGain 0.5 (≈ −6 dB), dưới ngưỡng limiter
    CHECK(maxStep(x, 0.0, 0.6) < 0.06f);                  // sine 500 Hz 0.4: bước lớn nhất ≈ 0.026 — không có bậc
    CHECK(peak(x, 0.0, 0.0001) < 0.05f);                  // fade-in 2 ms
    CHECK(peak(x, 0.51, 1.0) == 0.0f);                    // hết durationMs (fade 10 ms nằm TRONG 500 ms)
    CHECK(r.state().playing == 0);                        // không đụng transport
    CHECK(r.e->previewLiveTickets() == 0);
    CHECK(r.e->previewCacheSize() == 1);
}

TEST_CASE("preview kit: groove K-H-S-H 1 bar — 120 BPM khi transport dừng, BPM đang chạy khi transport phát", "[core][preview]") {
    Rig r;
    r.play(R"("source":{"kind":"sfz","path":"kit/kit.sfz"})");
    const auto x = r.render(48000 * 5 / 2);
    const double E = 0.25;   // nốt móc đơn @120
    for (int i = 0; i < 8; ++i) {
        const float hit = peak(x, i * E, i * E + 0.06);
        const float gap = peak(x, i * E + 0.07, (i + 1) * E - 0.005);
        INFO("nốt móc đơn " << i << " hit " << hit << " gap " << gap);
        CHECK(gap == 0.0f);
        // biên độ × kGain × đường cong velocity của Sampler (K 0.9 / S 0.8 / H 0.55)
        if (i % 2 == 1) CHECK((hit > 0.002f && hit < 0.03f));        // hat
        else if (i % 4 == 0) CHECK(hit > 0.15f);                     // kick
        else CHECK((hit > 0.03f && hit < 0.12f));                    // snare
    }
    CHECK(peak(x, 2.0, 2.5) == 0.0f);   // đúng 1 bar
    CHECK(r.e->previewLiveTickets() == 0);

    // Transport chạy 150 BPM: groove theo BPM (nốt móc đơn 0.2 s); transport / track không đổi
    r.send(LE_CMD_SET_BPM, -1, -1, 0, 150.0);
    r.send(LE_CMD_TRANSPORT_PLAY);
    r.render(kBlock * 4);
    r.play(R"("source":{"kind":"sfz","path":"kit/kit.sfz"})");   // cache → không job
    const auto y = r.render(48000 * 2);
    CHECK(peak(y, 0.2, 0.26) > 0.002f);    // hat thứ nhất ở 0.2 s
    CHECK(peak(y, 0.27, 0.395) == 0.0f);
    CHECK(peak(y, 0.4, 0.46) > 0.03f);     // snare ở 0.4 s
    CHECK(peak(y, 1.65, 2.0) == 0.0f);     // 1 bar = 1.6 s
    CHECK(r.state().playing == 1);
    CHECK(r.state().bpm == 150.0);
}

TEST_CASE("preview nhạc cụ: arpeggio C-E-G-C; stop → fade 5 ms không click; lượt mới thay lượt cũ", "[core][preview]") {
    Rig r;
    r.play(R"("source":{"kind":"sfz","path":"tone.sfz"},"note":60,"durationMs":1000)");
    const auto x = r.render(48000 * 3 / 2);
    const double want[4] = {261.63, 329.63, 392.0, 523.25};
    for (int k = 0; k < 4; ++k) {
        const double f = freq(x, k * 0.25 + 0.02, k * 0.25 + 0.23);
        INFO("nốt " << k << ": " << f << " Hz");
        CHECK(std::fabs(f / want[k] - 1.0) < 0.03);
    }
    CHECK(peak(x, 1.05, 1.5) == 0.0f);   // hết durationMs + release 5 ms
    CHECK(r.e->previewLiveTickets() == 0);

    // stop giữa nốt: im sau ≤ 10 ms, không có bậc (sine 0.25 @ 262 Hz: bước ≈ 0.009)
    r.play(R"("source":{"kind":"sfz","path":"tone.sfz"},"durationMs":4000)");
    r.render(kBlock * 40);
    REQUIRE((bool) r.call(R"({"op":"preview.stop"})")["ok"]);
    const auto s = r.render(kBlock * 20);
    CHECK(maxStep(s, 0.0, 0.05) < 0.02f);
    CHECK(peak(s, 0.012, 0.05) == 0.0f);
    CHECK(r.e->previewLiveTickets() == 0);

    // Lượt mới (khác nhạc cụ) khi lượt cũ đang kêu: lượt cũ fade, vé cũ được nhả; chỉ còn vé mới
    r.play(R"("source":{"kind":"sfz","path":"tone.sfz"},"durationMs":4000)");
    r.render(kBlock * 40);
    r.play(R"("source":{"kind":"audio","file":"loop.wav"},"durationMs":4000)");
    const auto n = r.render(kBlock * 40);
    CHECK(maxStep(n, 0.0, 0.1) < 0.06f);
    CHECK(std::fabs(freq(n, 0.03, 0.1) - 500.0) < 20.0);   // chỉ còn loop 500 Hz
    CHECK(r.e->previewLiveTickets() == 1);
    // Lượt mới CÙNG nhạc cụ (cache) cũng dừng lượt cũ
    r.play(R"("source":{"kind":"sfz","path":"tone.sfz"},"note":72,"durationMs":4000)");
    r.render(kBlock * 4);
    r.play(R"("source":{"kind":"sfz","path":"tone.sfz"},"note":60,"durationMs":4000)");
    const auto m = r.render(kBlock * 120);
    CHECK(std::fabs(freq(m, 0.03, 0.3) / 261.63 - 1.0) < 0.03);
    CHECK(r.e->previewLiveTickets() == 1);
    REQUIRE((bool) r.call(R"({"op":"preview.stop"})")["ok"]);
    r.render(kBlock * 8);
    CHECK(r.e->previewLiveTickets() == 0);
}

TEST_CASE("preview: cache LRU 3 mục, stop bỏ lượt đang nạp, lỗi tham số", "[core][preview]") {
    Rig r;
    r.play(R"("source":{"kind":"sfz","path":"tone.sfz"})");
    r.play(R"("source":{"kind":"sfz","path":"kit/kit.sfz"})");
    r.play(R"("source":{"kind":"audio","file":"loop.wav"})");
    r.play(R"("source":{"kind":"sfz","path":"tone2.sfz"})");
    CHECK(r.e->previewCacheSize() == 3);
    r.render(kBlock * 8);
    CHECK(r.e->previewLiveTickets() == 1);   // 3 vé đầu RT chưa kịp lấy → main nhả ngay khi đưa vé mới

    // tone.sfz đã bị đẩy ra (cũ nhất) → job; loop.wav còn trong cache → không job
    REQUIRE((bool) r.call(R"({"op":"preview.play","source":{"kind":"audio","file":"loop.wav"}})")["ok"]);
    CHECK(r.e->previewJob() == 0);
    REQUIRE((bool) r.call(R"({"op":"preview.play","source":{"kind":"sfz","path":"tone.sfz"}})")["ok"]);
    CHECK(r.e->previewJob() != 0);
    // stop trước khi job áp kết quả → không phát (chỉ vào cache)
    REQUIRE((bool) r.call(R"({"op":"preview.stop"})")["ok"]);
    CHECK(r.e->previewJob() == 0);
    const auto x = r.render(kBlock * 80);
    CHECK(peak(x, 0.05, 0.2) == 0.0f);
    CHECK(r.e->previewLiveTickets() == 0);

    CHECK(r.call(R"({"op":"preview.play","source":{"kind":"sfz","path":"nope.sfz"}})")["error"]["code"].toString() == "FILE_NOT_FOUND");
    CHECK(r.call(R"({"op":"preview.play","source":{"kind":"midi","path":"tone.sfz"}})")["error"]["code"].toString() == "INVALID_ARG");
    CHECK(r.call(R"({"op":"preview.play","source":{"kind":"sfz","path":"tone.sfz"},"note":128})")["error"]["code"].toString() == "INVALID_ARG");
    CHECK(r.call(R"({"op":"preview.play","source":{"kind":"audio"}})")["error"]["code"].toString() == "INVALID_ARG");
    CHECK((bool) r.call(R"({"op":"preview.stop"})")["ok"]);   // stop khi không nghe gì: vẫn ok
}

TEST_CASE("preview source.base: \"project\" = bản thu trong project đang mở; mặc định \"library\"", "[core][preview]") {
    Rig r;
    const std::string rec = R"({"op":"preview.play","source":{"kind":"audio","file":"rec.wav","base":"project"},"durationMs":500})";
    CHECK(r.call(rec)["error"]["code"].toString() == "INVALID_ARG");   // chưa project.open
    CHECK(r.call(R"({"op":"preview.play","source":{"kind":"audio","file":"loop.wav","base":"cloud"}})")["error"]["code"].toString() ==
          "INVALID_ARG");
    const juce::File proj = r.dir.getChildFile("proj");
    proj.createDirectory();
    REQUIRE(r.dir.getChildFile("loop.wav").copyFileTo(proj.getChildFile("rec.wav")));
    REQUIRE((bool) r.call(R"({"op":"project.open","dir":")" + proj.getFullPathName().toStdString() + R"("})")["ok"]);
    // mặc định library: rec.wav không có trong thư viện
    CHECK(r.call(R"({"op":"preview.play","source":{"kind":"audio","file":"rec.wav"}})")["error"]["code"].toString() == "FILE_NOT_FOUND");
    r.play(R"("source":{"kind":"audio","file":"rec.wav","base":"project"},"durationMs":500)");
    const auto x = r.render(48000 * 3 / 4);
    CHECK(std::fabs(peak(x, 0.1, 0.4) - 0.4f) < 0.01f);
    CHECK(peak(x, 0.51, 0.75) == 0.0f);
    // library tường minh vẫn như mặc định
    r.play(R"("source":{"kind":"audio","file":"loop.wav","base":"library"},"durationMs":200)");
    CHECK(peak(r.render(48000 / 4), 0.05, 0.15) > 0.3f);
}

TEST_CASE("preview nhánh rảnh: nghe thử xong → output bit-exact như engine chưa từng nghe thử", "[core][preview]") {
    Rig a, b;
    a.play(R"("source":{"kind":"sfz","path":"kit/kit.sfz"})");
    CHECK(peak(a.render(48000 * 5 / 2), 0.0, 2.0) > 0.15f);   // groove đã phát
    b.render(48000 * 5 / 2);
    CHECK(a.e->previewLiveTickets() == 0);
    for (Rig* r : {&a, &b}) {
        r->send(LE_CMD_METRONOME, -1, -1, 1, 0.0, 0.8f);
        r->send(LE_CMD_TRANSPORT_PLAY);
    }
    const auto x = a.render(48000 * 2), y = b.render(48000 * 2);
    REQUIRE(peak(y, 0.0, 2.0) > 0.0f);   // metronome kêu
    REQUIRE(x.size() == y.size());
    std::size_t diff = 0;
    for (std::size_t i = 0; i < x.size(); ++i) diff += x[i] != y[i] ? 1 : 0;
    CHECK(diff == 0);
}

TEST_CASE("track.setInstrument dùng lại nhạc cụ trong cache preview: CHUNG Instrument, job.result y như nạp mới", "[core][preview][instrument]") {
    Rig r;
    // loop_sustain chưa hỗ trợ → SfzLoader cảnh báo: warnings phải giữ nguyên khi lấy từ cache
    r.dir.getChildFile("warn.sfz").replaceWithText("<region> sample=tone.wav lokey=0 hikey=127 pitch_keycenter=60 loop_mode=loop_sustain\n");
    const auto fresh = r.job(R"({"op":"track.setInstrument","track":1,"instrument":{"kind":"sfz","path":"warn.sfz"}})");
    REQUIRE(fresh["status"].toString() == "done");
    REQUIRE(fresh["result"]["warnings"].size() >= 1);
    const auto* freshInst = r.e->model().tracks[1].instrument.get();

    r.play(R"("source":{"kind":"sfz","path":"warn.sfz"},"durationMs":200)");   // Browser nghe thử → cache
    const auto* cachedInst = r.e->previewCachedInstrument(0);
    REQUIRE(cachedInst != nullptr);
    CHECK(cachedInst != freshInst);

    const auto hit = r.job(R"({"op":"track.setInstrument","track":0,"instrument":{"kind":"sfz","path":"warn.sfz"}})");
    REQUIRE(hit["status"].toString() == "done");
    CHECK(r.e->model().tracks[0].instrument.get() == cachedInst);   // dùng chung, không nạp lại
    CHECK(juce::JSON::toString(hit["result"]) == juce::JSON::toString(fresh["result"]));

    r.render(48000 / 2);   // lượt nghe thử 200 ms đã xong
    CHECK(r.e->previewLiveTickets() == 0);
    r.send(LE_CMD_NOTE_ON, 0, -1, 60, 0.0, 0.8f);   // track phát được bằng bản dùng chung
    CHECK(peak(r.render(4800), 0.0, 0.1) > 0.05f);
    r.send(LE_CMD_NOTE_OFF, 0, -1, 60);

    // Vòng đời: track đổi nhạc cụ → cache preview còn giữ; cache đẩy ra (3 mục khác) + snapshot retire → nhả
    std::weak_ptr<const le::dsp::Instrument> w = r.e->model().tracks[0].instrument;
    REQUIRE(r.job(R"({"op":"track.setInstrument","track":0,"instrument":{"kind":"sfz","path":"tone.sfz"}})")["status"].toString() == "done");
    r.render(kBlock * 8);
    CHECK_FALSE(w.expired());
    r.play(R"("source":{"kind":"sfz","path":"kit/kit.sfz"},"durationMs":100)");
    r.play(R"("source":{"kind":"audio","file":"loop.wav"},"durationMs":100)");
    r.play(R"("source":{"kind":"sfz","path":"tone2.sfz"},"durationMs":100)");
    r.render(48000 / 2);
    CHECK(r.e->previewCacheSize() == 3);
    CHECK(w.expired());
}
