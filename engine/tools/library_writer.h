// library_writer.h — phần chung của le-gen-kits và le-gen-instruments (P2-35): ghi FLAC 24-bit rồi decode lại bằng
// ĐÚNG đường của engine (io::decodeAudioFile) để chắc file khớp buffer từng bit, ghi text (.sfz / .json), dọn file
// cũ trong samples/, và render thử bằng Sampler để người dùng nghe. [main] (CLI, header-only)
#pragma once

#include "dsp/Instrument.h"
#include "dsp/Sampler.h"
#include "io/AudioFileIO.h"
#include "spike/measure/WavIO.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace le::tools {

namespace fs = std::filesystem;

inline bool writeText(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
    if (!f) std::fprintf(stderr, "không ghi được %s\n", path.string().c_str());
    return static_cast<bool>(f);
}

// Ghi FLAC 24-bit mono rồi decode lại bằng io::decodeAudioFile (như engine nạp kit) và so từng giá trị.
// Trả số byte của file (0 = lỗi, đã in lý do).
inline uintmax_t writeFlacChecked(const fs::path& path, const std::vector<float>& x, double sr) {
    std::string err;
    if (!spike::writeFlac24Mono(path.string(), x.data(), static_cast<int64_t>(x.size()), sr, &err)) {
        std::fprintf(stderr, "lỗi ghi %s: %s\n", path.string().c_str(), err.c_str());
        return 0;
    }
    const io::DecodeResult d = io::decodeAudioFile(fs::absolute(path).string());
    if (d.error != LE_OK || d.data == nullptr || d.data->numChannels() != 1 ||
        d.data->numFrames() != static_cast<int64_t>(x.size()) || std::fabs(d.data->sampleRate() - sr) > 0.5 ||
        !std::equal(x.begin(), x.end(), d.data->channel(0))) {
        std::fprintf(stderr, "decode lại %s KHÔNG khớp buffer (%s)\n", path.string().c_str(), d.message.c_str());
        return 0;
    }
    std::error_code ec;
    return fs::file_size(path, ec);
}

// Xoá file âm thanh (.wav / .flac) trong `dir` không nằm trong `keep` (VD WAV của P2-30 sau khi chuyển FLAC).
inline void removeStale(const fs::path& dir, const std::set<std::string>& keep) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;
    std::vector<fs::path> stale;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        const std::string ext = e.path().extension().string();
        if ((ext == ".wav" || ext == ".flac") && keep.count(e.path().filename().string()) == 0) stale.push_back(e.path());
    }
    std::sort(stale.begin(), stale.end());
    for (const fs::path& p : stale) {
        fs::remove(p, ec);
        std::printf("  xoá file cũ %s\n", p.filename().string().c_str());
    }
}

inline std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

// Mục manifest gợi ý (06 §4): 77 gộp vào app/assets/library/manifest.json.
struct ManifestEntry {
    std::string id, nameEn, nameVi, category, path, license;
    std::vector<std::string> tags;
    int rangeLo = -1, rangeHi = -1;   // chỉ nhạc cụ
};
inline std::string manifestJson(const ManifestEntry& m) {
    std::string s = "{\n  \"id\": \"" + jsonEscape(m.id) + "\",\n  \"name\": { \"en\": \"" + jsonEscape(m.nameEn) +
                    "\", \"vi\": \"" + jsonEscape(m.nameVi) + "\" },\n  \"category\": \"" + jsonEscape(m.category) +
                    "\",\n  \"path\": \"" + jsonEscape(m.path) + "\",\n";
    if (m.rangeLo >= 0) s += "  \"range\": [" + std::to_string(m.rangeLo) + ", " + std::to_string(m.rangeHi) + "],\n";
    s += "  \"tags\": [";
    for (size_t i = 0; i < m.tags.size(); ++i) s += (i ? ", \"" : "\"") + jsonEscape(m.tags[i]) + "\"";
    s += "],\n  \"license\": \"" + jsonEscape(m.license) + "\"\n}\n";
    return s;
}

