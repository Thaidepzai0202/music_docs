// WarpRenderer chọn preset STFT theo loại clip (04 §10, 29/09): loop percussive → block ngắn (pre-echo gói sát onset),
// âm có cao độ → preset mặc định (giữ độ phân giải tần số, cao độ < 5 cent). Kết quả tất định, độ dài đúng từng sample.
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_test_macros.hpp>

#include "dsp/AudioData.h"
#include "io/AudioFileIO.h"
#include "render/WarpRenderer.h"
#include "render/Yin.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace le;
using namespace le::render;

namespace {

constexpr double kSr = 48000.0;
constexpr double kPi = 3.14159265358979323846;

// Loop kiểu trống: mỗi beat một burst nhiễu trắng (attack 0.5 ms, decay 25 ms), không có cao độ. Tất định (LCG).
std::shared_ptr<dsp::AudioData> noiseLoop(double bpm, int beats) {
    const double spb = kSr * 60.0 / bpm;
    const auto n = static_cast<int64_t>(std::llround(spb * beats));
    auto d = std::make_shared<dsp::AudioData>(1, n, kSr);
    float* x = d->writePointer(0);
    uint32_t s = 12345;
    for (int b = 0; b < beats; ++b) {
        const auto on = static_cast<int64_t>(std::llround(b * spb));
        for (int64_t i = 0; on + i < n && i < static_cast<int64_t>(0.2 * kSr); ++i) {
            s = s * 1664525u + 1013904223u;
            const double noise = static_cast<double>(s >> 8) / 8388608.0 - 1.0;
            const double t = static_cast<double>(i) / kSr;
            const double env = std::min(1.0, t / 0.0005) * std::exp(-t / 0.025);
            x[on + i] = static_cast<float>(0.6 * env * noise);
        }
    }
    return d;
}

// Nguyên âm tổng hợp: f0 hài âm 1..30, biên độ theo 3 formant (/a/: 700, 1220, 2600 Hz), fade 30 ms hai đầu.
std::shared_ptr<dsp::AudioData> vowel(double f0, double seconds) {
    const auto n = static_cast<int64_t>(seconds * kSr);
    auto d = std::make_shared<dsp::AudioData>(1, n, kSr);
    float* x = d->writePointer(0);
    const double formants[3] = {700.0, 1220.0, 2600.0}, bw[3] = {130.0, 70.0, 160.0}, gain[3] = {1.0, 0.5, 0.25};
    double amp[31] = {};
    double norm = 0.0;
    for (int h = 1; h <= 30; ++h) {
        const double f = f0 * h;
        for (int k = 0; k < 3; ++k) amp[h] += gain[k] / (1.0 + std::pow((f - formants[k]) / bw[k], 2.0));
        norm += amp[h];
    }
    for (int64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kSr;
        double y = 0.0;
        for (int h = 1; h <= 30; ++h) y += amp[h] * std::sin(2.0 * kPi * f0 * h * t);
        const double fade = std::min({1.0, t / 0.03, (seconds - t) / 0.03});
        x[i] = static_cast<float>(0.5 * fade * y / norm);
    }
    return d;
}

// Pre-echo: RMS trong [onset − 15 ms, onset − 5 ms] (dB so với đỉnh burst), trung bình các beat 1..n−1 của output.
double preEchoDb(const dsp::AudioData& out, double bpm, int beats, double fromMs = 15.0, double toMs = 5.0) {
    const float* x = out.channel(0);
    const double spb = kSr * 60.0 / bpm;
    double sum = 0.0;
    int count = 0;
    for (int b = 1; b < beats; ++b) {
        const auto on = static_cast<int64_t>(std::llround(b * spb));
        float peak = 0.0f;
        for (int64_t i = on; i < on + static_cast<int64_t>(0.005 * kSr) && i < out.numFrames(); ++i) peak = std::max(peak, std::fabs(x[i]));
        const auto a = on - static_cast<int64_t>(fromMs * kSr / 1000.0), z = on - static_cast<int64_t>(toMs * kSr / 1000.0);
        double e = 0.0;
        for (int64_t i = a; i < z; ++i) e += static_cast<double>(x[i]) * x[i];
        const double rms = std::sqrt(e / static_cast<double>(z - a));
        sum += 20.0 * std::log10(std::max(1e-12, rms / std::max(1e-12, static_cast<double>(peak))));
        ++count;
    }
    return sum / count;
}

float yinHz(const dsp::AudioData& d) {
    Yin yin;
    return yin.analyze(d.channel(0), d.numFrames(), d.sampleRate()).hz;
}

} // namespace

