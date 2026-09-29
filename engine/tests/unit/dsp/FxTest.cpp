// P3-12 → P3-14 (lõi) — FX: đáp ứng tần số ±0.5 dB (Filter, EQ3), delay đúng thời gian theo BPM + feedback +
// ping-pong, compressor đúng đường cong tĩnh, reverb có đuôi / mix 0 nguyên vẹn, làm mượt tham số, metadata.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (passthrough / impulse phải đúng từng sample).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Processor.h"
#include "dsp/fx/CompressorFx.h"
#include "dsp/fx/DelayFx.h"

#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace le::dsp;

namespace {
constexpr double kSr = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int kBlock = 128;

std::unique_ptr<Processor> make(FxType t, std::initializer_list<std::pair<int, float>> params = {}) {
    auto p = createProcessor(t);
    p->prepare(kSr, kBlock);
    for (auto [id, v] : params) p->setParam(id, v);
    p->reset();                                    // như FxChain: nhảy tới giá trị đã đặt trước khi publish
    return p;
}

// Chạy buffer (1 hoặc 2 kênh) qua processor theo block 128, tại chỗ.
void run(Processor& p, std::vector<float>& l, std::vector<float>* r = nullptr, double bpm = 120.0) {
    const int n = static_cast<int>(l.size());
    for (int s = 0; s < n; s += kBlock) {
        const int m = std::min(kBlock, n - s);
        float* io[2] = {l.data() + s, r != nullptr ? r->data() + s : nullptr};
        ProcessContext ctx;
        ctx.numFrames = m;
        ctx.sampleRate = kSr;
        ctx.bpm = bpm;
        p.process(io, r != nullptr ? 2 : 1, ctx);
    }
}

// Gain (dB) ở tần số f: sine qua processor, so RMS nửa sau (bỏ quá độ) với input.
double gainDbAt(Processor& p, double f) {
    p.reset();
    const size_t n = 24000;
    std::vector<float> x(n);
    for (size_t i = 0; i < n; ++i) x[i] = static_cast<float>(0.25 * std::sin(2 * kPi * f * static_cast<double>(i) / kSr));
    const std::vector<float> in = x;
    run(p, x);
    double eo = 0.0, ei = 0.0;
    for (size_t i = n / 2; i < n; ++i) {
        eo += static_cast<double>(x[i]) * x[i];
        ei += static_cast<double>(in[i]) * in[i];
    }
    return 10.0 * std::log10(eo / ei);
}

float maxJump(const std::vector<float>& x, size_t from = 1) {
    float m = 0.0f;
    for (size_t i = std::max<size_t>(1, from); i < x.size(); ++i) m = std::max(m, std::fabs(x[i] - x[i - 1]));
    return m;
}
double levelDb(const std::vector<float>& x, size_t a, size_t b) {
    double e = 0.0;
    for (size_t i = a; i < b; ++i) e += static_cast<double>(x[i]) * x[i];
    return 10.0 * std::log10(e / static_cast<double>(b - a) + 1e-30);
}
} // namespace

