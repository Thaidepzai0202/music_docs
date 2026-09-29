// P1-25 / P1-26 — Sampler: chọn zone, inc (cao độ), stealing không vượt 64 voice và không click,
// choke, one-shot, generation, độc lập với kích thước block.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (output phải giống nhau từng bit / đúng bằng 0).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace le::dsp;

namespace {
constexpr double kSr = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr float  kCenter = 0.70710678f;   // pan giữa, mono: cos(π/4)

std::shared_ptr<AudioData> dc(float value, int64_t n, double sr = kSr) {
    auto d = std::make_shared<AudioData>(1, n, sr);
    std::fill(d->writePointer(0), d->writePointer(0) + n, value);
    return d;
}
std::shared_ptr<AudioData> sine(double hz, double sr, int64_t n, float amp = 0.5f) {
    auto d = std::make_shared<AudioData>(1, n, sr);
    for (int64_t i = 0; i < n; ++i)
        d->writePointer(0)[i] = amp * static_cast<float>(std::sin(2 * kPi * hz * static_cast<double>(i) / sr));
    return d;
}

Zone zone(const std::shared_ptr<AudioData>& d, int lo, int hi, int root, LoopMode mode = LoopMode::LoopContinuous) {
    Zone z;
    z.loKey = static_cast<int16_t>(lo);
    z.hiKey = static_cast<int16_t>(hi);
    z.rootKey = static_cast<int16_t>(root);
    z.loopMode = mode;
    z.env = {0.0f, 0.0f, 1.0f, 0.010f};
    z.data = d.get();
    return z;
}

Instrument makeInstrument(std::vector<Zone> zones, std::vector<std::shared_ptr<AudioData>> samples) {
    Instrument inst;
    inst.zones = std::move(zones);
    for (auto& s : samples) inst.samples.push_back(s);
    return inst;
}

struct Out {
    std::vector<float> l, r;
};

// Render n sample theo block `block` (stereo), nối vào out.
void renderInto(Sampler& s, Out& out, int n, int block = 128) {
    std::vector<float> l(static_cast<size_t>(block)), r(static_cast<size_t>(block));
    float* ch[2] = {l.data(), r.data()};
    for (int done = 0; done < n; done += block) {
        const int m = std::min(block, n - done);
        std::fill(l.begin(), l.end(), 0.0f);
        std::fill(r.begin(), r.end(), 0.0f);
        s.render(ch, 2, 0, m);
        out.l.insert(out.l.end(), l.begin(), l.begin() + m);
        out.r.insert(out.r.end(), r.begin(), r.begin() + m);
    }
}
Out render(Sampler& s, int n, int block = 128) {
    Out o;
    renderInto(s, o, n, block);
    return o;
}

float maxJump(const std::vector<float>& x) {
    float m = 0.0f;
    for (size_t i = 1; i < x.size(); ++i) m = std::max(m, std::fabs(x[i] - x[i - 1]));
    return m;
}

double zeroCrossHz(const std::vector<float>& x, size_t a, size_t b, double sr) {
    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = a + 1; i < b; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
            const double t = static_cast<double>(i - 1) + x[i - 1] / static_cast<double>(x[i - 1] - x[i]);
            if (first < 0.0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? count * sr / (last - first) : 0.0;
}
} // namespace

TEST_CASE("Sampler: chọn zone theo phím và velocity", "[dsp][sampler]") {
    auto a = dc(0.1f, 48000), b = dc(0.2f, 48000), c = dc(0.3f, 48000);
    Zone za = zone(a, 0, 59, 60), zb = zone(b, 0, 59, 60), zc = zone(c, 60, 100, 60);
    za.hiVel = 63;
    zb.loVel = 64;
    const Instrument inst = makeInstrument({za, zb, zc}, {a, b, c});

    struct Case { int note; float vel; float value; };
    for (const Case k : {Case{40, 0.3f, 0.1f}, Case{40, 0.9f, 0.2f}, Case{80, 1.0f, 0.3f}}) {
        CAPTURE(k.note, k.vel);
        Sampler s;
        s.prepare(kSr, 128);
        s.setInstrument(&inst, 1);
        s.noteOn(k.note, k.vel);
        REQUIRE(s.activeVoices() == 1);
        const Out o = render(s, 256);
        const int vel = static_cast<int>(k.vel * 127.0f + 0.5f);
        const float g = (vel / 127.0f) * (vel / 127.0f) * kCenter;   // velocity² · pan −3 dB
        CHECK(o.l.back() == Catch::Approx(k.value * g).epsilon(1e-4));
        CHECK(o.r.back() == Catch::Approx(k.value * g).epsilon(1e-4));
    }

    Sampler s;
    s.prepare(kSr, 128);
    s.setInstrument(&inst, 1);
    s.noteOn(110, 1.0f);     // ngoài mọi zone
    CHECK(s.activeVoices() == 0);
    s.setInstrument(nullptr, 2);
    s.noteOn(60, 1.0f);      // không có instrument
    CHECK(s.activeVoices() == 0);
}

TEST_CASE("Sampler: inc đúng cao độ (khác sample rate, tune cent), chế độ Classic", "[dsp][sampler]") {
    // Zone gốc A4 = sine 440 Hz, sample rate 44.1 kHz; engine 48 kHz.
    auto d = sine(440.0, 44100.0, 3 * 44100);
    Zone z = zone(d, 0, 127, 69, LoopMode::NoLoop);
    struct Case { int note; float tune; double hz; };
    for (const Case k : {Case{69, 0.0f, 440.0}, Case{81, 0.0f, 880.0}, Case{57, 0.0f, 220.0},
                         Case{69, 50.0f, 440.0 * std::pow(2.0, 0.5 / 12.0)}, Case{76, -25.0f, 440.0 * std::pow(2.0, 6.75 / 12.0)}}) {
        CAPTURE(k.note, k.tune);
        z.tuneCents = k.tune;
        const Instrument inst = makeInstrument({z}, {d});
        Sampler s;
        s.prepare(kSr, 256);
        s.setInstrument(&inst, 1);
        s.noteOn(k.note, 1.0f);
        const Out o = render(s, 24000, 256);
        const double hz = zeroCrossHz(o.l, 2400, 24000, kSr);
        CHECK(std::fabs(1200.0 * std::log2(hz / k.hz)) < 1.0);   // < 1 cent
    }

    // Classic: mọi phím dùng zones[classicZone] (biên độ 0.5), kể cả phím thuộc zone khác (biên độ 0.25)
    auto lo = sine(261.63, kSr, 48000, 0.5f), hi = sine(523.25, kSr, 48000, 0.25f);
    Instrument inst = makeInstrument({zone(lo, 0, 71, 60, LoopMode::NoLoop), zone(hi, 72, 127, 72, LoopMode::NoLoop)}, {lo, hi});
    inst.mode = Instrument::Mode::Classic;
    inst.classicZone = 0;
    Sampler s;
    s.prepare(kSr, 256);
    s.setInstrument(&inst, 1);
    s.noteOn(72, 1.0f);
    const Out o = render(s, 12000, 256);
    float peak = 0.0f;
    for (size_t i = 2400; i < o.l.size(); ++i) peak = std::max(peak, std::fabs(o.l[i]));
    CHECK(peak == Catch::Approx(0.5f * kCenter).epsilon(0.01));
    CHECK(std::fabs(1200.0 * std::log2(zeroCrossHz(o.l, 2400, 12000, kSr) / 523.26)) < 1.0);
}

TEST_CASE("Sampler: 100 note-on → không bao giờ quá 64 voice, cướp voice cũ nhất, không click", "[dsp][sampler][steal]") {
    auto d = dc(1.0f, 4800);
    Zone z = zone(d, 0, 127, 60);
    z.env = {0.005f, 0.0f, 1.0f, 0.050f};
    const Instrument inst = makeInstrument({z}, {d});
    Sampler s;
    s.prepare(kSr, 128);
    s.setInstrument(&inst, 1);

    Out o;
    for (int k = 0; k < 100; ++k) {             // 1 nốt mỗi ms, nốt 0..99
        s.noteOn(k, 1.0f);
        REQUIRE(s.activeVoices() <= Sampler::kMaxVoices);
        REQUIRE(s.fadingVoices() <= Sampler::kSpareVoices);
        renderInto(s, o, 48, 48);
    }
    CHECK(s.activeVoices() == Sampler::kMaxVoices);
    renderInto(s, o, 480);
    CHECK(s.fadingVoices() == 0);               // mọi voice bị cướp đã fade xong (3 ms)

    // Không có bước nhảy cỡ 1 voice (cắt cứng = kCenter). Fade 3 ms chỉ nhảy ~kCenter/144 mỗi voice.
    INFO("max jump " << maxJump(o.l) << " so với 1 voice " << kCenter);
    CHECK(maxJump(o.l) < 0.25f * kCenter);

    // Voice bị cướp là 36 nốt đầu (cũ nhất): nhả chúng không làm voice nào tắt
    for (int k = 0; k < 36; ++k) s.noteOff(k);
    render(s, 24000);
    CHECK(s.activeVoices() == Sampler::kMaxVoices);
    for (int k = 36; k < 100; ++k) s.noteOff(k);
    render(s, 24000);
    CHECK(s.activeVoices() == 0);
}

TEST_CASE("Sampler: khi đầy, ưu tiên cướp voice đang release; 100 nốt cùng lúc vẫn an toàn", "[dsp][sampler][steal]") {
    auto d = dc(1.0f, 4800);
    Zone z = zone(d, 0, 127, 60);
    z.env = {0.0f, 0.0f, 1.0f, 2.0f};           // release dài: voice nhả vẫn còn kêu
    const Instrument inst = makeInstrument({z}, {d});
    Sampler s;
    s.prepare(kSr, 128);
    s.setInstrument(&inst, 1);
    for (int k = 0; k < 64; ++k) s.noteOn(k, 1.0f);
    render(s, 128);
    s.noteOff(10);                               // nốt 10 đang release
    render(s, 128);
    s.noteOn(100, 1.0f);                         // phải cướp nốt 10, không cướp nốt 0 (cũ nhất)
    CHECK(s.fadingVoices() == 1);
    s.noteOff(0);                                // nốt 0 vẫn còn → nhả nó thì sau release sẽ mất 1 voice
    render(s, 3 * 48000);
    CHECK(s.activeVoices() == 63);

    Sampler burst;
    burst.prepare(kSr, 128);
    burst.setInstrument(&inst, 1);
    for (int k = 0; k < 100; ++k) burst.noteOn(k % 128, 0.8f);
    CHECK(burst.activeVoices() == Sampler::kMaxVoices);
    CHECK(burst.fadingVoices() <= Sampler::kSpareVoices);
    const Out o = render(burst, 1024);
    for (float x : o.l) REQUIRE(std::isfinite(x));
}

TEST_CASE("Sampler: choke — hi-hat đóng chặn hi-hat mở trong 5 ms, không click", "[dsp][sampler][choke]") {
    auto open = dc(1.0f, 48000), closed = dc(0.0f, 48000);   // hat đóng = 0 để chỉ nghe phần hat mở
    Zone zo = zone(open, 46, 46, 46), zc = zone(closed, 42, 42, 42, LoopMode::OneShot);
    zo.group = 2; zo.offBy = 1;                  // như ví dụ 06 §4
    zc.group = 1; zc.offBy = 2;
    const Instrument inst = makeInstrument({zc, zo}, {closed, open});
    Sampler s;
    s.prepare(kSr, 64);
    s.setInstrument(&inst, 1);
    s.noteOn(46, 1.0f);
    const Out before = render(s, 960, 64);
    REQUIRE(before.l.back() == Catch::Approx(kCenter));
    s.noteOn(42, 1.0f);                          // hat đóng
    const Out fade = render(s, 240, 60);         // đúng 5 ms
    CHECK(fade.l.back() == 0.0f);
    CHECK(maxJump(fade.l) <= kCenter / 240.0f + 1e-5f);
    CHECK(fade.l.front() > 0.9f * kCenter);      // bắt đầu fade từ mức đang kêu (không cắt)
    CHECK(s.countVoicesUsing(1) == 1);           // chỉ còn hat đóng

    // Chiều ngược lại: hat mở (group 2) chặn hat đóng (offBy 2)
    Sampler t;
    t.prepare(kSr, 64);
    t.setInstrument(&inst, 1);
    t.noteOn(42, 1.0f);
    t.noteOn(46, 1.0f);
    render(t, 240, 60);
    CHECK(t.activeVoices() == 1);
}

TEST_CASE("Sampler: one-shot bỏ qua note-off; no-loop nhả thì release; hết dữ liệu thì voice rảnh", "[dsp][sampler]") {
    auto d = dc(0.5f, 4800);
    const Instrument oneShot = makeInstrument({zone(d, 0, 127, 60, LoopMode::OneShot)}, {d});
    Sampler s;
    s.prepare(kSr, 128);
    s.setInstrument(&oneShot, 1);
    s.noteOn(60, 1.0f);
    s.noteOff(60);
    render(s, 4700);
    CHECK(s.activeVoices() == 1);                // vẫn kêu dù đã nhả
    render(s, 200);
    CHECK(s.activeVoices() == 0);                // hết 4800 sample → rảnh

    const Instrument noLoop = makeInstrument({zone(d, 0, 127, 60, LoopMode::NoLoop)}, {d});
    s.setInstrument(&noLoop, 2);
    s.noteOn(60, 1.0f);
    render(s, 1000);
    s.noteOff(60);
    render(s, 700);                              // release 10 ms → −80 dB sau ~13 ms
    CHECK(s.activeVoices() == 0);
}

TEST_CASE("Sampler: allNotesOff thường giữ one-shot, fast tắt hết trong 5 ms", "[dsp][sampler]") {
    auto a = dc(0.5f, 48000), b = dc(0.5f, 48000);
    const Instrument inst = makeInstrument({zone(a, 0, 59, 36, LoopMode::OneShot), zone(b, 60, 127, 72)}, {a, b});
    Sampler s;
    s.prepare(kSr, 128);
    s.setInstrument(&inst, 1);
    s.noteOn(36, 1.0f);
    s.noteOn(72, 1.0f);
    s.allNotesOff(false);
    render(s, 4800);
    CHECK(s.activeVoices() == 1);                // one-shot còn kêu, voice loop đã release xong
    s.allNotesOff(true);
    render(s, 240);
    CHECK(s.activeVoices() == 0);
}

TEST_CASE("Sampler: generation — cùng instrument thì chuyển gen ngay, đổi instrument thì fade rồi mới hết", "[dsp][sampler][gen]") {
    auto d = dc(0.5f, 48000);
    auto d2 = dc(0.5f, 48000);                   // instrument KHÁC: dữ liệu riêng (không phải bản phái sinh)
    const Instrument a = makeInstrument({zone(d, 0, 127, 60)}, {d});
    const Instrument b = makeInstrument({zone(d2, 0, 127, 60)}, {d2});
    Sampler s;
    s.prepare(kSr, 128);
    s.setInstrument(&a, 1);
    s.noteOn(60, 1.0f);
    CHECK(s.countVoicesUsing(1) == 1);

    s.setInstrument(&a, 2);                      // snapshot mới, cùng instrument
    CHECK(s.countVoicesUsing(1) == 0);           // → 68 retire được snapshot 1 ngay
    CHECK(s.countVoicesUsing(2) == 1);

    s.setInstrument(&b, 3);                      // đổi instrument
    CHECK(s.countVoicesUsing(2) == 1);           // voice cũ còn fade, vẫn đọc dữ liệu snapshot 2
    s.noteOn(64, 1.0f);
    CHECK(s.countVoicesUsing(3) == 1);
    CHECK(s.instrument() == &b);
    const Out o = render(s, 240, 60);            // 5 ms
    CHECK(s.countVoicesUsing(2) == 0);           // fade xong → retire được snapshot 2
    CHECK(maxJump(o.l) < 0.05f);

    s.fastReleaseGeneration(3);                  // retiring[] đầy → ép fade 3 ms
    render(s, 144, 48);
    CHECK(s.countVoicesUsing(3) == 0);
    CHECK(s.activeVoices() == 0);
}

TEST_CASE("Sampler: kết quả giống hệt nhau với mọi kích thước block; mono = trung bình L/R; cộng dồn", "[dsp][sampler]") {
    auto d = sine(220.0, 44100.0, 44100);
    Zone z = zone(d, 0, 127, 57, LoopMode::NoLoop);
    z.env = {0.002f, 0.1f, 0.6f, 0.05f};
    z.pan = -0.3f;
    const Instrument inst = makeInstrument({z}, {d});

    // Sự kiện ở các sample tuyệt đối; block được chia tại mốc sự kiện (như BlockSplitter)
    struct Ev { int at; int note; bool on; };
    const std::vector<Ev> evs = {{0, 57, true}, {1000, 64, true}, {5003, 57, false}, {7777, 69, true}, {15000, 64, false}, {20000, 69, false}};
    auto renderAll = [&](int block, int numCh) {
        Sampler s;
        s.prepare(kSr, block);
        s.setInstrument(&inst, 1);
        const int total = 24000;
        std::vector<float> l(total, 0.25f), r(total, 0.25f);   // có sẵn giá trị → kiểm cộng dồn
        float* ch[2] = {l.data(), r.data()};
        size_t e = 0;
        for (int pos = 0; pos < total;) {
            while (e < evs.size() && evs[e].at == pos) {
                if (evs[e].on) s.noteOn(evs[e].note, 0.8f); else s.noteOff(evs[e].note);
                ++e;
            }
            int end = std::min(total, pos + block);
            if (e < evs.size()) end = std::min(end, evs[e].at);
            s.render(ch, numCh, pos, end - pos);
            pos = end;
        }
        return std::make_pair(l, r);
    };
    const auto ref = renderAll(1024, 2);
    for (int block : {1, 64, 128, 333}) {
        CAPTURE(block);
        const auto got = renderAll(block, 2);
        CHECK(got.first == ref.first);
        CHECK(got.second == ref.second);
    }
    const auto mono = renderAll(128, 1);
    double err = 0.0;
    for (size_t i = 0; i < mono.first.size(); ++i)
        err = std::max(err, static_cast<double>(std::fabs((mono.first[i] - 0.25f) - 0.5f * ((ref.first[i] - 0.25f) + (ref.second[i] - 0.25f)))));
    CHECK(err < 1e-6);
    CHECK(mono.second[100] == 0.25f);            // mono: không đụng kênh 2
}

TEST_CASE("Sampler: đổi mềm — instrument phái sinh (chung samples, envelope mới) không cắt nốt đang ngân", "[dsp][sampler][gen]") {
    auto d = dc(0.5f, 96000);
    Zone za = zone(d, 0, 127, 60);
    za.env = {0.0f, 0.0f, 1.0f, 0.010f};         // release 10 ms
    za.group = 1;
    za.offBy = 1;                                 // tự choke nhóm 1 (kiểm offBy được chép vào voice)
    const Instrument a = makeInstrument({za}, {d});
    Zone zb = za;
    zb.env = {0.100f, 0.0f, 1.0f, 0.500f};        // setEnvelope: attack 100 ms, release 500 ms
    const Instrument b = makeInstrument({zb}, {d});   // cùng AudioData (bản phái sinh)

    Sampler s;
    s.prepare(kSr, 128);
    s.setInstrument(&a, 1);
    s.noteOn(60, 1.0f);
    render(s, 480, 60);                           // tới sustain
    s.setInstrument(&b, 2);
    CHECK(s.countVoicesUsing(1) == 0);            // không giữ chân snapshot cũ → 68 retire ngay
    CHECK(s.countVoicesUsing(2) == 1);
    CHECK(s.activeVoices() == 1);
    Out o = render(s, 960, 60);                   // 20 ms: KHÔNG fade (đổi cứng thì 5 ms là tắt)
    CHECK(s.activeVoices() == 1);
    CHECK(o.l.back() > 0.99f * o.l.front());
    CHECK(maxJump(o.l) < 1e-6f);

    s.noteOff(60);                                // nốt cũ nhả theo envelope CŨ (10 ms), không phải 500 ms
    render(s, 4800, 60);                          // 100 ms
    CHECK(s.activeVoices() == 0);

    s.noteOn(62, 1.0f);                           // nốt mới dùng envelope MỚI: attack 100 ms
    o = render(s, 2400, 62);                      // 50 ms → mới lên khoảng nửa
    CHECK(o.l.back() < 0.6f * 0.5f * kCenter);
    CHECK(o.l.back() > 0.3f * 0.5f * kCenter);
    s.noteOn(64, 1.0f);                           // choke nhóm 1 vẫn chạy (offBy của voice)
    render(s, 480, 64);
    CHECK(s.activeVoices() == 1);
}
