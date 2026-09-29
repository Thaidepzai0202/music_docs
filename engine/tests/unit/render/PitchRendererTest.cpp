// P3-04 (lõi) — PitchRenderer: 13 zone đúng nốt (sau tuneCents), phủ kín bàn phím, RMS chuẩn hoá về zone gốc,
// deterministic, cancel < 100 ms, progress, xử lý nốt gốc do người dùng chọn và input không có cao độ.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (không có cao độ để đo → tune phải đúng bằng 0).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"
#include "le/engine_api.h"
#include "render/PitchRenderer.h"
#include "render/Yin.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <random>
#include <thread>
#include <vector>

using namespace le;
using render::PitchRenderConfig;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSr = 48000.0;

// "la" tổng hợp: hoạ âm 1/h qua formant "a", vibrato nhẹ, hơi thở −35 dB (giống YinTest)
std::vector<float> voice(double f0, double seconds, uint32_t seed = 9) {
    const size_t n = static_cast<size_t>(seconds * kSr);
    std::vector<float> x(n);
    auto formant = [](double f) {
        return std::exp(-std::pow((f - 730.0) / 120.0, 2)) + 0.6 * std::exp(-std::pow((f - 1090.0) / 150.0, 2)) +
               0.25 * std::exp(-std::pow((f - 2440.0) / 250.0, 2)) + 0.15;
    };
    double phase = 0.0, e = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kSr;
        const double f = f0 * std::pow(2.0, 0.15 / 12.0 * std::sin(2 * kPi * 5.0 * t));
        phase += 2 * kPi * f / kSr;
        double s = 0.0;
        for (int h = 1; f0 * h < 5000.0; ++h) s += formant(f * h) / h * std::sin(h * phase);
        const double env = std::min({1.0, t / 0.03, (seconds - t) / 0.05});
        x[i] = static_cast<float>(env * s);
        e += static_cast<double>(x[i]) * x[i];
    }
    const double g = 0.2 / std::sqrt(e / static_cast<double>(n));
    std::mt19937 rng(seed);
    std::normal_distribution<double> gauss(0.0, 1.0);
    for (auto& v : x) v = static_cast<float>(v * g + 0.2 * std::pow(10.0, -35.0 / 20.0) * gauss(rng));
    return x;
}

double rmsDb(const float* x, int64_t n) {
    double e = 0.0;
    for (int64_t i = 0; i < n; ++i) e += static_cast<double>(x[i]) * x[i];
    return 10.0 * std::log10(e / static_cast<double>(n) + 1e-30);
}
} // namespace

