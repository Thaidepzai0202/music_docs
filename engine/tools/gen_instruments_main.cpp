// le-gen-instruments — P2-35: ghi nhạc cụ giai điệu TỔNG HỢP (render/SynthInstruments: E-Piano FM, Organ drawbar,
// Synth Tone — bản thư viện của fixture inst_synth, KHÔNG đụng tests/fixtures/inst_synth)
// ra content/instruments/<id>/ và render thử để người dùng nghe duyệt. [main] (CLI)
//
//   le-gen-instruments                                  # content/instruments/ + engine/tools/out/instrument_preview/
//   le-gen-instruments <thư mục instruments> [thư mục preview]
//
// Mỗi nhạc cụ: <id>.sfz + samples/*.flac (FLAC 24-bit 48 kHz mono, có loop) + <id>.json (mục manifest gợi ý, 06 §4).
// Từng FLAC được decode lại bằng io::decodeAudioFile và so với buffer từng bit. Bản nghe thử (nạp lại từ đĩa bằng
// SfzLoader, chơi bằng Sampler; WAV float stereo, không vào git):
//   <id>_preview.wav  Cmaj7 – Am7 – Dm7 – G7 lớp nhẹ (velocity 0.6), rồi lớp mạnh (0.95) có giai điệu, rồi nốt trầm /
//                     cao ở hai đầu dải phím
//   <id>_zones.wav    từng phím gốc (mỗi 3 nửa cung) lớp nhẹ rồi lớp mạnh, 0.5 s mỗi nốt — nghe chỗ nối giữa các zone
//   <id>_edges.wav    quét C0 → C8 rồi phím 0 và 127: mọi phím đều kêu (ngoài dải tự nhiên là repitch)
#include "io/SfzLoader.h"
#include "library_writer.h"
#include "render/SynthInstruments.h"

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ins = le::render::instruments;
namespace fs = std::filesystem;
using le::tools::Hit;

