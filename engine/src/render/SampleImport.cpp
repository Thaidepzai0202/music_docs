#include "render/SampleImport.h"

#include "render/DrumKits.h"
#include "render/Yin.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <numeric>

namespace le::render::import {

namespace {

constexpr double kPi = 3.14159265358979323846;

double midiHz(double note) { return 440.0 * std::pow(2.0, (note - 69.0) / 12.0); }

std::vector<float> monoOf(const std::vector<float>& l, const std::vector<float>& r) {
    if (r.empty()) return l;
    std::vector<float> m(l.size());
    for (size_t i = 0; i < l.size(); ++i) m[i] = 0.5f * (l[i] + (i < r.size() ? r[i] : 0.0f));
    return m;
}

// RMS theo cửa sổ `win` mẫu (không chồng), dB tuyệt đối.
std::vector<double> rmsWindows(const std::vector<float>& x, size_t win) {
    std::vector<double> out;
    for (size_t a = 0; a + win <= x.size(); a += win) {
        double e = 0.0;
        for (size_t i = a; i < a + win; ++i) e += static_cast<double>(x[i]) * x[i];
        out.push_back(10.0 * std::log10(e / static_cast<double>(win) + 1e-30));
    }
    return out;
}

// Cao độ (MIDI, số thực) của đoạn ổn định; −1 nếu Yin không chắc. expected: nốt theo tên (đã đúng quy ước).
double measurePitch(const std::vector<float>& mono, double sr, int64_t from, int64_t to, int expected) {
    YinConfig cfg;
    cfg.minHz = static_cast<float>(midiHz(expected - 14));   // cho phép Yin nhầm quãng tám xuống: vẫn thấy được
    cfg.maxHz = static_cast<float>(midiHz(expected + 14));
    const int tauMax = static_cast<int>(std::ceil(sr / cfg.minHz));
    int n = 1024;
    while (n < 2 * (tauMax + 2) && n < 16384) n *= 2;
    cfg.frameSize = n;
    cfg.hopSize = 512;   // đủ frame cho trung vị ổn định cả ở nốt trầm (frame 8192)
    cfg.silenceDb = -60.0f;
    from = std::max<int64_t>(0, from);
    to = std::min<int64_t>(static_cast<int64_t>(mono.size()), to);
    if (to - from < n) return -1.0;
    // Đo trên bản đã chuẩn hoá đỉnh = 1: nguồn thu nhỏ (VD harp G1 "mp") không bị ngưỡng lặng của Yin bỏ qua
    std::vector<float> seg(mono.begin() + from, mono.begin() + to);
    float peak = 0.0f;
    for (float v : mono) peak = std::max(peak, std::fabs(v));
    if (peak > 0.0f)
        for (float& v : seg) v /= peak;
    Yin yin(cfg);
    const PitchEstimate p = yin.analyze(seg.data(), static_cast<int64_t>(seg.size()), sr);
    if (!p.ok || p.confidence < 0.3f || !(p.hz > 0.0f)) return -1.0;
    return 69.0 + 12.0 * std::log2(static_cast<double>(p.hz) / 440.0);
}

void fadeOutTail(std::vector<float>& x, size_t fadeLen) {
    const size_t n = x.size();
    fadeLen = std::min(fadeLen, n);
    for (size_t i = 0; i < fadeLen; ++i) {
        const double u = static_cast<double>(i + 1) / static_cast<double>(fadeLen);   // tới đúng 0 ở mẫu cuối
        x[n - fadeLen + i] *= static_cast<float>(0.5 * (1.0 + std::cos(kPi * u)));
    }
}

const char* layerName(int layer, int layers) {
    if (layers <= 1) return "all";
    if (layers == 2) return layer == 0 ? "soft" : "hard";
    return layer == 0 ? "soft" : (layer == layers - 1 ? "hard" : "mid");
}

void velRange(int layer, int layers, int& lo, int& hi) {
    static const int two[2][2] = {{1, 95}, {96, 127}};
    static const int three[3][2] = {{1, 60}, {61, 100}, {101, 127}};
    if (layers <= 1) {
        lo = 1;
        hi = 127;
    } else if (layers == 2) {
        lo = two[layer][0];
        hi = two[layer][1];
    } else {
        lo = three[layer][0];
        hi = three[layer][1];
    }
}

} // namespace

int noteFromName(const std::string& t) {
    if (t.empty()) return -1;
    static const int pcs[7] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
    const char c = static_cast<char>(t[0] & ~0x20);
    if (c < 'A' || c > 'G') return -1;
    int pc = pcs[c - 'A'];
    size_t i = 1;
    if (i < t.size() && t[i] == '#') {
        ++pc;
        ++i;
    } else if (i < t.size() && t[i] == 'b') {
        --pc;
        ++i;
    }
    bool neg = false;
    if (i < t.size() && t[i] == '-') {
        neg = true;
        ++i;
    }
    if (i >= t.size() || t[i] < '0' || t[i] > '9') return -1;
    int oct = 0;
    while (i < t.size() && t[i] >= '0' && t[i] <= '9') oct = oct * 10 + (t[i++] - '0');
    if (i != t.size()) return -1;
    const int note = 12 * ((neg ? -oct : oct) + 1) + pc;
    return note >= 0 && note <= 127 ? note : -1;
}

int64_t findOnset(const std::vector<float>& x, double sr, double thresholdDb, double preRollSec) {
    float peak = 0.0f;
    for (float v : x) peak = std::max(peak, std::fabs(v));
    if (peak <= 0.0f) return 0;
    const float thr = peak * static_cast<float>(std::pow(10.0, thresholdDb / 20.0));
    int64_t i = 0;
    while (i < static_cast<int64_t>(x.size()) && std::fabs(x[static_cast<size_t>(i)]) < thr) ++i;
    return std::max<int64_t>(0, i - static_cast<int64_t>(std::llround(preRollSec * sr)));
}

LoopPoints findLoop(const std::vector<float>& x, double sr, int64_t searchStart, int64_t searchEnd, double minLenSec) {
    LoopPoints best;
    const int64_t win = std::max<int64_t>(64, std::llround(0.008 * sr));   // ±8 ms quanh điểm nối
    const int64_t minLen = std::llround(minLenSec * sr);
    const int64_t n = static_cast<int64_t>(x.size());
    searchEnd = std::min(searchEnd, n - win - 1);
    searchStart = std::max(searchStart, win);
    if (searchEnd - searchStart < minLen) return best;
    const auto score = [&](int64_t s, int64_t e) {
        double xy = 0.0, xx = 0.0, yy = 0.0;
        for (int64_t k = -win; k < win; ++k) {
            const double a = x[static_cast<size_t>(s + k)], b = x[static_cast<size_t>(e + k)];
            xy += a * b;
            xx += a * a;
            yy += b * b;
        }
        if (xx <= 0.0 || yy <= 0.0) return -1.0;
        const double levelDb = std::fabs(10.0 * std::log10(xx / yy));   // lệch mức giữa hai điểm nối
        return xy / std::sqrt(xx * yy) - levelDb / 12.0;
    };
    // loopEnd: vài ứng viên gần cuối vùng ổn định; loopStart: quét thô (bước 8) rồi tinh (±8) — tất định
    for (int64_t e = searchEnd; e > searchEnd - std::llround(0.1 * sr) && e - searchStart >= minLen; e -= std::llround(0.02 * sr)) {
        const int64_t sHi = e - minLen, sLo = searchStart;
        int64_t coarse = sLo;
        double cBest = -2.0;
        for (int64_t s = sLo; s <= sHi; s += 8) {
            const double v = score(s, e);
            if (v > cBest) {
                cBest = v;
                coarse = s;
            }
        }
        for (int64_t s = std::max(sLo, coarse - 8); s <= std::min(sHi, coarse + 8); ++s) {
            const double v = score(s, e);
            if (v > best.score) best = {s, e, v};
        }
    }
    return best;
}

std::vector<float> resample(const std::vector<float>& x, int srIn, int srOut) {
    if (srIn <= 0 || srOut <= 0 || srIn == srOut || x.empty()) return x;
    const int g = std::gcd(srIn, srOut);
    const int64_t up = srOut / g, down = srIn / g;   // y[i] ở vị trí vào i·down/up
    constexpr int kHalf = 32;
    const double fc = 0.95 * 0.5 * std::min(1.0, static_cast<double>(srOut) / srIn);   // chu kỳ / mẫu VÀO
    const double beta = 8.6;
    const auto i0 = [](double v) {   // Bessel I0 (chuỗi)
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k) {
            term *= (v / (2.0 * k)) * (v / (2.0 * k));
            sum += term;
        }
        return sum;
    };
    const double i0b = i0(beta);
    // Bảng hệ số: pha φ (0..up−1) ứng với lệch φ/up mẫu vào; tap j = −kHalf+1 .. kHalf
    std::vector<double> h(static_cast<size_t>(up) * 2 * kHalf);
    for (int64_t ph = 0; ph < up; ++ph) {
        double norm = 0.0;
        for (int j = -kHalf + 1; j <= kHalf; ++j) {
            const double t = static_cast<double>(j) - static_cast<double>(ph) / static_cast<double>(up);   // khoảng cách tới mẫu vào
            const double sinc = t == 0.0 ? 1.0 : std::sin(2.0 * kPi * fc * t) / (kPi * t) / (2.0 * fc);
            const double u = t / kHalf;
            const double win = std::fabs(u) >= 1.0 ? 0.0 : i0(beta * std::sqrt(1.0 - u * u)) / i0b;
            const double c = sinc * win;
            h[static_cast<size_t>(ph * 2 * kHalf + (j + kHalf - 1))] = c;
            norm += c;
        }
        for (int j = 0; j < 2 * kHalf; ++j) h[static_cast<size_t>(ph * 2 * kHalf + j)] /= norm;   // DC gain = 1 mọi pha
    }
    const int64_t n = static_cast<int64_t>(x.size());
    const int64_t outN = (n * up + down - 1) / down;
    std::vector<float> y(static_cast<size_t>(outN));
    for (int64_t i = 0; i < outN; ++i) {
        const int64_t pos = i * down, base = pos / up, ph = pos % up;
        const double* c = &h[static_cast<size_t>(ph * 2 * kHalf)];
        double acc = 0.0;
        for (int j = -kHalf + 1; j <= kHalf; ++j) {
            const int64_t k = base + j;
            if (k >= 0 && k < n) acc += c[j + kHalf - 1] * static_cast<double>(x[static_cast<size_t>(k)]);
        }
        y[static_cast<size_t>(i)] = static_cast<float>(acc);
    }
    return y;
}

