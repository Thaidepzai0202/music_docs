#include "render/DrumKits.h"

#include "render/KitSynth.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace le::render::kits {

namespace {

using namespace synth;
using dsp::BiquadCoeffs;
using dsp::BiquadState;

std::vector<float> kick(double sr, bool is909) {
    Voice v(sr, is909 ? 0.9 : 1.5);
    if (is909) {
        sweepSine(v, 240.0, 52.0, 0.02, 1.0, 0.22, 0.0003, 1.8);
        noise(v, 909, {bp(4000, 0.7, sr)}, 0.6, ex(0.0015, 0.0001));   // click
    } else {
        sweepSine(v, 120.0, 48.0, 0.035, 1.0, 0.35, 0.0005, 1.2);
        noise(v, 808, {hp(2000, 0.7, sr)}, 0.08, ex(0.001, 0.0001));
    }
    return finish(v);
}

std::vector<float> rim(double sr, bool is909) {
    Voice v(sr, 0.12);
    const double a = is909 ? 500.0 : 455.0, b = is909 ? 1800.0 : 1665.0;
    partials(v, {{a, 0.7, 0.02}, {b, 0.5, 0.012}}, 0.0001);
    noise(v, is909 ? 37u : 38u, {bp(3000, 1.0, sr)}, 0.4, ex(0.004, 0.0001));
    return finish(v, 4.0);
}

std::vector<float> snare(double sr, bool is909, bool second) {
    Voice v(sr, second ? 0.35 : 0.5);
    const double t1 = second ? (is909 ? 230.0 : 250.0) : (is909 ? 190.0 : 185.0);
    const double t2 = second ? (is909 ? 420.0 : 420.0) : (is909 ? 350.0 : 330.0);
    const double tauT = second ? 0.03 : (is909 ? 0.04 : 0.05);
    const double toneAmp = is909 ? 0.45 : 0.6;
    sweepSine(v, t1 * 1.08, t1, 0.01, toneAmp, tauT);
    sweepSine(v, t2 * 1.06, t2, 0.01, toneAmp * 0.6, tauT * 0.8);
    const double noiseAmp = is909 ? 0.85 : 0.5;
    const double tauN = second ? (is909 ? 0.1 : 0.08) : (is909 ? 0.16 : 0.12);
    if (is909)
        noise(v, second ? 402u : 401u, {hp(second ? 1800 : 1200, 0.7, sr), lp(9000, 0.7, sr)}, noiseAmp, ex(tauN, 0.0003));
    else
        noise(v, second ? 302u : 301u, {bp(second ? 2600 : 1800, 0.8, sr), hp(700, 0.7, sr)}, noiseAmp * 1.6, ex(tauN, 0.0003));
    return finish(v, is909 ? 3.0 : 2.5);
}

std::vector<float> clap(double sr, bool is909) {
    Voice v(sr, is909 ? 0.7 : 0.6);
    const std::vector<double> bursts = is909 ? std::vector<double>{0.0, 0.011, 0.022} : std::vector<double>{0.0, 0.009, 0.018, 0.027};
    const double tailStart = bursts.back() + 0.003, tailTau = is909 ? 0.2 : 0.12, tailAmp = is909 ? 0.6 : 0.5;
    const Env env = [=](double t) {
        double e = 0.0;
        for (double b : bursts)
            if (t >= b) e += std::exp(-(t - b) / 0.006);
        if (t >= tailStart) e += tailAmp * std::exp(-(t - tailStart) / tailTau);
        return e * attack(t, 0.0002);
    };
    if (is909) noise(v, 39u, {bp(1600, 1.0, sr), hp(800, 0.7, sr)}, 1.0, env);
    else noise(v, 38u, {bp(1200, 1.2, sr)}, 1.0, env);
    return finish(v, 3.0);
}

// Tom: sine quét từ ~1.5–1.8 × f về f
std::vector<float> tom(double sr, bool is909, double f, double tauA, double seconds, uint32_t seed) {
    Voice v(sr, seconds);
    sweepSine(v, f * (is909 ? 1.8 : 1.5), f, is909 ? 0.04 : 0.05, 1.0, tauA);
    noise(v, seed, {hp(1000, 0.7, sr)}, is909 ? 0.3 : 0.15, ex(is909 ? 0.02 : 0.003, 0.0001));
    return finish(v);
}

std::vector<float> hat(double sr, bool is909, int kind /*0 đóng, 1 chân, 2 mở*/) {
    const double seconds = kind == 2 ? 1.2 : (kind == 1 ? 0.14 : 0.18);
    const double tau = kind == 2 ? (is909 ? 0.3 : 0.28) : (kind == 1 ? (is909 ? 0.018 : 0.02) : (is909 ? 0.03 : 0.035));
    Voice v(sr, seconds);
    if (is909) {
        metal(v, {hp(8000, 0.7, sr), bp(11000, 0.8, sr)}, 0.6, ex(tau, 0.0002));
        noise(v, static_cast<uint32_t>(4200 + kind), {hp(8000, 0.7, sr)}, 0.5, ex(tau, 0.0002));
    } else {
        metal(v, {bp(kind == 1 ? 6000 : 7200, 1.5, sr), hp(6000, 0.7, sr)}, 1.0, ex(tau, 0.0002));
        if (kind == 1) noise(v, 4401u, {bp(1500, 1.0, sr)}, 0.15, ex(0.006, 0.0002));   // tiếng "chick" của chân
    }
    return finish(v, kind == 2 ? 1.5 : 3.0);
}

std::vector<float> crash(double sr, bool is909) {
    Voice v(sr, is909 ? 3.2 : 3.0);
    metal(v, {bp(6000, 0.7, sr), hp(3500, 0.7, sr)}, 0.5, ex(is909 ? 1.0 : 0.9, 0.001), 1.37);
    noise(v, is909 ? 4901u : 4902u, {hp(is909 ? 4000 : 3500, 0.7, sr)}, 0.5, ex(is909 ? 1.0 : 0.9, 0.002));
    return finish(v);
}

std::vector<float> ride(double sr, bool is909) {
    Voice v(sr, is909 ? 3.2 : 3.0);
    const double tau = is909 ? 1.3 : 1.1;
    partials(v, {{2510, 0.35, tau * 0.6}, {3790, 0.25, tau * 0.5}, {5310, 0.18, tau * 0.4}}, 0.0005);   // "ping" chuông
    metal(v, {bp(5000, 1.2, sr), hp(3000, 0.7, sr)}, 0.5, ex(tau, 0.0005), 1.9);
    noise(v, is909 ? 5101u : 5102u, {hp(is909 ? 5000 : 4500, 0.7, sr)}, 0.25, ex(tau, 0.002));
    return finish(v);
}

// ── Percussion ──
std::vector<float> handDrum(double sr, double f, double tauA, double seconds, uint32_t seed) {
    Voice v(sr, seconds);
    sweepSine(v, f * 1.17, f, 0.015, 1.0, tauA);
    partials(v, {{f * 1.59, 0.25, tauA * 0.5}, {f * 2.14, 0.12, tauA * 0.35}});   // mode màng tròn (Bessel)
    noise(v, seed, {bp(1500, 1.0, sr)}, 0.2, ex(0.004, 0.0001));                  // slap
    return finish(v, 1.2);
}

std::vector<float> cowbell(double sr) {
    Voice v(sr, 0.8);
    std::vector<float> sq(v.size(), 0.0f);
    for (double f : {540.0, 800.0}) {
        double ph = 0.0;
        for (size_t i = 0; i < v.size(); ++i) {
            sq[i] += ph < 0.5 ? 0.5f : -0.5f;
            ph += f / sr;
            if (ph >= 1.0) ph -= 1.0;
        }
    }
    Filter b = bp(800, 2.5, sr), h = hp(400, 0.7, sr);
    for (size_t i = 0; i < v.size(); ++i) {
        const double t = v.t(i);
        v.add(i, h(b(sq[i])) * (0.7 * expDecay(t, 0.012) + 0.3 * expDecay(t, 0.25)) * attack(t, 0.0003));
    }
    return finish(v, 2.0);
}

std::vector<float> tambourine(double sr) {
    Voice v(sr, 0.8);
    const Env env = [](double t) { return expDecay(t, 0.25) * (1.0 + 0.5 * std::sin(kTwoPi * 14.0 * t)) * attack(t, 0.002); };
    noise(v, 4101u, {bp(8000, 1.5, sr)}, 0.6, env);
    partials(v, {{5400, 0.12, 0.2}, {6900, 0.1, 0.18}, {8200, 0.08, 0.16}, {9700, 0.06, 0.14}}, 0.002);   // lục lạc
    return finish(v);
}

std::vector<float> shaker(double sr) {
    Voice v(sr, 0.3);
    noise(v, 4201u, {bp(6500, 1.2, sr), hp(3000, 0.7, sr)}, 1.0,
          [](double t) { return t < 0.015 ? t / 0.015 : std::exp(-(t - 0.015) / 0.06); });
    return finish(v, 2.0);
}

std::vector<float> clave(double sr) {
    Voice v(sr, 0.25);
    partials(v, {{2480, 1.0, 0.03}, {3740, 0.2, 0.02}}, 0.0001);
    return finish(v, 1.5);
}

std::vector<float> triangle(double sr, bool open) {
    Voice v(sr, open ? 3.0 : 0.25);
    const double tau = open ? 1.2 : 0.06;
    partials(v, {{1245, 1.0, tau}, {3486, 0.6, tau * 0.8}, {5230, 0.4, tau * 0.7}, {7110, 0.25, tau * 0.6}}, 0.0002);
    return finish(v, open ? 0.0 : 1.5);
}

std::vector<float> agogo(double sr, double f) {
    Voice v(sr, 1.0);
    partials(v, {{f, 1.0, 0.35}, {f * 2.07, 0.45, 0.2}, {f * 3.94, 0.25, 0.12}}, 0.0003);
    return finish(v);
}

std::vector<float> guiro(double sr) {
    Voice v(sr, 0.4);
    const Env env = [](double t) {
        double e = 0.0;
        for (int k = 0; k < 14; ++k) {
            const double tk = 0.02 * k;
            if (t >= tk) e += std::sin(3.14159265358979323846 * (k + 0.5) / 14.0) * std::exp(-(t - tk) / 0.004);
        }
        return e;
    };
    noise(v, 4801u, {bp(2800, 1.5, sr)}, 1.0, env);
    return finish(v, 3.0);
}

std::vector<float> cabasa(double sr) {
    Voice v(sr, 0.3);
    noise(v, 4901u, {bp(9000, 1.0, sr), hp(5000, 0.7, sr)}, 1.0, ex(0.09, 0.005));
    return finish(v, 1.5);
}

std::vector<float> timbale(double sr, double f, uint32_t seed) {
    Voice v(sr, 0.9);
    sweepSine(v, f * 1.09, f, 0.01, 1.0, 0.3);
    partials(v, {{f * 2.3, 0.3, 0.2}, {f * 3.6, 0.2, 0.15}});   // vang vỏ kim loại
    noise(v, seed, {bp(4000, 1.0, sr)}, 0.35, ex(0.015, 0.0001));
    return finish(v);
}

std::vector<KitPad> drumPads(double sr, bool is909) {
    // Tom: cao độ tăng dần 41 < 43 < 45 < 47 < 48 < 50 (≈ quãng ba thứ), tom cao ngắn hơn
    const double f808[6] = {82, 98, 116, 138, 164, 196}, f909[6] = {90, 110, 130, 155, 185, 220};
    const double* f = is909 ? f909 : f808;
    auto T = [&](int i) {
        const double tau = 0.30 - 0.024 * i, dur = 1.0 - 0.06 * i;
        return tom(sr, is909, f[i], is909 ? tau * 0.85 : tau, dur, static_cast<uint32_t>(600 + 10 * i + (is909 ? 1 : 0)));
    };
    return {
        pad(36, "Kick", "kick.flac", kick(sr, is909)),
        pad(37, "Rim", "rim.flac", rim(sr, is909)),
        pad(38, "Snare", "snare.flac", snare(sr, is909, false)),
        pad(39, "Clap", "clap.flac", clap(sr, is909)),
        pad(40, "Snare 2", "snare2.flac", snare(sr, is909, true)),
        pad(41, "Floor Tom L", "tom_floor_lo.flac", T(0)),
        pad(42, "Closed Hat", "hat_closed.flac", hat(sr, is909, 0), 1),
        pad(43, "Floor Tom H", "tom_floor_hi.flac", T(1)),
        pad(44, "Pedal Hat", "hat_pedal.flac", hat(sr, is909, 1), 1),
        pad(45, "Low Tom", "tom_lo.flac", T(2)),
        pad(46, "Open Hat", "hat_open.flac", hat(sr, is909, 2), 2, 1),   // bị 42 / 44 (group 1) chặn
        pad(47, "Mid Tom", "tom_mid.flac", T(3)),
        pad(48, "Hi-Mid Tom", "tom_mid_hi.flac", T(4)),
        pad(49, "Crash", "crash.flac", crash(sr, is909)),
        pad(50, "High Tom", "tom_hi.flac", T(5)),
        pad(51, "Ride", "ride.flac", ride(sr, is909)),
    };
}

} // namespace

