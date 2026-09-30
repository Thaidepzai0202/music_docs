// le-import-library — P2-35 đợt B: rút gọn sample THU THẬT (Salamander Grand Piano V3, VSCO-2 CE) thành nhạc cụ SFZ
// FLAC 24-bit 48 kHz trong content/instruments/<id>/ + bản nghe thử. [main] (CLI)
//
//   le-import-library <thư mục nguồn>                       # nguồn: <src>/salamander/*.flac, <src>/vsco/<nhóm>/*.wav
//   le-import-library <thư mục nguồn> <thư mục instruments> [thư mục preview]
//   le-import-library --probe <thư mục nguồn>               # chỉ đo quy ước quãng tám của từng thư mục nguồn (Yin)
//   le-import-library --kit-only <thư mục nguồn> [...]      # chỉ làm kit_acoustic (Big Rusty Drums, <src>/brd/)
// Kit acoustic ghi vào <thư mục instruments>/../kits/kit_acoustic, nghe thử vào <thư mục preview>/../kit_preview.
//
// Nguồn KHÔNG vào repo (tải về scratch; danh sách URL + license ở content/LICENSES/). Tool chỉ đọc file đã tải,
// không tự tải. Bảng nhạc cụ bên dưới: thư mục nguồn, quy ước tên nốt (đa số VSCO: C3 = 60 → +12; Solo Violin, Harp
// và Salamander: C4 = 60 → 0 — đã đo bằng `--probe`, Yin xác nhận từng file), dải phím nhận
// (tách String Ensemble / Pizzicato theo bè), số lớp velocity, sustain (loop) hay decay, độ dài, release.
// Âm ra: mono (tổng L+R) để vừa ngân sách ≤ 150 MB (06 §4), resample 44.1 → 48 kHz bằng import::resample.
#include "io/AudioFileIO.h"
#include "io/SfzLoader.h"
#include "library_writer.h"
#include "render/SampleImport.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace imp = le::render::import;
namespace fs = std::filesystem;
using le::tools::Hit;

