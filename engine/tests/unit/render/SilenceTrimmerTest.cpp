// P3-02 (lõi) — SilenceTrimmer: giữ đúng 5 ms pre-roll / post-roll, ngưỡng −45 dBFS, có tham số, im lặng → nullptr.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (phần có tiếng phải được giữ nguyên từng bit).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "render/SilenceTrimmer.h"

#include <cmath>
#include <memory>
#include <random>

using le::dsp::AudioData;
using le::render::TrimConfig;

namespace {
constexpr double kSr = 48000.0;

// 0.5 s nhiễu nền −60 dB, "tiếng" DC 0.5 ở [24000, 72000), rồi 0.5 s nhiễu nền
std::shared_ptr<AudioData> take(int channels = 1, int loudChannel = 0) {
    auto d = std::make_shared<AudioData>(channels, 96000, kSr);
    std::mt19937 rng(4);
    std::uniform_real_distribution<float> u(-0.001f, 0.001f);   // −60 dBFS < −45 dBFS
    for (int c = 0; c < channels; ++c)
        for (int64_t i = 0; i < 96000; ++i) d->writePointer(c)[i] = u(rng);
    for (int64_t i = 24000; i < 72000; ++i) d->writePointer(loudChannel)[i] = 0.5f;
    return d;
}
} // namespace

TEST_CASE("SilenceTrimmer: giữ đúng 5 ms (240 sample) pre-roll và post-roll", "[render][trim]") {
    const auto d = take();
    const auto r = le::render::findTrimRange(*d);
    REQUIRE_FALSE(r.silent);
    CHECK(r.start == 24000 - 240);
    CHECK(r.end == 72000 + 240);

    le::render::TrimRange got;
    const auto t = le::render::trimSilence(*d, {}, &got);
    REQUIRE(t != nullptr);
    CHECK(got.start == r.start);
    CHECK(t->numFrames() == 48000 + 480);
    CHECK(t->sampleRate() == kSr);
    CHECK(t->channel(0)[240] == 0.5f);                    // phần có tiếng giữ nguyên
    CHECK(t->channel(0)[240 + 47999] == 0.5f);
    CHECK(t->channel(0)[0] == 0.0f);                      // fade-in trên pre-roll → bắt đầu từ 0
    CHECK(t->channel(0)[t->numFrames() - 1] == 0.0f);     // fade-out trên post-roll
}

TEST_CASE("SilenceTrimmer: tham số — ngưỡng, pre/post-roll, chỉ cắt đầu hoặc cuối, tắt fade", "[render][trim]") {
    const auto d = take();
    TrimConfig c;
    c.preRollMs = 10.0;
    c.postRollMs = 0.0;
    auto r = le::render::findTrimRange(*d, c);
    CHECK(r.start == 24000 - 480);
    CHECK(r.end == 72000);

    c = {};
    c.trimEnd = false;
    r = le::render::findTrimRange(*d, c);
    CHECK(r.start == 24000 - 240);
    CHECK(r.end == 96000);

    c = {};
    c.trimStart = false;
    r = le::render::findTrimRange(*d, c);
    CHECK(r.start == 0);
    CHECK(r.end == 72000 + 240);

    c = {};
    c.thresholdDb = 0.0f;                                  // 0.5 (−6 dBFS) dưới ngưỡng 0 dBFS → coi như im lặng
    CHECK(le::render::findTrimRange(*d, c).silent);
    c.thresholdDb = -70.0f;                                // nhiễu nền −60 dB vượt ngưỡng → gần như không cắt
    r = le::render::findTrimRange(*d, c);
    CHECK(r.start < 100);

    c = {};
    c.fades = false;
    const auto t = le::render::trimSilence(*d, c);
    REQUIRE(t != nullptr);
    CHECK(t->channel(0)[0] == d->channel(0)[24000 - 240]);   // không fade: copy nguyên
}

TEST_CASE("SilenceTrimmer: tiếng sát mép buffer, stereo (tiếng chỉ ở kênh phải), im lặng hoàn toàn", "[render][trim]") {
    auto edge = std::make_shared<AudioData>(1, 1000, kSr);
    for (int i = 0; i < 1000; ++i) edge->writePointer(0)[i] = 0.3f;
    auto r = le::render::findTrimRange(*edge);
    CHECK(r.start == 0);                                    // pre-roll không vượt ra ngoài buffer
    CHECK(r.end == 1000);
    const auto e = le::render::trimSilence(*edge);
    REQUIRE(e != nullptr);
    CHECK(e->channel(0)[0] == 0.3f);                        // không có pre-roll dưới ngưỡng → không fade phần có tiếng

    const auto st = take(2, 1);
    r = le::render::findTrimRange(*st);
    CHECK(r.start == 24000 - 240);
    const auto t = le::render::trimSilence(*st);
    REQUIRE(t != nullptr);
    CHECK(t->numChannels() == 2);
    CHECK(t->channel(1)[240] == 0.5f);

    AudioData silent(1, 48000, kSr);
    le::render::TrimRange sr;
    CHECK(le::render::trimSilence(silent, {}, &sr) == nullptr);
    CHECK(sr.silent);
    CHECK(le::render::findTrimRange(AudioData(1, 0, kSr)).silent);
}
