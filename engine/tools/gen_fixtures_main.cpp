// le-gen-fixtures — P1-28: ghi bộ trống + nhạc cụ TỔNG HỢP ra engine/tests/fixtures/ (WAV float32 mono 48 kHz,
// đi qua Git LFS theo .gitattributes). [main] (CLI)
//
//   le-gen-fixtures                 # ghi vào <engine>/tests/fixtures/{kit_synth,inst_synth}/
//   le-gen-fixtures <thư mục gốc>   # ghi vào <thư mục gốc>/{kit_synth,inst_synth}/
//
// Âm thanh sinh deterministic (render/SynthFixtures): chạy lại ra đúng từng bit. Tool đọc lại từng file
// vừa ghi và so mã băm để chắc file trên đĩa đúng như buffer.
#include "render/SynthFixtures.h"
#include "spike/measure/WavIO.h"

#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fx = le::render::fixtures;

namespace {
int writeSet(const fx::SynthSet& set, const std::filesystem::path& root) {
    const std::filesystem::path dir = root / set.name;
    std::error_code ec;
    std::filesystem::create_directories(dir / "samples", ec);
    {
        std::ofstream sfz(dir / set.sfzFileName, std::ios::binary | std::ios::trunc);
        sfz << set.sfzText;
        if (!sfz) {
            std::fprintf(stderr, "không ghi được %s\n", (dir / set.sfzFileName).string().c_str());
            return 1;
        }
    }
    std::printf("%s/%s\n", set.name.c_str(), set.sfzFileName.c_str());
    for (const auto& f : set.files) {
        const std::string path = (dir / f.relativePath).string();
        std::string err;
        if (!le::spike::writeWavMono(path, f.samples.data(), static_cast<int64_t>(f.samples.size()), set.sampleRate, &err)) {
            std::fprintf(stderr, "lỗi ghi %s: %s\n", path.c_str(), err.c_str());
            return 1;
        }
        std::vector<float> back;
        double sr = 0.0;
        // So theo GIÁ TRỊ (không so bit: −0.0 và +0.0 bằng nhau nhưng khác bit)
        if (!le::spike::readAudioMono(path, back, sr, &err) || back != f.samples) {
            std::fprintf(stderr, "đọc lại %s KHÔNG khớp buffer\n", path.c_str());
            return 1;
        }
        std::printf("  %-26s %7zu frame  %.2f s  fnv1a %016" PRIx64 "\n", f.relativePath.c_str(), f.samples.size(),
                    static_cast<double>(f.samples.size()) / set.sampleRate, fx::fnv1a(f.samples));
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 2) {
        std::fprintf(stderr, "usage: le-gen-fixtures [thư mục gốc]\n");
        return 1;
    }
    const std::filesystem::path root = argc == 2 ? std::filesystem::path(argv[1])
                                                 : std::filesystem::path(LE_ENGINE_DIR) / "tests" / "fixtures";
    std::printf("Ghi fixture tổng hợp vào %s\n", root.string().c_str());
    if (writeSet(fx::makeDrumKit(), root) != 0) return 1;
    if (writeSet(fx::makeInstrument(), root) != 0) return 1;
    std::printf("Xong. Nghe thử: mở các file .wav ở trên (hoặc nạp .sfz vào le-harness khi 68 nối track.setInstrument).\n");
    return 0;
}
