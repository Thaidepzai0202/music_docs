// P1-12 (lõi) — AudioClipPlayer: vị trí tính từ beat, null test vòng lặp 120 BPM, Re-Pitch 100 → 120,
// không click ở điểm loop (crossfade / dip), fade start/stop, đổi data cùng pha, generation.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (null test: output phải đúng từng bit).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/AudioClipPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using le::dsp::AudioClipPlayer;
using le::dsp::AudioData;

namespace {
constexpr double kSr = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kSpb120 = 24000.0;   // 120 BPM @ 48 kHz

std::shared_ptr<AudioData> sine(double hz, int64_t n, float amp = 0.5f, double sr = kSr) {
    auto d = std::make_shared<AudioData>(1, n, sr);
    for (int64_t i = 0; i < n; ++i)
        d->writePointer(0)[i] = amp * static_cast<float>(std::sin(2 * kPi * hz * static_cast<double>(i) / sr));
    return d;
}
std::shared_ptr<AudioData> dc(float l, int64_t n, float r = -1.0f) {
    auto d = std::make_shared<AudioData>(r < 0.0f ? 1 : 2, n, kSr);
    std::fill(d->writePointer(0), d->writePointer(0) + n, l);
    if (r >= 0.0f) std::fill(d->writePointer(1), d->writePointer(1) + n, r);
    return d;
}

struct Stereo {
    std::vector<float> l, r;
};

// Render như RtEngine: beat của mỗi segment tính từ sample tuyệt đối (beat = s / spb, transport neo ở 0).
Stereo play(AudioClipPlayer& p, int total, int block, double spb, int numCh = 2) {
    Stereo o{std::vector<float>(static_cast<size_t>(total), 0.0f), std::vector<float>(static_cast<size_t>(total), 0.0f)};
    float* ch[2] = {o.l.data(), o.r.data()};
    for (int s0 = 0; s0 < total; s0 += block) {
        const int m = std::min(block, total - s0);
        p.render(ch, numCh, s0, m, static_cast<double>(s0) / spb, spb);
    }
    return o;
}

float maxJump(const std::vector<float>& x, size_t from = 1, size_t to = SIZE_MAX) {
    float m = 0.0f;
    for (size_t i = std::max<size_t>(from, 1); i < std::min(to, x.size()); ++i) m = std::max(m, std::fabs(x[i] - x[i - 1]));
    return m;
}

double zeroCrossHz(const std::vector<float>& x, size_t a, size_t b) {
    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = a + 1; i < b; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            const double t = static_cast<double>(i - 1) + x[i - 1] / static_cast<double>(x[i - 1] - x[i]);
            if (first < 0.0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? count * kSr / (last - first) : 0.0;
}
} // namespace

TEST_CASE("AudioClipPlayer: clip_loop_120 — null test đúng từng bit, mọi kích thước block", "[dsp][clip]") {
    // 2 beat @120 BPM = 48000 sample, sine 440 Hz (đúng 440 chu kỳ → hai đầu vòng khớp nhau)
    const auto d = sine(440.0, 48000);
    auto run = [&](int block) {
        AudioClipPlayer p;
        p.prepare(kSr, block);
        p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
        p.start(0.0);
        return play(p, 4 * 48000, block, kSpb120);
    };
    const Stereo ref = run(128);
    const int fade = 96;                                         // 2 ms
    int mismatches = 0;
    for (int k = fade; k < 4 * 48000; ++k)
        if (ref.l[static_cast<size_t>(k)] != d->channel(0)[k % 48000]) ++mismatches;
    CHECK(mismatches == 0);                                      // đọc thẳng, không nội suy → null test tuyệt đối
    CHECK(ref.r == ref.l);                                       // data mono → 2 kênh giống nhau
    CHECK(std::fabs(ref.l[0]) <= std::fabs(d->channel(0)[0]) + 1e-7f);   // fade-in
    for (int block : {1, 64, 333, 1024}) {
        CAPTURE(block);
        CHECK(run(block).l == ref.l);
    }
}

TEST_CASE("AudioClipPlayer: clip_repitch_100_to_120 — vòng dài đúng theo BPM mới, cao độ ×1.2", "[dsp][clip]") {
    // Clip thu ở 100 BPM: 4 beat = 115200 sample (sine 440, 1056 chu kỳ). Project chạy 120 BPM.
    const auto d = sine(440.0, 115200);
    AudioClipPlayer p;
    p.prepare(kSr, 256);
    p.setClip(d.get(), 4.0, 100.0, 1.0f, 1);
    p.start(0.0);
    const Stereo o = play(p, 3 * 96000, 256, kSpb120);
    // Độ dài vòng = 4 beat × 24000 = 96000 sample → output tuần hoàn chu kỳ 96000
    double err = 0.0;
    for (size_t k = 1000; k < 96000; ++k) err = std::max(err, static_cast<double>(std::fabs(o.l[k] - o.l[k + 96000])));
    CHECK(err < 1e-4);
    // Re-Pitch: nhanh hơn 1.2× → 528 Hz
    CHECK(std::fabs(1200.0 * std::log2(zeroCrossHz(o.l, 2000, 90000) / 528.0)) < 1.0);
    CHECK(p.nextLoopBeat(1.0) == Catch::Approx(4.0));
}

TEST_CASE("AudioClipPlayer: điểm loop không click khi hai đầu lệch và không có đuôi (dip 1 ms)", "[dsp][clip][loop]") {
    // sine 450.3 Hz trong 48000 sample: không tròn chu kỳ → nhảy ~0.48 ở điểm loop nếu nối thẳng
    const auto d = sine(450.3, 48000);
    REQUIRE(std::fabs(d->channel(0)[47999] - d->channel(0)[0]) > 0.4f);
    AudioClipPlayer p;
    p.prepare(kSr, 128);
    p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
    p.start(0.0);
    const Stereo o = play(p, 3 * 48000, 128, kSpb120);
    const float natural = 0.5f * static_cast<float>(2 * kPi * 450.3 / kSr);   // độ dốc lớn nhất của sine
    INFO("max jump " << maxJump(o.l) << ", độ dốc tự nhiên " << natural);
    CHECK(maxJump(o.l) < 1.6f * natural);
    CHECK(o.l[48000] == Catch::Approx(0.0f).margin(1e-6));      // đáy "trũng" đúng tại điểm loop
    CHECK(o.l[60000] == d->channel(0)[12000]);                   // giữa vòng: nguyên dữ liệu
}

TEST_CASE("AudioClipPlayer: data có đuôi sau điểm loop → crossfade 2 ms liền mạch", "[dsp][clip][loop]") {
    // Take thu có lề: 48000 sample của vòng + 2400 sample đuôi (sine chạy tiếp tự nhiên)
    const auto d = sine(450.3, 48000 + 2400);
    AudioClipPlayer p;
    p.prepare(kSr, 128);
    p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
    p.start(0.0);
    const Stereo o = play(p, 3 * 48000, 128, kSpb120);
    const float natural = 0.5f * static_cast<float>(2 * kPi * 450.3 / kSr);
    CHECK(maxJump(o.l, 96) < 1.6f * natural);
    CHECK(o.l[48000] == d->channel(0)[48000]);                   // t = 0: nghe đúng phần đuôi nối tiếp
    CHECK(o.l[48000 + 500] == d->channel(0)[500]);               // sau crossfade: đầu clip
    CHECK(o.l[47999] == d->channel(0)[47999]);                   // trước điểm loop: không đụng
}

TEST_CASE("AudioClipPlayer: fade-in 2 ms khi start, fade-out 5 ms khi stop rồi im lặng", "[dsp][clip]") {
    const auto d = dc(1.0f, 48000);
    AudioClipPlayer p;
    p.prepare(kSr, 64);
    CHECK(p.isSilent());
    p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
    CHECK(p.isSilent());                                         // setClip chưa phát
    p.start(0.0);
    CHECK(p.isPlaying());
    Stereo a = play(p, 1000, 64, kSpb120);
    CHECK(a.l[0] == Catch::Approx(1.0f / 96.0f));
    CHECK(a.l[95] == Catch::Approx(1.0f));
    CHECK(maxJump(a.l) <= 1.0f / 96.0f + 1e-6f);

    p.stop();
    CHECK_FALSE(p.isPlaying());
    std::vector<float> l(400, 0.0f), r(400, 0.0f);
    float* ch[2] = {l.data(), r.data()};
    p.render(ch, 2, 0, 400, 1000.0 / kSpb120, kSpb120);
    CHECK(l[0] == Catch::Approx(1.0f - 1.0f / 240.0f));
    CHECK(l[239] == 0.0f);
    CHECK(l[399] == 0.0f);
    CHECK(maxJump(l) <= 1.0f / 240.0f + 1e-6f);
    CHECK(p.isSilent());
}

TEST_CASE("AudioClipPlayer: sample ĐẦU TIÊN sau start() khác 0 = data[0]/96 (onset đúng sample launch)", "[dsp][clip]") {
    // Giống fixture click của scenario: sample đầu rất nhỏ nhưng khác 0
    auto d = std::make_shared<AudioData>(1, 48000, kSr);
    for (int i = 0; i < 48000; ++i) d->writePointer(0)[i] = 0.5f * static_cast<float>(std::sin(2 * kPi * 1000.0 * (i + 1) / kSr));
    AudioClipPlayer p;
    p.prepare(kSr, 128);
    p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
    std::vector<float> l(256, 0.0f), r(256, 0.0f);
    float* ch[2] = {l.data(), r.data()};
    p.render(ch, 2, 0, 128, 0.0, kSpb120);                       // chưa start: im lặng
    p.start(128.0 / kSpb120);
    p.render(ch, 2, 128, 128, 128.0 / kSpb120, kSpb120);
    CHECK(l[127] == 0.0f);
    CHECK(l[128] == Catch::Approx(d->channel(0)[0] / 96.0f));   // KHÁC 0 ngay tại sample launch
    CHECK(l[128] != 0.0f);
    CHECK(l[129] == Catch::Approx(d->channel(0)[1] * 2.0f / 96.0f));
}

TEST_CASE("AudioClipPlayer: TRANSPORT_STOP reset beat về 0 trong lúc fade-out → không nhảy về đầu clip", "[dsp][clip]") {
    const auto d = sine(500.0, 96000);                           // 4 beat, sine 500 Hz
    AudioClipPlayer p;
    p.prepare(kSr, 128);
    p.setClip(d.get(), 4.0, 120.0, 1.0f, 1);
    p.start(0.0);
    std::vector<float> l(61000, 0.0f), r(61000, 0.0f);
    float* ch[2] = {l.data(), r.data()};
    int s0 = 0;
    for (; s0 < 60000; s0 += 128) p.render(ch, 2, s0, 128, static_cast<double>(s0) / kSpb120, kSpb120);
    p.stop();                                                    // transport dừng ở sample s0
    for (; s0 + 128 <= 61000; s0 += 128) p.render(ch, 2, s0, 128, 0.0, kSpb120);   // beat đứng yên ở 0
    const float natural = 0.5f * static_cast<float>(2 * kPi * 500.0 / kSr);
    INFO("stop tại sample " << 60032);
    CHECK(maxJump(l, 59000, 60800) < 1.2f * natural);            // không có bước nhảy về đầu clip
    CHECK(l[60032 + 239] == Catch::Approx(0.0f).margin(1e-6));  // fade-out xong sau đúng 240 sample
    CHECK(l[60032 + 300] == 0.0f);
    CHECK(p.isSilent());
}

TEST_CASE("AudioClipPlayer: đổi data lúc đang phát → crossfade 5 ms cùng pha; generation cũ hết sau fade", "[dsp][clip][gen]") {
    const auto a = sine(440.0, 48000, 0.5f), b = sine(440.0, 48000, 0.25f);
    AudioClipPlayer p;
    p.prepare(kSr, 120);
    p.setClip(a.get(), 2.0, 120.0, 1.0f, 1);
    p.start(0.0);
    float lbuf[1200] = {}, rbuf[1200] = {};
    float* ch[2] = {lbuf, rbuf};
    p.render(ch, 2, 0, 960, 0.0, kSpb120);
    p.setClip(b.get(), 2.0, 120.0, 1.0f, 2);                    // snapshot mới thay data của clip
    CHECK(p.usesGeneration(1));
    CHECK(p.usesGeneration(2));
    p.render(ch, 2, 960, 240, 960.0 / kSpb120, kSpb120);         // đúng 5 ms
    CHECK_FALSE(p.usesGeneration(1));                            // bản cũ fade xong → 68 retire được
    std::vector<float> after(240, 0.0f);
    float* ch2[2] = {after.data(), after.data()};
    p.render(ch2, 1, 0, 240, 1200.0 / kSpb120, kSpb120);
    for (int k = 0; k < 240; ++k)                                // bản mới phát tiếp đúng pha
        REQUIRE(after[static_cast<size_t>(k)] == Catch::Approx(b->channel(0)[1200 + k]).margin(1e-6));
    CHECK(maxJump(std::vector<float>(lbuf, lbuf + 1200), 96) < 0.035f);   // sine 0.5 dốc tối đa ~0.029

    // Cùng data, generation mới → generation cũ không còn bị giữ, âm thanh không đổi
    p.setClip(b.get(), 2.0, 120.0, 1.0f, 3);
    CHECK_FALSE(p.usesGeneration(2));
    CHECK(p.usesGeneration(3));
    CHECK(p.isPlaying());

    // Gỡ clip (data = nullptr) → fade-out 5 ms, rồi không giữ generation nào
    p.setClip(nullptr, 0.0, 0.0, 1.0f, 4);
    CHECK(p.usesGeneration(3));
    p.render(ch2, 1, 0, 240, 1440.0 / kSpb120, kSpb120);
    CHECK_FALSE(p.usesGeneration(3));
    CHECK(p.isSilent());
}

TEST_CASE("AudioClipPlayer: launch lại khi đang phát, clip dài hơn data, mono/stereo, progress", "[dsp][clip]") {
    SECTION("launch lại → phát lại từ đầu, không nhảy bậc") {
        const auto d = dc(1.0f, 48000);
        AudioClipPlayer p;
        p.prepare(kSr, 128);
        p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
        p.start(0.0);
        play(p, 4800, 128, kSpb120);
        p.start(4800.0 / kSpb120);
        std::vector<float> l(480, 0.0f), r(480, 0.0f);
        float* ch[2] = {l.data(), r.data()};
        p.render(ch, 2, 0, 480, 4800.0 / kSpb120, kSpb120);
        CHECK(maxJump(l) < 0.02f);                               // cũ ra 5 ms, mới vào 2 ms: tổng ≈ 1
        CHECK(p.progress(4800.0 / kSpb120 + 1.0) == Catch::Approx(0.5f));
    }
    SECTION("lengthBeats dài hơn data → phần thiếu là im lặng, không đọc lố") {
        const auto d = dc(1.0f, 24000);                          // 1 beat dữ liệu
        AudioClipPlayer p;
        p.prepare(kSr, 128);
        p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
        p.start(0.0);
        const Stereo o = play(p, 48000, 128, kSpb120);
        CHECK(o.l[12000] == 1.0f);
        CHECK(o.l[36000] == 0.0f);
    }
    SECTION("data stereo: ra đúng 2 kênh; output mono = trung bình") {
        const auto d = dc(0.8f, 48000, 0.2f);
        AudioClipPlayer p;
        p.prepare(kSr, 128);
        p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
        p.start(0.0);
        const Stereo st = play(p, 1000, 128, kSpb120);
        CHECK(st.l[500] == Catch::Approx(0.8f));
        CHECK(st.r[500] == Catch::Approx(0.2f));
        AudioClipPlayer q;
        q.prepare(kSr, 128);
        q.setClip(d.get(), 2.0, 120.0, 0.5f, 1);                 // gain 0.5
        q.start(0.0);
        const Stereo mono = play(q, 1000, 128, kSpb120, 1);
        CHECK(mono.l[500] == Catch::Approx(0.25f));              // 0.5 · (0.8 + 0.2) / 2
        CHECK(mono.r[500] == 0.0f);
    }
    SECTION("nextLoopBeat / progress") {
        const auto d = dc(1.0f, 48000);
        AudioClipPlayer p;
        p.prepare(kSr, 128);
        p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
        CHECK(std::isinf(p.nextLoopBeat(0.0)));                  // chưa phát
        p.start(4.0);
        CHECK(p.nextLoopBeat(3.0) == Catch::Approx(4.0));
        CHECK(p.nextLoopBeat(4.0) == Catch::Approx(6.0));
        CHECK(p.nextLoopBeat(5.9) == Catch::Approx(6.0));
        CHECK(p.nextLoopBeat(6.0) == Catch::Approx(8.0));
        CHECK(p.progress(5.0) == Catch::Approx(0.5f));
        CHECK(p.progress(3.0) == 0.0f);
    }
}

// ───────────────────────── clip.setParams (gain) + warp hybrid P3-10 ─────────────────────────

namespace {
// Chạy player với transport ở `bpmAt(sample)`; beat tính từ sample tuyệt đối theo từng đoạn BPM không đổi.
// `onBlock(s0)` được gọi đầu mỗi block (để đổi tham số giữa chừng).
template <class OnBlock>
Stereo playWith(AudioClipPlayer& p, int total, int block, double spb, OnBlock&& onBlock) {
    Stereo o{std::vector<float>(static_cast<size_t>(total), 0.0f), std::vector<float>(static_cast<size_t>(total), 0.0f)};
    float* ch[2] = {o.l.data(), o.r.data()};
    for (int s0 = 0; s0 < total; s0 += block) {
        const int m = std::min(block, total - s0);
        onBlock(s0);
        p.render(ch, 2, s0, m, static_cast<double>(s0) / spb, spb);
    }
    return o;
}
} // namespace

TEST_CASE("AudioClipPlayer: đổi gain cùng data (clip.setParams) → trượt 20 ms, không click", "[dsp][clip]") {
    const auto d = sine(440.0, 48000);
    AudioClipPlayer p;
    p.prepare(kSr, 120);
    p.setClip(d.get(), 2.0, 120.0, 1.0f, 1);
    p.start(0.0);
    const Stereo o = playWith(p, 9600, 120, kSpb120, [&](int s0) {
        if (s0 == 4800) p.setClip(d.get(), 2.0, 120.0, 0.5f, 2);   // 0 dB → −6 dB, cùng buffer
    });
    const float natural = 0.5f * static_cast<float>(2 * kPi * 440.0 / kSr);
    CHECK(maxJump(o.l, 96) <= natural * 1.01f);
    for (int k = 4800 + 960; k < 9600; ++k)                               // sau 20 ms: đúng 0.25·sine
        REQUIRE(o.l[static_cast<size_t>(k)] == Catch::Approx(0.5f * d->channel(0)[k % 48000]).margin(1e-6));
    CHECK(o.l[4799] == d->channel(0)[4799]);                               // trước khi đổi: nguyên vẹn
    CHECK_FALSE(p.usesGeneration(1));
    CHECK(p.usesGeneration(2));
}

TEST_CASE("AudioClipPlayer: warp — chuyển sang Stretched ĐÚNG sample ranh giới bar kế tiếp, crossfade 10 ms không nhảy",
          "[dsp][clip][warp]") {
    // Clip 4 beat thu ở 100 BPM (sine 440, 115200 sample), phát ở 120 BPM → Re-Pitch 528 Hz.
    // "Bản stretched" lý tưởng tới 120 BPM: sine 440 dài 96000 sample (4 beat @120).
    const auto d = sine(440.0, 115200);
    const auto st = sine(440.0, 96000);
    auto run = [&](int block, bool offer) {
        AudioClipPlayer p;
        p.prepare(kSr, block);
        p.setClip(d.get(), 4.0, 100.0, 1.0f, 1);
        p.start(0.0);
        const Stereo o = playWith(p, 150000, block, kSpb120, [&](int s0) {
            if (offer && s0 <= 31200 && s0 + block > 31200) p.setStretched(st.get(), 120.0, 2);   // job xong ở beat ~1.3
        });
        return std::make_pair(o, p.isStretched());
    };
    const auto [ref, refSt] = run(128, false);
    const auto [o, isSt] = run(128, true);
    CHECK_FALSE(refSt);
    CHECK(isSt);
    int before = 0, after = 0;
    for (int k = 0; k < 96000; ++k) if (o.l[static_cast<size_t>(k)] != ref.l[static_cast<size_t>(k)]) ++before;
    for (int k = 96480; k < 150000; ++k) if (o.l[static_cast<size_t>(k)] != st->channel(0)[k % 96000]) ++after;
    CHECK(before == 0);                     // trước ranh giới bar (beat 4 = sample 96000): đúng Re-Pitch từng bit
    CHECK(after == 0);                      // sau crossfade 10 ms: đúng bản Stretched từng bit (đọc thẳng)
    CHECK(o.l[95999] == ref.l[95999]);
    CHECK(o.l[96001] != ref.l[96001]);      // crossfade bắt đầu ĐÚNG sample 96000 (sample 96000 = điểm loop, cả hai nguồn = 0)
    CHECK(maxJump(o.l, 95000) < 0.06f);     // equal-power, không có bước nhảy (chuyển cứng sẽ tới ~0.5)
    for (int block : {64, 333, 1024}) {
        CAPTURE(block);
        const auto [ob, sb] = run(block, true);
        CHECK(sb);
        double err = 0.0;
        for (size_t k = 0; k < ob.l.size(); ++k) err = std::max(err, static_cast<double>(std::fabs(ob.l[k] - o.l[k])));
        CHECK(err < 1e-6);                  // giống hệt nhau với mọi block size
    }
}

TEST_CASE("AudioClipPlayer: warp — BPM đổi trước ranh giới thì huỷ; BPM đổi sau khi chuyển thì về Re-Pitch ngay",
          "[dsp][clip][warp]") {
    const auto d = sine(440.0, 115200);
    const auto st = sine(440.0, 96000);
    const double spb121 = 60.0 * kSr / 121.0;

    SECTION("BPM đổi ở beat 3 (trước ranh giới beat 4) → không chuyển") {
        AudioClipPlayer p;
        p.prepare(kSr, 128);
        p.setClip(d.get(), 4.0, 100.0, 1.0f, 1);
        p.setStretched(st.get(), 120.0, 2);
        p.start(0.0);
        std::vector<float> l(128), r(128);
        float* ch[2] = {l.data(), r.data()};
        int s0 = 0;
        for (; s0 < 72000; s0 += 128) p.render(ch, 2, 0, 128, s0 / kSpb120, kSpb120);
        const double beat = s0 / kSpb120;
        for (int i = 0; i < 1000; ++i) p.render(ch, 2, 0, 128, beat + i * 128 / spb121, spb121);   // 121 BPM từ beat 3
        CHECK_FALSE(p.isStretched());
    }
    SECTION("đang Stretched, BPM đổi → về Re-Pitch trong 5 ms, không nhảy; gỡ stretched thì hết giữ generation") {
        AudioClipPlayer p;
        p.prepare(kSr, 128);
        p.setClip(d.get(), 4.0, 100.0, 1.0f, 1);
        p.setStretched(st.get(), 120.0, 2);
        p.start(0.0);
        Stereo o{std::vector<float>(130000, 0.0f), std::vector<float>(130000, 0.0f)};
        float* ch[2] = {o.l.data(), o.r.data()};
        int s0 = 0;
        for (; s0 < 110080; s0 += 128) p.render(ch, 2, s0, 128, s0 / kSpb120, kSpb120);
        REQUIRE(p.isStretched());
        const double beat0 = s0 / kSpb120;
        for (int i = 0; s0 + 128 <= 130000; ++i, s0 += 128) p.render(ch, 2, s0, 128, beat0 + i * 128 / spb121, spb121);
        CHECK_FALSE(p.isStretched());
        // Chỉ xét phần đã render (khúc lẻ cuối < 128 sample không render → còn 0)
        const std::vector<float> rendered(o.l.begin(), o.l.begin() + s0);
        CHECK(maxJump(rendered, 109000) < 0.06f);
        CHECK(p.usesGeneration(2));                // bản stretched vẫn được "đưa vào" (68 chưa gỡ)
        p.setStretched(nullptr, 0.0, 3);
        CHECK_FALSE(p.usesGeneration(2));          // đã về Re-Pitch + gỡ → snapshot 2 retire được
    }
    SECTION("gỡ stretched lúc đang phát Stretched → giữ generation tới khi crossfade 5 ms xong") {
        AudioClipPlayer p;
        p.prepare(kSr, 120);
        p.setClip(d.get(), 4.0, 100.0, 1.0f, 1);
        p.setStretched(st.get(), 120.0, 2);
        p.start(0.0);
        std::vector<float> l(120), r(120);
        float* ch[2] = {l.data(), r.data()};
        int s0 = 0;
        for (; s0 < 100080; s0 += 120) p.render(ch, 2, 0, 120, s0 / kSpb120, kSpb120);
        REQUIRE(p.isStretched());
        p.setStretched(nullptr, 0.0, 3);
        p.render(ch, 2, 0, 120, s0 / kSpb120, kSpb120);
        CHECK(p.usesGeneration(2));                // đang crossfade về Re-Pitch, vẫn đọc bản cũ
        s0 += 120;
        p.render(ch, 2, 0, 120, s0 / kSpb120, kSpb120);
        CHECK_FALSE(p.usesGeneration(2));          // 240 sample = 5 ms xong
        CHECK_FALSE(p.isStretched());
    }
}