void crossfadeLoop(std::vector<float>& x, int64_t loopStart, int64_t loopEnd, int64_t xf) {
    if (xf <= 0 || loopStart < xf || loopEnd > static_cast<int64_t>(x.size()) || loopEnd - loopStart < xf) return;
    for (int64_t k = 0; k < xf; ++k) {
        const double w = 0.5 * (1.0 - std::cos(kPi * static_cast<double>(k + 1) / static_cast<double>(xf)));   // → 1 tại k = xf−1
        const size_t dst = static_cast<size_t>(loopEnd - xf + k), src = static_cast<size_t>(loopStart - xf + k);
        x[dst] = static_cast<float>(static_cast<double>(x[dst]) * (1.0 - w) + static_cast<double>(x[src]) * w);
    }
}

std::vector<float> prepareOneShot(const std::vector<float>& mono, double sr, double maxSec) {
    std::vector<float> x(mono.begin() + findOnset(mono, sr), mono.end());
    const size_t w = static_cast<size_t>(std::llround(0.02 * sr));
    const auto rms = rmsWindows(x, w);
    double peakDb = -200.0;
    for (double v : rms) peakDb = std::max(peakDb, v);
    size_t lastLoud = 0;
    for (size_t k = 0; k < rms.size(); ++k)
        if (rms[k] >= peakDb - 60.0) lastLoud = k;
    const size_t keep = std::min({x.size(), (lastLoud + 1) * w, static_cast<size_t>(std::llround(maxSec * sr))});
    x.resize(keep);
    fadeOutTail(x, static_cast<size_t>(std::max(0.03 * sr, 0.25 * static_cast<double>(keep))));
    float peak = 0.0f;
    for (float v : x) peak = std::max(peak, std::fabs(v));
    if (peak > 0.0f) {
        const float g = std::pow(10.0f, kits::kPeakDb / 20.0f) / peak;
        for (float& v : x) v *= g;
    }
    kits::quantize24(x);
    return x;
}