TEST_CASE("Fx metadata: bảng tham số, tên, factory, kẹp dải", "[dsp][fx]") {
    struct Case { FxType t; size_t n; const char* name; };
    for (const Case c : {Case{FxType::Filter, 3, "filter"}, Case{FxType::Delay, 4, "delay"}, Case{FxType::Reverb, 4, "reverb"},
                         Case{FxType::EQ3, 3, "eq3"}, Case{FxType::Compressor, 5, "comp"}}) {
        CAPTURE(c.name);
        CHECK(paramInfo(c.t).size() == c.n);
        CHECK(std::string(fxTypeName(c.t)) == c.name);
        FxType back{};
        REQUIRE(fxTypeFromName(c.name, back));
        CHECK(back == c.t);
        auto p = make(c.t);
        CHECK(p->type() == c.t);
        CHECK(p->latencySamples() == 0);
        for (const ParamInfo& info : paramInfo(c.t)) {
            CHECK(p->getParam(info.id) == Catch::Approx(info.defaultValue));   // mặc định đúng bảng
            p->setParam(info.id, info.max + 1000.0f);
            CHECK(p->getParam(info.id) == info.max);                            // kẹp trên
            p->setParam(info.id, info.min - 1000.0f);
            CHECK(p->getParam(info.id) == info.min);                            // kẹp dưới
        }
        p->setParam(99, 1.0f);                                                  // id lạ: bỏ qua, không crash
    }
    FxType alias{};
    REQUIRE(fxTypeFromName("compressor", alias));                               // bí danh cũ vẫn đọc được
    CHECK(alias == FxType::Compressor);
    CHECK(std::string(fxTypeName(alias)) == "comp");                            // nhưng xuất ra luôn là tên chuẩn
    FxType dummy{};
    CHECK_FALSE(fxTypeFromName("chorus", dummy));
    CHECK_FALSE(fxTypeFromName("Comp", dummy));                                 // phân biệt hoa thường
    CHECK(clampParam(FxType::Filter, 0, 1.4f) == 1.0f);                         // tham số nguyên được làm tròn
}

TEST_CASE("Filter SVF: biên độ tại tần số cắt đúng ±0.5 dB (LP/HP −3 dB, BP 0 dB, cộng hưởng Q)", "[dsp][fx][filter]") {
    auto lp = make(FxType::Filter, {{0, 0.0f}, {1, 1000.0f}, {2, 0.707f}});
    CHECK(gainDbAt(*lp, 1000.0) == Catch::Approx(-3.01).margin(0.5));
    CHECK(gainDbAt(*lp, 100.0) == Catch::Approx(0.0).margin(0.5));
    CHECK(gainDbAt(*lp, 10000.0) < -35.0);                                       // 12 dB/quãng tám

    auto hp = make(FxType::Filter, {{0, 1.0f}, {1, 1000.0f}, {2, 0.707f}});
    CHECK(gainDbAt(*hp, 1000.0) == Catch::Approx(-3.01).margin(0.5));
    CHECK(gainDbAt(*hp, 10000.0) == Catch::Approx(0.0).margin(0.5));
    CHECK(gainDbAt(*hp, 100.0) < -35.0);

    auto bp = make(FxType::Filter, {{0, 2.0f}, {1, 2000.0f}, {2, 2.0f}});
    CHECK(gainDbAt(*bp, 2000.0) == Catch::Approx(0.0).margin(0.5));
    CHECK(gainDbAt(*bp, 500.0) < -10.0);
    CHECK(gainDbAt(*bp, 8000.0) < -10.0);

    auto res = make(FxType::Filter, {{0, 0.0f}, {1, 500.0f}, {2, 4.0f}});
    CHECK(gainDbAt(*res, 500.0) == Catch::Approx(20.0 * std::log10(4.0)).margin(0.5));   // đỉnh cộng hưởng = Q
}

TEST_CASE("Filter: đổi mode và quét cutoff khi đang phát không click", "[dsp][fx][filter]") {
    auto f = make(FxType::Filter, {{0, 0.0f}, {1, 800.0f}, {2, 0.707f}});
    std::vector<float> x(48000);
    for (size_t i = 0; i < x.size(); ++i) x[i] = static_cast<float>(0.5 * std::sin(2 * kPi * 600.0 * static_cast<double>(i) / kSr));
    const float natural = 0.5f * static_cast<float>(2 * kPi * 600.0 / kSr);
    std::vector<float> a(x.begin(), x.begin() + 12000), b(x.begin() + 12000, x.begin() + 24000),
        c(x.begin() + 24000, x.end());
    run(*f, a);
    f->setParam(0, 1.0f);                                    // LP → HP (crossfade 5 ms)
    run(*f, b);
    f->setParam(1, 12000.0f);                                // quét cutoff lên 12 kHz (trượt 20 ms)
    run(*f, c);
    std::vector<float> all = a;
    all.insert(all.end(), b.begin(), b.end());
    all.insert(all.end(), c.begin(), c.end());
    for (float v : all) REQUIRE(std::isfinite(v));
    CHECK(maxJump(all, 2000) < 1.5f * natural);
}

