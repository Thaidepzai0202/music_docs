#include "render/SynthFixtures.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace le::render::fixtures {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Nhiễu trắng đều [−1, 1) — xorshift32, cùng seed → cùng chuỗi trên mọi máy.
struct Noise {
    uint32_t s;
    explicit Noise(uint32_t seed) : s(seed | 1u) {}
    double next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<double>(s) / 2147483648.0 - 1.0;
    }
};

// Biquad high-pass (RBJ Audio EQ Cookbook), chạy double. Chỉ dùng lúc sinh fixture.
struct HighPass {
    double b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    HighPass(double fc, double q, double sr) {
        const double w = 2 * kPi * fc / sr, alpha = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + alpha;
        b0 = (1 + c) / 2 / a0;
        b1 = -(1 + c) / a0;
        b2 = (1 + c) / 2 / a0;
        a1 = -2 * c / a0;
        a2 = (1 - alpha) / a0;
    }
    double process(double x) {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

std::vector<float> finish(std::vector<double> x, double peak, double sr) {
    double m = 0.0;
    for (double v : x) m = std::max(m, std::fabs(v));
    const double g = m > 0.0 ? peak / m : 0.0;
    const size_t fade = static_cast<size_t>(0.005 * sr);   // 5 ms cuối về 0: hết sample không click
    std::vector<float> out(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        double v = x[i] * g;
        const size_t left = x.size() - 1 - i;
        if (left < fade) v *= static_cast<double>(left) / static_cast<double>(fade);
        out[i] = static_cast<float>(v) + 0.0f;   // + 0.0f: đổi −0.0 thành +0.0 (bit ổn định khi đọc/ghi lại)
    }
    return out;
}

std::vector<float> kick(double sr) {
    const size_t n = static_cast<size_t>(0.5 * sr);
    std::vector<double> x(n);
    Noise nz(0xC0FFEE);
    double phase = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        const double f = 45.0 + (150.0 - 45.0) * std::exp(-t / 0.03);   // quét cao độ 150 → 45 Hz
        const double body = std::exp(-t / 0.25) * std::sin(phase);
        const double click = 0.3 * std::exp(-t / 0.002) * nz.next();
        x[i] = body + click;
        phase += 2 * kPi * f / sr;
    }
    return finish(std::move(x), 0.9, sr);
}

std::vector<float> snare(double sr) {
    const size_t n = static_cast<size_t>(0.35 * sr);
    std::vector<double> x(n);
    Noise nz(0x5A4E);
    HighPass hp(1500.0, 0.707, sr);
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        const double tone = 0.5 * std::exp(-t / 0.08) * std::sin(2 * kPi * 185.0 * t) +
                            0.3 * std::exp(-t / 0.06) * std::sin(2 * kPi * 330.0 * t);
        x[i] = tone + 0.6 * std::exp(-t / 0.15) * hp.process(nz.next());
    }
    return finish(std::move(x), 0.8, sr);
}

std::vector<float> hat(double sr, double seconds, double decay, uint32_t seed) {
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<double> x(n);
    Noise nz(seed);
    HighPass hp1(7000.0, 0.707, sr), hp2(7000.0, 0.707, sr);   // 2 tầng → dốc 24 dB/oct
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        x[i] = std::exp(-t / decay) * hp2.process(hp1.process(nz.next()));
    }
    return finish(std::move(x), 0.5, sr);
}

// Tone hoạ âm tần số nguyên Hz: sau attack 10 ms là đều tuyệt đối → vòng 1 s liền mạch.
std::vector<float> tone(double sr, double hz, int harmonics, double amp) {
    const size_t n = static_cast<size_t>(1.5 * sr);
    std::vector<float> out(n);
    double norm = 0.0;
    for (int h = 1; h <= harmonics; ++h) norm += 1.0 / h;
    const size_t fadeStart = n - static_cast<size_t>(0.005 * sr);
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        double s = 0.0;
        for (int h = 1; h <= harmonics; ++h) s += std::sin(2 * kPi * hz * h * t + 0.7 * h) / h;
        double env = std::min(1.0, t / 0.010);
        if (i >= fadeStart) env *= static_cast<double>(n - 1 - i) / static_cast<double>(n - 1 - fadeStart);   // ngoài vòng loop
        out[i] = static_cast<float>(amp * env * s / norm) + 0.0f;   // −0.0 → +0.0
    }
    return out;
}