void limitPeaks(std::vector<float>& x, double sr, double reductionDb) {
    if (x.empty() || !(reductionDb > 0.0)) return;
    float peak = 0.0f;
    for (float v : x) peak = std::max(peak, std::fabs(v));
    if (peak <= 0.0f) return;
    const double thr = static_cast<double>(peak) * std::pow(10.0, -reductionDb / 20.0);
    const size_t n = x.size(), la = static_cast<size_t>(std::llround(0.002 * sr));
    std::vector<double> want(n);
    for (size_t i = 0; i < n; ++i) want[i] = std::min(1.0, thr / std::max(1e-12, static_cast<double>(std::fabs(x[i]))));
    // lookahead: g[i] = min(want[i .. i+la]) (deque đơn điệu → O(n)); nhả: tăng dần về 1 với hằng số 40 ms
    std::vector<double> g(n);
    std::vector<size_t> dq(n + la + 1);
    size_t head = 0, tail = 0;
    for (size_t j = 0; j < std::min(n, la + 1); ++j) {
        while (tail > head && want[dq[tail - 1]] >= want[j]) --tail;
        dq[tail++] = j;
    }
    for (size_t i = 0; i < n; ++i) {
        const size_t j = i + la + 1;
        if (j < n) {
            while (tail > head && want[dq[tail - 1]] >= want[j]) --tail;
            dq[tail++] = j;
        }
        while (dq[head] < i) ++head;
        g[i] = want[dq[head]];
    }
    const double rel = 1.0 - std::exp(-1.0 / (0.04 * sr));
    double cur = g[0];
    for (size_t i = 0; i < n; ++i) {
        cur = std::min(g[i], cur + (1.0 - cur) * rel);   // giảm tức thì (đã nhìn trước), tăng lại chậm
        x[i] = static_cast<float>(static_cast<double>(x[i]) * cur);
    }
    float p2 = 0.0f;
    for (float v : x) p2 = std::max(p2, std::fabs(v));
    if (p2 > 0.0f) {
        const float gg = std::pow(10.0f, kits::kPeakDb / 20.0f) / p2;
        for (float& v : x) v *= gg;
    }
    kits::quantize24(x);
}

