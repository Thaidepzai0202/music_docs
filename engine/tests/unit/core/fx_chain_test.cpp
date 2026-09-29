// P3-12 FxChain (crossfade thêm/đổi/xoá 20 ms, bypass dry/wet 10 ms, generation, stash lệnh theo instanceId)
// · P3-15 MasterEq (phẳng → bỏ qua, giống hệt từng bit) + tham số limiter.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

#include "core/FxChain.h"
#include "core/Mixer.h"
#include "dsp/Processor.h"

using namespace le::core;
using Catch::Approx;

namespace {

// Processor giả: y = gain · x (param 0 = gain). Ghi lại input có im lặng không (bypass hẳn → phải nhận im lặng).
struct GainFx final : le::dsp::Processor {
    explicit GainFx(float g) : gain(g) {}
    le::dsp::FxType type() const noexcept override { return le::dsp::FxType::Filter; }
    void prepare(double, int) override {}
    void reset() noexcept [[clang::nonblocking]] override {}
    void process(float* const* io, int numCh, const le::dsp::ProcessContext& ctx) noexcept [[clang::nonblocking]] override {
        ++calls;
        maxFrames = std::max(maxFrames, ctx.numFrames);
        silentInput = true;
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < ctx.numFrames; ++i) {
                if (io[c][i] != 0.0f) silentInput = false;
                io[c][i] *= gain;
            }
    }
    void setParam(int id, float v) noexcept [[clang::nonblocking]] override {
        if (id == 0) gain = v;
    }
    float getParam(int id) const noexcept [[clang::nonblocking]] override { return id == 0 ? gain : 0.0f; }

    float gain;
    int calls = 0;
    int maxFrames = 0;
    bool silentInput = false;
};

le::dsp::ProcessContext ctx48() {
    le::dsp::ProcessContext c;
    c.sampleRate = 48000.0;
    c.bpm = 120.0;
    c.playing = true;
    return c;
}

struct Chain {
    FxChain fx;
    FxSlotSnapshot slots[kFxSlots];
    std::uint32_t gen = 1;
    std::int64_t pos = 0;
    Chain() {
        fx.prepare(48000.0);
        fx.remap(slots, gen);
    }
    std::vector<std::shared_ptr<GainFx>> alive;   // như snapshot cũ trong retiring[]: giữ instance tới hết test
    std::shared_ptr<GainFx> put(int slot, float gain, std::uint32_t id, bool bypass = false) {
        auto p = std::make_shared<GainFx>(gain);
        alive.push_back(p);
        slots[slot].proc = p;
        slots[slot].instanceId = id;
        slots[slot].bypass = bypass;
        return p;
    }
    void clear(int slot) { slots[slot] = FxSlotSnapshot{}; }
    void publish() { fx.remap(slots, ++gen); }
    // Input: sine 500 Hz biên độ 0.5 (L) / 0.25 (R). Trả L; R được kiểm = L/2.
    std::vector<float> run(int n, int block = 128) {
        std::vector<float> L((size_t) n), R((size_t) n);
        for (int i = 0; i < n; ++i) {
            const float x = 0.5f * std::sin(2.0f * 3.14159265f * 500.0f * (float) (pos + i) / 48000.0f);
            L[(size_t) i] = x;
            R[(size_t) i] = 0.5f * x;
        }
        for (int done = 0; done < n; done += block) {
            const int m = std::min(block, n - done);
            le::dsp::ProcessContext c = ctx48();
            c.numFrames = m;
            fx.process(L.data() + done, R.data() + done, m, c);
        }
        for (int i = 0; i < n; ++i) REQUIRE(R[(size_t) i] == Approx(0.5f * L[(size_t) i]).margin(1e-6));
        pos += n;
        return L;
    }
    float dry(std::int64_t at) const { return 0.5f * std::sin(2.0f * 3.14159265f * 500.0f * (float) at / 48000.0f); }
};

float maxJump(const std::vector<float>& x) {
    float j = 0.0f;
    for (size_t i = 1; i < x.size(); ++i) j = std::max(j, std::fabs(x[i] - x[i - 1]));
    return j;
}

constexpr float kSineJump = 0.5f * 2.0f * 3.14159265f * 500.0f / 48000.0f;   // bước lớn nhất của sine dry ≈ 0.0327

} // namespace

TEST_CASE("FxChain: chuỗi trống → bus giữ nguyên từng bit", "[core][fx]") {
    Chain c;
    const auto out = c.run(1000);
    for (int i = 0; i < 1000; ++i) REQUIRE(out[(size_t) i] == c.dry(i));
}