const std::vector<ToneZoneInfo>& zones() {
    // hz nguyên gần nốt chuẩn; tune = −1200·log2(hz / nốt chuẩn)
    static const std::vector<ToneZoneInfo> z = [] {
        std::vector<ToneZoneInfo> v = {{48, 36, 54, 131.0, 0.0}, {60, 55, 66, 262.0, 0.0}, {72, 67, 84, 523.0, 0.0}};
        for (auto& e : v) {
            const double ref = 440.0 * std::pow(2.0, (e.rootKey - 69) / 12.0);
            e.tuneCents = -1200.0 * std::log2(e.hz / ref);
        }
        return v;
    }();
    return z;
}
} // namespace

std::vector<ToneZoneInfo> instrumentZones() { return zones(); }

SynthSet makeDrumKit(double sr) {
    SynthSet s;
    s.name = "kit_synth";
    s.sfzFileName = "kit_synth.sfz";
    s.sampleRate = sr;
    s.files = {{"samples/kick.wav", kick(sr)},
               {"samples/snare.wav", snare(sr)},
               {"samples/hat_closed.wav", hat(sr, 0.15, 0.04, 0xAA11)},
               {"samples/hat_open.wav", hat(sr, 0.8, 0.40, 0xBB22)}};
    s.sfzText =
        "// kit_synth — bộ trống TỔNG HỢP bằng code cho test (P1-28). Sinh bởi le-gen-fixtures.\n"
        "// Không dùng sample có license. Cấu trúc giống ví dụ ở docs/06 §4.\n"
        "<control> default_path=samples/\n"
        "<global> loop_mode=one_shot ampeg_release=0.05\n"
        "<region> key=36 sample=kick.wav\n"
        "<region> key=38 sample=snare.wav\n"
        "<region> key=42 sample=hat_closed.wav group=1 off_by=2\n"
        "<region> key=46 sample=hat_open.wav   group=2 off_by=1\n";
    return s;
}

SynthSet makeInstrument(double sr) {
    SynthSet s;
    s.name = "inst_synth";
    s.sfzFileName = "inst_synth.sfz";
    s.sampleRate = sr;
    const long loopStart = std::lround(0.25 * sr), loopEnd = std::lround(1.25 * sr) - 1;   // SFZ: loop_end gồm
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "// inst_synth — nhạc cụ TỔNG HỢP bằng code cho test (P1-28). Sinh bởi le-gen-fixtures.\n"
                  "// 3 root (C3/C4/C5) x 2 lớp velocity. Tần số nguyên Hz, tune bù về nốt chuẩn. Không có license bên thứ ba.\n"
                  "<control> default_path=samples/\n"
                  "<global> loop_mode=loop_continuous loop_start=%ld loop_end=%ld ampeg_attack=0.005 ampeg_release=0.3\n",
                  loopStart, loopEnd);
    s.sfzText = buf;
    const char* names[] = {"c3", "c4", "c5"};
    for (int layer = 0; layer < 2; ++layer) {
        const bool loud = layer == 1;
        s.sfzText += loud ? "<group> lovel=64 hivel=127\n" : "<group> lovel=1 hivel=63\n";
        for (size_t z = 0; z < zones().size(); ++z) {
            const ToneZoneInfo& info = zones()[z];
            const std::string file = std::string("tone_") + names[z] + (loud ? "_loud.wav" : "_soft.wav");
            std::snprintf(buf, sizeof(buf), "<region> sample=%s lokey=%d hikey=%d pitch_keycenter=%d tune=%.2f\n",
                          file.c_str(), info.loKey, info.hiKey, info.rootKey, info.tuneCents);
            s.sfzText += buf;
            s.files.push_back({"samples/" + file, tone(sr, info.hz, loud ? 10 : 4, loud ? 0.6 : 0.25)});
        }
    }
    return s;
}

uint64_t fnv1a(const std::vector<float>& x) {
    uint64_t h = 1469598103934665603ull;
    const auto* p = reinterpret_cast<const unsigned char*>(x.data());
    for (size_t i = 0; i < x.size() * sizeof(float); ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

} // namespace le::render::fixtures
