// P0-08 — LoadGenerator: đúng số voice, cộng dồn đúng, không clip, không NaN.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "spike/measure/LoadGenerator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using le::spike::LoadGenerator;

namespace {
struct Block {
    std::vector<float> l, r;
    float* ch[2];
    explicit Block(int n, float fill = 0.0f) : l(static_cast<size_t>(n), fill), r(static_cast<size_t>(n), fill) {
        ch[0] = l.data();
        ch[1] = r.data();
    }
    void clear(float v = 0.0f) {
        std::fill(l.begin(), l.end(), v);
        std::fill(r.begin(), r.end(), v);
    }
};
} // namespace

TEST_CASE("LoadGenerator: 0 voice không cộng gì vào output", "[spike][load]") {
    LoadGenerator gen;
    gen.prepare(48000.0, 128);
    Block b(128, 0.25f);
    for (int i = 0; i < 10; ++i) gen.processRt(b.ch, 2, 128);
    CHECK(gen.activeVoices() == 0);
    for (float x : b.l) REQUIRE(x == 0.25f);
    for (float x : b.r) REQUIRE(x == 0.25f);
}

TEST_CASE("LoadGenerator: số voice đúng theo setVoices, kẹp vào 0..128", "[spike][load]") {
    LoadGenerator gen;
    gen.prepare(48000.0, 256);
    Block b(256);

    gen.setVoices(64);
    gen.processRt(b.ch, 2, 256);
    CHECK(gen.activeVoices() == 64);

    // Chạy 5 giây: voice liên tục hết nốt rồi bấm nốt mới, số voice phải giữ nguyên 64
    for (int i = 0; i < 48000 * 5 / 256; ++i) {
        b.clear();
        gen.processRt(b.ch, 2, 256);
        REQUIRE(gen.activeVoices() == 64);
    }

    // Giảm xuống 16: voice thừa fade 3 ms rồi tắt → sau ~20 ms còn đúng 16
    gen.setVoices(16);
    for (int i = 0; i < 4; ++i) gen.processRt(b.ch, 2, 256);
    CHECK(gen.activeVoices() == 16);

    gen.setVoices(1000);
    CHECK(gen.targetVoices() == LoadGenerator::kMaxVoices);
    gen.processRt(b.ch, 2, 256);
    CHECK(gen.activeVoices() == LoadGenerator::kMaxVoices);

    gen.setVoices(-3);
    CHECK(gen.targetVoices() == 0);
    for (int i = 0; i < 4; ++i) gen.processRt(b.ch, 2, 256);
    CHECK(gen.activeVoices() == 0);
}

TEST_CASE("LoadGenerator: 128 voice trong 10 giây không NaN, không clip, có tiếng ở cả 2 kênh", "[spike][load]") {
    LoadGenerator gen;
    gen.prepare(48000.0, 128);
    gen.setVoices(128);
    Block b(128);
    float peak = 0.0f;
    double energyL = 0.0, energyR = 0.0;
    for (int i = 0; i < 48000 * 10 / 128; ++i) {
        b.clear();
        gen.processRt(b.ch, 2, 128);
        for (int s = 0; s < 128; ++s) {
            const float l = b.l[static_cast<size_t>(s)], r = b.r[static_cast<size_t>(s)];
            REQUIRE(std::isfinite(l));
            REQUIRE(std::isfinite(r));
            peak = std::max({peak, std::fabs(l), std::fabs(r)});
            energyL += static_cast<double>(l) * l;
            energyR += static_cast<double>(r) * r;
        }
    }
    CHECK(peak < 1.0f);
    CHECK(energyL > 0.0);
    CHECK(energyR > 0.0);
}

TEST_CASE("LoadGenerator: CỘNG DỒN vào output (không ghi đè) và lặp lại được với cùng seed", "[spike][load]") {
    LoadGenerator a, b;
    a.prepare(48000.0, 128, 42);
    b.prepare(48000.0, 128, 42);
    a.setVoices(32);
    b.setVoices(32);
    Block onZero(128, 0.0f), onOne(128, 1.0f);
    for (int i = 0; i < 100; ++i) {
        onZero.clear(0.0f);
        onOne.clear(1.0f);
        a.processRt(onZero.ch, 2, 128);
        b.processRt(onOne.ch, 2, 128);
        for (int s = 0; s < 128; ++s) {
            REQUIRE(onOne.l[static_cast<size_t>(s)] - 1.0f == Catch::Approx(onZero.l[static_cast<size_t>(s)]).margin(1e-6));
            REQUIRE(onOne.r[static_cast<size_t>(s)] - 1.0f == Catch::Approx(onZero.r[static_cast<size_t>(s)]).margin(1e-6));
        }
    }
}

TEST_CASE("LoadGenerator: mono output chỉ ghi kênh 0", "[spike][load]") {
    LoadGenerator gen;
    gen.prepare(48000.0, 64);
    gen.setVoices(8);
    std::vector<float> mono(64, 0.0f);
    float* ch[1] = {mono.data()};
    double e = 0.0;
    for (int i = 0; i < 50; ++i) {
        std::fill(mono.begin(), mono.end(), 0.0f);
        gen.processRt(ch, 1, 64);
        for (float x : mono) e += static_cast<double>(x) * x;
    }
    CHECK(e > 0.0);
    CHECK(gen.activeVoices() == 8);
}

TEST_CASE("LoadGenerator: chi phí tăng theo số voice (thông tin, không assert thời gian)", "[spike][load][.perf]") {
    // Chạy tay: le-tests "[.perf]". Số trên Mac chỉ để so sánh tương đối, số thật phải đo trên iPad 8.
    for (int voices : {0, 16, 32, 64, 96, 128}) {
        LoadGenerator gen;
        gen.prepare(48000.0, 128);
        gen.setVoices(voices);
        Block b(128);
        const auto t0 = std::chrono::steady_clock::now();
        const int blocks = 48000 * 5 / 128;
        for (int i = 0; i < blocks; ++i) {
            b.clear();
            gen.processRt(b.ch, 2, 128);
        }
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        WARN(voices << " voice: " << (100.0 * sec / 5.0) << "% của 1 core (Mac)");
    }
}
