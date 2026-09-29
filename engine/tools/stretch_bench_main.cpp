// le-stretch-bench — P0-09 trên Mac: render 13 zone (-18…+18 nửa cung) bằng Signalsmith, in thời gian,
// ghi WAV để tự nghe. [main] (CLI, không có audio thread)
//
//   le-stretch-bench in.wav outdir                 # cả 2 biến thể: 26 file (13 plain + 13 formant)
//   le-stretch-bench in.wav outdir --formant       # chỉ formant on (13 file)
//   le-stretch-bench in.wav outdir --no-formant    # chỉ formant off (13 file)
// Tuỳ chọn thêm:
//   --base-hz F        f0 ước lượng cho formant (mặc định 0 = Signalsmith tự đoán)
//   --tonality HZ      tonality limit (mặc định tắt)
//   --block MS --interval MS   tự chọn cấu hình STFT (mặc định presetDefault 120/30 ms)
//   --cheaper          presetCheaper (100/40 ms)
#include "spike/measure/StretchBench.h"
#include "spike/measure/WavIO.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using le::spike::StretchBench;

namespace {

void usage() {
    std::fprintf(stderr,
        "usage: le-stretch-bench in.wav outdir [--formant | --no-formant] [--base-hz F] [--tonality HZ]\n"
        "                        [--block MS --interval MS] [--cheaper]\n"
        "  Mặc định chạy cả formant off và on → 26 file WAV float32 trong outdir.\n");
}

bool parseDouble(const char* s, double& out) {
    char* end = nullptr;
    out = std::strtod(s, &end);
    return end != s && *end == '\0';
}

void printResult(const char* label, const StretchBench::Result& r, double seconds) {
    std::printf("\n[%s]\n", label);
    std::printf("  zone  nửa cung   thời gian (ms)   peak    RMS so với gốc\n");
    for (size_t z = 0; z < r.semitones.size(); ++z)
        std::printf("  z%02zu    %+4d      %8.1f      %6.3f   %+6.1f dB%s\n", z, r.semitones[z], r.msPerZone[z],
                    static_cast<double>(r.peakPerZone[z]), static_cast<double>(r.rmsDbPerZone[z] - r.inputRmsDb),
                    r.peakPerZone[z] > 1.0f ? "  (peak > 1.0: sẽ clip nếu xuất PCM)" : "");
    std::printf("  TỔNG render: %.1f ms cho %zu zone, mẫu %.2f s  (setup %.1f ms, ghi file %.1f ms)\n",
                r.msTotal, r.semitones.size(), seconds, r.msSetup, r.msWrite);
    std::printf("  Quy về mẫu 4 giây: ~%.1f ms  (DoD iPad 8: < 3000 ms)\n", seconds > 0 ? r.msTotal * 4.0 / seconds : 0.0);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 1;
    }
    const std::string inPath = argv[1];
    const std::string outDir = argv[2];
    bool runPlain = true, runFormant = true;
    StretchBench::Config base;

    for (int i = 3; i < argc; ++i) {
        const char* a = argv[i];
        const bool hasNext = i + 1 < argc;
        if (std::strcmp(a, "--formant") == 0) {
            runPlain = false; runFormant = true;
        } else if (std::strcmp(a, "--no-formant") == 0) {
            runPlain = true; runFormant = false;
        } else if (std::strcmp(a, "--cheaper") == 0) {
            base.cheaper = true;
        } else if (std::strcmp(a, "--base-hz") == 0 && hasNext && parseDouble(argv[i + 1], base.formantBaseHz)) {
            ++i;
        } else if (std::strcmp(a, "--tonality") == 0 && hasNext && parseDouble(argv[i + 1], base.tonalityLimitHz)) {
            ++i;
        } else if (std::strcmp(a, "--block") == 0 && hasNext && parseDouble(argv[i + 1], base.blockMs)) {
            ++i;
        } else if (std::strcmp(a, "--interval") == 0 && hasNext && parseDouble(argv[i + 1], base.intervalMs)) {
            ++i;
        } else {
            std::fprintf(stderr, "tham số không hợp lệ: %s\n", a);
            usage();
            return 1;
        }
    }
    if ((base.blockMs > 0.0) != (base.intervalMs > 0.0)) {
        std::fprintf(stderr, "--block và --interval phải đi cùng nhau\n");
        return 1;
    }

    std::vector<float> mono;
    double sr = 0.0;
    std::string err;
    if (!le::spike::readAudioMono(inPath, mono, sr, &err)) {
        std::fprintf(stderr, "lỗi đọc input: %s\n", err.c_str());
        return 2;
    }
    const double seconds = static_cast<double>(mono.size()) / sr;
    std::printf("input: %s  (%.0f Hz, %.2f s, %zu sample, đã trộn về mono)\n", inPath.c_str(), sr, seconds, mono.size());

    int filesWritten = 0;
    for (int variant = 0; variant < 2; ++variant) {
        const bool formant = variant == 1;
        if ((formant && !runFormant) || (!formant && !runPlain)) continue;
        StretchBench::Config cfg = base;
        cfg.formant = formant;
        cfg.outDir = outDir;
        const auto r = StretchBench::run(mono.data(), static_cast<int64_t>(mono.size()), sr, cfg);
        if (!r.ok) {
            std::fprintf(stderr, "lỗi: %s\n", r.error.c_str());
            return 2;
        }
        if (variant == 0 || !runPlain)
            std::printf("STFT: block %d, interval %d sample; latency in %d / out %d sample\n",
                        r.blockSamples, r.intervalSamples, r.inputLatency, r.outputLatency);
        printResult(formant ? "formant ON" : "formant OFF", r, seconds);
        filesWritten += static_cast<int>(r.files.size());
    }
    std::printf("\nĐã ghi %d file WAV vào %s\n", filesWritten, outDir.c_str());
    std::printf("Tên file: zNN_±KKst_{plain|formant}.wav  (z00 = -18 … z06 = gốc … z12 = +18)\n");
    return 0;
}
