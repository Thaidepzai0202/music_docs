// le-gen-kits — P2-30 / P2-35: ghi 8 bộ trống 16 pad TỔNG HỢP (render/DrumKits) ra content/kits/<id>/ và render thử
// để người dùng nghe duyệt. [main] (CLI)
//
//   le-gen-kits                                  # content/kits/ + engine/tools/out/kit_preview/
//   le-gen-kits <thư mục kits> [thư mục preview]
//
// Mỗi kit: <id>.sfz + samples/*.flac (FLAC 24-bit 48 kHz mono) + <id>.json (mục manifest gợi ý cho 77, 06 §4).
// Từng FLAC được decode lại bằng io::decodeAudioFile (đường engine nạp kit) và so với buffer từng bit; file .wav /
// .flac cũ không còn dùng trong samples/ bị xoá (WAV của P2-30). Sau đó NẠP LẠI kit từ đĩa bằng SfzLoader và chơi
// bằng Sampler để ghi bản nghe thử (WAV float stereo, không vào git):
//   <id>_preview.wav  bar 1: 16 pad lần lượt (móc đơn, 36 → 51) · bar 2: một groove ngắn (có choke hat) · 120 BPM
//   <id>_pads.wav     từng pad riêng, cách nhau 0.8 s (36 → 51) để nghe kỹ từng âm
#include "io/SfzLoader.h"
#include "library_writer.h"
#include "render/DrumKits.h"

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace kits = le::render::kits;
namespace fs = std::filesystem;
using le::tools::Hit;

namespace {

struct KitInfo { const char* en; const char* vi; const char* tag; };
const std::map<std::string, KitInfo>& kitInfo() {
    static const std::map<std::string, KitInfo> m = {
        {"kit_808", {"808 Kit", "Kit 808", "drums"}},        {"kit_909", {"909 Kit", "Kit 909", "drums"}},
        {"kit_perc", {"Percussion", "Bộ gõ", "percussion"}}, {"kit_trap", {"Trap Kit", "Kit Trap", "drums"}},
        {"kit_lofi", {"Lo-fi Kit", "Kit Lo-fi", "drums"}},   {"kit_606", {"606 Kit", "Kit 606", "drums"}},
        {"kit_707", {"707 Kit", "Kit 707", "drums"}},        {"kit_linn", {"Linn Kit", "Kit Linn", "drums"}},
    };
    return m;
}

int writeKit(const kits::Kit& k, const fs::path& root) {
    const fs::path dir = root / k.id;
    const fs::path sfzPath = dir / k.sfzFileName;
    if (!le::tools::writeText(sfzPath, k.sfzText)) return 1;
    const auto it = kitInfo().find(k.id);
    if (it == kitInfo().end()) {
        std::fprintf(stderr, "thiếu tên hiển thị cho %s\n", k.id.c_str());
        return 1;
    }
    le::tools::ManifestEntry m;
    m.id = k.id;
    m.nameEn = it->second.en;
    m.nameVi = it->second.vi;
    m.category = "Drums";
    m.path = "kits/" + k.id + "/" + k.sfzFileName;
    m.tags = {it->second.tag};
    m.license = "Self-synthesized (le-gen-kits), no third-party license";
    const fs::path jsonPath = dir / (k.id + ".json");
    if (!le::tools::writeText(jsonPath, le::tools::manifestJson(m))) return 1;

    std::error_code ec;
    uintmax_t total = fs::file_size(sfzPath, ec) + fs::file_size(jsonPath, ec);
    std::printf("%s/%s (%ju B) + %s.json\n", k.id.c_str(), k.sfzFileName.c_str(), fs::file_size(sfzPath, ec), k.id.c_str());
    std::set<std::string> keep;
    for (const kits::KitPad& p : k.pads) {
        const uintmax_t bytes = le::tools::writeFlacChecked(dir / "samples" / p.file, p.samples, k.sampleRate);
        if (bytes == 0) return 1;
        keep.insert(p.file);
        total += bytes;
        std::printf("  %2d %-14s samples/%-20s %.3f s %8ju B  loudness %6.1f dB  volume %5.1f\n", p.key, p.label.c_str(),
                    p.file.c_str(), static_cast<double>(p.samples.size()) / k.sampleRate, bytes, p.loudnessDb,
                    static_cast<double>(p.volumeDb));
    }
    le::tools::removeStale(dir / "samples", keep);
    std::printf("  tổng %s: %.1f KB\n", k.id.c_str(), static_cast<double>(total) / 1024.0);
    return 0;
}

int renderKit(const kits::Kit& k, const fs::path& kitsRoot, const fs::path& outDir) {
    le::io::SfzLoadOptions opt;
    opt.loadSample = &le::io::decodeAudioFile;
    const auto r = le::io::loadSfzFile(fs::absolute(kitsRoot / k.id / k.sfzFileName).string(), opt);
    if (!r.ok || r.regions != 16 || !r.warnings.empty()) {
        std::fprintf(stderr, "nạp lại %s lỗi: %s (%d region, %zu cảnh báo)\n", k.id.c_str(), r.message.c_str(), r.regions,
                     r.warnings.size());
        return 1;
    }
    return le::tools::renderKitPreviews(*r.instrument, k.id, k.sampleRate, outDir) ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 3) {
        std::fprintf(stderr, "usage: le-gen-kits [thư mục kits] [thư mục preview]\n");
        return 1;
    }
    const fs::path engine = fs::path(LE_ENGINE_DIR).lexically_normal();
    const fs::path kitsRoot = argc >= 2 ? fs::path(argv[1]) : engine.parent_path() / "content" / "kits";
    const fs::path previewDir = argc >= 3 ? fs::path(argv[2]) : engine / "tools" / "out" / "kit_preview";
    const std::vector<kits::Kit> all = kits::makeAllKits(48000.0);
    std::printf("Ghi %zu kit tổng hợp (FLAC 24-bit) vào %s\n", all.size(), kitsRoot.string().c_str());
    for (const kits::Kit& k : all)
        if (writeKit(k, kitsRoot) != 0) return 1;
    std::printf("Render thử (nạp lại từ đĩa bằng SfzLoader + decodeAudioFile + Sampler) vào %s\n", previewDir.string().c_str());
    for (const kits::Kit& k : all)
        if (renderKit(k, kitsRoot, previewDir) != 0) return 1;
    std::printf("Xong. Nghe: *_preview.wav (1 bar đi qua 16 pad + 1 bar groove) và *_pads.wav (từng pad riêng).\n");
    return 0;
}
