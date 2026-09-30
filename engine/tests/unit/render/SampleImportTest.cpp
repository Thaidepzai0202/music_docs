// P2-35 đợt B — render/SampleImport trên nguồn TỔNG HỢP (biết trước đáp án): tên nốt, onset, resample 44.1 → 48 kHz,
// loop crossfade liền mạch, đo tune bằng Yin, sửa tên lệch 1 nửa cung, zone phủ kín, lớp dùng chung file, decay cắt
// đuôi, cân loudness, SFZ nạp được, tất định.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"
#include "io/SfzLoader.h"
#include "render/SampleImport.h"
#include "render/Yin.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace le;
namespace imp = le::render::import;

namespace {

constexpr double kTwoPi = 6.283185307179586;

// Tone "nhạc cụ" có vibrato (không tuần hoàn khít), attack 30 ms, sustain, nhả 0.3 s cuối.
std::vector<float> bowed(double hz, double sec, double amp, double sr = 48000.0, double vibCents = 20.0) {
    std::vector<float> x(static_cast<size_t>(sec * sr));
    double ph = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        const double t = static_cast<double>(i) / sr;
        const double f = hz * std::pow(2.0, vibCents / 1200.0 * std::sin(kTwoPi * 5.3 * t));
        ph += kTwoPi * f / sr;
        const double env = std::min(1.0, t / 0.03) * std::min(1.0, (sec - t) / 0.3);
        x[i] = static_cast<float>(amp * env * (std::sin(ph) + 0.4 * std::sin(2.0 * ph) + 0.2 * std::sin(3.0 * ph)));
    }
    return x;
}
std::vector<float> plucked(double hz, double sec, double amp, double sr = 48000.0) {
    std::vector<float> x(static_cast<size_t>(sec * sr));
    for (size_t i = 0; i < x.size(); ++i) {
        const double t = static_cast<double>(i) / sr;
        x[i] = static_cast<float>(amp * std::exp(-t / 0.4) * (std::sin(kTwoPi * hz * t) + 0.3 * std::sin(kTwoPi * 2 * hz * t)));
    }
    return x;
}
double hzOf(int note, double cents = 0.0) { return 440.0 * std::pow(2.0, (note - 69 + cents / 100.0) / 12.0); }

imp::SourceSample src(const std::string& name, int note, int layer, std::vector<float> x) {
    imp::SourceSample s;
    s.name = name;
    s.nameNote = note;
    s.layer = layer;
    std::vector<float> pad(4800, 0.0f);   // 100 ms lặng đầu — importer phải cắt
    pad.insert(pad.end(), x.begin(), x.end());
    s.left = std::move(pad);
    s.sampleRate = 48000.0;
    return s;
}

struct Loader {
    std::map<std::string, dsp::AudioDataPtr> files;
    explicit Loader(const imp::ImportResult& r) {
        for (const auto& z : r.zones) {
            if (z.shared) continue;
            auto d = std::make_shared<dsp::AudioData>(1, static_cast<int64_t>(z.left.size()), z.sampleRate);
            std::copy(z.left.begin(), z.left.end(), d->writePointer(0));
            files["/imp/samples/" + z.file] = d;
        }
    }
    io::SfzLoadOptions options() const {
        io::SfzLoadOptions o;
        o.loadSample = [this](const std::string& p) {
            const auto it = files.find(p);
            return it == files.end() ? io::DecodeResult{LE_ERR_FILE_NOT_FOUND, "thiếu " + p, nullptr} : io::DecodeResult{LE_OK, "", it->second};
        };
        return o;
    }
};

} // namespace

TEST_CASE("SampleImport: noteFromName theo quy ước C4 = 60", "[render][import]") {
    CHECK(imp::noteFromName("C4") == 60);
    CHECK(imp::noteFromName("A4") == 69);
    CHECK(imp::noteFromName("A#2") == 46);
    CHECK(imp::noteFromName("Db4") == 61);
    CHECK(imp::noteFromName("C-1") == 0);
    CHECK(imp::noteFromName("G9") == 127);
    CHECK(imp::noteFromName("a0") == 21);
    for (const char* bad : {"", "C", "H3", "C#", "C4v", "ArcoVib", "mf", "A10"}) {
        CAPTURE(bad);
        CHECK(imp::noteFromName(bad) == -1);
    }
}