namespace {

struct Part {
    const char* dir;     // thư mục dưới <src>/
    int octaveShift;     // cộng vào nốt đọc từ tên (VSCO +12)
    int lo, hi;          // dải nốt (sau khi cộng) nhận từ thư mục này
};

struct Def {
    const char* id;
    const char* nameEn;
    const char* nameVi;
    const char* category;
    const char* tag;
    const char* title;
    bool salamander;     // license: Salamander (CC-BY 3.0) hay VSCO-2 CE (CC0)
    imp::ImportSpec spec;
    std::vector<Part> parts;
};

imp::ImportSpec sus(int layers, double release, double attack = 0.01) {
    imp::ImportSpec s;
    s.mode = imp::Mode::Sustain;
    s.maxLayers = layers;
    s.release = release;
    s.attack = attack;
    return s;
}
imp::ImportSpec natural(imp::ImportSpec s) {   // giữ cao độ gốc (piano: stretch tuning)
    s.measureTune = false;
    return s;
}
imp::ImportSpec dec(int layers, double maxLow, double maxHigh, double release) {
    imp::ImportSpec s;
    s.mode = imp::Mode::Decay;
    s.maxLayers = layers;
    s.maxSecLow = maxLow;
    s.maxSecHigh = maxHigh;
    s.release = release;
    s.attack = 0.001;
    return s;
}

const std::vector<Def>& defs() {
    static const std::vector<Def> d = {
        {"inst_piano", "Grand Piano", "Đại dương cầm", "Instruments/Keys", "keys",
         "Grand Piano (Salamander Grand Piano V3, Yamaha C5; giữ stretch tuning gốc)", true, natural(dec(3, 6.0, 2.0, 0.45)),   // đuôi 6 s → 2 s: ~62 MB RAM (người dùng chốt 30/09)
         {{"salamander", 0, 0, 127}}},
        {"inst_violin", "Violin", "Violin", "Instruments/Strings", "strings",
         "Violin (VSCO-2 CE Solo Violin, arco vibrato)", false, sus(2, 0.35), {{"vsco/violin", 0, 0, 127}}},
        {"inst_viola", "Viola", "Viola", "Instruments/Strings", "strings",
         "Viola (VSCO-2 CE Viola Section, sustain vibrato)", false, sus(2, 0.35), {{"vsco/viola", 12, 0, 127}}},
        {"inst_cello", "Cello", "Cello", "Instruments/Strings", "strings",
         "Cello (VSCO-2 CE Cello Section, sustain vibrato)", false, sus(2, 0.35), {{"vsco/cello", 12, 0, 127}}},
        {"inst_contrabass", "Contrabass", "Contrabass", "Instruments/Strings", "strings",
         "Contrabass (VSCO-2 CE Solo Contrabass, sustain vibrato)", false, sus(2, 0.35), {{"vsco/contrabass", 12, 0, 127}}},
        {"inst_strings", "String Ensemble", "Dàn dây", "Instruments/Strings", "strings",
         "String Ensemble (VSCO-2 CE: contrabass < C2 · cello section C2–B2 · viola section C3–F#3 · violin section ≥ G3)",
         false, sus(2, 0.4), {{"vsco/contrabass", 12, 0, 35}, {"vsco/cello", 12, 36, 47}, {"vsco/viola", 12, 48, 54}, {"vsco/vln_sec", 12, 55, 127}}},
        {"inst_pizzicato", "Pizzicato", "Pizzicato", "Instruments/Strings", "strings",
         "Pizzicato (VSCO-2 CE: contrabass < C2 · cello C2–B2 · viola C3–F#3 · violin ≥ G3)",
         false, dec(2, 1.6, 0.8, 0.3), {{"vsco/pz_cb", 12, 0, 35}, {"vsco/pz_vc", 12, 36, 47}, {"vsco/pz_vla", 12, 48, 54}, {"vsco/pz_vln", 12, 55, 127}}},
        {"inst_harp", "Harp", "Đàn hạc", "Instruments/Strings", "strings",
         "Harp (VSCO-2 CE Harp, mf)", false, dec(1, 6.0, 2.0, 1.2), {{"vsco/harp", 0, 0, 127}}},
        {"inst_flute", "Flute", "Sáo", "Instruments/Winds & Brass", "winds",
         "Flute (VSCO-2 CE Flute, sustain vibrato)", false, sus(1, 0.25), {{"vsco/flute", 12, 0, 127}}},
        {"inst_clarinet", "Clarinet", "Clarinet", "Instruments/Winds & Brass", "winds",
         "Clarinet (VSCO-2 CE Clarinet, long sustain)", false, sus(2, 0.25), {{"vsco/clarinet", 12, 0, 127}}},
        {"inst_oboe", "Oboe", "Oboe", "Instruments/Winds & Brass", "winds",
         "Oboe (VSCO-2 CE Oboe, vibrato)", false, sus(2, 0.25), {{"vsco/oboe", 12, 0, 127}}},
        {"inst_trumpet", "Trumpet", "Kèn trumpet", "Instruments/Winds & Brass", "brass",
         "Trumpet (VSCO-2 CE Trumpet, sustain vibrato)", false, sus(2, 0.25), {{"vsco/trumpet", 12, 0, 127}}},
        {"inst_horn", "French Horn", "Kèn horn", "Instruments/Winds & Brass", "brass",
         "French Horn (VSCO-2 CE F Horn, sustain)", false, sus(2, 0.3), {{"vsco/horn", 12, 0, 127}}},
        {"inst_trombone", "Trombone", "Kèn trombone", "Instruments/Winds & Brass", "brass",
         "Trombone (VSCO-2 CE Tenor Trombone, sustain)", false, sus(2, 0.3), {{"vsco/trombone", 12, 0, 127}}},
    };
    return d;
}

const char* kSalamanderCredit =
    "Salamander Grand Piano V3 by Alexander Holm, CC BY 3.0 (creativecommons.org/licenses/by/3.0/); source "
    "archive.org/details/SalamanderGrandPianoV3 via github.com/sfzinstruments/SalamanderGrandPiano. Modified (see content/LICENSES/).";
const char* kVscoCredit =
    "VSCO-2 Community Edition by Versilian Studios (Sam Gossner) / Ivy Audio (Simon Dalzell), CC0 1.0; "
    "github.com/sgossner/VSCO-2-CE — versilian-studios.com/vsco-community/";

// Tên file → (nốt theo tên, hạng velocity). VSCO: token "A#2" + "v1"/"p"/"f"/"mf"; Salamander: "D#1v10".
bool parseName(const std::string& file, int& note, int& layer) {
    std::string base = fs::path(file).stem().string();
    note = -1;
    layer = 0;
    const size_t vpos = base.find('v');
    if (base.find('_') == std::string::npos && vpos != std::string::npos && vpos > 0) {   // Salamander
        note = imp::noteFromName(base.substr(0, vpos));
        layer = std::atoi(base.c_str() + vpos + 1);
        return note >= 0;
    }
    static const std::map<std::string, int> dyn = {{"pp", 0}, {"p", 1}, {"mp", 2}, {"mf", 3}, {"f", 4}, {"ff", 5}};
    size_t a = 0;
    while (a <= base.size()) {
        const size_t b = std::min(base.find('_', a), base.size());
        const std::string tok = base.substr(a, b - a);
        if (note < 0 && imp::noteFromName(tok) >= 0) note = imp::noteFromName(tok);
        else if (tok.size() == 2 && tok[0] == 'v' && tok[1] >= '0' && tok[1] <= '9') layer = tok[1] - '0';
        else if (dyn.count(tok) != 0) layer = dyn.at(tok);
        a = b + 1;
    }
    return note >= 0;
}

bool loadSources(const Def& d, const fs::path& src, std::vector<imp::SourceSample>& out) {
    for (const Part& p : d.parts) {
        std::vector<fs::path> files;
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(src / p.dir, ec))
            if (e.path().extension() == ".wav" || e.path().extension() == ".flac") files.push_back(e.path());
        if (files.empty()) {
            std::fprintf(stderr, "%s: không có file trong %s\n", d.id, (src / p.dir).string().c_str());
            return false;
        }
        std::sort(files.begin(), files.end());
        for (const fs::path& f : files) {
            int note = -1, layer = 0;
            if (!parseName(f.filename().string(), note, layer)) {
                std::printf("  bỏ qua %s (không đọc được nốt)\n", f.filename().string().c_str());
                continue;
            }
            note += p.octaveShift;
            if (note < p.lo || note > p.hi) continue;
            const le::io::DecodeResult r = le::io::decodeAudioFile(fs::absolute(f).string());
            if (r.error != LE_OK) {
                std::fprintf(stderr, "decode lỗi %s: %s\n", f.string().c_str(), r.message.c_str());
                return false;
            }
            imp::SourceSample s;
            s.name = std::string(p.dir) + "/" + f.filename().string();
            s.nameNote = note;
            s.layer = layer;
            const int srIn = static_cast<int>(std::lround(r.data->sampleRate()));
            for (int c = 0; c < std::min(2, r.data->numChannels()); ++c) {
                std::vector<float> ch(r.data->channel(c), r.data->channel(c) + r.data->numFrames());
                if (srIn != 48000) ch = imp::resample(ch, srIn, 48000);
                (c == 0 ? s.left : s.right) = std::move(ch);
            }
            s.sampleRate = 48000.0;
            out.push_back(std::move(s));
        }
    }
    return !out.empty();
}

