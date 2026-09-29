// Chặn NaN / Inf phía dsp/ (R2 của engine/tools/docs/rt-review-2026-09-29.md): tiêm NaN / Inf vào input của mọi FX,
// vào dữ liệu sample của Sampler và dữ liệu clip của AudioClipPlayer → output luôn hữu hạn (ngay trong block đó),
// state tự reset, block sau chạy lại bình thường. Chạy cả ở mac-rtsan (process / render là [[clang::nonblocking]]).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_test_macros.hpp>

#include "dsp/AudioClipPlayer.h"
#include "dsp/Instrument.h"
#include "dsp/Processor.h"
#include "dsp/Sampler.h"
#include "dsp/Sanitize.h"

#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <vector>

using namespace le::dsp;

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 256;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

bool finite(const std::vector<float>& v) {
    for (float x : v)
        if (!std::isfinite(x)) return false;
    return true;
}

double energy(const std::vector<float>& v) {
    double e = 0.0;
    for (float x : v) e += static_cast<double>(x) * x;
    return e;
}

struct Block {
    std::vector<float> l = std::vector<float>(kBlock), r = std::vector<float>(kBlock);
    float* io[2] = {l.data(), r.data()};
    void noise(std::mt19937& rng, float amp = 0.3f) {
        std::uniform_real_distribution<float> u(-amp, amp);
        for (int i = 0; i < kBlock; ++i) {
            l[static_cast<size_t>(i)] = u(rng);
            r[static_cast<size_t>(i)] = u(rng);
        }
    }
};

// Tham số làm FX "ướt" nhất có thể (mix 100 %, feedback cao) → NaN có đường vào state lâu nhất.
void wetParams(Processor& p) {
    for (const ParamInfo& info : paramInfo(p.type())) p.setParam(info.id, info.defaultValue);
    switch (p.type()) {
        case FxType::Delay: p.setParam(1, 0.9f); p.setParam(2, 1.0f); break;    // feedback, mix
        case FxType::Reverb: p.setParam(0, 0.9f); p.setParam(3, 1.0f); break;   // size, mix
        case FxType::EQ3: p.setParam(0, 12.0f); p.setParam(2, 12.0f); break;
        case FxType::Filter: p.setParam(1, 1000.0f); p.setParam(2, 8.0f); break;   // cutoff, cộng hưởng cao
        case FxType::Compressor: p.setParam(0, -40.0f); p.setParam(1, 20.0f); break;
    }
}

std::shared_ptr<AudioData> sine(int64_t frames, double hz, float amp = 0.5f) {
    auto d = std::make_shared<AudioData>(1, frames, kSr);
    for (int64_t i = 0; i < frames; ++i)
        d->writePointer(0)[i] = amp * static_cast<float>(std::sin(2.0 * 3.14159265358979 * hz * static_cast<double>(i) / kSr));
    return d;
}

} // namespace

TEST_CASE("Sanitize: allFinite phát hiện NaN / ±Inf, sanitize chỉ sửa mẫu hỏng", "[dsp][nan]") {
    std::vector<float> x = {0.1f, -0.2f, 3.0e38f, -0.0f, 0.5f};
    CHECK(allFinite(x.data(), 5));
    x[2] = kInf;
    CHECK_FALSE(allFinite(x.data(), 5));
    x[2] = -kInf;
    CHECK_FALSE(allFinite(x.data(), 5));
    x[2] = kNaN;
    CHECK_FALSE(allFinite(x.data(), 5));
    CHECK(sanitize(x.data(), 5) == 1);
    CHECK(x == std::vector<float>{0.1f, -0.2f, 0.0f, -0.0f, 0.5f});
    CHECK(allFinite(x.data(), 0));
    float l[3] = {1, 2, 3}, r[3] = {kNaN, 1, kInf};
    CHECK(sanitizeIfNeeded(l, r, 3));
    CHECK((l[0] == 1 && r[0] == 0 && r[1] == 1 && r[2] == 0));
    CHECK_FALSE(sanitizeIfNeeded(l, nullptr, 3));
}

