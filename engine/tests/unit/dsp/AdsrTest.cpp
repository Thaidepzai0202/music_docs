// P1-26 — Adsr: từng pha đúng thời gian, không nhảy bậc, fast release đúng N sample.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (giá trị phải đúng chính xác, ví dụ 0 trong vùng đệm).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Adsr.h"

#include <cmath>
#include <vector>

using le::dsp::Adsr;
using Stage = Adsr::Stage;

namespace {
constexpr double kSr = 48000.0;

std::vector<float> run(Adsr& env, int n) {
    std::vector<float> v(static_cast<size_t>(n));
    for (auto& x : v) x = env.next();
    return v;
}
float maxStep(const std::vector<float>& v, float start = 0.0f) {
    float m = std::fabs(v.front() - start);
    for (size_t i = 1; i < v.size(); ++i) m = std::max(m, std::fabs(v[i] - v[i - 1]));
    return m;
}
} // namespace

TEST_CASE("Adsr: attack tuyến tính đúng thời gian, decay về sustain, giữ sustain", "[dsp][adsr]") {
    Adsr env;
    env.start({0.010f, 0.100f, 0.5f, 0.2f}, kSr);   // attack 10 ms = 480 sample
    const auto a = run(env, 480);
    CHECK(a[239] == Catch::Approx(0.5f).margin(0.01));       // giữa attack
    CHECK(a[479] == Catch::Approx(1.0f).margin(1e-5));       // hết attack đúng sample 480
    CHECK(env.stage() == Stage::Decay);

    const auto d = run(env, 4800);                            // decay 100 ms
    // Sau đúng 100 ms: còn cách sustain −60 dB (0.5 · 10^−3)
    CHECK(d.back() - 0.5f == Catch::Approx(0.5e-3f).margin(0.1e-3f));
    run(env, 4800);
    CHECK(env.stage() == Stage::Sustain);
    CHECK(env.level() == Catch::Approx(0.5f));
    const auto s = run(env, 1000);
    CHECK(s.front() == Catch::Approx(0.5f));
    CHECK(s.back() == Catch::Approx(0.5f));
}

TEST_CASE("Adsr: release −60 dB sau đúng release time rồi tắt hẳn", "[dsp][adsr]") {
    Adsr env;
    env.start({0.0f, 0.0f, 1.0f, 0.200f}, kSr);
    run(env, 100);
    CHECK(env.stage() == Stage::Sustain);
    env.noteOff();
    CHECK(env.stage() == Stage::Release);
    const auto r = run(env, 9600);                            // 200 ms
    CHECK(r.back() == Catch::Approx(1e-3f).margin(0.1e-3f));
    CHECK(env.isActive());
    run(env, 4800);                                           // xuống −80 dB (~267 ms) → Idle
    CHECK_FALSE(env.isActive());
    CHECK(env.next() == 0.0f);
}

TEST_CASE("Adsr: sàn chống click — attack 0 vẫn mất 0.5 ms, release 0 vẫn mất ≥ 5 ms", "[dsp][adsr]") {
    Adsr env;
    env.start({0.0f, 0.0f, 1.0f, 0.0f}, kSr);
    const auto a = run(env, 24);                              // 0.5 ms @48k
    CHECK(a.front() < 0.1f);
    CHECK(a.back() == Catch::Approx(1.0f));
    env.noteOff();
    const auto r = run(env, 240);                              // 5 ms: −60 dB, chưa tắt ngay
    CHECK(maxStep(r, 1.0f) < 0.05f);
    CHECK(r.back() == Catch::Approx(1e-3f).margin(0.2e-3f));
}

TEST_CASE("Adsr: decay = 0 với sustain < 1 không nhảy bậc; sustain = 0 thì nốt tự tắt", "[dsp][adsr]") {
    Adsr env;
    env.start({0.001f, 0.0f, 0.3f, 0.1f}, kSr);
    const auto v = run(env, 2000);
    CHECK(maxStep(v) < 0.05f);                                // không có bước 1 → 0.3 trong 1 sample
    CHECK(v.back() == Catch::Approx(0.3f).margin(1e-3f));

    Adsr drum;
    drum.start({0.0f, 0.050f, 0.0f, 0.1f}, kSr);             // sustain 0: kiểu nhạc cụ gõ
    run(drum, 48000);
    CHECK_FALSE(drum.isActive());
}

TEST_CASE("Adsr: fast release tuyến tính về 0 trong đúng N sample", "[dsp][adsr]") {
    Adsr env;
    env.start({0.0f, 0.0f, 0.8f, 1.0f}, kSr);
    run(env, 2000);
    const float before = env.level();
    env.fastRelease(0.003f, kSr);                             // 3 ms = 144 sample
    CHECK(env.stage() == Stage::FastRelease);
    const auto f = run(env, 143);
    CHECK(env.isActive());
    CHECK(maxStep(f, before) <= before / 144.0f + 1e-6f);
    env.next();                                               // sample thứ 144
    CHECK_FALSE(env.isActive());
    env.noteOff();                                            // gọi trên voice đã tắt: vô hại
    CHECK_FALSE(env.isActive());
}

TEST_CASE("Adsr: bấm lại khi đang release → attack từ mức hiện tại, không rơi về 0", "[dsp][adsr]") {
    Adsr env;
    env.start({0.005f, 0.0f, 1.0f, 0.5f}, kSr);
    run(env, 1000);
    env.noteOff();
    run(env, 2000);
    const float mid = env.level();
    REQUIRE(mid > 0.1f);
    REQUIRE(mid < 0.9f);
    env.start({0.005f, 0.0f, 1.0f, 0.5f}, kSr);
    const float next = env.next();
    CHECK(next > mid);
    CHECK(next - mid < 0.01f);
}