TEST_CASE("SampleImport: findOnset lùi 2 ms trước tiếng đầu tiên", "[render][import]") {
    std::vector<float> x(48000, 0.0f);
    for (size_t i = 10000; i < x.size(); ++i) x[i] = 0.5f * static_cast<float>(std::sin(0.05 * static_cast<double>(i)));
    x[500] = 0.001f;   // nhiễu nhỏ dưới ngưỡng −40 dB → bỏ qua
    const int64_t on = imp::findOnset(x, 48000.0);
    CHECK(on >= 10000 - 96 - 20);
    CHECK(on <= 10000 - 96 + 20);
}

TEST_CASE("SampleImport: resample 44.1 → 48 kHz giữ tần số và biên độ, sai số < −70 dB", "[render][import]") {
    std::vector<float> in(44100);
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * static_cast<float>(std::sin(kTwoPi * 1000.0 * static_cast<double>(i) / 44100.0));
    const auto out = imp::resample(in, 44100, 48000);
    REQUIRE(out.size() == 48000);
    double err = 0.0, ref = 0.0;
    for (size_t i = 2400; i < 45600; ++i) {   // bỏ 50 ms hai đầu (biên bộ lọc)
        const double want = 0.5 * std::sin(kTwoPi * 1000.0 * static_cast<double>(i) / 48000.0);
        err += (out[i] - want) * (out[i] - want);
        ref += want * want;
    }
    CHECK(10.0 * std::log10(err / ref) < -70.0);
    // dải thông: 15 kHz vẫn qua (≥ −0.5 dB)
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.5f * static_cast<float>(std::sin(kTwoPi * 15000.0 * static_cast<double>(i) / 44100.0));
    const auto hi = imp::resample(in, 44100, 48000);
    double e = 0.0;
    for (size_t i = 2400; i < 45600; ++i) e += static_cast<double>(hi[i]) * hi[i];
    CHECK(10.0 * std::log10(e / 43200.0 / 0.125) > -0.5);
    CHECK(imp::resample(in, 48000, 48000) == in);   // cùng tần số: giữ nguyên
}

