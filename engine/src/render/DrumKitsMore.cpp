// P2-35: 5 kit tổng hợp thêm (Trap, Lo-fi / Boom-bap, 606, 707, Linn) — cùng map GM 36–51 và cùng khối dựng
// (render/KitSynth.h) với 808 / 909 / Percussion. [worker]
#include "render/DrumKits.h"

#include "render/KitSynth.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace le::render::kits {

namespace {

using namespace synth;

// Clap: các burst nhiễu cách nhau vài ms + đuôi (vang ngắn). Envelope là tổng các xung mũ.
Env clapEnv(std::initializer_list<double> bursts, double burstTau, double tailAmp, double tailTau) {
    const std::vector<double> b(bursts);
    const double tailStart = b.back() + 0.003;
    return [=](double t) {
        double e = 0.0;
        for (double x : b)
            if (t >= x) e += std::exp(-(t - x) / burstTau);
        if (t >= tailStart) e += tailAmp * std::exp(-(t - tailStart) / tailTau);
        return e * attack(t, 0.0002);
    };
}

// Tom sine quét từ f·sweep về f (+ mode màng 1.5·f nếu mode > 0) + nhiễu gõ.
struct TomSpec {
    double sweep, tauF, tauA0, tauStep, dur0, durStep, mode, noiseAmp, noiseTau, noiseHz, drive;
};
std::vector<float> tomOf(double sr, const TomSpec& s, double f, int i, uint32_t seed) {
    const double tauA = s.tauA0 - s.tauStep * i;
    Voice v(sr, s.dur0 - s.durStep * i);
    sweepSine(v, f * s.sweep, f, s.tauF, 1.0, tauA, 0.0005, s.drive);
    if (s.mode > 0.0) partials(v, {{f * 1.5, s.mode, tauA * 0.45}});
    noise(v, seed, {bp(s.noiseHz, 0.8, sr)}, s.noiseAmp, ex(s.noiseTau, 0.0001));
    return finish(v);
}

// Xử lý chung sau khi tổng hợp một âm của kit "máy sample" / lo-fi (gọi trước finish).
enum class Character { Clean, Lofi, Sp707, SpLinn, Sp707Cymbal };
void character(Voice& v, Character c, uint32_t seed) {
    switch (c) {
        case Character::Clean: break;
        case Character::Lofi:   // bitcrush nhẹ 12 bit @ 24 kHz, lọc ấm 9 kHz, hiss + lách tách
            filterVoice(v, {lp(9000, 0.7, v.sr)});
            crush(v, 12, 24000.0);
            filterVoice(v, {lp(10000, 0.7, v.sr)});
            vinyl(v, seed, 0.012, 7.0);
            break;
        case Character::Sp707:        // TR-707: sample 8-bit companding @ 25 kHz
        case Character::Sp707Cymbal:  // cymbal của 707 chỉ 6-bit
            filterVoice(v, {lp(11000, 0.7, v.sr)});
            crush(v, c == Character::Sp707 ? 8 : 6, 25000.0, true);
            filterVoice(v, {lp(11500, 0.7, v.sr)});
            break;
        case Character::SpLinn:       // LinnDrum: 8-bit companding @ 28 kHz
            filterVoice(v, {lp(12000, 0.7, v.sr)});
            crush(v, 8, 28000.0, true);
            filterVoice(v, {lp(12500, 0.7, v.sr)});
            break;
    }
}
std::vector<float> done(Voice& v, Character c, uint32_t seed, double drive) {
    saturate(v, drive);
    character(v, c, seed);
    return finish(v);
}

// ─────────────── Trap ───────────────
std::vector<float> trapKick(double sr) {
    Voice v(sr, 2.4);
    // 808 dài có glide: trượt từ 110 Hz về 45 Hz (τ 60 ms → nghe rõ tiếng "vuốt" xuống), ngân ~2 s, méo tanh để
    // loa nhỏ (iPad) vẫn nghe được hài bậc cao của sub.
    sweepSine(v, 110.0, 45.0, 0.06, 1.0, 0.8, 0.0005, 2.5);
    noise(v, 7001u, {hp(1500, 0.7, sr)}, 0.12, ex(0.0015, 0.0001));
    return finish(v);
}
std::vector<float> trapPerc(double sr) {
    Voice v(sr, 0.25);
    partials(v, {{620, 1.0, 0.045}, {1590, 0.4, 0.02}, {2700, 0.2, 0.01}}, 0.0002);
    noise(v, 7011u, {bp(4000, 1.0, sr)}, 0.3, ex(0.002, 0.0001));
    return finish(v, 2.0);
}
std::vector<float> trapSnare(double sr) {
    Voice v(sr, 0.35);
    sweepSine(v, 260.0, 210.0, 0.01, 0.5, 0.035);
    noise(v, 7101u, {hp(2200, 0.7, sr), lp(12000, 0.7, sr)}, 1.0, ex(0.075, 0.0002));
    return finish(v, 3.5);
}
std::vector<float> trapClap(double sr) {
    Voice v(sr, 0.45);
    noise(v, 7201u, {bp(1500, 1.0, sr), hp(900, 0.7, sr)}, 1.0, clapEnv({0.0, 0.007, 0.014, 0.021}, 0.004, 0.45, 0.09));
    return finish(v, 3.0);
}
std::vector<float> trapSnap(double sr) {
    Voice v(sr, 0.18);
    noise(v, 7301u, {bp(2600, 2.0, sr), hp(1200, 0.7, sr)}, 1.0, clapEnv({0.0, 0.004}, 0.006, 0.3, 0.03));
    partials(v, {{1800, 0.3, 0.008}}, 0.0001);
    return finish(v, 3.0);
}
std::vector<float> trapHat(double sr, int kind /*0 đóng, 1 chân, 2 mở*/) {
    // Hat đóng rất ngắn (~12 ms) → roll 1/32, 1/64 vẫn tách từng tiếng.
    const double tau = kind == 2 ? 0.18 : (kind == 1 ? 0.011 : 0.014);
    Voice v(sr, kind == 2 ? 0.9 : (kind == 1 ? 0.06 : 0.07));
    metal(v, {hp(8500, 0.7, sr), bp(11000, 0.9, sr)}, 0.7, ex(tau, 0.0001), 1.3);
    noise(v, static_cast<uint32_t>(7400 + kind), {hp(9000, 0.7, sr)}, 0.4, ex(tau * 0.9, 0.0001));
    return finish(v, kind == 2 ? 1.2 : 3.5);
}
std::vector<float> trapCrash(double sr) {
    Voice v(sr, 3.0);
    noise(v, 7501u, {hp(4500, 0.7, sr)}, 0.6, ex(1.1, 0.002));
    metal(v, {bp(7000, 0.7, sr), hp(4000, 0.7, sr)}, 0.5, ex(1.1, 0.002), 1.5);
    return finish(v);
}
// Riser: nhiễu qua bandpass quét 300 Hz → 9 kHz, to dần trong 1.6 s rồi tắt nhanh (τ 80 ms).
std::vector<float> trapRiser(double sr) {
    Voice v(sr, 2.0);
    Noise nz(7601u);
    constexpr double kRise = 1.6;
    BiquadState st1, st2;
    BiquadCoeffs k = BiquadCoeffs::bandPass(300.0, 1.5, sr);
    for (size_t i = 0; i < v.size(); ++i) {
        const double t = v.t(i), u = std::min(1.0, t / kRise);
        if (i % 32 == 0) k = BiquadCoeffs::bandPass(300.0 * std::pow(30.0, u * u), 1.5, sr);   // 300 Hz · 30 = 9 kHz
        const double env = t < kRise ? u * u : std::exp(-(t - kRise) / 0.08);
        v.add(i, env * static_cast<double>(st2.process(k, st1.process(k, nz.next()))));
    }
    return finish(v, 1.5, 0.2);
}

std::vector<KitPad> trapPads(double sr) {
    const double f[6] = {73, 87, 104, 123, 147, 175};   // 808 tom (sub) cách ~3 nửa cung
    const TomSpec ts{1.6, 0.03, 0.22, 0.02, 0.9, 0.06, 0.0, 0.1, 0.003, 1200, 1.8};
    auto T = [&](int i) { return tomOf(sr, ts, f[i], i, static_cast<uint32_t>(7700 + i)); };
    return {
        pad(36, "808 Kick", "kick.flac", trapKick(sr)),
        pad(37, "Perc", "perc.flac", trapPerc(sr)),
        pad(38, "Snare", "snare.flac", trapSnare(sr)),
        pad(39, "Clap", "clap.flac", trapClap(sr)),
        pad(40, "Snap", "snap.flac", trapSnap(sr)),
        pad(41, "Floor Tom L", "tom_floor_lo.flac", T(0)),
        pad(42, "Closed Hat", "hat_closed.flac", trapHat(sr, 0), 1),
        pad(43, "Floor Tom H", "tom_floor_hi.flac", T(1)),
        pad(44, "Pedal Hat", "hat_pedal.flac", trapHat(sr, 1), 1),
        pad(45, "Low Tom", "tom_lo.flac", T(2)),
        pad(46, "Open Hat", "hat_open.flac", trapHat(sr, 2), 2, 1),
        pad(47, "Mid Tom", "tom_mid.flac", T(3)),
        pad(48, "Hi-Mid Tom", "tom_mid_hi.flac", T(4)),
        pad(49, "Crash", "crash.flac", trapCrash(sr)),
        pad(50, "High Tom", "tom_hi.flac", T(5)),
        pad(51, "Riser", "riser.flac", trapRiser(sr)),
    };
}

// ─────────────── Lo-fi / Boom-bap ───────────────
constexpr Character kLofi = Character::Lofi;

std::vector<float> lofiKick(double sr) {
    Voice v(sr, 0.8);
    sweepSine(v, 150.0, 52.0, 0.03, 1.0, 0.25, 0.001, 1.3);
    noise(v, 8001u, {lp(2500, 0.7, sr)}, 0.15, ex(0.005, 0.0002));   // tiếng dùi
    filterVoice(v, {lp(5000, 0.7, sr)});
    return done(v, kLofi, 8002u, 0.0);
}
std::vector<float> lofiRim(double sr) {
    Voice v(sr, 0.15);
    partials(v, {{520, 0.8, 0.02}, {1500, 0.4, 0.012}}, 0.0002);
    noise(v, 8011u, {bp(2500, 1.0, sr)}, 0.3, ex(0.004, 0.0001));
    return done(v, kLofi, 8012u, 3.0);
}
std::vector<float> lofiSnare(double sr, bool second) {
    Voice v(sr, second ? 0.35 : 0.5);
    const double t1 = second ? 240.0 : 200.0, t2 = second ? 400.0 : 340.0;
    sweepSine(v, t1 * 1.08, t1, 0.01, 0.7, second ? 0.05 : 0.07);
    sweepSine(v, t2 * 1.05, t2, 0.01, 0.35, second ? 0.04 : 0.05);
    noise(v, second ? 8102u : 8101u, {bp(second ? 2200 : 1500, 0.7, sr), lp(second ? 8000 : 7000, 0.7, sr)}, 0.9,
          ex(second ? 0.1 : 0.17, 0.0005));
    return done(v, kLofi, second ? 8104u : 8103u, 2.5);
}
std::vector<float> lofiClap(double sr) {
    Voice v(sr, 0.55);
    noise(v, 8201u, {bp(1100, 1.0, sr), lp(5000, 0.7, sr)}, 1.0, clapEnv({0.0, 0.012, 0.024}, 0.006, 0.5, 0.15));
    return done(v, kLofi, 8202u, 2.5);
}
std::vector<float> lofiHat(double sr, int kind) {
    const double tau = kind == 2 ? 0.25 : (kind == 1 ? 0.02 : 0.03);
    Voice v(sr, kind == 2 ? 0.9 : (kind == 1 ? 0.15 : 0.2));
    noise(v, static_cast<uint32_t>(8300 + kind), {bp(7000, 0.7, sr), lp(9000, 0.7, sr)}, 1.0, ex(tau, 0.001));   // mềm
    if (kind == 1) noise(v, 8311u, {bp(1200, 1.0, sr)}, 0.15, ex(0.006, 0.0005));
    return done(v, kLofi, static_cast<uint32_t>(8320 + kind), kind == 2 ? 0.0 : 2.8);
}
std::vector<float> lofiCymbal(double sr, bool ride) {
    Voice v(sr, 2.5);
    if (ride) {
        partials(v, {{2300, 0.3, 0.5}, {3500, 0.2, 0.4}}, 0.0005);
        metal(v, {bp(4500, 1.2, sr), lp(7000, 0.7, sr)}, 0.5, ex(1.0, 0.0005), 1.9);
        noise(v, 8401u, {lp(6000, 0.7, sr), hp(2500, 0.7, sr)}, 0.2, ex(1.0, 0.002));
    } else {
        noise(v, 8402u, {lp(7000, 0.7, sr), hp(2500, 0.7, sr)}, 0.6, ex(0.9, 0.003));
        metal(v, {bp(5000, 0.7, sr), lp(7000, 0.7, sr)}, 0.4, ex(0.9, 0.003), 1.37);
    }
    return done(v, kLofi, ride ? 8403u : 8404u, 0.0);
}
std::vector<float> lofiTom(double sr, double f, int i) {
    Voice v(sr, 0.95 - 0.06 * i);
    const double tauA = 0.28 - 0.02 * i;
    sweepSine(v, f * 1.4, f, 0.04, 1.0, tauA, 0.001, 1.2);
    noise(v, static_cast<uint32_t>(8500 + i), {bp(900, 0.8, sr)}, 0.12, ex(0.006, 0.0003));
    filterVoice(v, {lp(4000, 0.7, sr)});
    return done(v, kLofi, static_cast<uint32_t>(8510 + i), 0.0);
}

std::vector<KitPad> lofiPads(double sr) {
    const double f[6] = {80, 95, 113, 134, 160, 190};
    auto T = [&](int i) { return lofiTom(sr, f[i], i); };
    return {
        pad(36, "Kick", "kick.flac", lofiKick(sr)),
        pad(37, "Rim", "rim.flac", lofiRim(sr)),
        pad(38, "Snare", "snare.flac", lofiSnare(sr, false)),
        pad(39, "Clap", "clap.flac", lofiClap(sr)),
        pad(40, "Snare 2", "snare2.flac", lofiSnare(sr, true)),
        pad(41, "Floor Tom L", "tom_floor_lo.flac", T(0)),
        pad(42, "Closed Hat", "hat_closed.flac", lofiHat(sr, 0), 1),
        pad(43, "Floor Tom H", "tom_floor_hi.flac", T(1)),
        pad(44, "Pedal Hat", "hat_pedal.flac", lofiHat(sr, 1), 1),
        pad(45, "Low Tom", "tom_lo.flac", T(2)),
        pad(46, "Open Hat", "hat_open.flac", lofiHat(sr, 2), 2, 1),
        pad(47, "Mid Tom", "tom_mid.flac", T(3)),
        pad(48, "Hi-Mid Tom", "tom_mid_hi.flac", T(4)),
        pad(49, "Crash", "crash.flac", lofiCymbal(sr, false)),
        pad(50, "High Tom", "tom_hi.flac", T(5)),
        pad(51, "Ride", "ride.flac", lofiCymbal(sr, true)),
    };
}

// ─────────────── 606 ───────────────
std::vector<float> k606Kick(double sr) {
    Voice v(sr, 0.45);
    sweepSine(v, 170.0, 58.0, 0.012, 1.0, 0.11, 0.0003, 1.4);   // ngắn, chặt
    noise(v, 6001u, {bp(3500, 1.0, sr)}, 0.2, ex(0.002, 0.0001));
    return finish(v, 1.0);
}
std::vector<float> k606Rim(double sr) {
    Voice v(sr, 0.1);
    partials(v, {{560, 0.7, 0.016}, {1900, 0.5, 0.01}}, 0.0001);
    return finish(v, 4.5);
}
std::vector<float> k606Snare(double sr, bool second) {
    Voice v(sr, second ? 0.25 : 0.3);
    const double t1 = second ? 280.0 : 240.0, t2 = second ? 460.0 : 400.0;
    sweepSine(v, t1 * 1.08, t1, 0.005, 0.35, 0.03);
    sweepSine(v, t2 * 1.05, t2, 0.005, 0.2, 0.025);
    noise(v, second ? 6102u : 6101u, {hp(second ? 2500 : 1500, 0.7, sr), lp(12000, 0.7, sr)}, 1.0,
          ex(second ? 0.06 : 0.08, 0.0002));
    return finish(v, 3.0);
}
std::vector<float> k606Clap(double sr) {
    Voice v(sr, 0.35);
    noise(v, 6201u, {bp(1400, 1.2, sr), hp(800, 0.7, sr)}, 1.0, clapEnv({0.0, 0.008, 0.016}, 0.004, 0.45, 0.08));
    return finish(v, 3.0);
}
std::vector<float> k606Hat(double sr, int kind) {
    const double tau = kind == 2 ? 0.2 : (kind == 1 ? 0.015 : 0.022);
    Voice v(sr, kind == 2 ? 0.8 : (kind == 1 ? 0.1 : 0.13));
    metal(v, {bp(8500, 1.2, sr), hp(7000, 0.7, sr)}, 1.0, ex(tau, 0.0001), 1.25);
    return finish(v, kind == 2 ? 1.2 : 3.5);
}
std::vector<float> k606Cymbal(double sr, bool ride) {
    Voice v(sr, 2.4);
    if (ride) {
        metal(v, {bp(5500, 1.5, sr), hp(4000, 0.7, sr)}, 0.7, ex(0.9, 0.0005), 1.8);
        partials(v, {{2800, 0.2, 0.4}}, 0.0005);
    } else {
        metal(v, {bp(6500, 0.8, sr), hp(4500, 0.7, sr)}, 0.7, ex(0.75, 0.001), 1.6);
        noise(v, 6301u, {hp(6000, 0.7, sr)}, 0.3, ex(0.75, 0.001));
    }
    return finish(v);
}

std::vector<KitPad> pads606(double sr) {
    const double f[6] = {100, 119, 141, 168, 200, 238};
    const TomSpec ts{1.3, 0.02, 0.14, 0.01, 0.6, 0.03, 0.0, 0.12, 0.003, 1500, 1.2};
    auto T = [&](int i) { return tomOf(sr, ts, f[i], i, static_cast<uint32_t>(6400 + i)); };
    return {
        pad(36, "Kick", "kick.flac", k606Kick(sr)),
        pad(37, "Rim", "rim.flac", k606Rim(sr)),
        pad(38, "Snare", "snare.flac", k606Snare(sr, false)),
        pad(39, "Clap", "clap.flac", k606Clap(sr)),
        pad(40, "Snare 2", "snare2.flac", k606Snare(sr, true)),
        pad(41, "Floor Tom L", "tom_floor_lo.flac", T(0)),
        pad(42, "Closed Hat", "hat_closed.flac", k606Hat(sr, 0), 1),
        pad(43, "Floor Tom H", "tom_floor_hi.flac", T(1)),
        pad(44, "Pedal Hat", "hat_pedal.flac", k606Hat(sr, 1), 1),
        pad(45, "Low Tom", "tom_lo.flac", T(2)),
        pad(46, "Open Hat", "hat_open.flac", k606Hat(sr, 2), 2, 1),
        pad(47, "Mid Tom", "tom_mid.flac", T(3)),
        pad(48, "Hi-Mid Tom", "tom_mid_hi.flac", T(4)),
        pad(49, "Crash", "crash.flac", k606Cymbal(sr, false)),
        pad(50, "High Tom", "tom_hi.flac", T(5)),
        pad(51, "Ride", "ride.flac", k606Cymbal(sr, true)),
    };
}

// ─────────────── 707 / Linn (máy trống sample 8-bit) ───────────────
struct SpSpec {
    Character c, cym;                                              // xử lý cho trống / cho cymbal
    double kickF0, kickF1, kickTauF, kickTauA, kickDur;
    double sn1, sn2, snTau, snNoiseHz, room;                       // room > 0: đuôi phòng của snare (Linn)
    double clap[4];                                                // thời điểm các burst (s)
    int clapN;
    double clapTail, tomSweep, tomTauF, tomTauA0, tomDur0, hatHp, hatBp, hatTauC, hatTauO, cymTau;
    double tomF[6];
    uint32_t seed;
};

std::vector<float> spKick(double sr, const SpSpec& s) {
    Voice v(sr, s.kickDur);
    sweepSine(v, s.kickF0, s.kickF1, s.kickTauF, 1.0, s.kickTauA, 0.0003, 1.5);
    noise(v, s.seed + 1, {bp(3000, 1.0, sr)}, 0.3, ex(0.003, 0.0001));   // tiếng dùi
    if (s.room > 0.0) noise(v, s.seed + 2, {lp(2500, 0.7, sr)}, 0.2, ex(0.006, 0.0002));
    return done(v, s.c, s.seed + 3, 0.0);
}
std::vector<float> spRim(double sr, const SpSpec& s) {
    Voice v(sr, 0.15);
    partials(v, {{s.room > 0.0 ? 480.0 : 400.0, 0.7, 0.02}, {s.room > 0.0 ? 1650.0 : 1200.0, 0.5, 0.012}}, 0.0001);
    noise(v, s.seed + 11, {bp(2500, 1.0, sr)}, 0.4, ex(0.004, 0.0001));
    return done(v, s.c, s.seed + 12, 3.0);
}
std::vector<float> spSnare(double sr, const SpSpec& s, bool second) {
    Voice v(sr, second ? 0.4 : (s.room > 0.0 ? 0.7 : 0.45));
    const double t1 = second ? s.sn1 * 1.2 : s.sn1, t2 = second ? s.sn2 * 1.2 : s.sn2;
    sweepSine(v, t1 * 1.08, t1, 0.008, 0.6, 0.05);
    sweepSine(v, t2 * 1.05, t2, 0.008, 0.35, 0.04);
    noise(v, s.seed + (second ? 22 : 21), {bp(second ? s.snNoiseHz * 1.3 : s.snNoiseHz, 0.5, sr), hp(second ? 1200 : 450, 0.7, sr)},
          0.9, ex(second ? s.snTau * 0.65 : s.snTau, 0.0003));
    if (s.room > 0.0) {   // "vang phòng" kiểu snare thập niên 80: nhiễu tối hơn, vào sau 10 ms, tắt chậm
        noise(v, s.seed + (second ? 24 : 23), {lp(6000, 0.7, sr), hp(300, 0.7, sr)}, second ? s.room * 0.7 : s.room,
              [](double t) { return t < 0.01 ? 0.0 : std::exp(-(t - 0.01) / 0.3) * std::min(1.0, (t - 0.01) / 0.01); });
    }
    return done(v, s.c, s.seed + (second ? 26 : 25), second ? 2.5 : 2.2);
}
std::vector<float> spClap(double sr, const SpSpec& s) {
    Voice v(sr, 0.55);
    const std::vector<double> b(s.clap, s.clap + s.clapN);
    const double tailStart = b.back() + 0.003, tail = s.clapTail;
    noise(v, s.seed + 31, {bp(1200, 1.05, sr), hp(650, 0.7, sr)}, 1.0, [=](double t) {
        double e = 0.0;
        for (double x : b)
            if (t >= x) e += std::exp(-(t - x) / 0.005);
        if (t >= tailStart) e += 0.5 * std::exp(-(t - tailStart) / tail);
        return e * attack(t, 0.0002);
    });
    return done(v, s.c, s.seed + 32, 2.5);
}
std::vector<float> spTom(double sr, const SpSpec& s, int i) {
    const double f = s.tomF[i], tauA = s.tomTauA0 - 0.022 * i;
    Voice v(sr, s.tomDur0 - 0.055 * i);
    sweepSine(v, f * s.tomSweep, f, s.tomTauF, 1.0, tauA);
    partials(v, {{f * 1.5, 0.22, tauA * 0.45}});                       // mode màng
    noise(v, s.seed + 40 + static_cast<uint32_t>(i), {bp(1100, 0.8, sr)}, 0.22, ex(0.007, 0.0001));
    return done(v, s.c, s.seed + 50 + static_cast<uint32_t>(i), 1.35);
}
std::vector<float> spHat(double sr, const SpSpec& s, int kind) {
    const double tau = kind == 2 ? s.hatTauO : (kind == 1 ? s.hatTauC * 0.7 : s.hatTauC);
    Voice v(sr, kind == 2 ? s.hatTauO * 3.4 : (kind == 1 ? 0.18 : 0.22));
    noise(v, s.seed + 60 + static_cast<uint32_t>(kind), {hp(s.hatHp, 0.7, sr), bp(s.hatBp, 0.8, sr)}, 1.0, ex(tau, 0.0002));
    metal(v, {hp(8000, 0.7, sr)}, 0.22, ex(tau, 0.0002), 1.15);
    if (kind == 1) noise(v, s.seed + 65, {bp(1300, 1.0, sr)}, 0.15, ex(0.006, 0.0002));
    return done(v, s.c, s.seed + 70 + static_cast<uint32_t>(kind), kind == 2 ? 1.0 : 3.2);
}
std::vector<float> spCymbal(double sr, const SpSpec& s, bool ride) {
    Voice v(sr, s.cymTau * 2.3);
    if (ride) {
        partials(v, {{2600, 0.3, s.cymTau * 0.5}, {3900, 0.2, s.cymTau * 0.4}}, 0.0005);
        noise(v, s.seed + 81, {hp(5000, 0.7, sr)}, 0.3, ex(s.cymTau, 0.002));
        metal(v, {bp(5000, 1.2, sr)}, 0.4, ex(s.cymTau, 0.0005), 1.9);
    } else {
        noise(v, s.seed + 82, {hp(3500, 0.7, sr)}, 0.6, ex(s.cymTau * 0.9, 0.002));
        metal(v, {bp(6000, 0.7, sr)}, 0.4, ex(s.cymTau * 0.9, 0.002), 1.37);
    }
    return done(v, s.cym, s.seed + (ride ? 83 : 84), 0.0);
}

std::vector<KitPad> spPads(double sr, const SpSpec& s, const char* rimLabel) {
    auto T = [&](int i) { return spTom(sr, s, i); };
    return {
        pad(36, "Kick", "kick.flac", spKick(sr, s)),
        pad(37, rimLabel, "rim.flac", spRim(sr, s)),
        pad(38, "Snare", "snare.flac", spSnare(sr, s, false)),
        pad(39, "Clap", "clap.flac", spClap(sr, s)),
        pad(40, "Snare 2", "snare2.flac", spSnare(sr, s, true)),
        pad(41, "Floor Tom L", "tom_floor_lo.flac", T(0)),
        pad(42, "Closed Hat", "hat_closed.flac", spHat(sr, s, 0), 1),
        pad(43, "Floor Tom H", "tom_floor_hi.flac", T(1)),
        pad(44, "Pedal Hat", "hat_pedal.flac", spHat(sr, s, 1), 1),
        pad(45, "Low Tom", "tom_lo.flac", T(2)),
        pad(46, "Open Hat", "hat_open.flac", spHat(sr, s, 2), 2, 1),
        pad(47, "Mid Tom", "tom_mid.flac", T(3)),
        pad(48, "Hi-Mid Tom", "tom_mid_hi.flac", T(4)),
        pad(49, "Crash", "crash.flac", spCymbal(sr, s, false)),
        pad(50, "High Tom", "tom_hi.flac", T(5)),
        pad(51, "Ride", "ride.flac", spCymbal(sr, s, true)),
    };
}

// TR-707 (1985): sample 8-bit @ 25 kHz, cymbal 6-bit; kick bó, snare "rè", clap 3 burst, tom kiểu acoustic.
const SpSpec k707{.c = Character::Sp707, .cym = Character::Sp707Cymbal,
                  .kickF0 = 180.0, .kickF1 = 60.0, .kickTauF = 0.02, .kickTauA = 0.17, .kickDur = 0.6,
                  .sn1 = 210.0, .sn2 = 350.0, .snTau = 0.18, .snNoiseHz = 3000.0, .room = 0.0,
                  .clap = {0.0, 0.010, 0.020, 0.0}, .clapN = 3,
                  .clapTail = 0.12, .tomSweep = 1.25, .tomTauF = 0.03, .tomTauA0 = 0.30, .tomDur0 = 0.9,
                  .hatHp = 6000.0, .hatBp = 9000.0, .hatTauC = 0.035, .hatTauO = 0.32, .cymTau = 1.2,
                  .tomF = {85, 100, 119, 141, 168, 200}, .seed = 70700u};
// LinnDrum (1982): 8-bit companding @ 28 kHz; kick dày, snare có đuôi phòng, clap 4 burst, tom sâu vang.
const SpSpec kLinn{.c = Character::SpLinn, .cym = Character::SpLinn,
                   .kickF0 = 140.0, .kickF1 = 55.0, .kickTauF = 0.025, .kickTauA = 0.22, .kickDur = 0.7,
                   .sn1 = 200.0, .sn2 = 330.0, .snTau = 0.14, .snNoiseHz = 2000.0, .room = 0.22,
                   .clap = {0.0, 0.008, 0.016, 0.024}, .clapN = 4,
                   .clapTail = 0.18, .tomSweep = 1.45, .tomTauF = 0.06, .tomTauA0 = 0.38, .tomDur0 = 1.1,
                   .hatHp = 7000.0, .hatBp = 10000.0, .hatTauC = 0.045, .hatTauO = 0.40, .cymTau = 1.3,
                   .tomF = {70, 84, 100, 119, 141, 168}, .seed = 80800u};

} // namespace

Kit makeTrap(double sr) { return synth::assemble("kit_trap", sr, trapPads(sr)); }
Kit makeLofi(double sr) { return synth::assemble("kit_lofi", sr, lofiPads(sr)); }
Kit make606(double sr) { return synth::assemble("kit_606", sr, pads606(sr)); }
Kit make707(double sr) { return synth::assemble("kit_707", sr, spPads(sr, k707, "Rim")); }
Kit makeLinn(double sr) { return synth::assemble("kit_linn", sr, spPads(sr, kLinn, "Side Stick")); }

} // namespace le::render::kits
