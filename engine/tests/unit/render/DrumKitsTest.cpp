// P2-30 / P2-35 — 8 bộ trống tổng hợp (render/DrumKits*): tất định từng bit, 16 pad map GM 36–51, mọi file đỉnh
// −1 dBFS, không NaN, nằm trên lưới 24-bit (FLAC), đuôi đã cắt, loudness cân trong kit, tom cao độ tăng dần (Yin),
// SFZ nạp bằng SfzLoader (label, one-shot, volume), choke hoạt động qua Sampler.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"
#include "io/SfzLoader.h"
#include "render/DrumKits.h"
#include "render/KitSynth.h"
#include "render/Yin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace le;
namespace kits = le::render::kits;

namespace {

const std::vector<kits::Kit>& allKits() {
    static const std::vector<kits::Kit> k = kits::makeAllKits(48000.0);   // ~1 s ở -O2: sinh một lần cho cả file
    return k;
}

const kits::Kit& kit(const std::string& id) {
    for (const kits::Kit& k : allKits())
        if (k.id == id) return k;
    FAIL("không có kit " << id);
    return allKits().front();
}

const kits::KitPad& pad(const kits::Kit& k, int key) { return k.pads.at(static_cast<size_t>(key - 36)); }

// Kit trống (có hat 42/44/46 + 6 tom) — mọi kit trừ Percussion
const std::vector<std::string> kDrumKits = {"kit_808", "kit_909", "kit_trap", "kit_lofi", "kit_606", "kit_707", "kit_linn"};

float peakDb(const std::vector<float>& x, size_t from = 0) {
    float p = 0.0f;
    for (size_t i = from; i < x.size(); ++i) p = std::max(p, std::fabs(x[i]));
    return 20.0f * std::log10(std::max(p, 1e-12f));
}

// Loader giả: "/kits/<id>/samples/<file>" → AudioData từ buffer đã tổng hợp
struct KitLoader {
    std::map<std::string, dsp::AudioDataPtr> files;
    explicit KitLoader(const kits::Kit& k) {
        for (const kits::KitPad& p : k.pads) {
            auto d = std::make_shared<dsp::AudioData>(1, static_cast<int64_t>(p.samples.size()), k.sampleRate);
            std::copy(p.samples.begin(), p.samples.end(), d->writePointer(0));
            files["/kits/" + k.id + "/samples/" + p.file] = d;
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

io::SfzLoadResult load(const kits::Kit& k, const KitLoader& l) {
    return io::loadSfzText(k.sfzText, "/kits/" + k.id, k.sfzFileName, l.options());
}

// Chơi nốt rồi render `frames` frame (cộng dồn vào out nếu có)
void play(dsp::Sampler& s, int note, int frames, std::vector<float>* out = nullptr) {
    std::vector<float> l(static_cast<size_t>(frames), 0.0f), r(l.size(), 0.0f);
    float* ch[2] = {l.data(), r.data()};
    if (note >= 0) s.noteOn(note, 0.9f);
    for (int i = 0; i < frames; i += 256) s.render(ch, 2, i, std::min(256, frames - i));
    if (out != nullptr) out->insert(out->end(), l.begin(), l.end());
}

double rmsDb(const std::vector<float>& x, size_t a, size_t b) {
    double e = 0.0;
    for (size_t i = a; i < b; ++i) e += static_cast<double>(x[i]) * x[i];
    return 10.0 * std::log10(e / static_cast<double>(b - a) + 1e-30);
}

} // namespace

TEST_CASE("DrumKits: sinh lại ra đúng từng bit (deterministic)", "[render][kits]") {
    const std::vector<kits::Kit> again = kits::makeAllKits(48000.0);
    REQUIRE(again.size() == allKits().size());
    for (size_t k = 0; k < again.size(); ++k) {
        const kits::Kit &a = allKits()[k], &b = again[k];
        CAPTURE(a.id);
        CHECK(a.sfzText == b.sfzText);
        REQUIRE(a.pads.size() == b.pads.size());
        for (size_t i = 0; i < a.pads.size(); ++i) {
            CAPTURE(a.pads[i].file);
            REQUIRE(a.pads[i].samples.size() == b.pads[i].samples.size());
            CHECK(std::memcmp(a.pads[i].samples.data(), b.pads[i].samples.data(), a.pads[i].samples.size() * sizeof(float)) == 0);
        }
    }
}

TEST_CASE("DrumKits: 16 pad map GM 36–51, đỉnh −1 dBFS ±0.2, không NaN, lưới 24-bit, đuôi đã cắt", "[render][kits]") {
    const std::vector<std::string> ids = {"kit_808", "kit_909", "kit_perc", "kit_trap", "kit_lofi", "kit_606", "kit_707", "kit_linn"};
    REQUIRE(allKits().size() == ids.size());
    for (size_t k = 0; k < ids.size(); ++k) {
        const kits::Kit& kt = allKits()[k];
        CAPTURE(kt.id);
        CHECK(kt.id == ids[k]);
        CHECK(kt.sfzFileName == kt.id + ".sfz");
        CHECK(kt.sampleRate == 48000.0);
        REQUIRE(kt.pads.size() == 16);
        std::set<std::string> files, labels;
        for (size_t i = 0; i < 16; ++i) {
            const kits::KitPad& p = kt.pads[i];
            CAPTURE(p.file);
            CHECK(p.key == 36 + static_cast<int>(i));
            CHECK(!p.label.empty());
            CHECK(p.label.size() <= 14);                                   // vừa ô pad
            CHECK(p.file.size() > 4);
            CHECK(p.file.substr(p.file.size() - 5) == ".flac");
            files.insert(p.file);
            labels.insert(p.label);
            REQUIRE(!p.samples.empty());
            CHECK(std::all_of(p.samples.begin(), p.samples.end(), [](float v) { return std::isfinite(v); }));
            // Lưới 24-bit: v · 2^23 là số nguyên → file FLAC 24-bit chứa đúng buffer này
            CHECK(std::all_of(p.samples.begin(), p.samples.end(), [](float v) {
                const double q = static_cast<double>(v) * 8388608.0;
                return q == std::round(q);
            }));
            CHECK(peakDb(p.samples) == Catch::Approx(-1.0).margin(0.2));
            const double sec = static_cast<double>(p.samples.size()) / kt.sampleRate;
            CHECK(sec > 0.05);
            CHECK(sec < 3.5);
            // Đuôi đã fade + cắt: 2 ms cuối dưới −60 dBFS (điểm cắt là mẫu cuối còn ≥ −80 dBFS)
            CHECK(peakDb(p.samples, p.samples.size() - 96) < -60.0f);
            CHECK(std::fabs(p.samples.back()) >= std::pow(10.0f, kits::kTailDb / 20.0f));
        }
        CHECK(files.size() == 16);
        CHECK(labels.size() == 16);
    }
}

TEST_CASE("DrumKits: loudness K-weighting đúng thang (sine 1 kHz 0 dBFS ≈ −3 dB)", "[render][kits]") {
    std::vector<float> sine(48000);
    for (size_t i = 0; i < sine.size(); ++i) sine[i] = static_cast<float>(std::sin(6.283185307179586 * 997.0 * static_cast<double>(i) / 48000.0));
    CHECK(kits::loudnessDb(sine, 48000.0) == Catch::Approx(-3.0).margin(0.3));   // BS.1770: −3.01 LKFS
    std::vector<float> half = sine;
    for (float& v : half) v *= 0.5f;
    CHECK(kits::loudnessDb(half, 48000.0) == Catch::Approx(kits::loudnessDb(sine, 48000.0) - 6.02).margin(0.01));
    CHECK(kits::loudnessDb({}, 48000.0) < -100.0);
}

TEST_CASE("DrumKits: loudness cân trong kit (volume ≤ 0, lệch ≤ 0.2 dB)", "[render][kits]") {
    for (const kits::Kit& kt : allKits()) {
        CAPTURE(kt.id);
        double lo = 1e9, hi = -1e9;
        for (const kits::KitPad& p : kt.pads) {
            CAPTURE(p.file, p.loudnessDb, p.volumeDb);
            CHECK(p.loudnessDb == Catch::Approx(kits::loudnessDb(p.samples, kt.sampleRate)).margin(1e-9));
            CHECK(p.volumeDb <= 0.0f);
            CHECK(p.volumeDb >= kits::kMaxCutDb);
            CHECK(std::fabs(p.volumeDb * 10.0f - std::round(p.volumeDb * 10.0f)) < 1e-3f);   // bước 0.1 dB như trong SFZ
            const double eff = p.loudnessDb + static_cast<double>(p.volumeDb);
            lo = std::min(lo, eff);
            hi = std::max(hi, eff);
        }
        CHECK(hi - lo <= 0.2);
        CHECK(lo > -16.0);   // kit không bị kéo quá nhỏ vì một âm quá bé
    }
}

TEST_CASE("DrumKits: tom cao độ tăng dần 41 < 43 < 45 < 47 < 48 < 50 (Yin), perc thấp < cao", "[render][kits]") {
    Yin yin;
    const auto hz = [&](const kits::Kit& kt, int key) {
        const auto& x = pad(kt, key).samples;
        const auto e = yin.analyze(x.data(), static_cast<int64_t>(x.size()), kt.sampleRate);
        CAPTURE(kt.id, key, e.hz, e.confidence);
        CHECK(e.ok);
        return e.hz;
    };
    for (const std::string& id : kDrumKits) {
        const kits::Kit& kt = kit(id);
        float prev = 0.0f;
        for (int key : {41, 43, 45, 47, 48, 50}) {
            const float f = hz(kt, key);
            CAPTURE(id, key, f, prev);
            CHECK(f > prev * 1.08f);   // cách nhau ≥ ~1.3 nửa cung (thiết kế ~3 nửa cung)
            CHECK(f > 60.0f);
            CHECK(f < 300.0f);
            prev = f;
        }
    }
    const kits::Kit& perc = kit("kit_perc");
    CHECK(hz(perc, 36) < hz(perc, 37));   // conga
    CHECK(hz(perc, 37) < hz(perc, 38));
    CHECK(hz(perc, 38) < hz(perc, 39));   // bongo
    CHECK(hz(perc, 46) < hz(perc, 47));   // agogô
    CHECK(hz(perc, 50) < hz(perc, 51));   // timbale
}

TEST_CASE("DrumKits: SFZ nạp bằng SfzLoader — 16 region, one-shot, region_label, volume, choke", "[render][kits]") {
    for (const kits::Kit& kt : allKits()) {
        CAPTURE(kt.id);
        const KitLoader loader(kt);
        const auto r = load(kt, loader);
        INFO(r.message);
        REQUIRE(r.ok);
        CHECK(r.warnings.empty());
        REQUIRE(r.regions == 16);
        CHECK(r.samplesLoaded == 16);
        const auto& zones = r.instrument->zones;
        REQUIRE(zones.size() == 16);
        for (size_t i = 0; i < 16; ++i) {
            const dsp::Zone& z = zones[i];
            const kits::KitPad& p = kt.pads[i];
            CAPTURE(p.file);
            CHECK(z.loKey == p.key);
            CHECK(z.hiKey == p.key);
            CHECK(z.rootKey == p.key);
            CHECK(z.label == p.label);                       // gồm nhãn có dấu cách ("Floor Tom L")
            CHECK(z.loopMode == dsp::LoopMode::OneShot);
            CHECK(z.gainDb == Catch::Approx(p.volumeDb).margin(1e-4));
            CHECK(z.group == p.group);
            CHECK(z.offBy == p.offBy);
            REQUIRE(z.data != nullptr);
            CHECK(z.data->numFrames() == static_cast<int64_t>(p.samples.size()));
        }
    }
    // Choke khai báo đúng: 42/44 (group 1) chặn 46; perc 44 (group 3) chặn 45
    for (const std::string& id : kDrumKits) {
        CHECK(pad(kit(id), 42).group == 1);
        CHECK(pad(kit(id), 44).group == 1);
        CHECK(pad(kit(id), 46).offBy == 1);
        CHECK(pad(kit(id), 46).group != 1);   // hat mở không tự chặn mình / hat đóng
    }
    CHECK(pad(kit("kit_perc"), 44).group == 3);
    CHECK(pad(kit("kit_perc"), 45).offBy == 3);
    CHECK(pad(kit("kit_808"), 41).label == "Floor Tom L");
    CHECK(kit("kit_808").sfzText.find("region_label=Floor Tom L\n") != std::string::npos);
}

TEST_CASE("DrumKits: choke qua Sampler — hat đóng/chân tắt hat mở, triangle tắt tắt triangle mở, không chặn ngược", "[render][kits]") {
    struct Case { std::string id; int choker, choked; };
    std::vector<Case> cases = {{"kit_perc", 44, 45}};
    for (const std::string& id : kDrumKits) {
        cases.push_back({id, 42, 46});
        cases.push_back({id, 44, 46});
    }
    for (const Case& c : cases) {
        CAPTURE(c.id, c.choker, c.choked);
        const kits::Kit& kt = kit(c.id);
        const KitLoader loader(kt);
        const auto r = load(kt, loader);
        REQUIRE(r.ok);

        // Chỉ âm bị chặn (tham chiếu), rồi âm bị chặn + âm chặn ở 50 ms
        dsp::Sampler ref, s;
        for (dsp::Sampler* x : {&ref, &s}) {
            x->prepare(48000.0, 256);
            x->setInstrument(r.instrument.get(), 1);
        }
        std::vector<float> a, b;
        play(ref, c.choked, 2400, &a);
        play(s, c.choked, 2400, &b);
        CHECK(s.activeVoices() == 1);
        play(s, c.choker, 480, &b);                          // 10 ms: đã qua fade choke 5 ms
        CHECK(s.activeVoices() == 1);                        // chỉ còn âm chặn
        play(ref, -1, 480, &a);
        const int tailStart = static_cast<int>(pad(kt, c.choker).samples.size());
        play(s, -1, tailStart + 4800, &b);                   // qua hết âm chặn…
        play(ref, -1, tailStart + 4800, &a);
        CHECK(s.activeVoices() == 0);
        CHECK(ref.activeVoices() == 1);                      // … âm bị chặn nếu không choke vẫn còn ngân
        const size_t from = 2880 + static_cast<size_t>(tailStart);
        CHECK(rmsDb(b, from, b.size()) < -120.0);            // sau khi âm chặn hết: im lặng hoàn toàn
        CHECK(rmsDb(a, from, a.size()) > -60.0);

        // Không chặn ngược: âm chặn đang kêu, âm bị chặn vào → cả hai cùng kêu
        dsp::Sampler rev;
        rev.prepare(48000.0, 256);
        rev.setInstrument(r.instrument.get(), 1);
        play(rev, c.choker, 240);
        play(rev, c.choked, 240);
        CHECK(rev.activeVoices() == 2);
    }
    // Pad không thuộc nhóm choke không chặn hat mở
    const KitLoader loader(kit("kit_808"));
    const auto r = load(kit("kit_808"), loader);
    REQUIRE(r.ok);
    dsp::Sampler s;
    s.prepare(48000.0, 256);
    s.setInstrument(r.instrument.get(), 1);
    play(s, 46, 2400);
    play(s, 36, 480);
    play(s, 38, 480);
    CHECK(s.activeVoices() == 3);
}

TEST_CASE("DrumKits: đặc trưng từng kit P2-35 (trap roll + glide + riser, lo-fi có hiss, Linn side stick)", "[render][kits]") {
    // Trap: hat đóng ngắn để roll 1/64 ở 120 BPM (31 ms) vẫn tách tiếng; kick dài có glide; 51 là Riser
    const kits::Kit& trap = kit("kit_trap");
    CHECK(pad(trap, 42).samples.size() < static_cast<size_t>(0.1 * trap.sampleRate));
    CHECK(pad(trap, 36).label == "808 Kick");
    CHECK(pad(trap, 36).samples.size() > static_cast<size_t>(2.0 * trap.sampleRate));
    CHECK(pad(trap, 51).label == "Riser");
    {   // riser to dần: nửa sau to hơn nửa đầu rõ rệt
        const auto& x = pad(trap, 51).samples;
        CHECK(rmsDb(x, x.size() / 2, x.size() * 3 / 4) > rmsDb(x, 0, x.size() / 4) + 12.0);
    }
    {   // kick glide: đoạn đầu (20–62 ms) cao hơn đoạn sau (400–800 ms) ít nhất 3 nửa cung
        YinConfig cfg;
        cfg.minHz = 30.0f;
        Yin yin(cfg);
        const auto& x = pad(trap, 36).samples;
        const auto early = yin.analyzeFrame(x.data() + 960, trap.sampleRate);
        const auto late = yin.analyze(x.data() + 19200, 19200, trap.sampleRate);
        CAPTURE(early.hz, late.hz);
        CHECK(late.ok);
        CHECK(early.hz > late.hz * 1.19f);
    }
    // Lo-fi: có hiss đĩa than — giữa đuôi crash vẫn còn nhiễu, không im tuyệt đối
    const auto& crash = pad(kit("kit_lofi"), 49).samples;
    CHECK(rmsDb(crash, crash.size() / 2, crash.size() / 2 + 4800) > -90.0);
    CHECK(pad(kit("kit_linn"), 37).label == "Side Stick");
}

TEST_CASE("KitSynth: crush (μ-law / tuyến tính, giữ mẫu) và vinyl tất định", "[render][kits]") {
    using namespace kits::synth;
    const auto ramp = [] {
        Voice v(48000.0, 0.1);
        for (size_t i = 0; i < v.size(); ++i) v.x[i] = static_cast<float>(std::sin(0.013 * static_cast<double>(i)) * 0.7);
        return v;
    };
    Voice a = ramp();
    crush(a, 8, 0.0, true);                      // μ-law 8 bit: 0 + 2 dấu × 128 mức độ lớn
    CHECK(std::set<float>(a.x.begin(), a.x.end()).size() <= 257);
    Voice b = ramp();
    crush(b, 12, 0.0);                           // tuyến tính 12 bit: k / 2048, k ∈ [−2048, 2048]
    CHECK(std::set<float>(b.x.begin(), b.x.end()).size() <= 4097);
    CHECK(std::fabs(b.x[100] - a.x[100]) < 0.02f);
    Voice c = ramp();
    crush(c, 24, 24000.0);                       // giữ mẫu ở sr/2: từng cặp mẫu bằng nhau
    size_t pairs = 0;
    for (size_t i = 0; i + 1 < c.size(); i += 2) pairs += c.x[i] == c.x[i + 1] ? 1 : 0;
    CHECK(pairs == c.size() / 2);
    Voice d1 = ramp(), d2 = ramp();
    vinyl(d1, 7u, 0.01, 50.0);
    vinyl(d2, 7u, 0.01, 50.0);
    CHECK(d1.x == d2.x);
    CHECK(d1.x != ramp().x);
}