TEST_CASE("analyzeTransients: loop nhiễu kiểu trống = percussive, nguyên âm = tonal", "[render][warp][preset]") {
    const auto drum = noiseLoop(100.0, 8);
    const TransientStats a = analyzeTransients(*drum);
    INFO("drum onsets/s " << a.onsetsPerSecond << " periodic " << a.periodicFraction);
    CHECK(a.onsets == 8);
    CHECK(a.percussive);
    CHECK(a.periodicFraction < 0.2f);

    const auto v = vowel(220.0, 2.4);
    const TransientStats b = analyzeTransients(*v);
    INFO("vowel onsets/s " << b.onsetsPerSecond << " periodic " << b.periodicFraction);
    CHECK(b.onsets <= 1);
    CHECK(b.periodicFraction > 0.8f);
    CHECK_FALSE(b.percussive);

    // Arpeggio có cao độ (nhiều onset nhưng tuần hoàn) → vẫn tonal: nguyên âm cắt thành 8 nốt ngắt quãng
    auto arp = vowel(220.0, 2.4);
    float* x = arp->writePointer(0);
    for (int64_t i = 0; i < arp->numFrames(); ++i) {
        const double ph = std::fmod(static_cast<double>(i) / (0.3 * kSr), 1.0);   // nốt 300 ms: 250 ms kêu, 50 ms lặng
        x[i] *= static_cast<float>(ph < 0.83 ? std::min(1.0, ph * 0.3 * kSr / 48.0) : 0.0);
    }
    const TransientStats c = analyzeTransients(*arp);
    INFO("arp onsets/s " << c.onsetsPerSecond << " periodic " << c.periodicFraction);
    CHECK(c.onsetsPerSecond >= 2.0);
    CHECK(c.dense);
    CHECK_FALSE(c.percussive);
    // Auto trên arpeggio: nén pre-echo (onset dày) nhưng GIỮ block mặc định → cao độ vẫn < 5 cent
    WarpConfig wc;
    wc.originalBpm = 120.0;
    wc.newBpm = 100.0;
    const WarpResult wr = renderWarp(*arp, wc);
    WarpConfig wt = wc;
    wt.preset = WarpPreset::Tonal;
    const WarpResult wtr = renderWarp(*arp, wt);
    REQUIRE(wr.ok);
    CHECK_FALSE(wr.percussive);
    CHECK(wr.blockSamples == wtr.blockSamples);
    CHECK(wtr.suppressedOnsets == 0);
    const double cents = 1200.0 * std::log2(static_cast<double>(yinHz(*wr.data)) / yinHz(*arp));
    INFO("arp sau warp: " << cents << " cent, nén " << wr.suppressedOnsets << " onset");
    CHECK(std::fabs(cents) < 5.0);

    const dsp::AudioData empty(1, 0, kSr);
    CHECK_FALSE(analyzeTransients(empty).percussive);
}

TEST_CASE("renderWarp Auto: loop percussive → pre-echo 10 ms trước onset giảm rõ, độ dài đúng, tất định", "[render][warp][preset]") {
    const auto drum = noiseLoop(100.0, 8);
    WarpConfig c;
    c.originalBpm = 100.0;
    c.newBpm = 120.0;
    const WarpResult autoR = renderWarp(*drum, c);
    WarpConfig t = c;
    t.preset = WarpPreset::Tonal;
    const WarpResult tonal = renderWarp(*drum, t);
    REQUIRE(autoR.ok);
    REQUIRE(tonal.ok);
    CHECK(autoR.percussive);
    CHECK_FALSE(tonal.percussive);
    CHECK(autoR.blockSamples < tonal.blockSamples);
    const int64_t want = warpedLength(drum->numFrames(), 100.0, 120.0);
    CHECK(autoR.data->numFrames() == want);
    CHECK(tonal.data->numFrames() == want);

    const double preTonal = preEchoDb(*tonal.data, 120.0, 8), preAuto = preEchoDb(*autoR.data, 120.0, 8);
    const double nearTonal = preEchoDb(*tonal.data, 120.0, 8, 8.0, 3.0), nearAuto = preEchoDb(*autoR.data, 120.0, 8, 8.0, 3.0);
    std::printf("  [warp preset] loop nhiễu 100→120: pre-echo [−15, −5] ms: mặc định %.1f dB → percussive %.1f dB · "
                "[−8, −3] ms: %.1f → %.1f dB (block %d → %d sample)\n",
                preTonal, preAuto, nearTonal, nearAuto, tonal.blockSamples, autoR.blockSamples);
    CHECK(preAuto <= preTonal - 10.0);
    CHECK(nearAuto <= nearTonal - 3.0);

    const WarpResult again = renderWarp(*drum, c);   // tất định
    REQUIRE(again.ok);
    CHECK(std::equal(again.data->channel(0), again.data->channel(0) + want, autoR.data->channel(0)));
}

