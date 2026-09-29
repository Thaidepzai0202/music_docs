// P1-08 Transport (08 §3.1): beat ↔ sample, đổi BPM giữ neo, không trôi sau 10^8 sample, toán quantize.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "core/Transport.h"

using le::core::Transport;
using Catch::Approx;

TEST_CASE("Transport: beat ↔ sample ở 120 BPM / 48 kHz", "[core][transport]") {
    Transport t;
    t.prepare(48000.0);
    REQUIRE(t.samplesPerBeat() == 24000.0);
    t.play();
    REQUIRE(t.beatNow() == 0.0);
    REQUIRE(t.sampleAtBeat(4.0) == 96000);
    REQUIRE(t.sampleAtBeat(1.3) == 31200);
    REQUIRE(t.beatAt(96000) == 4.0);
    t.advance(12000);
    REQUIRE(t.beatNow() == 0.5);
}

TEST_CASE("Transport: sampleAtBeat trả sample ĐẦU TIÊN có beat ≥ b", "[core][transport]") {
    Transport t;
    t.prepare(44100.0);
    t.setBpm(123.0);   // spb = 21512.195…
    t.play();
    for (int k = 0; k < 1000; ++k) {
        const auto s = t.sampleAtBeat(k);
        REQUIRE(t.beatAt(s) >= k - 1e-9);
        REQUIRE(t.beatAt(s - 1) < k);
    }
}

TEST_CASE("Transport: không trôi sau 10^8 sample với block lung tung", "[core][transport]") {
    Transport t;
    t.prepare(48000.0);
    t.setBpm(137.0);
    t.play();
    const int blocks[] = {64, 128, 256, 1024, 37, 1, 511};
    std::int64_t total = 0;
    int i = 0;
    while (total < 100'000'000) {
        const int n = blocks[i++ % 7];
        t.advance(n);
        total += n;
    }
    const double expected = (double) total / (60.0 * 48000.0 / 137.0);
    REQUIRE(t.samplePos() == total);
    REQUIRE(std::fabs(t.beatNow() - expected) < 1e-9 * expected);
    REQUIRE(t.sampleAtBeat(t.beatAt(total)) == total);
}

TEST_CASE("Transport: đổi BPM giữ đúng vị trí beat", "[core][transport]") {
    Transport t;
    t.prepare(48000.0);
    t.play();
    t.advance(96000);   // beat 4
    REQUIRE(t.beatNow() == 4.0);
    t.setBpm(90.0);     // spb 32000, neo tại (96000, 4)
    REQUIRE(t.beatNow() == 4.0);
    REQUIRE(t.sampleAtBeat(5.0) == 128000);
    t.advance(32000);
    REQUIRE(t.beatNow() == Approx(5.0).epsilon(1e-12));
    t.setBpm(300.5);    // kẹp 300
    REQUIRE(t.bpm() == 300.0);
    t.setBpm(5.0);
    REQUIRE(t.bpm() == 20.0);
}

TEST_CASE("Transport: play luôn bắt đầu ở beat 0, stop reset vị trí", "[core][transport]") {
    Transport t;
    t.prepare(48000.0);
    t.advance(1000);          // đang dừng: không chạy
    REQUIRE(t.samplePos() == 0);
    t.play();
    t.advance(50000);
    t.play();                 // play khi đang chạy: không làm gì
    REQUIRE(t.samplePos() == 50000);
    t.stop();
    REQUIRE_FALSE(t.playing());
    REQUIRE(t.beatNow() == 0.0);
    t.play();
    REQUIRE(t.beatNow() == 0.0);
}

TEST_CASE("Transport: prepare với SR mới giữ nguyên beat", "[core][transport]") {
    Transport t;
    t.prepare(48000.0);
    t.play();
    t.advance(48000);   // beat 2
    t.prepare(44100.0);
    REQUIRE(t.beatNow() == Approx(2.0));
    t.advance(22050);   // +1 beat ở 44.1k
    REQUIRE(t.beatNow() == Approx(3.0));
}

TEST_CASE("Transport: quantize boundary() ở mọi mức", "[core][transport][quantize]") {
    Transport t;
    t.prepare(48000.0);
    struct Case { int q; double len; };
    const Case cases[] = {{LE_Q_NONE, 0}, {LE_Q_1_16, 0.25}, {LE_Q_1_8, 0.5}, {LE_Q_1_4, 1}, {LE_Q_1_2, 2},
                          {LE_Q_1_BAR, 4}, {LE_Q_2_BAR, 8}, {LE_Q_4_BAR, 16}};
    for (const auto& c : cases) {
        INFO("q = " << c.q);
        t.setQuantize(c.q);
        REQUIRE(t.quantizeLength() == c.len);
        if (c.len == 0) {
            REQUIRE(t.boundary(1.3) == 1.3);   // None: ngay lập tức
            continue;
        }
        REQUIRE(t.boundary(0.0) == 0.0);                           // đứng đúng ranh giới → dùng luôn
        REQUIRE(t.boundary(c.len) == c.len);
        REQUIRE(t.boundary(c.len + 1e-12) == c.len);               // lệch dưới ε → vẫn là ranh giới đó
        REQUIRE(t.boundary(c.len * 0.5) == c.len);
        REQUIRE(t.boundary(c.len * 1.0001) == 2 * c.len);
    }
    t.setQuantize(LE_Q_1_BAR);
    REQUIRE(t.boundary(1.3) == 4.0);   // ví dụ 08 §3.2
    t.setTimeSignature(3, 4);
    REQUIRE(t.boundary(1.3) == 3.0);
    t.setTimeSignature(6, 8);          // beat = nốt móc đơn → 1/16 = 0.5 beat
    t.setQuantize(LE_Q_1_16);
    REQUIRE(t.quantizeLength() == 0.5);
    t.setTimeSignature(0, 5);          // sai → 4/4
    REQUIRE(t.beatsPerBar() == 4);
    REQUIRE(t.beatUnit() == 4);
}