TEST_CASE("SampleImport: Sustain — loop crossfade liền mạch, tune đo bằng Yin, zone phủ kín, loudness cân", "[render][import]") {
    // 3 nốt × 2 lớp, lệch +15 cent, lớp mạnh to gấp 3
    std::vector<imp::SourceSample> s;
    for (int note : {55, 58, 62})
        for (int layer : {1, 3}) s.push_back(src("vln_" + std::to_string(note) + "_v" + std::to_string(layer), note, layer, bowed(hzOf(note, 15.0), 4.0, layer == 1 ? 0.2 : 0.6)));
    imp::ImportSpec spec;
    spec.id = "inst_test";
    spec.mode = imp::Mode::Sustain;
    const auto r = imp::importInstrument(spec, s);
    INFO(r.error);
    REQUIRE(r.ok);
    REQUIRE(r.zones.size() == 6);
    CHECK(r.rangeLo == 53);
    CHECK(r.rangeHi == 64);
    double lo = 1e9, hi = -1e9;
    for (const auto& z : r.zones) {
        CAPTURE(z.file, z.measuredCents, z.loopStart, z.loopEnd);
        CHECK(z.pitchMeasured);
        CHECK(z.tuneCents == Catch::Approx(-15.0).margin(3.0));
        REQUIRE(z.loopEnd == static_cast<int64_t>(z.left.size()));
        CHECK(z.loopEnd - z.loopStart >= static_cast<int64_t>(0.8 * 48000));
        CHECK(z.loopEnd <= static_cast<int64_t>(spec.maxSustainSec * 48000) + 1);
        const size_t e = static_cast<size_t>(z.loopEnd), st = static_cast<size_t>(z.loopStart);
        CHECK(z.left[e - 1] == z.left[st - 1]);          // crossfade: mẫu cuối loop = mẫu ngay trước loopStart
        float maxStep = 0.0f;
        for (size_t i = st + 1; i < e; ++i) maxStep = std::max(maxStep, std::fabs(z.left[i] - z.left[i - 1]));
        CHECK(std::fabs(z.left[st] - z.left[e - 1]) <= maxStep);   // bước nhảy khi vòng không lớn hơn bước thường
        CHECK(z.left.front() == Catch::Approx(0.0).margin(0.02)); // đã cắt 100 ms lặng (còn 2 ms trước tiếng)
        lo = std::min(lo, z.loudnessDb + static_cast<double>(z.volumeDb));
        hi = std::max(hi, z.loudnessDb + static_cast<double>(z.volumeDb));
    }
    CHECK(hi - lo <= 0.2);
    for (int key = 0; key <= 127; ++key)   // fullKeyboard: mọi phím đều có zone (ngoài 53..64 là repitch)
        for (int vel : {1, 95, 96, 127}) {
            int n = 0;
            for (const auto& z : r.zones) n += key >= z.loKey && key <= z.hiKey && vel >= z.loVel && vel <= z.hiVel ? 1 : 0;
            CAPTURE(key, vel);
            CHECK(n == 1);
        }

    // SFZ nạp được, loop + tune đúng, Sampler phát đúng cao độ ở phím gốc
    const Loader loader(r);
    const auto l = io::loadSfzText(r.sfzText, "/imp", "inst_test.sfz", loader.options());
    INFO(l.message);
    REQUIRE(l.ok);
    CHECK(l.warnings.empty());
    REQUIRE(l.regions == 6);
    dsp::Sampler smp;
    smp.prepare(48000.0, 256);
    smp.setInstrument(l.instrument.get(), 1);
    std::vector<float> L(96000, 0.0f), R(96000, 0.0f);
    float* ch[2] = {L.data(), R.data()};
    smp.noteOn(58, 1.0f);
    for (int i = 0; i < 96000; i += 256) smp.render(ch, 2, i, std::min(256, 96000 - i));
    CHECK(smp.activeVoices() == 1);                      // 2 s > độ dài loop: vẫn ngân
    Yin yin;
    const auto p = yin.analyze(L.data() + 24000, 48000, 48000.0);
    REQUIRE(p.ok);
    CHECK(p.rootNote == 58);
    CHECK(std::fabs(p.cents) < 8.0f);                    // tune đã bù +15 cent (vibrato ±20 cent)

    // Tất định
    const auto r2 = imp::importInstrument(spec, s);
    REQUIRE(r2.ok);
    CHECK(r2.sfzText == r.sfzText);
    for (size_t i = 0; i < r.zones.size(); ++i) CHECK(r2.zones[i].left == r.zones[i].left);
}

TEST_CASE("SampleImport: nốt chỉ có 1 lớp dùng chung file; tên lệch 1 nửa cung được sửa theo Yin", "[render][import]") {
    std::vector<imp::SourceSample> s;
    s.push_back(src("a_v1", 60, 1, bowed(hzOf(60), 3.0, 0.2)));
    s.push_back(src("a_v3", 60, 3, bowed(hzOf(60), 3.0, 0.6)));
    s.push_back(src("b_v1", 64, 1, bowed(hzOf(65), 3.0, 0.3)));   // tên E4 nhưng thật ra F4
    imp::ImportSpec spec;
    spec.id = "inst_t2";
    const auto r = imp::importInstrument(spec, s);
    INFO(r.error);
    REQUIRE(r.ok);
    REQUIRE(r.zones.size() == 4);   // 2 nốt × 2 lớp
    int shared = 0;
    for (const auto& z : r.zones) {
        if (z.loKey > 61) {          // zone của nốt thứ 2
            CHECK(z.root == 65);     // đã sửa theo Yin
            CHECK(z.tuneCents == Catch::Approx(0.0).margin(3.0));
            CHECK(z.file == "t2_064_all.flac");
        }
        shared += z.shared ? 1 : 0;
    }
    CHECK(shared == 1);
    CHECK(std::any_of(r.notes.begin(), r.notes.end(), [](const std::string& n) { return n.find("tên lệch 1 nửa cung") != std::string::npos; }));
    const Loader loader(r);
    const auto l = io::loadSfzText(r.sfzText, "/imp", "t2.sfz", loader.options());
    INFO(l.message);
    REQUIRE(l.ok);
    CHECK(l.regions == 4);
    CHECK(l.samplesLoaded == 3);    // file dùng chung chỉ nạp 1 lần
}