TEST_CASE("renderWarp percussive: nén pre-echo KHÔNG khoét nền liên tục (lớp nhiễu −30 dB giữa các cú đánh)", "[render][warp][preset]") {
    auto drum = noiseLoop(100.0, 8);
    float* x = drum->writePointer(0);
    uint32_t s = 999;
    double bgE = 0.0;
    for (int64_t i = 0; i < drum->numFrames(); ++i) {   // nền nhiễu hằng −30 dB so với burst
        s = s * 1664525u + 1013904223u;
        const float bg = 0.02f * (static_cast<float>(s >> 8) / 8388608.0f - 1.0f);
        x[i] += bg;
        bgE += static_cast<double>(bg) * bg;
    }
    const double bgRms = std::sqrt(bgE / static_cast<double>(drum->numFrames()));
    WarpConfig c;
    c.originalBpm = 100.0;
    c.newBpm = 120.0;
    const WarpResult r = renderWarp(*drum, c);
    REQUIRE(r.ok);
    REQUIRE(r.percussive);
    // Mức nền trong [−15, −5] ms trước mỗi onset của output ≈ mức nền nguồn (±3 dB), không bị kéo xuống
    const float* y = r.data->channel(0);
    const double spb = kSr * 0.5;
    for (int b = 1; b < 8; ++b) {
        const auto on = static_cast<int64_t>(std::llround(b * spb));
        double e = 0.0;
        const auto a = on - static_cast<int64_t>(0.015 * kSr), z = on - static_cast<int64_t>(0.005 * kSr);
        for (int64_t i = a; i < z; ++i) e += static_cast<double>(y[i]) * y[i];
        const double db = 20.0 * std::log10(std::sqrt(e / static_cast<double>(z - a)) / bgRms);
        INFO("beat " << b << ": nền trước onset " << db << " dB so với nền nguồn");
        CHECK(db > -3.0);
        CHECK(db < 6.1);   // nén về ≤ +6 dB so với nguồn
    }
}

TEST_CASE("renderWarp Auto: nguyên âm → preset mặc định, cao độ giữ < 5 cent", "[render][warp][preset]") {
    const auto v = vowel(220.0, 2.4);
    const float inHz = yinHz(*v);
    for (const double newBpm : {100.0, 140.0}) {
        WarpConfig c;
        c.originalBpm = 120.0;
        c.newBpm = newBpm;
        const WarpResult r = renderWarp(*v, c);
        REQUIRE(r.ok);
        CHECK_FALSE(r.percussive);
        WarpConfig t = c;
        t.preset = WarpPreset::Tonal;
        CHECK(r.blockSamples == renderWarp(*v, t).blockSamples);   // đúng preset mặc định
        CHECK(r.data->numFrames() == warpedLength(v->numFrames(), 120.0, newBpm));
        const float outHz = yinHz(*r.data);
        const double cents = 1200.0 * std::log2(static_cast<double>(outHz) / inHz);
        INFO("bpm " << newBpm << " in " << inHz << " Hz out " << outHz << " Hz → " << cents << " cent");
        CHECK(std::fabs(cents) < 5.0);
    }
}