TEST_CASE("FX: NaN / Inf ở input → output hữu hạn NGAY block đó, reset, rồi chạy lại bình thường", "[dsp][fx][nan]") {
    for (int t = 0; t < kNumFxTypes; ++t) {
        const auto type = static_cast<FxType>(t);
        CAPTURE(fxTypeName(type));
        auto p = createProcessor(type);
        p->prepare(kSr, kBlock);
        wetParams(*p);
        p->reset();
        std::mt19937 rng(42);
        ProcessContext ctx;
        ctx.numFrames = kBlock;
        ctx.sampleRate = kSr;
        Block b;
        for (int k = 0; k < 20; ++k) {   // làm đầy state (đuôi reverb / delay line)
            b.noise(rng);
            p->process(b.io, 2, ctx);
            REQUIRE(finite(b.l));
        }
        // Block nhiễm: 1 NaN ở L, 1 +Inf ở R, 1 −Inf ở L
        b.noise(rng);
        b.l[10] = kNaN;
        b.r[20] = kInf;
        b.l[200] = -kInf;
        p->process(b.io, 2, ctx);
        CHECK(finite(b.l));
        CHECK(finite(b.r));
        // Delay mix 100 %: NaN vào delay line nhưng chưa ra ở block này → được bắt khi nó ra (vẫn hữu hạn)
        int resetsSeen = static_cast<int>(p->nonFiniteResets());
        // Chạy tiếp đủ lâu để delay line (tới 2 s) quay hết một vòng
        double tail = 0.0;
        for (int k = 0; k < 500; ++k) {
            b.noise(rng);
            p->process(b.io, 2, ctx);
            REQUIRE(finite(b.l));
            REQUIRE(finite(b.r));
            if (k >= 480) tail += energy(b.l) + energy(b.r);
        }
        resetsSeen = static_cast<int>(p->nonFiniteResets());
        CHECK(resetsSeen >= 1);
        CHECK(resetsSeen <= 2);           // không reset liên tục khi input đã sạch
        CHECK(tail > 1e-3);               // FX vẫn ra tiếng sau khi tự phục hồi
    }
}

TEST_CASE("FX: input NaN liên tục → output luôn hữu hạn (im lặng), không crash", "[dsp][fx][nan]") {
    for (int t = 0; t < kNumFxTypes; ++t) {
        const auto type = static_cast<FxType>(t);
        CAPTURE(fxTypeName(type));
        auto p = createProcessor(type);
        p->prepare(kSr, kBlock);
        wetParams(*p);
        p->reset();
        ProcessContext ctx;
        ctx.numFrames = kBlock;
        ctx.sampleRate = kSr;
        Block b;
        for (int k = 0; k < 50; ++k) {
            std::fill(b.l.begin(), b.l.end(), kNaN);
            std::fill(b.r.begin(), b.r.end(), k % 2 == 0 ? kInf : kNaN);
            p->process(b.io, 2, ctx);
            REQUIRE(finite(b.l));
            REQUIRE(finite(b.r));
        }
        // Mono cũng được bảo vệ
        std::fill(b.l.begin(), b.l.end(), kNaN);
        p->process(b.io, 1, ctx);
        CHECK(finite(b.l));
    }
}

TEST_CASE("FX: tham số NaN → giá trị mặc định, ±Inf → kẹp biên; output hữu hạn", "[dsp][fx][nan]") {
    for (int t = 0; t < kNumFxTypes; ++t) {
        const auto type = static_cast<FxType>(t);
        CAPTURE(fxTypeName(type));
        auto p = createProcessor(type);
        p->prepare(kSr, kBlock);
        for (const ParamInfo& info : paramInfo(type)) {
            p->setParam(info.id, kNaN);
            CHECK(p->getParam(info.id) == info.defaultValue);
            p->setParam(info.id, kInf);
            CHECK(p->getParam(info.id) == info.max);
            p->setParam(info.id, -kInf);
            CHECK(p->getParam(info.id) == info.min);
            p->setParam(info.id, kNaN);
        }
        std::mt19937 rng(7);
        ProcessContext ctx;
        ctx.numFrames = kBlock;
        ctx.sampleRate = kSr;
        Block b;
        for (int k = 0; k < 20; ++k) {
            b.noise(rng);
            p->process(b.io, 2, ctx);
            REQUIRE(finite(b.l));
        }
        CHECK(p->nonFiniteResets() == 0);   // tham số NaN không bao giờ vào tới DSP
    }
}