namespace {

struct InstInfo { const char* en; const char* vi; const char* category; const char* tag; };
const std::map<std::string, InstInfo>& instInfo() {
    static const std::map<std::string, InstInfo> m = {
        {"inst_epiano", {"E-Piano", "Piano điện", "Instruments/Keys", "keys"}},
        {"inst_organ", {"Organ", "Organ", "Instruments/Keys", "keys"}},
        {"inst_synth", {"Synth Tone", "Tone tổng hợp", "Instruments/Synth", "synth"}},
    };
    return m;
}

int writeInstrument(const ins::SynthInstrument& in, const fs::path& root) {
    const fs::path dir = root / in.id;
    const fs::path sfzPath = dir / in.sfzFileName;
    if (!le::tools::writeText(sfzPath, in.sfzText)) return 1;
    const auto it = instInfo().find(in.id);
    if (it == instInfo().end()) {
        std::fprintf(stderr, "thiếu tên hiển thị cho %s\n", in.id.c_str());
        return 1;
    }
    le::tools::ManifestEntry m;
    m.id = in.id;
    m.nameEn = it->second.en;
    m.nameVi = it->second.vi;
    m.category = it->second.category;
    m.path = "instruments/" + in.id + "/" + in.sfzFileName;
    m.tags = {it->second.tag};
    m.rangeLo = in.rangeLo;
    m.rangeHi = in.rangeHi;
    m.license = "Self-synthesized (le-gen-instruments), no third-party license";
    const fs::path jsonPath = dir / (in.id + ".json");
    if (!le::tools::writeText(jsonPath, le::tools::manifestJson(m))) return 1;

    std::error_code ec;
    uintmax_t total = fs::file_size(sfzPath, ec) + fs::file_size(jsonPath, ec);
    double seconds = 0.0;
    std::printf("%s/%s (%ju B) + %s.json — %zu sample, phím %d..%d\n", in.id.c_str(), in.sfzFileName.c_str(),
                fs::file_size(sfzPath, ec), in.id.c_str(), in.zones.size(), in.rangeLo, in.rangeHi);
    std::set<std::string> keep;
    for (const ins::InstSample& z : in.zones) {
        const uintmax_t bytes = le::tools::writeFlacChecked(dir / "samples" / z.file, z.samples, in.sampleRate);
        if (bytes == 0) return 1;
        keep.insert(z.file);
        total += bytes;
        seconds += static_cast<double>(z.samples.size()) / in.sampleRate;
        std::printf("  samples/%-20s phím %3d-%3d vel %3d-%3d  %4.0f Hz tune %+6.2f  %.2f s (loop %lld..%lld)  %7ju B  "
                    "loudness %5.1f  volume %5.1f\n",
                    z.file.c_str(), z.loKey, z.hiKey, z.loVel, z.hiVel, z.f0Hz, static_cast<double>(z.tuneCents),
                    static_cast<double>(z.samples.size()) / in.sampleRate, static_cast<long long>(z.loopStart),
                    static_cast<long long>(z.loopEnd), bytes, z.loudnessDb, static_cast<double>(z.volumeDb));
    }
    le::tools::removeStale(dir / "samples", keep);
    std::printf("  tổng %s: %.1f KB (%.1f s audio, %.1f MB khi nạp float32)\n", in.id.c_str(),
                static_cast<double>(total) / 1024.0, seconds, seconds * in.sampleRate * 4.0 / 1048576.0);
    return 0;
}

int renderInstrument(const ins::SynthInstrument& in, const fs::path& root, const fs::path& outDir) {
    le::io::SfzLoadOptions opt;
    opt.loadSample = &le::io::decodeAudioFile;
    const auto r = le::io::loadSfzFile(fs::absolute(root / in.id / in.sfzFileName).string(), opt);
    if (!r.ok || r.regions != static_cast<int>(in.zones.size()) || !r.warnings.empty()) {
        std::fprintf(stderr, "nạp lại %s lỗi: %s (%d region, %zu cảnh báo)\n", in.id.c_str(), r.message.c_str(), r.regions,
                     r.warnings.size());
        return 1;
    }
    constexpr float kSoft = 0.6f, kHard = 0.95f;
    const std::vector<std::vector<int>> chords = {{48, 52, 55, 59}, {45, 52, 55, 60}, {50, 53, 57, 60}, {43, 50, 53, 59}};
    std::vector<Hit> hits;
    double t = 0.0;
    for (float vel : {kSoft, kHard}) {
        for (const auto& c : chords) {
            for (int note : c) hits.push_back({t, note, vel * 0.85f, 1.4});
            t += 1.5;
        }
    }
    // Giai điệu lớp mạnh trên vòng hợp âm thứ hai
    const int melody[] = {72, 76, 79, 83, 81, 79, 76, 74, 77, 81, 79, 77, 74, 71, 72, 74};
    for (int i = 0; i < 16; ++i) hits.push_back({6.0 + i * 0.375, melody[i], kHard, 0.33});
    // Hai đầu dải phím
    const int lo = in.rangeLo, hi = in.rangeHi;
    for (int note : {lo, lo + 4, lo + 7, lo + 12}) {
        hits.push_back({t, note, kHard, 0.9});
        t += 1.0;
    }
    for (int note : {hi - 12, hi - 7, hi - 4, hi}) {
        hits.push_back({t, note, kSoft, 0.4});
        t += 0.5;
    }
    if (!le::tools::renderPreview(*r.instrument, hits, 2.5, in.sampleRate, outDir / (in.id + "_preview.wav"))) return 1;

    std::vector<Hit> zones;
    t = 0.0;
    for (const ins::InstSample& z : in.zones) {
        zones.push_back({t, z.root, z.hiVel <= ins::kSoftMaxVel ? kSoft : kHard, 0.4});
        t += 0.5;
    }
    if (!le::tools::renderPreview(*r.instrument, zones, 2.0, in.sampleRate, outDir / (in.id + "_zones.wav"))) return 1;
    return le::tools::renderEdges(*r.instrument, in.sampleRate, outDir / (in.id + "_edges.wav")) ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 3) {
        std::fprintf(stderr, "usage: le-gen-instruments [thư mục instruments] [thư mục preview]\n");
        return 1;
    }
    const fs::path engine = fs::path(LE_ENGINE_DIR).lexically_normal();
    const fs::path root = argc >= 2 ? fs::path(argv[1]) : engine.parent_path() / "content" / "instruments";
    const fs::path previewDir = argc >= 3 ? fs::path(argv[2]) : engine / "tools" / "out" / "instrument_preview";
    const std::vector<ins::SynthInstrument> all = ins::makeAllInstruments(48000.0);
    std::printf("Ghi %zu nhạc cụ tổng hợp (FLAC 24-bit) vào %s\n", all.size(), root.string().c_str());
    for (const ins::SynthInstrument& in : all)
        if (writeInstrument(in, root) != 0) return 1;
    std::printf("Render thử (nạp lại từ đĩa bằng SfzLoader + decodeAudioFile + Sampler) vào %s\n", previewDir.string().c_str());
    for (const ins::SynthInstrument& in : all)
        if (renderInstrument(in, root, previewDir) != 0) return 1;
    std::printf("Xong. Nghe: *_preview.wav (hợp âm + giai điệu), *_zones.wav (từng zone) và *_edges.wav (C0 → C8, phím 0 / 127).\n");
    return 0;
}