double loudnessDb(const std::vector<float>& x, double sr) {
    if (x.empty() || !(sr > 0.0)) return -200.0;
    BiquadState s1, s2;
    const BiquadCoeffs k1 = BiquadCoeffs::highShelf(1681.97, 4.0, sr), k2 = BiquadCoeffs::highPass(38.13, 0.5, sr);
    std::vector<double> sq(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        const double y = s2.process(k2, s1.process(k1, x[i]));
        sq[i] = y * y;
    }
    const size_t win = std::max<size_t>(1, static_cast<size_t>(0.1 * sr)), hop = std::max<size_t>(1, win / 10);
    double best = 0.0;
    for (size_t a = 0; a < x.size(); a += hop) {
        const size_t b = std::min(x.size(), a + win);
        double e = 0.0;
        for (size_t i = a; i < b; ++i) e += sq[i];
        best = std::max(best, e / static_cast<double>(win));   // chia cửa sổ đủ: âm ngắn hơn 100 ms bị tính nhỏ hơn
        if (b == x.size()) break;
    }
    return best > 0.0 ? -0.691 + 10.0 * std::log10(best) : -200.0;
}

Kit make808(double sr) { return assemble("kit_808", sr, drumPads(sr, false)); }
Kit make909(double sr) { return assemble("kit_909", sr, drumPads(sr, true)); }