KitImportResult importKit(const std::string& id, const std::string& title, const std::string& credit,
                          const std::vector<KitPadSpec>& pads) {
    KitImportResult res;
    std::map<int, std::vector<size_t>> byKey;   // key → chỉ số region (theo lớp)
    for (const KitPadSpec& p : pads) {
        if (p.sameAs >= 0) continue;
        if (p.layers.empty() || p.layers.size() > 2) {
            res.error = "pad " + std::to_string(p.key) + ": cần 1–2 lớp";
            return res;
        }
        for (size_t li = 0; li < p.layers.size(); ++li) {
            KitRegion r;
            r.key = p.key;
            velRange(static_cast<int>(li), static_cast<int>(p.layers.size()), r.loVel, r.hiVel);
            r.file = p.file + (p.layers.size() == 1 ? "" : (li == 0 ? "_soft" : "_hard")) + ".flac";
            r.label = p.label;
            r.group = p.group;
            r.offBy = p.offBy;
            r.tuneCents = p.tuneCents;
            r.sampleRate = p.layers[li].sampleRate;
            r.samples = prepareOneShot(p.layers[li].mono, r.sampleRate, p.maxSec);
            r.loudnessDb = kits::loudnessDb(r.samples, r.sampleRate);
            byKey[p.key].push_back(res.regions.size());
            res.regions.push_back(std::move(r));
        }
    }
    for (const KitPadSpec& p : pads) {
        if (p.sameAs < 0) continue;
        const auto it = byKey.find(p.sameAs);
        if (it == byKey.end()) {
            res.error = "pad " + std::to_string(p.key) + ": sameAs " + std::to_string(p.sameAs) + " không có nguồn";
            return res;
        }
        for (size_t idx : it->second) {
            KitRegion r = res.regions[idx];
            r.key = p.key;
            r.label = p.label;
            r.group = p.group;
            r.offBy = p.offBy;
            r.tuneCents = p.tuneCents;
            r.shared = true;
            r.samples.clear();
            res.regions.push_back(std::move(r));
        }
    }
    // Pad nhỏ hơn trung vị: nén đỉnh (≤ kMaxLimitDb) cho gần trung vị — lặp vài lần vì dB nén ≠ dB loudness tăng
    std::vector<double> ls;
    for (const KitRegion& r : res.regions)
        if (!r.shared) ls.push_back(r.loudnessDb);
    std::sort(ls.begin(), ls.end());
    const double median = ls.empty() ? 0.0 : ls[ls.size() / 2];
    for (KitRegion& r : res.regions) {
        if (r.shared || r.loudnessDb >= median - 0.5) continue;
        const double before = r.loudnessDb;
        double applied = 0.0;
        for (int it = 0; it < 4 && r.loudnessDb < median - 0.5 && applied < kMaxLimitDb - 0.05; ++it) {
            const double step = std::min(kMaxLimitDb - applied, median - r.loudnessDb);
            limitPeaks(r.samples, r.sampleRate, step);
            applied += step;
            r.loudnessDb = kits::loudnessDb(r.samples, r.sampleRate);
        }
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%s: nén đỉnh %.1f dB → loudness %.1f → %.1f", r.file.c_str(), applied, before, r.loudnessDb);
        res.notes.push_back(buf);
    }
    for (KitRegion& r : res.regions)   // pad dùng chung file (sameAs) lấy loudness của file đã nén
        if (r.shared)
            for (const KitRegion& o : res.regions)
                if (!o.shared && o.file == r.file) r.loudnessDb = o.loudnessDb;
    std::stable_sort(res.regions.begin(), res.regions.end(), [](const KitRegion& a, const KitRegion& b) {
        return a.key < b.key || (a.key == b.key && a.loVel < b.loVel);
    });
    // Mức đích = phân vị 25 % loudness (không phải âm nhỏ nhất như kit tổng hợp): một hai âm gõ thật đặc biệt "nhọn"
    // (rimshot: 100 ms đầu chủ yếu là tiếng nổ) không kéo cả kit nhỏ đi; các âm đó nằm dưới mức đích vài dB.
    std::vector<double> all;
    for (const KitRegion& r : res.regions) all.push_back(r.loudnessDb);
    std::sort(all.begin(), all.end());
    const double target = std::max(all[all.size() / 4], all.back() + static_cast<double>(kits::kMaxCutDb));
    for (KitRegion& r : res.regions) r.volumeDb = static_cast<float>(std::round(std::min(0.0, target - r.loudnessDb) * 10.0) / 10.0);

    std::string s = "// " + id + " — " + title + "\n// " + credit + "\n"
                    "// Rút gọn bằng engine/tools/le-import-library (agent 80, render/SampleImport.cpp): 16 pad map GM 36–51, "
                    "trộn micro → mono, cắt đầu / đuôi + fade, nén đỉnh ≤ 6 dB cho pad nhỏ, FLAC 24-bit, volume = cân loudness.\n"
                    "<control> default_path=samples/\n<global> loop_mode=one_shot ampeg_release=0.05\n";
    char buf[320];
    for (const KitRegion& r : res.regions) {
        std::snprintf(buf, sizeof(buf), "<region> key=%d lovel=%d hivel=%d sample=%s", r.key, r.loVel, r.hiVel, r.file.c_str());
        s += buf;
        if (r.tuneCents != 0.0f) {
            std::snprintf(buf, sizeof(buf), " tune=%.0f", static_cast<double>(r.tuneCents));
            s += buf;
        }
        if (r.volumeDb < 0.0f) {
            std::snprintf(buf, sizeof(buf), " volume=%.1f", static_cast<double>(r.volumeDb));
            s += buf;
        }
        if (r.group != 0) s += " group=" + std::to_string(r.group);
        if (r.offBy != 0) s += " off_by=" + std::to_string(r.offBy);
        s += " region_label=" + r.label + "\n";
    }
    res.sfzText = s;
    res.ok = true;
    return res;
}

