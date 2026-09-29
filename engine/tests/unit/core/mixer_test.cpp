// P1-13 Mixer (luật pan -3 dB, gain, mute/solo ramp 5 ms) · P1-14 Limiter (+6 dBFS → ≤ -0.3 dBFS) + meter.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <vector>

#include "core/Denormals.h"
#include "core/Mixer.h"

using namespace le::core;
using Catch::Approx;

namespace {
// Trộn 1 track tín hiệu hằng `v` trong n sample, trả (L, R) của sample cuối.
std::pair<float, float> mixConst(Mixer& m, int t, float v, int n) {
    std::vector<float> in((size_t) n, v), L((size_t) n, 0.f), R((size_t) n, 0.f), sL((size_t) n), sR((size_t) n);
    m.mixTrack(t, in.data(), in.data(), L.data(), R.data(), sL.data(), sR.data(), n);
    return {L.back(), R.back()};
}
} // namespace

TEST_CASE("Mixer: luật pan -3 dB (công suất không đổi)", "[core][mixer]") {
    REQUIRE(Mixer::panLeft(0.0f) == Approx(0.70710678f).margin(1e-6));
    REQUIRE(Mixer::panRight(0.0f) == Approx(0.70710678f).margin(1e-6));
    REQUIRE(Mixer::panLeft(-1.0f) == Approx(1.0f).margin(1e-6));
    REQUIRE(Mixer::panRight(-1.0f) == Approx(0.0f).margin(1e-6));
    REQUIRE(Mixer::panLeft(1.0f) == Approx(0.0f).margin(1e-6));
    REQUIRE(Mixer::panRight(1.0f) == Approx(1.0f).margin(1e-6));
    for (float p = -1.0f; p <= 1.0f; p += 0.125f) {
        const float l = Mixer::panLeft(p), r = Mixer::panRight(p);
        REQUIRE(l * l + r * r == Approx(1.0f).margin(1e-5));   // tổng công suất = 1 ở mọi vị trí
    }
}

TEST_CASE("Mixer: gain dB, -120 = câm, pan trượt 20 ms", "[core][mixer]") {
    Mixer m;
    m.prepare(48000.0);
    auto [l0, r0] = mixConst(m, 0, 1.0f, 16);
    REQUIRE(l0 == Approx(0.70710678f));
    m.setGainDb(0, -6.0f);
    m.setPan(0, -1.0f);
    auto [l1, r1] = mixConst(m, 0, 1.0f, 960);   // hết ramp 20 ms
    REQUIRE(l1 == Approx(std::pow(10.0f, -6.0f / 20.0f)).margin(1e-5));
    REQUIRE(r1 == Approx(0.0f).margin(1e-6));
    m.setGainDb(0, -120.0f);
    auto [l2, r2] = mixConst(m, 0, 1.0f, 960);
    REQUIRE(l2 == 0.0f);
    REQUIRE(Mixer::dbToLin(6.0f) == Approx(1.9953f).margin(1e-3));
}

TEST_CASE("Mixer: mute / solo ramp 5 ms, có solo thì track khác câm", "[core][mixer]") {
    Mixer m;
    m.prepare(48000.0);
    m.setSolo(1, true);
    // track 0 bị câm dần trong 240 sample (không nhảy)
    std::vector<float> in(240, 1.0f), L(240, 0.f), R(240, 0.f), sL(240), sR(240);
    m.mixTrack(0, in.data(), in.data(), L.data(), R.data(), sL.data(), sR.data(), 240);
    REQUIRE(L[0] < 0.7072f);
    REQUIRE(L[0] > 0.69f);
    for (int i = 1; i < 240; ++i) REQUIRE(L[(size_t) i] <= L[(size_t) i - 1]);   // giảm đều
    REQUIRE(L[239] == Approx(0.0f).margin(1e-6));
    REQUIRE(mixConst(m, 1, 1.0f, 16).first == Approx(0.70710678f));   // track solo vẫn kêu
    m.setSolo(1, false);
    m.setMute(1, true);
    mixConst(m, 0, 1.0f, 240);
    REQUIRE(mixConst(m, 0, 1.0f, 16).first == Approx(0.70710678f));   // hết solo → track 0 kêu lại
    mixConst(m, 1, 1.0f, 240);
    REQUIRE(mixConst(m, 1, 1.0f, 16).first == 0.0f);                  // mute
}