uintmax_t writeZone(const fs::path& path, const imp::ImportedZone& z) {
    if (z.right.empty()) return le::tools::writeFlacChecked(path, z.left, z.sampleRate);
    std::string err;
    const float* ch[2] = {z.left.data(), z.right.data()};
    if (!le::spike::writeFlac24(path.string(), ch, 2, static_cast<int64_t>(z.left.size()), z.sampleRate, 5, &err)) {
        std::fprintf(stderr, "lỗi ghi %s: %s\n", path.string().c_str(), err.c_str());
        return 0;
    }
    const le::io::DecodeResult d = le::io::decodeAudioFile(fs::absolute(path).string());
    if (d.error != LE_OK || d.data->numChannels() != 2 || !std::equal(z.left.begin(), z.left.end(), d.data->channel(0)) ||
        !std::equal(z.right.begin(), z.right.end(), d.data->channel(1))) {
        std::fprintf(stderr, "decode lại %s KHÔNG khớp\n", path.string().c_str());
        return 0;
    }
    std::error_code ec;
    return fs::file_size(path, ec);
}

int renderPreview(const Def& d, const imp::ImportResult& r, const fs::path& root, const fs::path& outDir) {
    le::io::SfzLoadOptions opt;
    opt.loadSample = &le::io::decodeAudioFile;
    const auto l = le::io::loadSfzFile(fs::absolute(root / d.id / (std::string(d.id) + ".sfz")).string(), opt);
    if (!l.ok || !l.warnings.empty() || l.regions != static_cast<int>(r.zones.size())) {
        std::fprintf(stderr, "nạp lại %s lỗi: %s\n", d.id, l.message.c_str());
        return 1;
    }
    const bool decay = d.spec.mode == imp::Mode::Decay;
    const int center = std::clamp((r.rangeLo + r.rangeHi) / 2, r.rangeLo + 7, r.rangeHi - 12);
    const int scale[] = {0, 2, 4, 5, 7, 9, 11, 12};
    std::vector<Hit> hits;
    double t = 0.0;
    for (float vel : {0.5f, 0.95f}) {   // thang âm trưởng lớp nhẹ rồi lớp mạnh
        for (int s : scale) {
            hits.push_back({t, center + s, vel, decay ? 0.35 : 0.45});
            t += 0.5;
        }
        for (int s : {0, 4, 7, 12}) hits.push_back({t, center + s - 12 >= r.rangeLo ? center + s - 12 : center + s, vel, 1.8});
        t += 2.2;
    }
    for (int k = r.rangeLo; k <= r.rangeHi; k += std::max(1, (r.rangeHi - r.rangeLo) / 10)) {   // quét cả dải
        hits.push_back({t, k, 0.8f, 0.5});
        t += 0.6;
    }
    if (!le::tools::renderPreview(*l.instrument, hits, decay ? 3.0 : 1.5, 48000.0, outDir / (std::string(d.id) + "_preview.wav")))
        return 1;
    return le::tools::renderEdges(*l.instrument, 48000.0, outDir / (std::string(d.id) + "_edges.wav")) ? 0 : 1;   // C0 → C8
}