TEST_CASE("EQ3: 0 dB = nguyên vẹn; shelf đạt nửa gain tại tần số góc, peak đạt đủ gain tại 1 kHz (±0.5 dB)", "[dsp][fx][eq]") {
    auto flat = make(FxType::EQ3);
    for (double f : {50.0, 1000.0, 12000.0}) CHECK(gainDbAt(*flat, f) == Catch::Approx(0.0).margin(0.05));

    auto low = make(FxType::EQ3, {{0, 12.0f}});
    CHECK(gainDbAt(*low, 30.0) == Catch::Approx(12.0).margin(0.5));
    CHECK(gainDbAt(*low, 200.0) == Catch::Approx(6.0).margin(0.5));        // tần số góc: nửa gain (S = 1)
    CHECK(gainDbAt(*low, 5000.0) == Catch::Approx(0.0).margin(0.5));

    auto mid = make(FxType::EQ3, {{1, 12.0f}});
    CHECK(gainDbAt(*mid, 1000.0) == Catch::Approx(12.0).margin(0.5));
    CHECK(std::fabs(gainDbAt(*mid, 60.0)) < 1.0);
    CHECK(std::fabs(gainDbAt(*mid, 15000.0)) < 1.0);

    auto high = make(FxType::EQ3, {{2, -12.0f}});
    CHECK(gainDbAt(*high, 5000.0) == Catch::Approx(-6.0).margin(0.5));
    CHECK(gainDbAt(*high, 18000.0) == Catch::Approx(-12.0).margin(1.0));
    CHECK(gainDbAt(*high, 100.0) == Catch::Approx(0.0).margin(0.5));

    // Đổi gain lúc đang phát: trượt 20 ms, không nhảy bậc
    auto eq = make(FxType::EQ3);
    std::vector<float> x(24000);
    for (size_t i = 0; i < x.size(); ++i) x[i] = static_cast<float>(0.3 * std::sin(2 * kPi * 1000.0 * static_cast<double>(i) / kSr));
    std::vector<float> a(x.begin(), x.begin() + 6000), b(x.begin() + 6000, x.end());
    run(*eq, a);
    eq->setParam(1, 15.0f);
    run(*eq, b);
    a.insert(a.end(), b.begin(), b.end());
    const float natural = 0.3f * static_cast<float>(2 * kPi * 1000.0 / kSr) * std::pow(10.0f, 15.0f / 20.0f);
    CHECK(maxJump(a) < 1.2f * natural);
}

TEST_CASE("Delay: đúng thời gian theo BPM (1/8 @120 = 12000 sample), feedback, kẹp 4 s", "[dsp][fx][delay]") {
    auto d = make(FxType::Delay, {{0, 4.0f}, {1, 0.0f}, {2, 1.0f}});   // 1/8, không feedback, chỉ wet
    std::vector<float> x(30000, 0.0f);
    x[0] = 1.0f;
    run(*d, x);
    CHECK(x[12000] == Catch::Approx(1.0f));                             // tiếng vọng đúng sample 12000
    double rest = 0.0;
    for (size_t i = 0; i < x.size(); ++i) if (i != 12000) rest += std::fabs(x[i]);
    CHECK(rest < 1e-6);

    auto* dly = static_cast<DelayFx*>(d.get());
    d->setParam(0, 8.0f);                                                // 1/4 chấm @100 BPM = 1.5 · 0.6 s
    CHECK(dly->delaySamplesFor(100.0) == Catch::Approx(43200.0));
    d->setParam(0, 10.0f);                                               // 1/2 @20 BPM = 6 s → kẹp 4 s
    CHECK(dly->delaySamplesFor(20.0) <= 4.0 * kSr);

    // Feedback 0.5: vọng thứ 2 ở 24000, tổng biên độ ≈ 0.5 (LP trong vòng feedback giữ nguyên DC gain)
    auto fb = make(FxType::Delay, {{0, 4.0f}, {1, 0.5f}, {2, 1.0f}});
    std::vector<float> y(40000, 0.0f);
    y[0] = 1.0f;
    run(*fb, y);
    double echo2 = 0.0, echo3 = 0.0;
    for (size_t i = 23990; i < 24500; ++i) echo2 += y[i];
    for (size_t i = 35990; i < 36500; ++i) echo3 += y[i];
    CHECK(y[12000] == Catch::Approx(1.0f));                             // vọng đầu nguyên vẹn
    CHECK(echo2 == Catch::Approx(0.5).margin(0.02));
    CHECK(echo3 == Catch::Approx(0.25).margin(0.02));
}