TEST_CASE("renderWarp: blockMs/intervalMs ghi đè preset; Percussive ép block ngắn", "[render][warp][preset]") {
    const auto v = vowel(220.0, 1.0);
    WarpConfig c;
    c.originalBpm = 120.0;
    c.newBpm = 100.0;
    c.preset = WarpPreset::Percussive;
    const WarpResult p = renderWarp(*v, c);
    REQUIRE(p.ok);
    CHECK(p.percussive);
    CHECK(p.blockSamples == static_cast<int>(std::lround(kPercussiveBlockMs * kSr / 1000.0)));
    c.blockMs = 80.0;
    c.intervalMs = 20.0;
    const WarpResult o = renderWarp(*v, c);
    CHECK(o.blockSamples == static_cast<int>(std::lround(80.0 * kSr / 1000.0)));
    CHECK_FALSE(o.percussive);   // ghi đè: không qua preset
}

// Chạy tay: le-tests "[.bench][warp]" — so preset trên loop nhiễu + fixture click của 68.
TEST_CASE("bench: pre-echo theo block/interval", "[.bench][warp]") {
    const auto drum = noiseLoop(100.0, 8);
    const struct { double b, i; } cfgs[] = {{0, 0}, {80, 20}, {60, 15}, {40, 10}, {30, 7.5}, {20, 5}};
    {   // preset percussive đầy đủ (40/10 + nén pre-echo) và preset mặc định + ép Percussive trên fixture click của 68
        WarpConfig c;
        c.originalBpm = 100.0;
        c.newBpm = 120.0;
        c.preset = WarpPreset::Percussive;
        const WarpResult r = renderWarp(*drum, c);
        std::printf("  percussive (40/10 + nén): pre-echo [−15,−5] %6.1f dB · [−8,−3] %6.1f dB · [−30,−15] %6.1f dB · nén %d onset\n",
                    preEchoDb(*r.data, 120.0, 8), preEchoDb(*r.data, 120.0, 8, 8.0, 3.0), preEchoDb(*r.data, 120.0, 8, 30.0, 15.0),
                    r.suppressedOnsets);
    }
    for (const auto& k : cfgs) {
        WarpConfig c;
        c.originalBpm = 100.0;
        c.newBpm = 120.0;
        c.preset = WarpPreset::Tonal;
        c.blockMs = k.b;
        c.intervalMs = k.i;
        const WarpResult r = renderWarp(*drum, c);
        const auto v = vowel(110.0, 2.4);   // nguyên âm TRẦM: cái giá của block ngắn
        WarpConfig cv = c;
        cv.originalBpm = 120.0;
        cv.newBpm = 100.0;
        const WarpResult rv = renderWarp(*v, cv);
        const double cents = 1200.0 * std::log2(static_cast<double>(yinHz(*rv.data)) / yinHz(*v));
        std::printf("  block %5.1f / %4.1f ms: pre-echo [−15,−5] %6.1f dB · [−8,−3] %6.1f dB · [−30,−15] %6.1f dB · vowel 110 Hz %+5.2f cent\n",
                    k.b, k.i, preEchoDb(*r.data, 120.0, 8), preEchoDb(*r.data, 120.0, 8, 8.0, 3.0),
                    preEchoDb(*r.data, 120.0, 8, 30.0, 15.0), cents);
    }
    const auto fx = io::decodeAudioFile(std::string(LE_TEST_FIXTURES_DIR) + "/clip_click_4beats_100.wav");
    if (fx.error == 0) {
        const TransientStats s = analyzeTransients(*fx.data);
        std::printf("  fixture clip_click_4beats_100: onsets/s %.2f · periodic %.2f · percussive %d\n", s.onsetsPerSecond,
                    static_cast<double>(s.periodicFraction), s.percussive ? 1 : 0);
        for (const WarpPreset p : {WarpPreset::Tonal, WarpPreset::Auto, WarpPreset::Percussive}) {
            WarpConfig c;
            c.originalBpm = 100.0;
            c.newBpm = 120.0;
            c.preset = p;
            const WarpResult r = renderWarp(*fx.data, c);
            std::printf("  fixture %s: pre-echo [−15,−5] %6.1f dB · [−8,−3] %6.1f dB · block %d · nén %d onset\n",
                        p == WarpPreset::Tonal ? "Tonal     " : (p == WarpPreset::Auto ? "Auto      " : "Percussive"),
                        preEchoDb(*r.data, 120.0, 4), preEchoDb(*r.data, 120.0, 4, 8.0, 3.0), r.blockSamples, r.suppressedOnsets);
        }
    }
}
