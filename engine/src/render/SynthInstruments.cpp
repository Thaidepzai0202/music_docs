#include "render/SynthInstruments.h"

#include "render/DrumKits.h"
#include "render/KitSynth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace le::render::instruments {

namespace {

constexpr double kTwoPi = 6.283185307179586476925;

double midiHz(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

// sin(2π · f · i / sr) với f nguyên Hz: pha = (i·f + offset) mod sr tính bằng số nguyên → tuần hoàn đúng từng bit
// với chu kỳ sr mẫu (1 giây), không tích luỹ sai số.
double sinInt(int64_t i, int64_t f, int64_t sr, int64_t offset = 0) {
    const int64_t m = (i * f + offset) % sr;
    return std::sin(kTwoPi * static_cast<double>(m) / static_cast<double>(sr));
}

// Đường giảm từ 1 (t = 0) về đúng 0 tại t = T, dạng mũ τ; sau T bằng 0.
double fadeTo0(double t, double tau, double T) {
    if (t >= T) return 0.0;
    const double eT = std::exp(-T / tau);
    return (std::exp(-t / tau) - eT) / (1.0 - eT);
}

// Chuẩn hoá đỉnh về −1 dBFS rồi làm tròn 24-bit (không fade, không cắt: phần cuối là loop).
void normalize(std::vector<float>& x) {
    float peak = 0.0f;
    for (float s : x) peak = std::max(peak, std::fabs(s));
    if (peak > 0.0f) {
        const float g = std::pow(10.0f, kits::kPeakDb / 20.0f) / peak;
        for (float& s : x) s *= g;
    }
    kits::quantize24(x);
}

struct ZonePlan {
    int root;
    bool hard;
};
std::vector<ZonePlan> plan(int lo, int hi) {
    std::vector<ZonePlan> z;
    for (int root = lo + 1; root - 1 <= hi; root += kZoneStep) {
        z.push_back({root, false});
        z.push_back({root, true});
    }
    return z;
}

InstSample baseZone(int root, bool hard, int64_t f0, const char* prefix) {
    InstSample z;
    z.root = root;
    z.loKey = root - 1;
    z.hiKey = root + 1;
    z.loVel = hard ? kSoftMaxVel + 1 : 1;
    z.hiVel = hard ? 127 : kSoftMaxVel;
    char name[48];
    std::snprintf(name, sizeof(name), "%s_%03d_%s.flac", prefix, root, hard ? "hard" : "soft");
    z.file = name;
    z.f0Hz = static_cast<double>(f0);
    z.tuneCents = static_cast<float>(1200.0 * std::log2(midiHz(root) / static_cast<double>(f0)));
    return z;
}

// ─────────────── E-Piano (FM 2 operator) ───────────────
InstSample epZone(double sr, int root, bool hard) {
    const int64_t isr = std::llround(sr);
    const int64_t f0 = std::max<int64_t>(1, std::llround(midiHz(root)));
    InstSample z = baseZone(root, hard, f0, "ep");
    const double T = std::clamp(1.3 - 0.01 * (root - 29), 0.6, 1.3);
    const int64_t nT = std::llround(T * sr), n = nT + isr;
    z.loopStart = nT;
    z.loopEnd = n;
    z.decaySec = static_cast<float>(std::round(std::clamp(9.0 - 0.09 * (root - 29), 2.5, 9.0) * 10.0) / 10.0);

    const double pitchScale = std::clamp(1.0 - (root - 60) / 90.0, 0.35, 1.3);   // nốt thấp: FM giàu hơn
    const double Ia = (hard ? 2.6 : 1.1) * pitchScale, Is = (hard ? 0.55 : 0.25) * pitchScale;
    const double As = 0.42, tauA = std::clamp(0.7 - 0.006 * (root - 29), 0.25, 0.7);
    const bool bell = 7 * f0 < 16000;
    const double bellAmp = hard ? 0.22 : 0.1, drive = 1.6;
    z.samples.resize(static_cast<size_t>(n));
    for (int64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        const double I = Is + (Ia - Is) * fadeTo0(t, 0.22, T);
        const double A = (As + (1.0 - As) * fadeTo0(t, tauA, T)) * std::min(1.0, t / 0.0015);
        const double trem = 0.94 + 0.06 * sinInt(i, 4, isr, isr / 4);   // 4 Hz, bắt đầu ở đỉnh (cos)
        const double mod = sinInt(i, f0, isr);
        // sin(pha_mang + I·sin(pha_điều_chế)): pha mang lấy từ số nguyên → cộng I·mod rồi sin
        const int64_t m = (i * f0) % isr;
        double y = A * trem * std::sin(kTwoPi * static_cast<double>(m) / sr + I * mod);
        if (bell) y += bellAmp * fadeTo0(t, 0.04, T) * std::min(1.0, t / 0.0015) * sinInt(i, 7 * f0, isr);
        if (hard) y = std::tanh(drive * y) / std::tanh(drive);
        z.samples[static_cast<size_t>(i)] = static_cast<float>(y);
    }
    normalize(z.samples);
    return z;
}

// ─────────────── Organ (drawbar tonewheel) ───────────────
InstSample organZone(double sr, int root, bool hard) {
    const int64_t isr = std::llround(sr);
    const int64_t f0 = std::max<int64_t>(2, 2 * std::llround(midiHz(root) / 2.0));   // chẵn: 16' = f0/2 vẫn nguyên Hz
    InstSample z = baseZone(root, hard, f0, "organ");
    constexpr double T = 0.6;
    const int64_t nT = std::llround(T * sr), n = nT + isr;
    z.loopStart = nT;
    z.loopEnd = n;

    // Drawbar: 16' 5⅓' 8' 4' 2⅔' 2' 1⅗' 1⅓' 1' → bội số của f0/2: 1 3 2 4 6 8 10 12 16
    static const int kHalfMul[9] = {1, 3, 2, 4, 6, 8, 10, 12, 16};
    const int soft[9] = {8, 8, 8, 0, 0, 0, 0, 0, 0}, loud[9] = {8, 8, 8, 5, 0, 0, 0, 0, 0};
    const int* reg = hard ? loud : soft;
    struct Wheel { int64_t f, off; double amp; };
    std::vector<Wheel> wheels;
    for (int d = 0; d < 9; ++d) {
        const int64_t f = kHalfMul[d] * f0 / 2;
        if (reg[d] == 0 || f > 16000) continue;
        // Tonewheel quay tự do: pha đầu tất định theo (nốt, drawbar), tránh mọi partial cùng pha lúc bắt đầu
        const int64_t off = ((static_cast<int64_t>(root) * 7919 + d * 104729) * 2654435761LL) % isr;
        wheels.push_back({f, (off + isr) % isr, std::pow(10.0, -3.0 * (8 - reg[d]) / 20.0)});
    }
    kits::synth::Noise nz(static_cast<uint32_t>(90000 + root * 2 + (hard ? 1 : 0)));
    kits::synth::Filter clickBp = kits::synth::bp(2500, 0.8, sr);
    const double clickAmp = hard ? 0.1 : 0.04, percAmp = hard ? 0.5 : 0.0;
    z.samples.resize(static_cast<size_t>(n));
    for (int64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        double y = 0.0;
        for (const Wheel& w : wheels) y += w.amp * sinInt(i, w.f, isr, w.off);
        y *= std::min(1.0, t / 0.004);
        if (percAmp > 0.0 && 3 * f0 <= 16000) y += percAmp * fadeTo0(t, 0.18, T) * sinInt(i, 3 * f0, isr);
        const double c = static_cast<double>(clickBp(nz.next()));   // luôn chạy filter → trạng thái tất định
        y += clickAmp * fadeTo0(t, 0.004, T) * c;
        z.samples[static_cast<size_t>(i)] = static_cast<float>(y);
    }
    normalize(z.samples);
    return z;
}

// ─────────────── Synth Tone (bản thư viện của fixture inst_synth) ───────────────
InstSample synthZone(double sr, int root, bool hard) {
    const int64_t isr = std::llround(sr);
    const int64_t f0 = std::max<int64_t>(1, std::llround(midiHz(root)));
    InstSample z = baseZone(root, hard, f0, "synth");
    constexpr double T = 0.25;   // như fixture: loop bắt đầu ở 0.25 s
    const int64_t nT = std::llround(T * sr), n = nT + isr;
    z.loopStart = nT;
    z.loopEnd = n;
    // Hài: như fixture (4 / 10), nốt trầm thêm hài tới ≥ 250 Hz; bỏ hài trên 16 kHz
    const int base = hard ? 10 : 4;
    int harmonics = std::max<int>(base, static_cast<int>(std::ceil(250.0 / static_cast<double>(f0))));
    harmonics = std::min<int>(harmonics, 48);
    while (harmonics > 1 && harmonics * f0 > 16000) --harmonics;
    double norm = 0.0;
    for (int h = 1; h <= harmonics; ++h) norm += 1.0 / h;
    z.samples.resize(static_cast<size_t>(n));
    for (int64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        double y = 0.0;
        for (int h = 1; h <= harmonics; ++h) {
            const int64_t off = std::llround(0.7 * h / kTwoPi * sr) % isr;   // pha 0.7·h rad như fixture (lượng tử 1/sr vòng)
            y += sinInt(i, h * f0, isr, off) / h;
        }
        z.samples[static_cast<size_t>(i)] = static_cast<float>(std::min(1.0, t / 0.010) * y / norm);
    }
    normalize(z.samples);
    return z;
}

SynthInstrument assemble(const std::string& id, double sr, int lo, int hi, std::vector<InstSample> zones,
                         const std::string& global, const char* what, bool balance = true) {
    SynthInstrument inst;
    inst.id = id;
    inst.sfzFileName = id + ".sfz";
    inst.sampleRate = sr;
    inst.rangeLo = lo;
    inst.rangeHi = hi;
    double mn = 1e9, mx = -1e9;
    for (InstSample& z : zones) {
        z.loudnessDb = kits::loudnessDb(z.samples, sr);
        mn = std::min(mn, z.loudnessDb);
        mx = std::max(mx, z.loudnessDb);
    }
    const double target = std::max(mn, mx + static_cast<double>(kits::kMaxCutDb));
    for (InstSample& z : zones)
        z.volumeDb = balance ? static_cast<float>(std::round(std::min(0.0, target - z.loudnessDb) * 10.0) / 10.0) : 0.0f;
    // Mọi phím 0..127 đều kêu: zone thấp nhất / cao nhất của từng lớp kéo ra hai đầu bàn phím (repitch)
    for (bool hard : {false, true}) {
        InstSample *first = nullptr, *last = nullptr;
        for (InstSample& z : zones) {
            if ((z.loVel > kSoftMaxVel) != hard) continue;
            if (first == nullptr || z.root < first->root) first = &z;
            if (last == nullptr || z.root > last->root) last = &z;
        }
        if (first != nullptr) first->loKey = 0;
        if (last != nullptr) last->hiKey = 127;
    }

    std::string s = "// " + id + " — " + what + "\n"
                    "// Tổng hợp bằng code: engine/tools/le-gen-instruments (agent 80, render/SynthInstruments.cpp). Không dùng\n"
                    "// sample bên thứ ba. Zone mỗi 3 nửa cung × 2 lớp velocity (zone đầu / cuối phủ tới phím 0 / 127);\n"
                    "// tune bù tần số nguyên Hz; " + std::string(balance ? "volume = cân loudness" : "không cân loudness (mỗi sample đỉnh −1 dBFS)") + ".\n"
                    "<control> default_path=samples/\n"
                    "<global> " + global + "\n";
    for (const InstSample& z : zones) {
        char buf[320];
        std::snprintf(buf, sizeof(buf),
                      "<region> lokey=%d hikey=%d pitch_keycenter=%d lovel=%d hivel=%d sample=%s tune=%.2f volume=%.1f "
                      "loop_start=%lld loop_end=%lld",
                      z.loKey, z.hiKey, z.root, z.loVel, z.hiVel, z.file.c_str(), static_cast<double>(z.tuneCents),
                      static_cast<double>(z.volumeDb), static_cast<long long>(z.loopStart),
                      static_cast<long long>(z.loopEnd - 1));
        s += buf;
        if (z.decaySec > 0.0f) {
            std::snprintf(buf, sizeof(buf), " ampeg_decay=%.1f", static_cast<double>(z.decaySec));
            s += buf;
        }
        s += "\n";
    }
    inst.sfzText = s;
    inst.zones = std::move(zones);
    return inst;
}

} // namespace

SynthInstrument makeEPiano(double sr) {
    constexpr int lo = 28, hi = 100;   // E1..E7 như Rhodes 73 phím
    std::vector<InstSample> zones;
    for (const ZonePlan& p : plan(lo, hi)) zones.push_back(epZone(sr, p.root, p.hard));
    return assemble("inst_epiano", sr, lo, hi, std::move(zones),
                    "loop_mode=loop_continuous ampeg_attack=0.001 ampeg_sustain=0 ampeg_release=0.3",
                    "E-Piano kiểu Rhodes (FM 2 operator + tremolo nhẹ), E1–E7");
}

SynthInstrument makeOrgan(double sr) {
    constexpr int lo = 36, hi = 96;    // C2..C7 (bàn phím 61 phím)
    std::vector<InstSample> zones;
    for (const ZonePlan& p : plan(lo, hi)) zones.push_back(organZone(sr, p.root, p.hard));
    return assemble("inst_organ", sr, lo, hi, std::move(zones),
                    "loop_mode=loop_continuous ampeg_attack=0.003 ampeg_release=0.06",
                    "Organ drawbar kiểu tonewheel (888000000; lớp mạnh + 4' và percussion), C2–C7");
}

SynthInstrument makeSynthTone(double sr) {
    constexpr int lo = 12, hi = 108;   // C0..C8: zone thật (track bass của demo cần nốt trầm tròn)
    std::vector<InstSample> zones;
    for (const ZonePlan& p : plan(lo, hi)) zones.push_back(synthZone(sr, p.root, p.hard));
    return assemble("inst_synth", sr, lo, hi, std::move(zones), "loop_mode=loop_continuous ampeg_attack=0.005 ampeg_release=0.3",
                    "Tone tổng hợp (hài 1/h như fixture inst_synth), C0–C8", false);
}

std::vector<SynthInstrument> makeAllInstruments(double sr) { return {makeEPiano(sr), makeOrgan(sr), makeSynthTone(sr)}; }

} // namespace le::render::instruments