// Một nốt của bản nghe thử. dur ≤ 0: không gửi note-off (drum one-shot).
struct Hit {
    double time;     // giây
    int note;
    float velocity;  // 0..1
    double dur = 0.0;
};

// Chơi `hits` bằng Sampler (chia block đúng tại mọi sự kiện), thêm `tailSec` giây, ghi WAV float stereo để nghe.
// Đỉnh > −1 dBFS → hạ về −1 (chỉ bản nghe thử). In tên file, thời lượng, đỉnh, dung lượng.
inline bool renderPreview(const dsp::Instrument& inst, std::vector<Hit> hits, double tailSec, double sr, const fs::path& path) {
    struct Ev { int64_t frame; int note; float vel; bool on; };
    std::vector<Ev> ev;
    double end = 0.0;
    for (const Hit& h : hits) {
        ev.push_back({std::llround(h.time * sr), h.note, h.velocity, true});
        if (h.dur > 0.0) ev.push_back({std::llround((h.time + h.dur) * sr), h.note, 0.0f, false});
        end = std::max(end, h.time + std::max(0.0, h.dur));
    }
    // cùng frame: note-off trước note-on (nốt lặp lại liền nhau)
    std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.frame < b.frame || (a.frame == b.frame && !a.on && b.on); });
    dsp::Sampler s;
    constexpr int kBlock = 256;
    s.prepare(sr, kBlock);
    s.setInstrument(&inst, 1);
    const int64_t n = std::llround((end + tailSec) * sr);
    std::vector<float> l(static_cast<size_t>(n), 0.0f), r(l.size(), 0.0f);
    float* ch[2] = {l.data(), r.data()};
    size_t next = 0;
    for (int64_t pos = 0; pos < n;) {
        while (next < ev.size() && ev[next].frame <= pos) {
            if (ev[next].on) s.noteOn(ev[next].note, ev[next].vel);
            else s.noteOff(ev[next].note);
            ++next;
        }
        int64_t stop = std::min<int64_t>(n, pos + kBlock);
        if (next < ev.size()) stop = std::min(stop, ev[next].frame);
        s.render(ch, 2, static_cast<int>(pos), static_cast<int>(stop - pos));
        pos = stop;
    }
    float peak = 0.0f;
    bool finite = true;
    for (size_t i = 0; i < l.size(); ++i) {
        peak = std::max({peak, std::fabs(l[i]), std::fabs(r[i])});
        finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
    }
    if (!finite) {
        std::fprintf(stderr, "%s: có NaN / Inf\n", path.string().c_str());
        return false;
    }
    const float limit = std::pow(10.0f, -1.0f / 20.0f);
    if (peak > limit)
        for (size_t i = 0; i < l.size(); ++i) {
            l[i] *= limit / peak;
            r[i] *= limit / peak;
        }
    std::string err;
    const float* out[2] = {l.data(), r.data()};
    if (!spike::writeWavFloat(path.string(), out, 2, n, sr, &err)) {
        std::fprintf(stderr, "lỗi ghi %s: %s\n", path.string().c_str(), err.c_str());
        return false;
    }
    std::error_code ec;
    std::printf("  %-34s %6.2f s  đỉnh %5.1f dBFS%s  %ju B\n", path.filename().string().c_str(), static_cast<double>(n) / sr,
                20.0 * std::log10(std::max(peak, 1e-9f)), peak > limit ? " (đã hạ về −1)" : "", fs::file_size(path, ec));
    return true;
}

constexpr double kKitSixteenth = 60.0 / 120.0 / 4.0;   // bản nghe thử kit: 120 BPM