TEST_CASE("Delay: ping-pong vọng trái → phải → trái; đổi BPM trượt mượt", "[dsp][fx][delay]") {
    auto d = make(FxType::Delay, {{0, 4.0f}, {1, 0.5f}, {2, 1.0f}, {3, 1.0f}});
    std::vector<float> l(40000, 0.0f), r(40000, 0.0f);
    l[0] = r[0] = 1.0f;                                                  // mono vào
    run(*d, l, &r);
    auto sum = [](const std::vector<float>& v, size_t a) { double s = 0; for (size_t i = a - 10; i < a + 500; ++i) s += v[i]; return s; };
    CHECK(sum(l, 12000) == Catch::Approx(1.0).margin(0.02));
    CHECK(std::fabs(sum(r, 12000)) < 1e-6);
    CHECK(std::fabs(sum(l, 24000)) < 1e-6);
    CHECK(sum(r, 24000) == Catch::Approx(0.5).margin(0.02));
    CHECK(sum(l, 36000) == Catch::Approx(0.25).margin(0.02));

    // Đổi BPM 120 → 90 giữa chừng: không NaN, không nhảy bậc lớn (thời gian trượt 100 ms)
    auto t = make(FxType::Delay, {{0, 4.0f}, {1, 0.4f}, {2, 0.5f}});
    std::vector<float> x(96000);
    for (size_t i = 0; i < x.size(); ++i) x[i] = static_cast<float>(0.4 * std::sin(2 * kPi * 330.0 * static_cast<double>(i) / kSr));
    std::vector<float> a(x.begin(), x.begin() + 48000), b(x.begin() + 48000, x.end());
    run(*t, a, nullptr, 120.0);
    run(*t, b, nullptr, 90.0);
    for (float v : b) REQUIRE(std::isfinite(v));
    CHECK(maxJump(b) < 0.1f);
}

TEST_CASE("Compressor: đường cong tĩnh, −6 dBFS qua ngưỡng −18 ratio 4 → ra −15 dBFS (±0.5 dB)", "[dsp][fx][comp]") {
    CHECK(CompressorFx::computeReductionDb(-6.0f, -18.0f, 4.0f) == Catch::Approx(9.0f));
    CHECK(CompressorFx::computeReductionDb(-30.0f, -18.0f, 4.0f) == 0.0f);
    CHECK(CompressorFx::computeReductionDb(-18.0f, -18.0f, 4.0f) == Catch::Approx(0.75f * 3.0f * 3.0f / 12.0f));   // giữa knee
    CHECK(CompressorFx::computeReductionDb(-6.0f, -18.0f, 1.0f) == 0.0f);   // ratio 1 = không nén

    // Tín hiệu mức không đổi (DC 0.5 = −6.02 dBFS) để đo trạng thái ổn định chính xác
    auto c = make(FxType::Compressor, {{0, -18.0f}, {1, 4.0f}, {2, 5.0f}, {3, 50.0f}, {4, 0.0f}});
    std::vector<float> x(24000, 0.5f);
    run(*c, x);
    const double outDb = 20.0 * std::log10(static_cast<double>(x.back()));
    CHECK(outDb == Catch::Approx(-6.02 - 0.75 * 12.04).margin(0.5));
    CHECK(static_cast<CompressorFx*>(c.get())->gainReductionDb() == Catch::Approx(9.03f).margin(0.1f));

    c->setParam(4, 6.0f);                                                   // makeup +6 dB
    std::vector<float> y(4800, 0.5f);
    run(*c, y);
    CHECK(20.0 * std::log10(static_cast<double>(y.back())) == Catch::Approx(outDb + 6.0).margin(0.5));

    auto quiet = make(FxType::Compressor);
    std::vector<float> q(9600, 0.03f);                                      // −30 dBFS < ngưỡng −18
    run(*quiet, q);
    CHECK(q.back() == Catch::Approx(0.03f).epsilon(0.01));

    // Attack 10 ms: sau 30 ms (3 hằng số thời gian) đã nén ≥ 90 % lượng cần nén
    auto att = make(FxType::Compressor, {{2, 10.0f}});
    std::vector<float> step(1440, 0.5f);
    run(*att, step);
    CHECK(static_cast<CompressorFx*>(att.get())->gainReductionDb() > 0.9f * 9.03f);
}