// ─────────────── kit_acoustic (Big Rusty Drums, CC0) ───────────────
// Mỗi pad: thư mục articulation dưới <src>/brd/, các micro (thư mục con) với hệ số trộn; 2 lớp = 2 file vl đã tải
// (vl giữa và vl mạnh nhất, round-robin 1 — xem library_sources.tsv). Trộn: kick chủ yếu mic trong trống; snare
// top + một ít bottom + overhead; tom / hat close + overhead; cymbal chủ yếu overhead (theo mặc định của bản SFZ gốc).
struct Mic { const char* dir; double gain; };
struct AcPad {
    int key;
    const char* label;
    const char* file;
    const char* art;             // VD "kick_24/kick"
    std::vector<Mic> mics;
    double maxSec;
    int group = 0, offBy = 0;
    int sameAs = -1;
    float tune = 0.0f;
};
const std::vector<Mic> kKickMics = {{"kick", 1.0}, {"oh", 0.35}};
const std::vector<Mic> kSnareMics = {{"top", 1.0}, {"btm", 0.4}, {"oh", 0.6}};
const std::vector<Mic> kTomMics = {{"cl", 1.0}, {"oh", 0.6}};
const std::vector<Mic> kHatMics = {{"cl", 1.0}, {"oh", 0.7}};
const std::vector<Mic> kCymMics = {{"cl", 0.4}, {"oh", 1.0}};
const std::vector<AcPad>& acousticPads() {
    static const std::vector<AcPad> p = {
        {36, "Kick", "kick", "kick_24/kick", kKickMics, 1.2},
        {37, "Side Stick", "sidestick", "snare_14/sidestick", kSnareMics, 0.5},
        {38, "Snare", "snare", "snare_14/center", kSnareMics, 1.2},
        {39, "Rim Click", "rimclick", "snare_14/rc", kSnareMics, 0.5},          // bộ này không có clap (người dùng chốt)
        {40, "Rimshot", "rimshot", "snare_14/rimshot", kSnareMics, 1.2},
        {41, "Floor Tom L", "tom_22", "tom_22/center", kTomMics, 2.0},
        {42, "Closed Hat", "hat_closed", "hihat_14/cl", kHatMics, 0.6, 1},
        {43, "Floor Tom H", "tom_18", "tom_18/center", kTomMics, 2.0},
        {44, "Pedal Hat", "hat_pedal", "hihat_14/chik", kHatMics, 0.6, 1},
        {45, "Low Tom", "tom_15", "tom_15/center", kTomMics, 1.8},
        {46, "Open Hat", "hat_open", "hihat_14/open", kHatMics, 2.5, 2, 1},
        {47, "Mid Tom", "tom_14", "tom_14/center", kTomMics, 1.6},
        {48, "Hi-Mid Tom", "", "", {}, 0.0, 0, 0, 47, 300.0f},               // bộ chỉ có 4 tom: dùng tom 14" nâng 3 / 6 nửa cung
        {49, "Crash", "crash", "crash_17/cr", kCymMics, 4.0},
        {50, "High Tom", "", "", {}, 0.0, 0, 0, 47, 600.0f},
        {51, "Ride", "ride", "ride_22/rd", kCymMics, 4.0},
    };
    return p;
}