TEST_CASE("Sampler: dữ liệu sample có NaN / Inf → output hữu hạn, voice đó bị tắt, voice khác vẫn kêu", "[dsp][sampler][nan]") {
    auto good = sine(48000, 440.0);
    auto bad = sine(48000, 330.0);
    bad->writePointer(0)[1000] = kNaN;
    bad->writePointer(0)[1001] = kInf;
    Instrument inst;
    Zone zg;
    zg.loKey = 0;
    zg.hiKey = 59;
    zg.rootKey = 57;
    zg.data = good.get();
    Zone zb = zg;
    zb.loKey = 60;
    zb.hiKey = 127;
    zb.rootKey = 60;
    zb.data = bad.get();
    inst.zones = {zg, zb};
    inst.samples = {good, bad};

    Sampler s;
    s.prepare(kSr, kBlock);
    s.setInstrument(&inst, 1);
    s.noteOn(57, 1.0f);   // zone tốt
    s.noteOn(60, 1.0f);   // zone hỏng: tới sample 1000 thì gặp NaN
    CHECK(s.activeVoices() == 2);
    Block b;
    double energyAfter = 0.0;
    for (int k = 0; k < 20; ++k) {
        std::fill(b.l.begin(), b.l.end(), 0.0f);
        std::fill(b.r.begin(), b.r.end(), 0.0f);
        s.render(b.io, 2, 0, kBlock);
        REQUIRE(finite(b.l));
        REQUIRE(finite(b.r));
        if (k >= 10) energyAfter += energy(b.l);
    }
    CHECK(s.nonFiniteEvents() >= 1);
    CHECK(s.activeVoices() == 1);        // voice đọc NaN đã bị tắt, voice tốt còn kêu
    CHECK(energyAfter > 1.0);

    // Zone có gain NaN (không qua parser) → không phát
    Instrument weird;
    Zone zn = zg;
    zn.gainDb = kNaN;
    weird.zones = {zn};
    weird.samples = {good};
    Sampler s2;
    s2.prepare(kSr, kBlock);
    s2.setInstrument(&weird, 1);
    s2.noteOn(57, 1.0f);
    CHECK(s2.activeVoices() == 0);
    CHECK(s2.nonFiniteEvents() == 1);
    std::fill(b.l.begin(), b.l.end(), 0.0f);
    s2.render(b.io, 2, 0, kBlock);
    CHECK(finite(b.l));
}

TEST_CASE("AudioClipPlayer: clip có NaN / Inf (file hỏng, overdub rác) → output hữu hạn, phần còn lại phát đúng", "[dsp][clip][nan]") {
    const double spb = kSr / 2.0;                       // 120 BPM
    auto clean = sine(96000, 220.0);                    // 4 beat
    auto dirty = sine(96000, 220.0);
    for (int64_t i = 30000; i < 30100; ++i) dirty->writePointer(0)[i] = (i % 2 == 0) ? kNaN : kInf;

    AudioClipPlayer a, ref;
    a.prepare(kSr, kBlock);
    ref.prepare(kSr, kBlock);
    a.setClip(dirty.get(), 4.0, 120.0, 1.0f, 1);
    ref.setClip(clean.get(), 4.0, 120.0, 1.0f, 1);
    a.start(0.0);
    ref.start(0.0);
    Block b, r;
    int differentBlocks = 0;
    for (int k = 0; k < 400; ++k) {                     // hơn 1 vòng clip
        std::fill(b.l.begin(), b.l.end(), 0.0f);
        std::fill(b.r.begin(), b.r.end(), 0.0f);
        std::fill(r.l.begin(), r.l.end(), 0.0f);
        std::fill(r.r.begin(), r.r.end(), 0.0f);
        const double beat = static_cast<double>(k * kBlock) / spb;
        a.render(b.io, 2, 0, kBlock, beat, spb);
        ref.render(r.io, 2, 0, kBlock, beat, spb);
        REQUIRE(finite(b.l));
        REQUIRE(finite(b.r));
        if (b.l != r.l) ++differentBlocks;
    }
    CHECK(a.nonFiniteEvents() >= 1);
    CHECK(differentBlocks >= 1);
    CHECK(differentBlocks <= 4);                        // chỉ các block chạm vùng hỏng (mỗi vòng 1–2 block)
    CHECK(a.isPlaying());                               // clip vẫn phát tiếp

    // gain NaN → im lặng; launchBeat NaN → không start
    AudioClipPlayer g;
    g.prepare(kSr, kBlock);
    g.setClip(clean.get(), 4.0, 120.0, kNaN, 1);
    g.start(0.0);
    std::fill(b.l.begin(), b.l.end(), 0.0f);
    g.render(b.io, 2, 0, kBlock, 0.0, spb);
    CHECK(finite(b.l));
    CHECK(energy(b.l) == 0.0);
    AudioClipPlayer h;
    h.prepare(kSr, kBlock);
    h.setClip(clean.get(), 4.0, 120.0, 1.0f, 1);
    h.start(static_cast<double>(kNaN));
    CHECK_FALSE(h.isPlaying());
    // transport đưa beat NaN (không được xảy ra) → head dừng, không đọc vị trí NaN
    h.start(0.0);
    std::fill(b.l.begin(), b.l.end(), 0.0f);
    h.render(b.io, 2, 0, kBlock, static_cast<double>(kNaN), spb);
    CHECK(finite(b.l));
    CHECK_FALSE(h.isPlaying());
}