ImportResult importInstrument(const ImportSpec& spec, const std::vector<SourceSample>& sources) {
    ImportResult res;
    if (sources.empty()) {
        res.error = "không có sample nguồn";
        return res;
    }
    // Gom theo nốt (tên, đã đúng quy ước); trong mỗi nốt xếp nguồn theo hạng velocity (nhẹ → mạnh). Hạng chỉ so
    // được TRONG một nốt (VSCO đặt v1/v3 ở nốt này, v1/v4 ở nốt khác).
    std::map<int, std::vector<const SourceSample*>> byNote;
    for (const SourceSample& s : sources)
        if (s.nameNote >= 0) byNote[s.nameNote].push_back(&s);
    if (byNote.empty()) {
        res.error = "không đọc được nốt từ tên file nào";
        return res;
    }
    size_t maxPerNote = 1;
    for (auto& [note, v] : byNote) {
        std::stable_sort(v.begin(), v.end(), [](const SourceSample* x, const SourceSample* y) { return x->layer < y->layer; });
        maxPerNote = std::max(maxPerNote, v.size());
    }
    const int L = std::max(1, std::min<int>(spec.maxLayers, static_cast<int>(maxPerNote)));
    // Phím gốc chung cho mọi lớp: cách nhau ≥ minSpacing (nguồn dày hơn thì bỏ bớt)
    std::vector<int> roots;
    for (const auto& [note, v] : byNote)
        if (roots.empty() || note - roots.back() >= spec.minSpacing) roots.push_back(note);
    const int lowest = roots.front(), highest = roots.back();
    res.rangeLo = std::max(0, lowest - spec.extendBelow);
    res.rangeHi = std::min(127, highest + spec.extendAbove);
    const std::string prefix = spec.id.rfind("inst_", 0) == 0 ? spec.id.substr(5) : spec.id;

    std::map<const SourceSample*, size_t> done;   // nguồn → chỉ số zone đã xử lý (nốt chỉ có 1 nguồn: các lớp dùng chung file)
    for (int li = 0; li < L; ++li) {
        for (size_t i = 0; i < roots.size(); ++i) {
            const auto& v = byNote.at(roots[i]);
            const size_t idx = v.size() == 1 || L == 1 ? v.size() - 1 : static_cast<size_t>(std::lround(li * (static_cast<double>(v.size()) - 1.0) / (L - 1)));
            const SourceSample& src = *v[idx];
            ImportedZone z;
            z.layer = li;
            z.root = src.nameNote;
            const int natLo = i == 0 ? res.rangeLo : (roots[i - 1] + roots[i]) / 2 + 1;   // biên trong dải tự nhiên
            const int natHi = i + 1 == roots.size() ? res.rangeHi : (roots[i] + roots[i + 1]) / 2;
            z.loKey = i == 0 && spec.fullKeyboard ? 0 : natLo;
            z.hiKey = i + 1 == roots.size() && spec.fullKeyboard ? 127 : natHi;
            velRange(li, L, z.loVel, z.hiVel);
            z.source = src.name;
            z.sampleRate = src.sampleRate;
            const auto prev = done.find(&src);
            if (prev != done.end()) {   // cùng file với lớp trước: chỉ thêm region
                const ImportedZone& p = res.zones[prev->second];
                z.root = p.root;   // có thể đã sửa theo Yin
                z.file = p.file;
                z.tuneCents = p.tuneCents;
                z.measuredCents = p.measuredCents;
                z.pitchMeasured = p.pitchMeasured;
                z.loudnessDb = p.loudnessDb;
                z.loopStart = p.loopStart;
                z.loopEnd = p.loopEnd;
                z.shared = true;
                res.zones.push_back(std::move(z));
                continue;
            }
            char name[64];
            std::snprintf(name, sizeof(name), "%s_%03d_%s.flac", prefix.c_str(), z.root, v.size() == 1 ? "all" : layerName(li, L));
            z.file = name;
            if (li == 0 && std::max(z.root - natLo, natHi - z.root) > 6)
                res.notes.push_back(z.file + ": zone dịch cao độ tới " + std::to_string(std::max(z.root - natLo, natHi - z.root)) + " nửa cung");

            // 1. kênh + cắt đầu
            std::vector<float> l = src.left, r = spec.stereo ? src.right : std::vector<float>{};
            if (!spec.stereo && !src.right.empty()) l = monoOf(src.left, src.right);
            const std::vector<float> m0 = monoOf(l, r);
            const int64_t on = findOnset(m0, src.sampleRate);
            l.erase(l.begin(), l.begin() + on);
            if (!r.empty()) r.erase(r.begin(), r.begin() + on);
            std::vector<float> mono = monoOf(l, r);
            const double sr = src.sampleRate;
            const int64_t n = static_cast<int64_t>(mono.size());

            // 2. cao độ
            // Âm tắt dần (piano / harp / pizz): đo sớm, trước khi tắt; âm ngân: bỏ tiếng gõ đầu. Thử lần lượt các cửa sổ.
            static const double kSusWin[][2] = {{0.15, 1.2}, {0.3, 2.0}, {0.08, 0.6}};
            static const double kDecWin[][2] = {{0.1, 0.7}, {0.03, 0.4}, {0.15, 1.2}};   // dây gảy lúc đầu hơi cao: đo sau 100 ms
            double measured = -1.0;
            for (int wi = 0; wi < 3 && measured < 0.0; ++wi) {
                const double* win = spec.mode == Mode::Decay ? kDecWin[wi] : kSusWin[wi];
                measured = measurePitch(mono, sr, std::llround(win[0] * sr), std::llround(win[1] * sr), z.root);
            }
            if (measured > 0.0) {
                double dev = measured - z.root;
                const double oct = std::round(dev / 12.0);
                if (oct != 0.0) res.notes.push_back(z.file + ": Yin thấy lệch " + std::to_string(static_cast<int>(oct)) + " quãng tám so với tên (dùng tên)");
                dev -= 12.0 * oct;
                const double semis = std::round(dev);
                if (std::fabs(semis) == 1.0 && std::fabs(dev - semis) < 0.3) {   // nguồn đặt tên sai 1 nửa cung
                    z.root += static_cast<int>(semis);
                    dev -= semis;
                    res.notes.push_back(z.file + ": tên lệch " + std::to_string(static_cast<int>(semis)) + " nửa cung so với Yin — phím gốc sửa thành " + std::to_string(z.root));
                }
                if (std::fabs(dev) < 0.6) {
                    z.pitchMeasured = true;
                    z.measuredCents = static_cast<float>(dev * 100.0);
                    if (spec.measureTune) z.tuneCents = static_cast<float>(std::round(-dev * 10000.0) / 100.0);   // 2 chữ số như .sfz
                } else {
                    res.notes.push_back(z.file + ": cao độ đo lệch " + std::to_string(static_cast<int>(std::lround(dev * 100.0))) + " cent — giữ tune 0");
                }
            } else if (spec.measureTune) {
                res.notes.push_back(z.file + ": Yin không đo chắc được cao độ — tune 0");
            }

            // 3. loop / đuôi
            if (spec.mode == Mode::Sustain) {
                const size_t w = static_cast<size_t>(std::llround(0.02 * sr));
                const auto rms = rmsWindows(mono, w);
                const size_t t0w = static_cast<size_t>(std::max(0.3 * sr, std::min(0.15 * static_cast<double>(n), 0.6 * sr)) / static_cast<double>(w));
                std::vector<double> body;
                for (size_t k = t0w; k < rms.size() * 8 / 10; ++k) body.push_back(rms[k]);
                double med = rms.empty() ? -200.0 : rms[rms.size() / 2];
                if (!body.empty()) {
                    std::nth_element(body.begin(), body.begin() + static_cast<std::ptrdiff_t>(body.size() / 2), body.end());
                    med = body[body.size() / 2];
                }
                size_t t1w = t0w;
                for (size_t k = t0w; k < rms.size(); ++k)
                    if (rms[k] >= med - 6.0) t1w = k;   // cửa sổ cuối còn ở mức sustain (trước pha nhả)
                const int64_t xfMax = std::llround(0.15 * sr);
                const int64_t sStart = static_cast<int64_t>(t0w * w) + xfMax;
                const int64_t sEnd = std::min(static_cast<int64_t>(t1w * w) - std::llround(0.03 * sr), std::llround(spec.maxSustainSec * sr));
                const LoopPoints lp = findLoop(mono, sr, sStart, sEnd, spec.minLoopSec);
                if (lp.start < 0) {
                    res.error = z.file + ": không tìm được vùng loop (sample quá ngắn: " + std::to_string(static_cast<double>(n) / sr) + " s)";
                    return res;
                }
                const int64_t xf = std::min<int64_t>({xfMax, (lp.end - lp.start) * 3 / 10, lp.start});
                crossfadeLoop(l, lp.start, lp.end, xf);
                if (!r.empty()) crossfadeLoop(r, lp.start, lp.end, xf);
                l.resize(static_cast<size_t>(lp.end));
                if (!r.empty()) r.resize(static_cast<size_t>(lp.end));
                z.loopStart = lp.start;
                z.loopEnd = lp.end;
                if (lp.score < 0.5) res.notes.push_back(z.file + ": điểm loop tương quan thấp (" + std::to_string(lp.score) + ")");
            } else {
                const size_t w = static_cast<size_t>(std::llround(0.05 * sr));
                const auto rms = rmsWindows(mono, w);
                double peakDb = -200.0;
                for (double v2 : rms) peakDb = std::max(peakDb, v2);
                size_t lastLoud = 0;
                for (size_t k = 0; k < rms.size(); ++k)
                    if (rms[k] >= peakDb - 60.0) lastLoud = k;
                const double u = highest > lowest ? static_cast<double>(z.root - lowest) / (highest - lowest) : 0.0;
                const double maxSec = spec.maxSecLow + (spec.maxSecHigh - spec.maxSecLow) * u;
                const int64_t keep = std::min<int64_t>({n, static_cast<int64_t>((lastLoud + 1) * w), std::llround(maxSec * sr)});
                l.resize(static_cast<size_t>(keep));
                if (!r.empty()) r.resize(static_cast<size_t>(keep));
                const size_t fade = static_cast<size_t>(std::max(0.3 * sr, 0.25 * static_cast<double>(keep)));
                fadeOutTail(l, fade);
                if (!r.empty()) fadeOutTail(r, fade);
            }

            // 4. đỉnh −1 dBFS + 24-bit
            float peak = 0.0f;
            for (float s2 : l) peak = std::max(peak, std::fabs(s2));
            for (float s2 : r) peak = std::max(peak, std::fabs(s2));
            if (peak > 0.0f) {
                const float g = std::pow(10.0f, kits::kPeakDb / 20.0f) / peak;
                for (float& s2 : l) s2 *= g;
                for (float& s2 : r) s2 *= g;
            }
            kits::quantize24(l);
            kits::quantize24(r);
            z.loudnessDb = kits::loudnessDb(monoOf(l, r), sr);
            z.left = std::move(l);
            z.right = std::move(r);
            done[&src] = res.zones.size();
            res.zones.push_back(std::move(z));
        }
    }

    // Cân loudness như kit
    double mn = 1e9, mx = -1e9;
    for (const ImportedZone& z : res.zones) {
        mn = std::min(mn, z.loudnessDb);
        mx = std::max(mx, z.loudnessDb);
    }
    const double target = std::max(mn, mx + static_cast<double>(kits::kMaxCutDb));
    for (ImportedZone& z : res.zones) z.volumeDb = static_cast<float>(std::round(std::min(0.0, target - z.loudnessDb) * 10.0) / 10.0);

    // .sfz
    char buf[400];
    std::string s = "// " + spec.id + " — " + spec.title + "\n// " + spec.credit + "\n"
                    "// Rút gọn bằng engine/tools/le-import-library (agent 80, render/SampleImport.cpp): chọn phím gốc ≥ " +
                    std::to_string(spec.minSpacing) + " nửa cung (zone đầu / cuối phủ tới phím 0 / 127), " + std::to_string(L) +
                    " lớp velocity, cắt đầu, " +
                    (spec.mode == Mode::Sustain ? "loop crossfade" : "cắt đuôi + fade") + ", " + (spec.stereo ? "stereo" : "mono") +
                    ", FLAC 24-bit, " + (spec.measureTune ? "tune đo bằng Yin" : "giữ cao độ gốc (tune 0)") +
                    ", volume = cân loudness.\n<control> default_path=samples/\n";
    std::snprintf(buf, sizeof(buf), "<global> loop_mode=%s ampeg_attack=%.3f ampeg_release=%.2f\n",
                  spec.mode == Mode::Sustain ? "loop_continuous" : "no_loop", spec.attack, spec.release);
    s += buf;
    for (const ImportedZone& z : res.zones) {
        std::snprintf(buf, sizeof(buf), "<region> lokey=%d hikey=%d pitch_keycenter=%d lovel=%d hivel=%d sample=%s tune=%.2f volume=%.1f",
                      z.loKey, z.hiKey, z.root, z.loVel, z.hiVel, z.file.c_str(), static_cast<double>(z.tuneCents),
                      static_cast<double>(z.volumeDb));
        s += buf;
        if (z.loopStart >= 0) {
            std::snprintf(buf, sizeof(buf), " loop_start=%lld loop_end=%lld", static_cast<long long>(z.loopStart),
                          static_cast<long long>(z.loopEnd - 1));
            s += buf;
        }
        s += "\n";
    }
    res.sfzText = s;
    res.ok = true;
    return res;
}

} // namespace le::render::import