TEST_CASE("Reverb: mix 0 nguyên vẹn; impulse có đuôi tắt dần, stereo rộng; reset xoá đuôi", "[dsp][fx][reverb]") {
    auto dry = make(FxType::Reverb, {{3, 0.0f}});
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-0.5f, 0.5f);
    std::vector<float> l(4800), r(4800);
    for (size_t i = 0; i < l.size(); ++i) { l[i] = u(rng); r[i] = u(rng); }
    const auto l0 = l, r0 = r;
    run(*dry, l, &r);
    CHECK(l == l0);
    CHECK(r == r0);

    auto rev = make(FxType::Reverb, {{0, 0.8f}, {3, 1.0f}});
    std::vector<float> il(96000, 0.0f), ir(96000, 0.0f);
    il[0] = ir[0] = 1.0f;
    run(*rev, il, &ir);
    for (float v : il) REQUIRE(std::isfinite(v));
    const double early = levelDb(il, 4800, 28800), late = levelDb(il, 72000, 96000);
    INFO("đuôi: 0.1–0.6 s " << early << " dB, 1.5–2 s " << late << " dB");
    CHECK(early > -80.0);                                                    // có đuôi
    CHECK(late < early - 10.0);                                              // và tắt dần
    CHECK(il != ir);                                                         // width 1 → 2 kênh khác nhau

    rev->reset();
    std::vector<float> zl(4800, 0.0f), zr(4800, 0.0f);
    run(*rev, zl, &zr);
    for (float v : zl) REQUIRE(v == 0.0f);
}

TEST_CASE("Mọi FX: mono/stereo, nhiễu ngẫu nhiên, kích thước block bất kỳ → không NaN", "[dsp][fx]") {
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (int t = 0; t < kNumFxTypes; ++t) {
        CAPTURE(fxTypeName(static_cast<FxType>(t)));
        auto p = make(static_cast<FxType>(t));
        for (int numCh : {1, 2}) {
            std::vector<float> l(kBlock), r(kBlock);
            for (int iter = 0; iter < 200; ++iter) {
                const int n = 1 + static_cast<int>(rng() % kBlock);
                for (int i = 0; i < n; ++i) { l[static_cast<size_t>(i)] = u(rng); r[static_cast<size_t>(i)] = u(rng); }
                float* io[2] = {l.data(), r.data()};
                ProcessContext ctx;
                ctx.numFrames = n;
                ctx.bpm = 60.0 + iter;
                if (iter % 50 == 25) for (const ParamInfo& info : paramInfo(p->type())) p->setParam(info.id, info.min + (info.max - info.min) * 0.7f);
                p->process(io, numCh, ctx);
                for (int i = 0; i < n; ++i) {
                    REQUIRE(std::isfinite(l[static_cast<size_t>(i)]));
                    REQUIRE(std::isfinite(r[static_cast<size_t>(i)]));
                }
            }
        }
    }
}
