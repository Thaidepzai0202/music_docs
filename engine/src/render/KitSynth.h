// KitSynth — khối dựng dùng chung cho các file DrumKits*.cpp (nội bộ render/, KHÔNG phải API). [worker]
// Header-only (inline) để mọi kit dùng đúng một bản code → cùng input ra cùng từng bit.
//   Noise    nhiễu trắng xorshift32 tất định          Filter / lp / hp / bp   biquad RBJ (dsp/Biquad.h)
//   Voice    buffer một âm + fade-out cosin 30 % cuối sweepSine / partials / noise / metal   các lớp âm cộng vào Voice
//   finish   bão hoà tanh → fade → đỉnh −1 dBFS → cắt đuôi −80 dBFS
//   crush    giảm bit (tuyến tính hoặc μ-law) + giữ mẫu (sample-and-hold) kiểu máy trống sample 8-bit / lo-fi
//   vinyl    hiss + tiếng lách tách đĩa than (tất định theo seed)
//   pad / assemble   ghép 16 pad → Kit (cân loudness bằng SFZ volume, sinh text .sfz)
#pragma once

#include "dsp/Biquad.h"
#include "render/DrumKits.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace le::render::kits::synth {

inline constexpr double kTwoPi = 6.283185307179586476925;
using dsp::BiquadCoeffs;
using dsp::BiquadState;

// Nhiễu trắng tất định [-1, 1)
struct Noise {
    uint32_t s;
    explicit Noise(uint32_t seed) : s(seed == 0 ? 0x9E3779B9u : seed) {}
    float next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }
};

struct Filter {
    BiquadCoeffs k;
    BiquadState st;
    float operator()(float x) { return st.process(k, x); }
};
inline Filter lp(double f, double q, double sr) { return {BiquadCoeffs::lowPass(f, q, sr), {}}; }
inline Filter hp(double f, double q, double sr) { return {BiquadCoeffs::highPass(f, q, sr), {}}; }
inline Filter bp(double f, double q, double sr) { return {BiquadCoeffs::bandPass(f, q, sr), {}}; }

// Một âm = hàm của (thời gian t, chỉ số i) trong `seconds` giây, nhân cửa sổ fade-out cosin ở 30 % cuối.
struct Voice {
    double sr;
    std::vector<float> x;
    Voice(double sampleRate, double seconds)
        : sr(sampleRate), x(static_cast<size_t>(std::llround(seconds * sampleRate)), 0.0f) {}
    size_t size() const { return x.size(); }
    double t(size_t i) const { return static_cast<double>(i) / sr; }
    void add(size_t i, double v) { x[i] += static_cast<float>(v); }
    void finishWindow(double fadeFrac = 0.3) {
        const size_t n = x.size(), start = static_cast<size_t>((1.0 - fadeFrac) * static_cast<double>(n));
        for (size_t i = start; i < n; ++i) {
            const double u = static_cast<double>(i - start) / static_cast<double>(n - start);
            x[i] *= static_cast<float>(0.5 * (1.0 + std::cos(3.14159265358979323846 * u)));
        }
    }
};

inline double expDecay(double t, double tau) { return std::exp(-t / tau); }
inline double attack(double t, double a) { return a <= 0.0 ? 1.0 : std::min(1.0, t / a); }

// Sine quét cao độ: f(t) = f1 + (f0 − f1)·e^(−t/τf), biên độ amp·e^(−t/τa), attack ngắn. Pha tích luỹ bằng double.
inline void sweepSine(Voice& v, double f0, double f1, double tauF, double amp, double tauA, double att = 0.0005, double drive = 0.0) {
    double ph = 0.0;
    for (size_t i = 0; i < v.size(); ++i) {
        const double t = v.t(i);
        const double f = f1 + (f0 - f1) * std::exp(-t / tauF);
        double s = std::sin(ph);
        if (drive > 0.0) s = std::tanh(drive * s) / std::tanh(drive);
        v.add(i, amp * s * expDecay(t, tauA) * attack(t, att));
        ph += kTwoPi * f / v.sr;
        if (ph > kTwoPi) ph -= kTwoPi;
    }
}

// Tổng các sine (partial: tần số, biên độ, τ decay)
struct Partial { double f, amp, tau; };
inline void partials(Voice& v, const std::vector<Partial>& ps, double att = 0.0005) {
    for (const Partial& p : ps) {
        double ph = 0.0;
        const double dph = kTwoPi * p.f / v.sr;
        for (size_t i = 0; i < v.size(); ++i) {
            const double t = v.t(i);
            v.add(i, p.amp * std::sin(ph) * expDecay(t, p.tau) * attack(t, att));
            ph += dph;
            if (ph > kTwoPi) ph -= kTwoPi;
        }
    }
}

