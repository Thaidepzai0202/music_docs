// le-load-bench — P0-08 trên Mac: CPU của LoadGenerator theo số voice, render offline. [main]
//
//   le-load-bench [--seconds S] [--sr HZ]
//
// "CPU %" = thời gian xử lý 1 block / thời lượng block đó (giống CPU meter của engine, 08 §2).
// Số trên Mac chỉ để so sánh tương đối. Số thật để quyết định phải đo trên iPad 8 (le_read_state).
#include "spike/measure/LoadGenerator.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using le::spike::LoadGenerator;

int main(int argc, char** argv) {
    double seconds = 5.0, sr = 48000.0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) seconds = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--sr") == 0 && i + 1 < argc) sr = std::atof(argv[++i]);
        else {
            std::fprintf(stderr, "usage: le-load-bench [--seconds S] [--sr HZ]\n");
            return 1;
        }
    }
    if (seconds <= 0.0 || sr <= 0.0) return 1;

    std::printf("LoadGenerator offline, %.0f Hz, %.1f s mỗi ô (1 core, không có I/O thật)\n", sr, seconds);
#ifndef NDEBUG
    std::printf("⚠️  Bản Debug (-O0): chậm hơn Release ~4×. Đo bằng: scripts/build_engine_mac.sh mac-release\n");
#endif
    std::printf("   CPU đỉnh trên Mac bị nhiễu do thread này không phải thread real-time.\n\n");
    std::printf("  buffer  voices   CPU TB %%   CPU đỉnh %%\n");
    for (int block : {128, 256}) {
        for (int voices : {0, 16, 32, 64, 96, 128}) {
            LoadGenerator gen;
            gen.prepare(sr, block);
            gen.setVoices(voices);
            std::vector<float> l(static_cast<size_t>(block)), r(static_cast<size_t>(block));
            float* out[2] = {l.data(), r.data()};
            const double blockSec = block / sr;
            const int blocks = static_cast<int>(seconds / blockSec);
            double sum = 0.0, peak = 0.0;
            for (int b = 0; b < blocks; ++b) {
                std::fill(l.begin(), l.end(), 0.0f);
                std::fill(r.begin(), r.end(), 0.0f);
                const auto t0 = std::chrono::steady_clock::now();
                gen.processRt(out, 2, block);
                const double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                sum += dt;
                peak = std::max(peak, dt);
            }
            std::printf("  %6d  %6d   %8.3f   %9.3f\n", block, voices, 100.0 * sum / (blocks * blockSec),
                        100.0 * peak / blockSec);
        }
        std::printf("\n");
    }
    return 0;
}