TEST_CASE("FxChain: thêm FX → crossfade dry → wet 20 ms, không click", "[core][fx]") {
    Chain c;
    auto p = c.put(0, 2.0f, 1);
    c.publish();
    const auto out = c.run(2000);
    REQUIRE(c.fx.usesGeneration(c.gen));
    REQUIRE(c.fx.currentInstance(0) == 1);
    REQUIRE_FALSE(c.fx.fading(0));
    // Trong lúc fade: y = x · (1 + k/960) → không có bước nhảy lớn hơn sine gấp đôi
    REQUIRE(maxJump(out) < 2.0f * kSineJump + 1e-3f);
    for (int i = 0; i < 960; ++i) {
        const float k = (float) (i + 1) / 960.0f;
        REQUIRE(out[(size_t) i] == Approx(c.dry(i) * (1.0f + k)).margin(1e-5));
    }
    for (int i = 960; i < 2000; ++i) REQUIRE(out[(size_t) i] == c.dry(i) * 2.0f);   // hết fade: đúng từng bit
    REQUIRE(p->maxFrames <= FxChain::kMaxFxBlock);
}

TEST_CASE("FxChain: đổi FX → cả hai cùng xử lý trong fade, generation cũ giữ tới khi xong", "[core][fx]") {
    Chain c;
    auto a = c.put(0, 2.0f, 1);
    c.publish();
    const std::uint32_t genA = c.gen;
    c.run(2000);
    auto b = c.put(0, 0.5f, 2);
    c.publish();
    const std::uint32_t genB = c.gen;
    REQUIRE(c.fx.usesGeneration(genA));   // chưa bắt đầu fade: A vẫn đang phát
    c.run(480);
    REQUIRE(c.fx.fading(0));
    REQUIRE(c.fx.usesGeneration(genA));
    REQUIRE(c.fx.usesGeneration(genB));
    const int aCalls = a->calls;
    const auto out = c.run(1000);
    REQUIRE_FALSE(c.fx.fading(0));
    REQUIRE_FALSE(c.fx.usesGeneration(genA));   // RtEngine được trả snapshot của A về main
    REQUIRE(a->calls > aCalls);                 // A vẫn chạy tới hết fade…
    const int aDone = a->calls;
    c.run(512);
    REQUIRE(a->calls == aDone);                 // …rồi thôi hẳn
    for (int i = 600; i < 1000; ++i) REQUIRE(out[(size_t) i] == c.dry(2480 + i) * 0.5f);
    (void) b;
}

TEST_CASE("FxChain: xoá FX → fade về dry rồi giống hệt input", "[core][fx]") {
    Chain c;
    c.put(1, 0.25f, 7);
    c.publish();
    const std::uint32_t g0 = c.gen;
    c.run(1500);
    c.clear(1);
    c.publish();
    const auto out = c.run(2000);
    REQUIRE(maxJump(out) < kSineJump + 1e-3f);
    REQUIRE_FALSE(c.fx.usesGeneration(g0));
    REQUIRE(c.fx.currentInstance(1) == 0);
    for (int i = 960; i < 2000; ++i) REQUIRE(out[(size_t) i] == c.dry(1500 + i));
}

TEST_CASE("FxChain: bypass crossfade 10 ms; bypass hẳn → dry đúng bit, FX nhận im lặng", "[core][fx]") {
    Chain c;
    auto p = c.put(0, 2.0f, 3);
    c.publish();
    c.run(1000);
    c.fx.setBypass(0, 3, true);
    auto out = c.run(1000);
    REQUIRE(maxJump(out) < 2.0f * kSineJump + 1e-3f);
    for (int i = 480; i < 1000; ++i) REQUIRE(out[(size_t) i] == c.dry(1000 + i));
    REQUIRE(p->silentInput);   // đuôi tắt tự nhiên, bật lại không nghe vọng cũ
    c.fx.setBypass(0, 3, false);
    out = c.run(1000);
    for (int i = 480; i < 1000; ++i) REQUIRE(out[(size_t) i] == c.dry(2000 + i) * 2.0f);
    REQUIRE_FALSE(p->silentInput);
}

TEST_CASE("FxChain: FX mới vào với bypass = true → vẫn là dry", "[core][fx]") {
    Chain c;
    c.put(2, 3.0f, 4, true);
    c.publish();
    const auto out = c.run(2000);
    for (int i = 0; i < 2000; ++i) REQUIRE(out[(size_t) i] == Approx(c.dry(i)).margin(1e-7));
}

TEST_CASE("FxChain: lệnh tới trước snapshot (theo instanceId) được giữ rồi áp; lệnh cho instance cũ bị bỏ", "[core][fx]") {
    Chain c;
    auto a = c.put(0, 2.0f, 1);
    c.publish();
    c.run(1000);
    // main: fx.set đổi loại (instance 2) rồi kéo knob + bypass NGAY — RT nhận lệnh trước snapshot
    c.fx.setParam(0, 2, 0, 3.0f);
    c.fx.setBypass(0, 2, true);
    REQUIRE(a->gain == 2.0f);   // không áp nhầm sang A
    auto b = c.put(0, 0.5f, 2);
    c.publish();
    REQUIRE(b->gain == 3.0f);
    c.fx.setParam(0, 1, 0, 9.0f);   // lệnh muộn cho A (đang bị thay) → bỏ
    REQUIRE(a->gain == 2.0f);
    const auto out = c.run(2000);
    for (int i = 1000; i < 2000; ++i) REQUIRE(out[(size_t) i] == Approx(c.dry(1000 + i)).margin(1e-7));   // B đã bypass
    c.fx.setParam(0, 2, 0, 4.0f);
    REQUIRE(b->gain == 4.0f);
}