TEST_CASE("SampleImport: Decay — cắt đuôi dưới −60 dB hoặc maxSec, fade về 0, no_loop", "[render][import]") {
    std::vector<imp::SourceSample> s;
    for (int note : {48, 60, 72}) s.push_back(src("h_" + std::to_string(note), note, 0, plucked(hzOf(note), 12.0, 0.5)));
    imp::ImportSpec spec;
    spec.id = "inst_t3";
    spec.mode = imp::Mode::Decay;
    spec.maxSecLow = 6.0;
    spec.maxSecHigh = 2.0;
    const auto r = imp::importInstrument(spec, s);
    INFO(r.error);
    REQUIRE(r.ok);
    REQUIRE(r.zones.size() == 3);
    CHECK(r.sfzText.find("loop_mode=no_loop") != std::string::npos);
    CHECK(r.sfzText.find("loop_start") == std::string::npos);
    for (size_t i = 0; i < 3; ++i) {
        const auto& z = r.zones[i];
        CAPTURE(z.file, z.left.size());
        CHECK(z.loopStart == -1);
        CHECK(z.left.back() == 0.0f);                               // fade tới đúng 0
        // τ 0.4 s → −60 dB sau ~2.8 s; nốt cao bị giới hạn 2 s
        const double sec = static_cast<double>(z.left.size()) / 48000.0;
        CHECK(sec <= (i == 2 ? 2.0 : 3.0) + 0.06);
        CHECK(sec > 1.5);
        float peak = 0.0f;
        for (float v : z.left) peak = std::max(peak, std::fabs(v));
        CHECK(20.0 * std::log10(peak) == Catch::Approx(-1.0).margin(0.05));
    }
}

TEST_CASE("SampleImport: measureTune = false giữ cao độ gốc (tune 0) nhưng vẫn đo", "[render][import]") {
    std::vector<imp::SourceSample> s;
    for (int note : {48, 60}) s.push_back(src("p_" + std::to_string(note), note, 0, plucked(hzOf(note, 25.0), 4.0, 0.5)));
    imp::ImportSpec spec;
    spec.id = "inst_t4";
    spec.mode = imp::Mode::Decay;
    spec.measureTune = false;
    const auto r = imp::importInstrument(spec, s);
    REQUIRE(r.ok);
    for (const auto& z : r.zones) {
        CHECK(z.tuneCents == 0.0f);
        CHECK(z.pitchMeasured);
        CHECK(z.measuredCents == Catch::Approx(25.0).margin(3.0));
    }
    CHECK(r.sfzText.find("giữ cao độ gốc") != std::string::npos);
}

TEST_CASE("SampleImport: limitPeaks nén transient, thân âm to lên, đỉnh vẫn −1 dBFS", "[render][import]") {
    // Gõ: xung nhọn 1 ms (biên độ 1) + thân tone 200 Hz biên độ 0.2
    std::vector<float> x(24000);
    for (size_t i = 0; i < x.size(); ++i) {
        const double t = static_cast<double>(i) / 48000.0;
        x[i] = static_cast<float>(0.2 * std::sin(kTwoPi * 200.0 * t) * std::exp(-t / 0.2) + (i < 48 ? 1.0 : 0.0));
    }
    const auto rms = [](const std::vector<float>& v, size_t a, size_t b) {
        double e = 0.0;
        for (size_t i = a; i < b; ++i) e += static_cast<double>(v[i]) * v[i];
        return 10.0 * std::log10(e / static_cast<double>(b - a));
    };
    std::vector<float> y = x;
    for (float& v : y) v *= std::pow(10.0f, -1.0f / 20.0f);   // cùng chuẩn đỉnh −1 dBFS như sau khi nén
    const double body0 = rms(y, 4800, 9600);
    std::vector<float> z = x;
    imp::limitPeaks(z, 48000.0, 6.0);
    float peak = 0.0f;
    for (float v : z) peak = std::max(peak, std::fabs(v));
    CHECK(20.0 * std::log10(peak) == Catch::Approx(-1.0).margin(0.05));
    // thân âm (100–200 ms, đã qua nhả 40 ms) to lên gần 6 dB (xung chồng lên thân nên thực tế ~4.4 dB)
    CHECK(rms(z, 4800, 9600) > body0 + 3.5);
    CHECK(rms(z, 4800, 9600) < body0 + 6.5);
    std::vector<float> z2 = x;
    imp::limitPeaks(z2, 48000.0, 6.0);
    CHECK(z2 == z);                                           // tất định
}

