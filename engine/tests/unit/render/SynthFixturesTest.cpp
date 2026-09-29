// P1-28 — fixture tổng hợp: deterministic, nạp được bằng SfzLoader, chơi được bằng Sampler, cao độ các
// zone đúng nốt chuẩn (Yin), vòng loop liền mạch, choke hi-hat, 2 lớp velocity khác âm lượng.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (file trên đĩa phải khớp đúng từng giá trị).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"
#include "io/SfzLoader.h"
#include "render/SynthFixtures.h"
#include "render/Yin.h"
#include "spike/measure/WavIO.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace le;
namespace fx = le::render::fixtures;

namespace {
// Loader giả: đường dẫn "/fx/<tên bộ>/<relativePath>" → AudioData từ buffer đã tổng hợp
struct SetLoader {
    std::map<std::string, dsp::AudioDataPtr> files;
    explicit SetLoader(const fx::SynthSet& s) {
        for (const auto& f : s.files) {
            auto d = std::make_shared<dsp::AudioData>(1, static_cast<int64_t>(f.samples.size()), s.sampleRate);
            std::copy(f.samples.begin(), f.samples.end(), d->writePointer(0));
            files["/fx/" + s.name + "/" + f.relativePath] = d;
        }
    }
    io::SfzLoadOptions options() const {
        io::SfzLoadOptions o;
        o.loadSample = [this](const std::string& p) {
            const auto it = files.find(p);
            return it == files.end() ? io::DecodeResult{LE_ERR_FILE_NOT_FOUND, "thiếu " + p, nullptr}
                                     : io::DecodeResult{LE_OK, "", it->second};
        };
        return o;
    }
};

io::SfzLoadResult load(const fx::SynthSet& s, const SetLoader& l) {
    return io::loadSfzText(s.sfzText, "/fx/" + s.name, s.name, l.options());
}

std::vector<float> renderNote(dsp::Sampler& s, int note, float vel, int frames) {
    std::vector<float> l(static_cast<size_t>(frames), 0.0f), r(l.size(), 0.0f);
    float* ch[2] = {l.data(), r.data()};
    s.noteOn(note, vel);
    for (int i = 0; i < frames; i += 256) s.render(ch, 2, i, std::min(256, frames - i));
    return l;
}

double rmsDb(const std::vector<float>& x, size_t a, size_t b) {
    double e = 0.0;
    for (size_t i = a; i < b; ++i) e += static_cast<double>(x[i]) * x[i];
    return 10.0 * std::log10(e / static_cast<double>(b - a) + 1e-30);
}
} // namespace

TEST_CASE("SynthFixtures: sinh lại ra đúng từng bit (deterministic)", "[render][fixtures]") {
    for (auto make : {&fx::makeDrumKit, &fx::makeInstrument}) {
        const fx::SynthSet a = make(48000.0), b = make(48000.0);
        REQUIRE(a.files.size() == b.files.size());
        CHECK(a.sfzText == b.sfzText);
        for (size_t i = 0; i < a.files.size(); ++i) {
            CAPTURE(a.files[i].relativePath);
            CHECK(fx::fnv1a(a.files[i].samples) == fx::fnv1a(b.files[i].samples));
            float peak = 0.0f;
            for (float v : a.files[i].samples) peak = std::max(peak, std::fabs(v));
            CHECK(peak <= 0.95f);                                  // không clip
            CHECK(peak > 0.1f);
            CHECK(a.files[i].samples.back() == Catch::Approx(0.0f).margin(1e-6));   // hết sample êm
        }
    }
}

TEST_CASE("SynthFixtures: bộ trống nạp bằng SfzLoader, choke hi-hat, one-shot", "[render][fixtures]") {
    const fx::SynthSet kit = fx::makeDrumKit();
    const SetLoader loader(kit);
    const auto r = load(kit, loader);
    INFO(r.message);
    REQUIRE(r.ok);
    CHECK(r.warnings.empty());
    REQUIRE(r.regions == 4);
    const auto& z = r.instrument->zones;
    CHECK(z[0].loKey == 36);
    CHECK(z[2].group == 1);
    CHECK(z[2].offBy == 2);
    CHECK(z[3].group == 2);
    for (const auto& zone : z) CHECK(zone.loopMode == dsp::LoopMode::OneShot);

    dsp::Sampler s;
    s.prepare(48000.0, 256);
    s.setInstrument(r.instrument.get(), 1);
    const auto k = renderNote(s, 36, 1.0f, 48000);
    CHECK(s.activeVoices() == 0);                                  // kick 0.5 s đã hết
    float peak = 0.0f;
    for (float v : k) peak = std::max(peak, std::fabs(v));
    CHECK(peak > 0.3f);

    renderNote(s, 46, 1.0f, 2400);                                 // hat mở đang kêu
    CHECK(s.activeVoices() == 1);
    renderNote(s, 42, 1.0f, 480);                                  // hat đóng chặn hat mở (5 ms)
    CHECK(s.activeVoices() == 1);
}

