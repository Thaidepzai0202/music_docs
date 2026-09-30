// P2-35 — nhạc cụ tổng hợp (render/SynthInstruments: E-Piano FM, Organ drawbar, Synth Tone C0–C8): tất định từng bit,
// lưới 24-bit, zone mỗi 3 nửa cung × 2 lớp velocity phủ kín phím 0–127 (không hở, không chồng), loop liền mạch, loudness cân,
// SFZ nạp bằng SfzLoader không cảnh báo, cao độ đúng (Yin, qua Sampler, kể cả phím dịch ±1 nửa cung).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"
#include "io/SfzLoader.h"
#include "render/DrumKits.h"
#include "render/SynthInstruments.h"
#include "render/Yin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace le;
namespace ins = le::render::instruments;

namespace {

const std::vector<ins::SynthInstrument>& all() {
    static const std::vector<ins::SynthInstrument> v = ins::makeAllInstruments(48000.0);
    return v;
}

struct Loader {
    std::map<std::string, dsp::AudioDataPtr> files;
    explicit Loader(const ins::SynthInstrument& in) {
        for (const ins::InstSample& z : in.zones) {
            auto d = std::make_shared<dsp::AudioData>(1, static_cast<int64_t>(z.samples.size()), in.sampleRate);
            std::copy(z.samples.begin(), z.samples.end(), d->writePointer(0));
            files["/inst/" + in.id + "/samples/" + z.file] = d;
        }
    }
    io::SfzLoadOptions options() const {
        io::SfzLoadOptions o;
        o.loadSample = [this](const std::string& path) {
            const auto it = files.find(path);
            return it == files.end() ? io::DecodeResult{LE_ERR_FILE_NOT_FOUND, "thiếu " + path, nullptr}
                                     : io::DecodeResult{LE_OK, "", it->second};
        };
        return o;
    }
};
io::SfzLoadResult load(const ins::SynthInstrument& in, const Loader& l) {
    return io::loadSfzText(in.sfzText, "/inst/" + in.id, in.sfzFileName, l.options());
}

std::vector<float> hold(dsp::Sampler& s, int note, float vel, int frames) {
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

TEST_CASE("SynthInstruments: sinh lại đúng từng bit, 24-bit, đỉnh −1 dBFS, không NaN", "[render][instruments]") {
    const auto again = ins::makeAllInstruments(48000.0);
    REQUIRE(again.size() == 3);
    CHECK(all()[0].id == "inst_epiano");
    CHECK(all()[1].id == "inst_organ");
    CHECK(all()[2].id == "inst_synth");
    for (size_t k = 0; k < again.size(); ++k) {
        const auto &a = all()[k], &b = again[k];
        CAPTURE(a.id);
        CHECK(a.sfzText == b.sfzText);
        CHECK(a.sfzFileName == a.id + ".sfz");
        REQUIRE(a.zones.size() == b.zones.size());
        for (size_t i = 0; i < a.zones.size(); ++i) {
            const auto& x = a.zones[i].samples;
            CAPTURE(a.zones[i].file);
            REQUIRE(x.size() == b.zones[i].samples.size());
            CHECK(std::memcmp(x.data(), b.zones[i].samples.data(), x.size() * sizeof(float)) == 0);
            CHECK(std::all_of(x.begin(), x.end(), [](float v) { return std::isfinite(v); }));
            CHECK(std::all_of(x.begin(), x.end(), [](float v) {
                const double q = static_cast<double>(v) * 8388608.0;
                return q == std::round(q);
            }));
            float peak = 0.0f;
            for (float v : x) peak = std::max(peak, std::fabs(v));
            CHECK(20.0 * std::log10(peak) == Catch::Approx(-1.0).margin(0.2));
        }
    }
}

TEST_CASE("SynthInstruments: zone mỗi 3 nửa cung × 2 lớp velocity, phủ kín MỌI phím 0–127", "[render][instruments]") {
    for (const auto& in : all()) {
        CAPTURE(in.id);
        std::map<std::string, int> names;
        int minRoot = 128, maxRoot = -1;
        for (const auto& z : in.zones) {
            minRoot = std::min(minRoot, z.root);
            maxRoot = std::max(maxRoot, z.root);
        }
        CHECK(minRoot == in.rangeLo + 1);                              // dải tự nhiên (manifest "range") giữ nguyên
        CHECK(maxRoot - 1 <= in.rangeHi);
        for (const auto& z : in.zones) {
            CAPTURE(z.file);
            ++names[z.file];
            // Zone đầu kéo xuống phím 0, zone cuối lên 127 (repitch ngoài dải); còn lại đúng gốc −1 .. gốc +1
            CHECK(z.loKey == (z.root == minRoot ? 0 : z.root - 1));
            CHECK(z.hiKey == (z.root == maxRoot ? 127 : z.root + 1));
            CHECK((z.loVel == 1 && z.hiVel == ins::kSoftMaxVel) != (z.loVel == ins::kSoftMaxVel + 1 && z.hiVel == 127));
            CHECK(std::fabs(z.tuneCents) <= 50.0f);                      // tần số nguyên Hz: lệch ≤ 0.5 Hz (C0 ~17 Hz → ≤ 50 cent)
            const double nominal = 440.0 * std::pow(2.0, (z.root - 69) / 12.0);
            CHECK(z.f0Hz * std::pow(2.0, z.tuneCents / 1200.0) == Catch::Approx(nominal).epsilon(1e-5));
        }
        for (const auto& [name, count] : names) CHECK(count == 1);
        // Mỗi (phím 0–127, velocity): đúng một zone
        for (int key = 0; key <= 127; ++key)
            for (int vel : {1, 64, ins::kSoftMaxVel, ins::kSoftMaxVel + 1, 127}) {
                int n = 0;
                for (const auto& z : in.zones) n += (key >= z.loKey && key <= z.hiKey && vel >= z.loVel && vel <= z.hiVel) ? 1 : 0;
                CAPTURE(key, vel);
                CHECK(n == 1);
            }
    }
}

TEST_CASE("SynthInstruments: loop liền mạch (mẫu trước điểm vòng khớp mẫu cuối loop), loop dài 1 s", "[render][instruments]") {
    for (const auto& in : all()) {
        CAPTURE(in.id);
        for (const auto& z : in.zones) {
            CAPTURE(z.file);
            const auto& x = z.samples;
            REQUIRE(z.loopEnd == static_cast<int64_t>(x.size()));
            REQUIRE(z.loopStart > 0);
            CHECK(z.loopEnd - z.loopStart == static_cast<int64_t>(in.sampleRate));   // 1 s: mọi tần số nguyên Hz vừa khít
            const size_t T = static_cast<size_t>(z.loopStart), n = x.size();
            // Tín hiệu đã ổn định trước T → vòng từ n−1 về T giống hệt đi từ T−1 sang T
            for (size_t k = 1; k <= 8; ++k) CHECK(std::fabs(x[n - k] - x[T - k]) < 1e-4f);
            float maxStep = 0.0f;
            for (size_t i = T + 1; i < n; ++i) maxStep = std::max(maxStep, std::fabs(x[i] - x[i - 1]));
            CHECK(std::fabs(x[T] - x[n - 1]) <= maxStep * 1.01f);             // bước nhảy khi vòng không lớn hơn bước thường
        }
    }
}

TEST_CASE("SynthInstruments: loudness cân trong nhạc cụ (lệch ≤ 0.2 dB, volume ≤ 0)", "[render][instruments]") {
    for (const auto& in : all()) {
        CAPTURE(in.id);
        if (in.id == "inst_synth") {   // không cân theo K-weighting (xem SynthInstruments.h): volume 0, đỉnh −1 dBFS
            for (const auto& z : in.zones) CHECK(z.volumeDb == 0.0f);
            continue;
        }
        double lo = 1e9, hi = -1e9;
        for (const auto& z : in.zones) {
            CHECK(z.loudnessDb == Catch::Approx(render::kits::loudnessDb(z.samples, in.sampleRate)).margin(1e-9));
            CHECK(z.volumeDb <= 0.0f);
            CHECK(z.volumeDb >= render::kits::kMaxCutDb);
            lo = std::min(lo, z.loudnessDb + static_cast<double>(z.volumeDb));
            hi = std::max(hi, z.loudnessDb + static_cast<double>(z.volumeDb));
        }
        CHECK(hi - lo <= 0.2);
    }
}

TEST_CASE("SynthInstruments: SFZ nạp bằng SfzLoader — loop_continuous, loop, tune, volume, envelope", "[render][instruments]") {
    for (const auto& in : all()) {
        CAPTURE(in.id);
        const Loader loader(in);
        const auto r = load(in, loader);
        INFO(r.message);
        REQUIRE(r.ok);
        CHECK(r.warnings.empty());
        REQUIRE(r.regions == static_cast<int>(in.zones.size()));
        for (const auto& z : in.zones) {
            CAPTURE(z.file);
            const dsp::Zone* found = nullptr;
            for (const auto& lz : r.instrument->zones)
                if (lz.loKey == z.loKey && lz.loVel == z.loVel) found = &lz;
            REQUIRE(found != nullptr);
            CHECK(found->hiKey == z.hiKey);
            CHECK(found->rootKey == z.root);
            CHECK(found->hiVel == z.hiVel);
            CHECK(found->loopMode == dsp::LoopMode::LoopContinuous);
            CHECK(found->loopStart == z.loopStart);
            CHECK(found->loopEnd == z.loopEnd);
            CHECK(found->tuneCents == Catch::Approx(z.tuneCents).margin(0.006));   // SFZ ghi 2 chữ số thập phân
            CHECK(found->gainDb == Catch::Approx(z.volumeDb).margin(1e-4));
            REQUIRE(found->data != nullptr);
            CHECK(found->data->numFrames() == static_cast<int64_t>(z.samples.size()));
            if (in.id == "inst_epiano") {
                CHECK(found->env.sustain == 0.0f);                              // tắt dần khi giữ phím
                CHECK(found->env.decay == Catch::Approx(z.decaySec));
                CHECK(found->env.release == Catch::Approx(0.3f));
            } else {
                CHECK(found->env.sustain == 1.0f);                              // organ / synth: ngân mãi khi giữ
                CHECK(found->env.release == Catch::Approx(in.id == "inst_organ" ? 0.06f : 0.3f));
            }
        }
    }
}

TEST_CASE("SynthInstruments: cao độ đúng qua Sampler (Yin) ở phím gốc và phím dịch ±1, cả 2 lớp", "[render][instruments]") {
    // Cấu hình Yin theo cao độ mong đợi: chi phí ~ τmax², τmax = sr / minHz → dò hẹp quanh nốt cần đo
    const auto yinFor = [](int key, bool organ) {
        const double f = 440.0 * std::pow(2.0, (key - 69) / 12.0);
        YinConfig cfg;
        cfg.minHz = static_cast<float>(0.6 * (organ ? f / 2.0 : f));   // organ: 16' = f/2 là chu kỳ chung
        cfg.maxHz = static_cast<float>(1.6 * f);
        const int tauMax = static_cast<int>(std::ceil(48000.0 / cfg.minHz));
        int n = 1024;
        while (n < 2 * (tauMax + 2) && n < 8192) n *= 2;
        cfg.frameSize = n;
        return Yin(cfg);
    };
    for (const auto& in : all()) {
        CAPTURE(in.id);
        const bool organ = in.id == "inst_organ";
        const Loader loader(in);
        const auto r = load(in, loader);
        REQUIRE(r.ok);
        for (const auto& z : in.zones) {
            if (z.loVel != 1) continue;   // mỗi phím gốc một lần; lớp velocity chạy trong vòng dưới
            for (int key : {z.root - 1, z.root, z.root + 1}) {
                if (key < in.rangeLo || key > in.rangeHi) continue;
                for (float vel : {0.5f, 1.0f}) {
                    dsp::Sampler s;
                    s.prepare(48000.0, 256);
                    s.setInstrument(r.instrument.get(), 1);
                    const auto out = hold(s, key, vel, 28800);
                    Yin yin = yinFor(key, organ);
                    const auto p = yin.analyze(out.data() + 9600, 19200, 48000.0);   // 0.2–0.6 s: bỏ tiếng gõ
                    CAPTURE(z.file, key, vel, p.hz, p.rootNote, p.cents, p.confidence);
                    REQUIRE(p.ok);
                    // Organ 888000000 có 16' (f/2) → chu kỳ chung dài gấp đôi: Yin thấy thấp 1 quãng tám
                    CHECK((p.rootNote == key || (organ && p.rootNote == key - 12)));
                    // > 2 kHz chu kỳ chỉ ~15 mẫu: Yin nội suy kém chính xác hơn
                    CHECK(std::fabs(p.cents) < (p.hz > 2000.0f ? 5.0f : 3.0f));
                }
            }
        }
    }
}

TEST_CASE("SynthInstruments: giữ phím qua nhiều vòng loop — organ đều tiếng, E-Piano tắt dần, nhả phím thì hết", "[render][instruments]") {
    for (const auto& in : all()) {
        CAPTURE(in.id);
        const Loader loader(in);
        const auto r = load(in, loader);
        REQUIRE(r.ok);
        dsp::Sampler s;
        s.prepare(48000.0, 256);
        s.setInstrument(r.instrument.get(), 1);
        const int note = 61;
        const auto out = hold(s, note, 0.5f, 48000 * 4);   // 4 s: đi qua ≥ 3 lần điểm vòng
        CHECK(std::all_of(out.begin(), out.end(), [](float v) { return std::isfinite(v); }));
        CHECK(s.activeVoices() == 1);
        std::vector<double> win;
        for (size_t a = 48000; a + 4800 <= out.size(); a += 4800) win.push_back(rmsDb(out, a, a + 4800));   // cửa sổ 100 ms từ 1 s
        const auto [mn, mx] = std::minmax_element(win.begin(), win.end());
        if (in.id != "inst_epiano") {
            CHECK(*mx - *mn < 0.3);                       // organ / synth: không hụt / nhảy tiếng ở điểm vòng
        } else {
            CHECK(win.front() > win.back() + 3.0);        // ampeg_decay: nhỏ dần khi giữ phím
            for (size_t i = 1; i < win.size(); ++i) CHECK(win[i] < win[i - 1] + 0.8);   // tremolo ±6 % nhưng không nhảy lên
        }
        s.noteOff(note);
        std::vector<float> l(48000, 0.0f), rr(48000, 0.0f);
        float* ch[2] = {l.data(), rr.data()};
        for (int i = 0; i < 48000; i += 256) s.render(ch, 2, i, std::min(256, 48000 - i));
        CHECK(s.activeVoices() == 0);                     // release 0.3 s / 0.06 s đã xong
    }
}

TEST_CASE("SynthInstruments: inst_synth — zone thật C0–C8, cao độ đúng ở C#0 / C1 / C2 / A#7 qua Sampler (Yin)", "[render][instruments]") {
    const ins::SynthInstrument* synth = nullptr;
    for (const auto& i : all())
        if (i.id == "inst_synth") synth = &i;
    REQUIRE(synth != nullptr);
    CHECK(synth->rangeLo == 12);
    CHECK(synth->rangeHi == 108);
    const Loader loader(*synth);
    const auto r = load(*synth, loader);
    REQUIRE(r.ok);
    for (int key : {13, 24, 36, 106}) {                   // gốc zone C#0, C1, C2, A#7: phát ĐÚNG sample của nó (không repitch xa)
        const dsp::Zone* z = r.instrument->findZone(key, 120);
        REQUIRE(z != nullptr);
        CAPTURE(key, z->rootKey);
        CHECK(std::abs(z->rootKey - key) <= 1);
        for (float vel : {0.5f, 1.0f}) {
            dsp::Sampler s;
            s.prepare(48000.0, 256);
            s.setInstrument(r.instrument.get(), 1);
            const auto out = hold(s, key, vel, 48000);
            const double f = 440.0 * std::pow(2.0, (key - 69) / 12.0);
            YinConfig cfg;
            cfg.minHz = static_cast<float>(0.6 * f);
            cfg.maxHz = static_cast<float>(1.6 * f);
            cfg.frameSize = f < 40.0 ? 8192 : 2048;
            Yin yin(cfg);
            const auto p = yin.analyze(out.data() + 14400, 24000, 48000.0);
            CAPTURE(vel, p.hz, p.cents);
            REQUIRE(p.ok);
            CHECK(p.rootNote == key);
            CHECK(std::fabs(p.cents) < 3.0f);
        }
    }
}