TEST_CASE("SampleImport: importKit — 2 lớp, pad dùng chung file + tune, choke, label, cân loudness, SFZ nạp được", "[render][import]") {
    const auto hit = [](double hz, double tau, double amp) {
        imp::KitLayerSource l;
        l.mono = plucked(hz, 1.0, amp);
        l.mono.insert(l.mono.begin(), 2400, 0.0f);
        (void)tau;
        return l;
    };
    std::vector<imp::KitPadSpec> pads(4);
    pads[0].key = 36; pads[0].label = "Kick"; pads[0].file = "kick"; pads[0].maxSec = 0.8;
    pads[0].layers = {hit(60, 0.3, 0.3), hit(60, 0.3, 0.9)};
    pads[1].key = 42; pads[1].label = "Closed Hat"; pads[1].file = "hat_closed"; pads[1].maxSec = 0.3; pads[1].group = 1;
    pads[1].layers = {hit(3000, 0.05, 0.5)};
    pads[2].key = 47; pads[2].label = "Mid Tom"; pads[2].file = "tom"; pads[2].maxSec = 1.0;
    pads[2].layers = {hit(110, 0.3, 0.4), hit(110, 0.3, 0.8)};
    pads[3].key = 48; pads[3].label = "Hi-Mid Tom"; pads[3].sameAs = 47; pads[3].tuneCents = 300.0f;
    const auto r = imp::importKit("kit_t", "test", "credit", pads);
    INFO(r.error);
    REQUIRE(r.ok);
    REQUIRE(r.regions.size() == 7);   // 2 + 1 + 2 + 2 (dùng chung)
    int shared = 0;
    for (const auto& z : r.regions) {
        CAPTURE(z.key, z.file);
        if (z.shared) {
            ++shared;
            CHECK(z.key == 48);
            CHECK(z.tuneCents == 300.0f);
            CHECK(z.file.rfind("tom_", 0) == 0);
            CHECK(z.samples.empty());
        } else {
            CHECK(z.samples.back() == 0.0f);                  // fade tới 0
            CHECK(static_cast<double>(z.samples.size()) <= 1.0 * 48000.0 + 1);
        }
        CHECK(z.volumeDb <= 0.0f);
    }
    CHECK(shared == 2);
    CHECK(r.sfzText.find("key=48 lovel=1 hivel=95 sample=tom_soft.flac tune=300") != std::string::npos);
    CHECK(r.sfzText.find("group=1 region_label=Closed Hat\n") != std::string::npos);
    CHECK(r.sfzText.find("loop_mode=one_shot") != std::string::npos);

    // SFZ nạp được (file dùng chung nạp 1 lần), key 48 phát cao hơn key 47 đúng 3 nửa cung
    std::map<std::string, dsp::AudioDataPtr> files;
    for (const auto& z : r.regions) {
        if (z.shared) continue;
        auto d = std::make_shared<dsp::AudioData>(1, static_cast<int64_t>(z.samples.size()), z.sampleRate);
        std::copy(z.samples.begin(), z.samples.end(), d->writePointer(0));
        files["/k/samples/" + z.file] = d;
    }
    io::SfzLoadOptions o;
    o.loadSample = [&](const std::string& p) {
        const auto it = files.find(p);
        return it == files.end() ? io::DecodeResult{LE_ERR_FILE_NOT_FOUND, "thiếu " + p, nullptr} : io::DecodeResult{LE_OK, "", it->second};
    };
    const auto l = io::loadSfzText(r.sfzText, "/k", "kit_t.sfz", o);
    INFO(l.message);
    REQUIRE(l.ok);
    CHECK(l.warnings.empty());
    CHECK(l.regions == 7);
    CHECK(l.samplesLoaded == 5);
    const dsp::Zone* z47 = l.instrument->findZone(47, 120);
    const dsp::Zone* z48 = l.instrument->findZone(48, 120);
    REQUIRE(z47 != nullptr);
    REQUIRE(z48 != nullptr);
    CHECK(z47->data == z48->data);
    CHECK(z48->tuneCents == Catch::Approx(300.0f));
    CHECK(z48->label == "Hi-Mid Tom");
}