TEST_CASE("SynthFixtures: nhạc cụ — zone đúng nốt chuẩn (tune bù), vòng loop liền mạch, 2 lớp velocity", "[render][fixtures]") {
    const fx::SynthSet inst = fx::makeInstrument();
    const SetLoader loader(inst);
    const auto r = load(inst, loader);
    INFO(r.message);
    REQUIRE(r.ok);
    CHECK(r.warnings.empty());
    REQUIRE(r.regions == 6);

    // Vòng loop [12000, 60000): hai đầu nối liền (tone tần số nguyên Hz, 1 s)
    for (const auto& f : inst.files) {
        CAPTURE(f.relativePath);
        CHECK(f.samples[60000] == Catch::Approx(f.samples[12000]).margin(1e-5));
        CHECK(f.samples[59999] == Catch::Approx(f.samples[11999]).margin(1e-5));
    }
    for (const auto& zone : r.instrument->zones) {
        CHECK(zone.loopMode == dsp::LoopMode::LoopContinuous);
        CHECK(zone.loopStart == 12000);
        CHECK(zone.loopEnd == 60000);
    }

    Yin yin;
    for (const auto& info : fx::instrumentZones()) {
        for (float vel : {0.3f, 0.9f}) {
            CAPTURE(info.rootKey, vel);
            dsp::Sampler s;
            s.prepare(48000.0, 256);
            s.setInstrument(r.instrument.get(), 1);
            const auto out = renderNote(s, info.rootKey, vel, 72000);     // 1.5 s: đi qua điểm loop
            const auto p = yin.analyze(out.data() + 4800, 48000, 48000.0);
            REQUIRE(p.ok);
            CHECK(p.rootNote == info.rootKey);                           // phím gốc ra đúng nốt chuẩn
            CHECK(std::fabs(p.cents) < 0.5f);                             // tune bù lệch ±2.5 cent của tần số nguyên
            CHECK(s.activeVoices() == 1);                                 // loop: vẫn kêu sau 1.5 s
        }
    }

    // Lớp mạnh to hơn lớp nhẹ rõ rệt (0.6 so với 0.25 · velocity²)
    dsp::Sampler soft, loud;
    soft.prepare(48000.0, 256);
    loud.prepare(48000.0, 256);
    soft.setInstrument(r.instrument.get(), 1);
    loud.setInstrument(r.instrument.get(), 1);
    const auto a = renderNote(soft, 60, 60.0f / 127.0f, 24000);
    const auto b = renderNote(loud, 60, 64.0f / 127.0f, 24000);
    CHECK(rmsDb(b, 4800, 24000) - rmsDb(a, 4800, 24000) > 6.0);
}

TEST_CASE("SynthFixtures: file trên đĩa (tests/fixtures) khớp bản sinh lại, nạp được qua SfzLoader", "[render][fixtures][disk]") {
#ifdef LE_TEST_FIXTURES_DIR
    const std::string root = LE_TEST_FIXTURES_DIR;
#else
    const std::string root = "tests/fixtures";
#endif
    for (auto make : {&fx::makeDrumKit, &fx::makeInstrument}) {
        const fx::SynthSet set = make(48000.0);
        const std::string dir = root + "/" + set.name;
        const std::string sfz = dir + "/" + set.sfzFileName;
        if (!std::filesystem::exists(sfz)) {
            WARN("Chưa có " << sfz << ": chạy build/<preset>/tools/le-gen-fixtures (hoặc git lfs pull)");
            continue;
        }
        io::SfzLoadOptions o;
        o.loadSample = [](const std::string& p) {
            std::vector<float> x;
            double sr = 0.0;
            std::string err;
            if (!spike::readAudioMono(p, x, sr, &err)) return io::DecodeResult{LE_ERR_FILE_NOT_FOUND, err, nullptr};
            auto d = std::make_shared<dsp::AudioData>(1, static_cast<int64_t>(x.size()), sr);
            std::copy(x.begin(), x.end(), d->writePointer(0));
            return io::DecodeResult{LE_OK, "", d};
        };
        const auto r = io::loadSfzFile(sfz, o);
        INFO(r.message);
        REQUIRE(r.ok);
        CHECK(r.regions == static_cast<int>(set.files.size()));
        for (const auto& f : set.files) {
            CAPTURE(f.relativePath);
            std::vector<float> x;
            double sr = 0.0;
            REQUIRE(spike::readAudioMono(dir + "/" + f.relativePath, x, sr));
            CHECK(sr == Catch::Approx(48000.0));
            INFO("File trên đĩa khác bản sinh lại → fixture cũ: chạy lại le-gen-fixtures rồi NGHE lại trước khi commit");
            CHECK(x == f.samples);
        }
    }
}