Kit makePerc(double sr) {
    return assemble("kit_perc", sr, {
        pad(36, "Conga Lo", "conga_lo.flac", handDrum(sr, 196, 0.16, 0.6, 3601u)),
        pad(37, "Conga Hi", "conga_hi.flac", handDrum(sr, 294, 0.12, 0.5, 3701u)),
        pad(38, "Bongo Lo", "bongo_lo.flac", handDrum(sr, 392, 0.07, 0.35, 3801u)),
        pad(39, "Bongo Hi", "bongo_hi.flac", handDrum(sr, 523, 0.055, 0.3, 3901u)),
        pad(40, "Cowbell", "cowbell.flac", cowbell(sr)),
        pad(41, "Tambourine", "tambourine.flac", tambourine(sr)),
        pad(42, "Shaker", "shaker.flac", shaker(sr)),
        pad(43, "Clave", "clave.flac", clave(sr)),
        pad(44, "Triangle Mute", "triangle_mute.flac", triangle(sr, false), 3),
        pad(45, "Triangle Open", "triangle_open.flac", triangle(sr, true), 4, 3),   // bị 44 (group 3) chặn
        pad(46, "Agogo Lo", "agogo_lo.flac", agogo(sr, 745)),
        pad(47, "Agogo Hi", "agogo_hi.flac", agogo(sr, 1115)),
        pad(48, "Guiro", "guiro.flac", guiro(sr)),
        pad(49, "Cabasa", "cabasa.flac", cabasa(sr)),
        pad(50, "Timbale Lo", "timbale_lo.flac", timbale(sr, 330, 5001u)),
        pad(51, "Timbale Hi", "timbale_hi.flac", timbale(sr, 440, 5101u)),
    });
}

std::vector<Kit> makeAllKits(double sr) {
    return {make808(sr), make909(sr), makePerc(sr), makeTrap(sr), makeLofi(sr), make606(sr), make707(sr), makeLinn(sr)};
}

void quantize24(std::vector<float>& x) {
    constexpr double kScale = 8388608.0;   // 2^23
    for (float& s : x) {
        const double k = std::clamp(std::round(static_cast<double>(s) * kScale), -kScale, kScale - 1.0);
        s = static_cast<float>(k / kScale);   // k / 2^23: float biểu diễn đúng (≤ 24 bit mantissa)
    }
}

} // namespace le::render::kits