// Nhiễu lọc qua chuỗi filter, nhân envelope env(t)
inline void noise(Voice& v, uint32_t seed, std::vector<Filter> chain, double amp, const std::function<double(double)>& env) {
    Noise nz(seed);
    for (size_t i = 0; i < v.size(); ++i) {
        float s = nz.next();
        for (Filter& f : chain) s = f(s);
        v.add(i, amp * s * env(v.t(i)));
    }
}

// "Kim loại" kiểu 808: 6 oscillator vuông ở tần số cố định của mạch 808 (không hài hoà), cộng lại.
inline std::vector<float> metal808(size_t n, double sr, double scale = 1.0) {
    static const double kF[6] = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0};
    std::vector<float> m(n, 0.0f);
    for (double f : kF) {
        double ph = 0.0;
        const double dph = f * scale / sr;
        for (size_t i = 0; i < n; ++i) {
            m[i] += (ph < 0.5 ? 1.0f : -1.0f) / 6.0f;
            ph += dph;
            if (ph >= 1.0) ph -= 1.0;
        }
    }
    return m;
}
inline void metal(Voice& v, std::vector<Filter> chain, double amp, const std::function<double(double)>& env, double scale = 1.0) {
    const std::vector<float> m = metal808(v.size(), v.sr, scale);
    for (size_t i = 0; i < v.size(); ++i) {
        float s = m[i];
        for (Filter& f : chain) s = f(s);
        v.add(i, amp * s * env(v.t(i)));
    }
}

// Bão hoà mềm tanh (drive > 0) làm âm "đặc" hơn → loudness tăng ở cùng đỉnh (như mạch 808/909 thật),
// rồi fade-out, chuẩn hoá đỉnh về kPeakDb, cắt đuôi dưới kTailDb.
inline void saturate(Voice& v, double drive) {
    if (drive > 0.0) {
        float peak = 0.0f;
        for (float s : v.x) peak = std::max(peak, std::fabs(s));
        if (peak > 0.0f) {
            const double norm = std::tanh(drive);
            for (float& s : v.x) s = static_cast<float>(std::tanh(drive * static_cast<double>(s) / peak) / norm);
        }
    }
}
// fadeFrac: phần cuối thời lượng được fade-out cosin (0.3 = 30 %).
inline std::vector<float> finish(Voice& v, double drive = 0.0, double fadeFrac = 0.3) {
    saturate(v, drive);
    v.finishWindow(fadeFrac);
    float peak = 0.0f;
    for (float s : v.x) peak = std::max(peak, std::fabs(s));
    std::vector<float> out = v.x;
    if (peak <= 0.0f) return out;
    const float g = std::pow(10.0f, kPeakDb / 20.0f) / peak;
    for (float& s : out) s *= g;
    const float tail = std::pow(10.0f, kTailDb / 20.0f);
    size_t last = 0;
    for (size_t i = 0; i < out.size(); ++i)
        if (std::fabs(out[i]) >= tail) last = i;
    out.resize(last + 1);
    return out;
}

// ─────────────── các âm ───────────────
using Env = std::function<double(double)>;
inline Env ex(double tau, double att = 0.0005) { return [=](double t) { return expDecay(t, tau) * attack(t, att); }; }

// Lọc cả buffer qua chuỗi filter (tại chỗ).
inline void filterVoice(Voice& v, std::vector<Filter> chain) {
    for (float& s : v.x)
        for (Filter& f : chain) s = f(s);
}

// Máy trống sample đời đầu / lo-fi: giữ mẫu ở holdHz (≤ 0 hoặc ≥ sr: không giữ) rồi lượng tử hoá `bits` bit theo đỉnh
// của âm. muLaw: nén μ-law (μ = 255) trước khi lượng tử, như chip 8-bit companding của 707 / LinnDrum — âm nhỏ vẫn
// mịn, âm to thì "sạn". Tất định: bước giữ mẫu là bộ tích luỹ pha double, không có ngẫu nhiên.
inline void crush(Voice& v, int bits, double holdHz, bool muLaw = false) {
    float peak = 0.0f;
    for (float s : v.x) peak = std::max(peak, std::fabs(s));
    if (peak <= 0.0f || bits < 2) return;
    const double step = holdHz > 0.0 ? std::min(1.0, holdHz / v.sr) : 1.0;
    const double q = std::ldexp(1.0, bits - 1), mu = 255.0, lmu = std::log1p(mu);
    double acc = 1.0, held = 0.0;
    for (float& s : v.x) {
        if (acc >= 1.0) {
            acc -= 1.0;
            held = static_cast<double>(s) / static_cast<double>(peak);
        }
        acc += step;
        double y = held;
        if (muLaw) {
            const double c = std::round(std::log1p(mu * std::fabs(y)) / lmu * q) / q;
            y = std::copysign(std::expm1(c * lmu) / mu, y);
        } else {
            y = std::round(y * q) / q;
        }
        s = static_cast<float>(y * static_cast<double>(peak));
    }
}