TEST_CASE("PitchRenderer: 13 zone đúng nốt tuyệt đối sau tuneCents, phủ kín phím, classicZone = zone 0", "[render][pitch]") {
    const auto in = voice(220.0, 2.0);        // A3 = 57
    PitchRenderConfig cfg;
    const auto r = render::renderPitchInstrument(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    INFO(r.message);
    REQUIRE(r.ok);
    CHECK(r.error == LE_OK);
    CHECK(r.rootNote == 57);
    CHECK_FALSE(r.rootFromUser);
    CHECK(r.confidence > 0.6f);
    const dsp::Instrument& inst = *r.instrument;
    REQUIRE(inst.zones.size() == 13);
    CHECK(inst.mode == dsp::Instrument::Mode::Natural);
    CHECK(inst.zones[static_cast<size_t>(inst.classicZone)].rootKey == 57);
    CHECK(inst.zones.front().loKey == 0);
    CHECK(inst.zones.back().hiKey == 127);
    for (size_t z = 0; z + 1 < inst.zones.size(); ++z)                 // liền nhau, không hở, không chồng
        CHECK(inst.zones[z].hiKey + 1 == inst.zones[z + 1].loKey);
    for (int note = 0; note <= 127; ++note) REQUIRE(inst.findZone(note, 100) != nullptr);

    // Cao độ mỗi zone (dữ liệu · 2^(tune/1200)) phải đúng nốt root + k, sai số < 3 cent
    Yin yin;
    for (size_t z = 0; z < inst.zones.size(); ++z) {
        const dsp::Zone& zone = inst.zones[z];
        CAPTURE(r.semitones[z], zone.tuneCents);
        CHECK(zone.rootKey == 57 + r.semitones[z]);
        const auto p = yin.analyze(zone.data->channel(0) + 24000, 48000, kSr);
        REQUIRE(p.ok);
        const double played = 1200.0 * std::log2(p.hz / 440.0) + 6900.0 + zone.tuneCents;   // cent tuyệt đối
        CHECK(std::fabs(played - 100.0 * zone.rootKey) < 3.0);
        CHECK(zone.loopMode == dsp::LoopMode::NoLoop);
        CHECK(zone.data->numFrames() == static_cast<int64_t>(in.size()));
    }

    // Chuẩn hoá RMS: mọi zone (sau gainDb) to xấp xỉ zone gốc (trong phạm vi kẹp ±12 dB)
    const auto& ref = inst.zones[static_cast<size_t>(inst.classicZone)];
    const double refDb = rmsDb(ref.data->channel(0), ref.data->numFrames()) + ref.gainDb;
    for (const auto& zone : inst.zones) {
        CAPTURE(zone.rootKey, zone.gainDb);
        const double db = rmsDb(zone.data->channel(0), zone.data->numFrames()) + zone.gainDb;
        if (std::fabs(zone.gainDb) < 11.9f) CHECK(std::fabs(db - refDb) < 0.1);
    }
}

TEST_CASE("PitchRenderer: chơi được bằng Sampler — phím bất kỳ ra đúng cao độ", "[render][pitch]") {
    const auto in = voice(196.0, 1.5);        // G3 = 55
    const auto r = render::renderPitchInstrument(in.data(), static_cast<int64_t>(in.size()), kSr, {});
    REQUIRE(r.ok);
    REQUIRE(r.rootNote == 55);
    Yin yin;
    for (int note : {40, 55, 61, 72}) {
        CAPTURE(note);
        dsp::Sampler s;
        s.prepare(kSr, 256);
        s.setInstrument(r.instrument.get(), 1);
        s.noteOn(note, 0.8f);
        std::vector<float> l(48000, 0.0f), rr(48000, 0.0f);
        float* ch[2] = {l.data(), rr.data()};
        for (int i = 0; i < 48000; i += 256) s.render(ch, 2, i, std::min(256, 48000 - i));
        const auto p = yin.analyze(l.data() + 9600, 24000, kSr);
        REQUIRE(p.ok);
        CHECK(p.rootNote == note);
        CHECK(std::fabs(p.cents) < 5.0f);
    }
}

TEST_CASE("PitchRenderer: deterministic — 2 lần render ra dữ liệu giống hệt nhau", "[render][pitch]") {
    const auto in = voice(261.63, 1.0);
    PitchRenderConfig cfg;
    cfg.semitones = {-12, 0, 12};
    const auto a = render::renderPitchInstrument(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    const auto b = render::renderPitchInstrument(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    REQUIRE(a.ok);
    REQUIRE(b.ok);
    for (size_t z = 0; z < 3; ++z) {
        const auto* x = a.instrument->zones[z].data;
        const auto* y = b.instrument->zones[z].data;
        CHECK(std::equal(x->channel(0), x->channel(0) + x->numFrames(), y->channel(0)));
        CHECK(a.zoneTuneCents[z] == Catch::Approx(b.zoneTuneCents[z]));
    }
}

TEST_CASE("PitchRenderer: không dò được cao độ → PITCH_NOT_DETECTED; người dùng chọn nốt → vẫn render", "[render][pitch]") {
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> u(-0.3f, 0.3f);
    std::vector<float> noise(48000);
    for (auto& v : noise) v = u(rng);
    const auto r = render::renderPitchInstrument(noise.data(), 48000, kSr, {});
    CHECK_FALSE(r.ok);
    CHECK(r.error == LE_ERR_PITCH_NOT_DETECTED);
    CHECK(r.instrument == nullptr);

    PitchRenderConfig cfg;
    cfg.rootNote = 60;                        // UI: người dùng chọn C4
    cfg.semitones = {-3, 0, 3};
    const auto u2 = render::renderPitchInstrument(noise.data(), 48000, kSr, cfg);
    REQUIRE(u2.ok);
    CHECK(u2.rootFromUser);
    CHECK(u2.rootNote == 60);
    CHECK(u2.instrument->zones[1].rootKey == 60);
    CHECK(u2.zoneTuneCents[1] == 0.0f);       // không có cao độ để đo → không bù

    // Người dùng chọn nốt KHÁC nốt thật (A3 mà chọn C4): zone vẫn giữ đúng tỉ lệ dịch, tune chỉ bù lỗi Signalsmith
    const auto in = voice(220.0, 1.0);
    const auto w = render::renderPitchInstrument(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    REQUIRE(w.ok);
    for (float t : w.zoneTuneCents) CHECK(std::fabs(t) < 10.0f);

    CHECK(render::renderPitchInstrument(nullptr, 100, kSr, {}).error == LE_ERR_INVALID_ARG);
    CHECK(render::renderPitchInstrument(in.data(), 0, kSr, {}).error == LE_ERR_INVALID_ARG);
}

TEST_CASE("PitchRenderer: cancel dừng < 100 ms, progress tăng tới 1", "[render][pitch]") {
    const auto in = voice(220.0, 4.0);        // mẫu 4 giây như 04 §8
    std::atomic<bool> cancel{false};
    std::atomic<float> progress{0.0f};
    PitchRenderConfig cfg;
    cfg.cancel = &cancel;
    cfg.progress = &progress;

    render::PitchRenderResult r;
    std::chrono::steady_clock::time_point cancelAt, doneAt;
    std::thread worker([&] {
        r = render::renderPitchInstrument(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
        doneAt = std::chrono::steady_clock::now();
    });
    while (progress.load() < 0.2f) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    cancelAt = std::chrono::steady_clock::now();
    cancel = true;
    worker.join();
    const double ms = std::chrono::duration<double, std::milli>(doneAt - cancelAt).count();
    INFO("dừng sau " << ms << " ms");
    CHECK(r.error == LE_ERR_JOB_CANCELLED);
    CHECK_FALSE(r.ok);
    CHECK(ms < 100.0);

    cancel = false;
    progress = 0.0f;
    const auto full = render::renderPitchInstrument(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    REQUIRE(full.ok);
    CHECK(progress.load() == Catch::Approx(1.0f));
    WARN("PitchRenderer 13 zone, mẫu 4 s: " << full.msTotal << " ms (DoD iPad 8: < 2000 ms)");
}