TEST_CASE("Limiter: +6 dBFS vào → ra ≤ -0.3 dBFS, tín hiệu nhỏ đi qua nguyên vẹn", "[core][limiter]") {
    Limiter lim;
    lim.prepare(48000.0);
    const float ceiling = std::pow(10.0f, -0.3f / 20.0f);
    std::vector<float> L(48000), R(48000);
    for (int i = 0; i < 48000; ++i) L[(size_t) i] = R[(size_t) i] = 2.0f * (float) std::sin(2.0 * M_PI * 440.0 * i / 48000.0);   // +6 dBFS
    for (int off = 0; off < 48000; off += 128) lim.process(L.data() + off, R.data() + off, 128);
    float peak = 0;
    for (float x : L) peak = std::max(peak, std::fabs(x));
    REQUIRE(peak <= ceiling + 1e-7f);
    REQUIRE(peak > ceiling * 0.95f);   // không nén quá tay

    Limiter clean;
    clean.prepare(48000.0);
    std::vector<float> a(512), b(512), a0(512);
    for (int i = 0; i < 512; ++i) a[(size_t) i] = b[(size_t) i] = a0[(size_t) i] = 0.5f * (float) std::sin(i * 0.1);
    clean.process(a.data(), b.data(), 512);
    REQUIRE(a == a0);   // dưới ngưỡng: không đổi gì
}

TEST_CASE("Mixer master: gain + limiter + meter peak", "[core][limiter][meter]") {
    Mixer m;
    m.prepare(48000.0);
    std::vector<float> L(4800), R(4800);
    for (int i = 0; i < 4800; ++i) { L[(size_t) i] = 0.5f * (float) std::sin(i * 0.05); R[(size_t) i] = 0.25f * (float) std::sin(i * 0.05); }
    m.processMaster(L.data(), R.data(), 4800);
    REQUIRE(m.masterPeak(0) == Approx(0.5f).margin(1e-3));
    REQUIRE(m.masterPeak(1) == Approx(0.25f).margin(1e-3));
    m.setMasterGainDb(12.0f);   // +12 dB → vượt ngưỡng → limiter
    for (int k = 0; k < 10; ++k) {
        for (int i = 0; i < 4800; ++i) { L[(size_t) i] = 0.5f * (float) std::sin(i * 0.05); R[(size_t) i] = L[(size_t) i]; }
        m.processMaster(L.data(), R.data(), 4800);
    }
    REQUIRE(m.masterPeak(0) <= std::pow(10.0f, -0.3f / 20.0f) + 1e-6f);
}

TEST_CASE("PeakMeter: giữ đỉnh ít nhất 1 cửa sổ 25 ms", "[core][meter]") {
    PeakMeter p;
    p.prepare(48000.0);
    std::vector<float> x(100, 0.0f);
    x[50] = 0.8f;
    p.push(x.data(), 100);
    REQUIRE(p.value() == 0.8f);
    std::vector<float> z(1200, 0.0f);
    p.push(z.data(), 1100);   // vẫn trong cửa sổ đầu / vừa chốt
    REQUIRE(p.value() == 0.8f);
    p.push(z.data(), 1200);
    p.push(z.data(), 1200);
    REQUIRE(p.value() == 0.0f);   // 2 cửa sổ im lặng → về 0
}

TEST_CASE("R2: NaN / Inf ở master bị đổi thành 0 và đếm lại; limiter không bị kéo về 0", "[core][mixer][master]") {
    Mixer m;
    m.prepare(48000.0);
    std::vector<float> L(64, 0.5f), R(64, 0.5f);
    L[10] = std::numeric_limits<float>::quiet_NaN();
    R[20] = std::numeric_limits<float>::infinity();
    R[21] = -std::numeric_limits<float>::infinity();
    m.processMaster(L.data(), R.data(), 64);
    REQUIRE(L[10] == 0.0f);
    REQUIRE(R[20] == 0.0f);
    REQUIRE(R[21] == 0.0f);
    REQUIRE(m.nonFiniteSamples() == 3);
    for (int i = 0; i < 64; ++i) {
        REQUIRE(std::isfinite(L[(size_t) i]));
        REQUIRE(std::isfinite(R[(size_t) i]));
    }
    REQUIRE(L[63] == 0.5f);                            // gain limiter vẫn = 1 (không tụt về 0 như trước)
    REQUIRE(m.limiter().gainReduction() == 1.0f);
}

TEST_CASE("R3: ScopedFlushDenormals đổi subnormal thành 0 trong scope, khôi phục khi ra", "[core][rt]") {
    volatile float tiny = 1e-38f;
    volatile float k = 0.01f;
    {
        const le::core::ScopedFlushDenormals ftz;
        const float y = tiny * k;   // 1e-40: subnormal
        REQUIRE(y == 0.0f);
    }
    const float y = tiny * k;
    REQUIRE(y != 0.0f);
    REQUIRE(std::fpclassify(y) == FP_SUBNORMAL);
}