TEST_CASE("FxChain: kết quả không phụ thuộc cỡ block", "[core][fx]") {
    auto render = [](int block) {
        Chain c;
        c.put(0, 2.0f, 1);
        c.put(1, 0.75f, 2);
        c.publish();
        return c.run(3000, block);
    };
    const auto a = render(1024), b = render(37), d = render(4096);
    for (size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i] == b[i]);
        REQUIRE(a[i] == d[i]);
    }
}

TEST_CASE("FxChain: fastReleaseGeneration rút fade còn 3 ms", "[core][fx]") {
    Chain c;
    c.put(0, 2.0f, 1);
    c.publish();
    const std::uint32_t g1 = c.gen;
    c.run(1000);
    c.put(0, 0.5f, 2);
    c.publish();
    c.run(100);
    REQUIRE(c.fx.usesGeneration(g1));
    c.fx.fastReleaseGeneration(g1);
    c.run(144);
    REQUIRE_FALSE(c.fx.usesGeneration(g1));
}

TEST_CASE("MasterEq: phẳng → không xử lý (giống hệt bit); có band ≠ 0 → EQ, về phẳng → tắt lại", "[core][fx][master]") {
    MasterEq eq;
    eq.prepare(48000.0);
    auto run = [&](int n, std::int64_t& pos) {
        std::vector<float> L((size_t) n), R((size_t) n);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = 0.5f * std::sin(0.0654f * (float) (pos + i));
        std::vector<float> in = L;
        for (int done = 0; done < n; done += 128) {
            le::dsp::ProcessContext c = ctx48();
            c.numFrames = std::min(128, n - done);
            eq.process(L.data() + done, R.data() + done, c.numFrames, c);
        }
        pos += n;
        return std::make_pair(in, L);
    };
    std::int64_t pos = 0;
    auto [in0, out0] = run(1000, pos);
    REQUIRE_FALSE(eq.running());
    for (size_t i = 0; i < in0.size(); ++i) REQUIRE(out0[i] == in0[i]);

    eq.setGainDb(1, 12.0f);   // mid +12 dB (1 kHz, Q 0.7) → sine 500 Hz vẫn được nâng rõ
    auto [in1, out1] = run(4800, pos);
    REQUIRE(eq.running());
    float peakIn = 0.0f, peakOut = 0.0f;
    for (size_t i = 2400; i < in1.size(); ++i) {
        peakIn = std::max(peakIn, std::fabs(in1[i]));
        peakOut = std::max(peakOut, std::fabs(out1[i]));
    }
    REQUIRE(peakOut > peakIn * 1.2f);
    REQUIRE(maxJump(out1) < 0.1f);   // bật EQ không click

    eq.setGainDb(1, 0.0f);
    run(4800, pos);   // gain trượt 20 ms + fade 10 ms
    REQUIRE_FALSE(eq.running());
    auto [in2, out2] = run(1000, pos);
    for (size_t i = 0; i < in2.size(); ++i) REQUIRE(out2[i] == in2[i]);

    eq.setGainDb(0, 6.0f);
    eq.setBypass(true);
    auto [in3, out3] = run(1000, pos);
    REQUIRE_FALSE(eq.running());
    for (size_t i = 0; i < in3.size(); ++i) REQUIRE(out3[i] == in3[i]);
    eq.setGainDb(0, 40.0f);
    REQUIRE(eq.gainDb(0) == 15.0f);   // kẹp theo ParamInfo (±15 dB)
}

TEST_CASE("Limiter: trần và release chỉnh được, giữ qua prepare", "[core][mixer][master]") {
    Mixer m;
    m.prepare(48000.0);
    m.setLimiterParam(0, -6.0f);
    std::vector<float> L(4800, 1.0f), R(4800, -1.0f);
    m.processMaster(L.data(), R.data(), 4800);
    const float c = std::pow(10.0f, -6.0f / 20.0f);
    for (size_t i = 0; i < L.size(); ++i) {
        REQUIRE(L[i] <= c + 1e-6f);
        REQUIRE(R[i] >= -c - 1e-6f);
    }
    REQUIRE(m.limiter().ceiling() == Approx(c));
    m.prepare(44100.0);   // restart device
    REQUIRE(m.limiter().ceiling() == Approx(c));
    m.setLimiterParam(0, 5.0f);   // kẹp ≤ 0 dBFS
    REQUIRE(m.limiter().ceiling() == Approx(1.0f));
    m.setLimiterParam(1, 200.0f);   // release: không đổi trần
    REQUIRE(m.limiter().ceiling() == Approx(1.0f));
}