// Trộn các micro của một lớp (file có cùng tên cơ sở "…_vl<N>_rr1.flac" trong từng thư mục micro) → mono 48 kHz.
bool mixLayer(const fs::path& artDir, const std::vector<Mic>& mics, int vl, imp::KitLayerSource& out) {
    std::vector<double> sum;
    int srIn = 0;
    for (const Mic& m : mics) {
        std::error_code ec;
        fs::path found;
        for (const auto& e : fs::directory_iterator(artDir / m.dir, ec)) {
            const std::string n = e.path().filename().string();
            if (n.find("_vl" + std::to_string(vl) + "_rr1.flac") != std::string::npos) found = e.path();
        }
        if (found.empty()) {
            std::fprintf(stderr, "thiếu vl%d trong %s\n", vl, (artDir / m.dir).string().c_str());
            return false;
        }
        const le::io::DecodeResult r = le::io::decodeAudioFile(fs::absolute(found).string());
        if (r.error != LE_OK) {
            std::fprintf(stderr, "decode lỗi %s: %s\n", found.string().c_str(), r.message.c_str());
            return false;
        }
        srIn = static_cast<int>(std::lround(r.data->sampleRate()));
        const int64_t n = r.data->numFrames();
        if (static_cast<int64_t>(sum.size()) < n) sum.resize(static_cast<size_t>(n), 0.0);
        const int ch = r.data->numChannels();
        for (int c = 0; c < ch; ++c)
            for (int64_t i = 0; i < n; ++i) sum[static_cast<size_t>(i)] += m.gain / ch * static_cast<double>(r.data->channel(c)[i]);
        out.name += (out.name.empty() ? "" : " + ") + found.filename().string() + "×" + std::to_string(m.gain).substr(0, 4);
    }
    std::vector<float> mono(sum.begin(), sum.end());
    out.mono = srIn == 48000 ? mono : imp::resample(mono, srIn, 48000);
    out.sampleRate = 48000.0;
    return true;
}