// Đĩa than: hiss (nhiễu lọc thấp) + tiếng lách tách thưa (~cracklePerSec lần/giây, xung qua bandpass 3 kHz).
// Mức tính theo đỉnh hiện tại của âm (level 0.01 ≈ −40 dB). Tất định theo seed.
inline void vinyl(Voice& v, uint32_t seed, double level, double cracklePerSec) {
    float peak = 0.0f;
    for (float s : v.x) peak = std::max(peak, std::fabs(s));
    Noise nz(seed);
    Filter hiss = lp(5000, 0.7, v.sr), tick = bp(3000, 0.8, v.sr);
    const double p = cracklePerSec / v.sr, a = level * static_cast<double>(peak);
    for (size_t i = 0; i < v.size(); ++i) {
        const float h = hiss(nz.next());
        const double r = 0.5 * (static_cast<double>(nz.next()) + 1.0);
        const float imp = r < p ? nz.next() * 6.0f : 0.0f;
        v.add(i, a * (0.5 * static_cast<double>(h) + static_cast<double>(tick(imp))));
    }
}

inline Kit assemble(const std::string& id, double sr, std::vector<KitPad> pads) {
    Kit k;
    k.id = id;
    k.sfzFileName = id + ".sfz";
    k.sampleRate = sr;
    for (KitPad& p : pads) {
        quantize24(p.samples);   // đúng giá trị file FLAC 24-bit sẽ chứa → test đo đúng thứ người dùng nghe
        p.loudnessDb = loudnessDb(p.samples, sr);
    }
    // Mức đích = âm NHỎ NHẤT của kit (không thể tăng âm nhỏ: file đã ở đỉnh −1 dBFS), nhưng không giảm âm nào quá
    // kMaxCutDb → nếu kit chênh > 12 dB thì âm nhỏ nhất nằm dưới mức đích.
    double lo = pads.front().loudnessDb, hi = lo;
    for (const KitPad& p : pads) {
        lo = std::min(lo, p.loudnessDb);
        hi = std::max(hi, p.loudnessDb);
    }
    const double target = std::max(lo, hi + static_cast<double>(kMaxCutDb));
    for (KitPad& p : pads) {
        const double cut = std::min(0.0, target - p.loudnessDb);
        p.volumeDb = static_cast<float>(std::round(cut * 10.0) / 10.0);
    }
    std::string s = "// " + id + " — 16 pad map General MIDI 36–51 (06 §4). Tổng hợp bằng code: engine/tools/le-gen-kits\n"
                    "// (agent 80, render/DrumKits*.cpp). Không dùng sample bên thứ ba. volume = cân loudness trong kit.\n"
                    "<control> default_path=samples/\n"
                    "<global> loop_mode=one_shot ampeg_release=0.05\n";
    for (const KitPad& p : pads) {
        char vol[32] = "";
        if (p.volumeDb < 0.0f) std::snprintf(vol, sizeof(vol), " volume=%.1f", static_cast<double>(p.volumeDb));
        s += "<region> key=" + std::to_string(p.key) + " sample=" + p.file + vol;
        if (p.group != 0) s += " group=" + std::to_string(p.group);
        if (p.offBy != 0) s += " off_by=" + std::to_string(p.offBy);
        s += " region_label=" + p.label + "\n";   // cuối dòng: nhãn có dấu cách
    }
    k.sfzText = s;
    k.pads = std::move(pads);
    return k;
}

inline KitPad pad(int key, const char* label, const char* file, std::vector<float> x, int group = 0, int offBy = 0) {
    KitPad p;
    p.key = key;
    p.label = label;
    p.file = file;
    p.samples = std::move(x);
    p.group = group;
    p.offBy = offBy;
    return p;
}

} // namespace le::render::kits::synth