// Groove 1 bar (16 móc đơn) cho mỗi kit — bắt đầu ở móc đơn `at`.
inline std::vector<Hit> kitGroove(const std::string& id, double at) {
    std::vector<Hit> g;
    const auto add = [&](int note, std::initializer_list<double> steps, float vel) {
        for (double st : steps) g.push_back({(at + st) * kKitSixteenth, note, vel});
    };
    if (id == "kit_perc") {
        add(36, {0, 6, 10}, 0.9f);             // conga thấp
        add(37, {3, 11, 14}, 0.8f);            // conga cao
        add(39, {2, 8}, 0.7f);                 // bongo cao
        add(40, {0, 4, 8, 12}, 0.6f);          // cowbell
        add(43, {0, 3, 6, 10, 12}, 0.8f);      // clave (son 3-2)
        for (int st = 0; st < 16; ++st) add(42, {static_cast<double>(st)}, st % 2 == 0 ? 0.7f : 0.4f);   // shaker
        add(45, {0, 8}, 0.6f);                 // triangle mở …
        add(44, {4, 12}, 0.6f);                // … bị triangle tắt chặn (choke)
    } else if (id == "kit_trap") {
        add(36, {0, 7, 10}, 1.0f);             // 808 kick
        add(38, {8}, 0.9f);                    // snare ở phách 3 (half-time)
        add(39, {8}, 0.7f);
        for (int st = 0; st < 16; st += 2) add(42, {static_cast<double>(st)}, 0.7f);
        add(42, {12.5, 13, 13.5, 14, 14.25, 14.5, 14.75}, 0.6f);   // roll hat 1/32 → 1/64
        add(37, {3, 11}, 0.6f);                // perc
        add(40, {15}, 0.7f);                   // snap
        add(51, {0}, 0.5f);                    // riser (ngân qua bar)
    } else {
        const bool boomBap = id == "kit_lofi";
        add(49, {0}, 0.8f);                    // crash
        if (boomBap) add(36, {0, 7, 10}, 1.0f);   // boom-bap: kick lệch phách
        else add(36, {0, 3, 8, 10}, 1.0f);
        add(38, {4, 12}, 0.9f);                // snare
        add(39, {12}, 0.7f);                   // clap
        add(42, {0, 2, 4, 8, 10, 12}, 0.7f);   // hat đóng
        add(46, {6, 14}, 0.7f);                // hat mở — bị hat đóng ở step 8 chặn (choke); step 14 ngân tới hết
        add(47, {13}, 0.8f);                   // fill tom
        add(45, {15}, 0.8f);
    }
    std::stable_sort(g.begin(), g.end(), [](const Hit& a, const Hit& b) { return a.time < b.time; });
    return g;
}

// Bản nghe thử kit: <id>_preview.wav (bar 1 đi qua 16 pad + bar 2 groove) và <id>_pads.wav (từng pad cách 0.8 s).
inline bool renderKitPreviews(const dsp::Instrument& inst, const std::string& id, double sr, const fs::path& outDir) {
    std::vector<Hit> bar;
    for (int i = 0; i < 16; ++i) bar.push_back({i * kKitSixteenth, 36 + i, 0.9f});
    for (const Hit& h : kitGroove(id, 16.0)) bar.push_back(h);
    if (!renderPreview(inst, bar, 3.0, sr, outDir / (id + "_preview.wav"))) return false;
    std::vector<Hit> pads;
    for (int i = 0; i < 16; ++i) pads.push_back({i * 0.8, 36 + i, 0.9f});
    return renderPreview(inst, pads, 3.0, sr, outDir / (id + "_pads.wav"));
}

// Bản nghe hai đầu bàn phím (P2-35 B2): quét C0 → C8 mỗi 3 nửa cung, rồi phím 0 và 127 — mọi phím đều phải kêu.
inline bool renderEdges(const dsp::Instrument& inst, double sr, const fs::path& path) {
    std::vector<Hit> hits;
    double t = 0.0;
    for (int note = 12; note <= 108; note += 3) {
        hits.push_back({t, note, 0.8f, 0.3});
        t += 0.4;
    }
    for (int note : {0, 127}) {
        hits.push_back({t, note, 0.8f, 0.6});
        t += 0.8;
    }
    return renderPreview(inst, hits, 1.5, sr, path);
}

} // namespace le::tools