int importAcousticKit(const fs::path& src, const fs::path& kitsRoot, const fs::path& previewDir) {
    const char* id = "kit_acoustic";
    std::vector<imp::KitPadSpec> specs;
    for (const AcPad& a : acousticPads()) {
        imp::KitPadSpec s;
        s.key = a.key;
        s.label = a.label;
        s.file = a.file;
        s.group = a.group;
        s.offBy = a.offBy;
        s.tuneCents = a.tune;
        s.maxSec = a.maxSec;
        s.sameAs = a.sameAs;
        if (a.sameAs < 0) {
            // 2 lớp = 2 số vl đã tải (nhỏ = nhẹ)
            std::set<int> vls;
            std::error_code ec;
            for (const auto& e : fs::directory_iterator(src / "brd" / a.art / a.mics.front().dir, ec)) {
                const std::string n = e.path().filename().string();
                const size_t p = n.find("_vl");
                if (p != std::string::npos) vls.insert(std::atoi(n.c_str() + p + 3));
            }
            if (vls.empty()) {
                std::fprintf(stderr, "%s: không có file trong %s\n", id, (src / "brd" / a.art).string().c_str());
                return 1;
            }
            for (int vl : vls) {
                imp::KitLayerSource l;
                if (!mixLayer(src / "brd" / a.art, a.mics, vl, l)) return 1;
                s.layers.push_back(std::move(l));
            }
        }
        specs.push_back(std::move(s));
    }
    const imp::KitImportResult r = imp::importKit(
        id, "Acoustic — Big Rusty Drums (Karoryfer Samples), bộ trống cổ Ba Lan, 16 pad map GM",
        "Big Rusty Drums by Karoryfer Samples, CC0 1.0 — github.com/sfzinstruments/karoryfer.big-rusty-drums", specs);
    if (!r.ok) {
        std::fprintf(stderr, "%s: %s\n", id, r.error.c_str());
        return 1;
    }
    const fs::path dir = kitsRoot / id;
    if (!le::tools::writeText(dir / (std::string(id) + ".sfz"), r.sfzText)) return 1;
    le::tools::ManifestEntry m;
    m.id = id;
    m.nameEn = "Acoustic";
    m.nameVi = "Trống thật";
    m.category = "Drums";
    m.path = std::string("kits/") + id + "/" + id + ".sfz";
    m.tags = {"drums"};
    m.license = "CC0 1.0 — Big Rusty Drums by Karoryfer Samples";
    if (!le::tools::writeText(dir / (std::string(id) + ".json"), le::tools::manifestJson(m))) return 1;
    for (const std::string& n : r.notes) std::printf("    · %s\n", n.c_str());
    uintmax_t total = 0;
    double seconds = 0.0;
    std::set<std::string> keep;
    for (const imp::KitRegion& z : r.regions) {
        if (z.shared) continue;
        const uintmax_t b = le::tools::writeFlacChecked(dir / "samples" / z.file, z.samples, z.sampleRate);
        if (b == 0) return 1;
        keep.insert(z.file);
        total += b;
        seconds += static_cast<double>(z.samples.size()) / z.sampleRate;
        std::printf("  %2d %-12s samples/%-20s vel %3d-%3d %.2f s %7ju B  loudness %5.1f  volume %5.1f\n", z.key, z.label.c_str(),
                    z.file.c_str(), z.loVel, z.hiVel, static_cast<double>(z.samples.size()) / z.sampleRate, b, z.loudnessDb,
                    static_cast<double>(z.volumeDb));
    }
    le::tools::removeStale(dir / "samples", keep);
    std::printf("%s: %zu region / %zu file, %.1f s audio, %.2f MB (RAM %.1f MB)\n", id, r.regions.size(), keep.size(), seconds,
                static_cast<double>(total) / 1048576.0, seconds * 48000.0 * 4.0 / 1048576.0);
    le::io::SfzLoadOptions opt;
    opt.loadSample = &le::io::decodeAudioFile;
    const auto l = le::io::loadSfzFile(fs::absolute(dir / (std::string(id) + ".sfz")).string(), opt);
    if (!l.ok || !l.warnings.empty()) {
        std::fprintf(stderr, "nạp lại %s lỗi: %s\n", id, l.message.c_str());
        return 1;
    }
    return le::tools::renderKitPreviews(*l.instrument, id, 48000.0, previewDir) ? 0 : 1;
}

int probe(const fs::path& src) {
    for (const Def& d : defs())
        for (const Part& p : d.parts) {
            std::vector<imp::SourceSample> s;
            Def one = d;
            one.parts = {{p.dir, 0, 0, 127}};
            if (!loadSources(one, src, s)) return 1;
            imp::ImportSpec spec = d.spec;
            spec.id = d.id;
            spec.maxLayers = 1;
            const auto r = imp::importInstrument(spec, s);
            std::map<int, int> oct;
            for (const std::string& n : r.notes)
                if (n.find("quãng tám") != std::string::npos) ++oct[std::atoi(n.c_str() + n.find("lệch ") + std::strlen("lệch "))];
            std::printf("%-12s %-18s %zu zone, lệch quãng tám theo Yin (tên C4=60):", d.id, p.dir, r.zones.size());
            int ok = static_cast<int>(r.zones.size());
            for (const auto& [k, n] : oct) {
                std::printf(" %+d×%d", k, n);
                ok -= n;
            }
            std::printf(" · 0×%d\n", ok);
        }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 3 && std::strcmp(argv[1], "--probe") == 0) return probe(argv[2]);
    const bool kitOnly = argc >= 2 && std::strcmp(argv[1], "--kit-only") == 0;   // chỉ làm kit_acoustic
    if (kitOnly) {
        --argc;
        ++argv;
    }
    if (argc < 2 || argc > 4) {
        std::fprintf(stderr, "usage: le-import-library <thư mục nguồn> [thư mục instruments] [thư mục preview]\n"
                             "       le-import-library --probe <thư mục nguồn>\n");
        return 1;
    }
    const fs::path engine = fs::path(LE_ENGINE_DIR).lexically_normal();
    const fs::path src = argv[1];
    const fs::path root = argc >= 3 ? fs::path(argv[2]) : engine.parent_path() / "content" / "instruments";
    const fs::path previewDir = argc >= 4 ? fs::path(argv[3]) : engine / "tools" / "out" / "instrument_preview";
    const fs::path kitsRoot = root.parent_path() / "kits";
    const fs::path kitPreview = previewDir.parent_path() / "kit_preview";
    uintmax_t grand = 0;
    for (const Def& d : (kitOnly ? std::vector<Def>{} : defs())) {
        std::vector<imp::SourceSample> sources;
        if (!loadSources(d, src, sources)) return 1;
        imp::ImportSpec spec = d.spec;
        spec.id = d.id;
        spec.title = d.title;
        spec.credit = d.salamander ? kSalamanderCredit : kVscoCredit;
        const imp::ImportResult r = imp::importInstrument(spec, sources);
        if (!r.ok) {
            std::fprintf(stderr, "%s: %s\n", d.id, r.error.c_str());
            return 1;
        }
        const fs::path dir = root / d.id;
        if (!le::tools::writeText(dir / (std::string(d.id) + ".sfz"), r.sfzText)) return 1;
        le::tools::ManifestEntry m;
        m.id = d.id;
        m.nameEn = d.nameEn;
        m.nameVi = d.nameVi;
        m.category = d.category;
        m.path = std::string("instruments/") + d.id + "/" + d.id + ".sfz";
        m.tags = {d.tag};
        m.rangeLo = r.rangeLo;
        m.rangeHi = r.rangeHi;
        m.license = d.salamander ? "CC BY 3.0 — Salamander Grand Piano V3 by Alexander Holm (modified)"
                                 : "CC0 1.0 — VSCO-2 Community Edition by Versilian Studios";
        if (!le::tools::writeText(dir / (std::string(d.id) + ".json"), le::tools::manifestJson(m))) return 1;
        uintmax_t total = 0;
        double seconds = 0.0;
        std::set<std::string> keep;
        for (const imp::ImportedZone& z : r.zones) {
            if (z.shared) continue;   // nốt chỉ có 1 sample: lớp khác dùng chung file
            const uintmax_t b = writeZone(dir / "samples" / z.file, z);
            if (b == 0) return 1;
            keep.insert(z.file);
            total += b;
            seconds += static_cast<double>(z.left.size()) / z.sampleRate;
        }
        le::tools::removeStale(dir / "samples", keep);
        grand += total;
        int measured = 0;
        for (const imp::ImportedZone& z : r.zones) measured += z.pitchMeasured && !z.shared ? 1 : 0;
        std::printf("%-16s %2zu region / %zu file (%d đo được cao độ), phím %d..%d, %.1f s audio, %.2f MB\n", d.id, r.zones.size(),
                    keep.size(), measured, r.rangeLo, r.rangeHi, seconds, static_cast<double>(total) / 1048576.0);
        for (const std::string& n : r.notes) std::printf("    · %s\n", n.c_str());
        if (renderPreview(d, r, root, previewDir) != 0) return 1;
    }
    if (!kitOnly) std::printf("Tổng mẫu thật (nhạc cụ): %.1f MB (ngân sách 06 §4: ≤ 150 MB)\n", static_cast<double>(grand) / 1048576.0);
    std::printf("Kit acoustic → %s (nghe thử: %s)\n", (kitsRoot / "kit_acoustic").string().c_str(), kitPreview.string().c_str());
    return importAcousticKit(src, kitsRoot, kitPreview);
}
