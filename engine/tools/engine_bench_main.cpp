// le-engine-bench — đo thời gian process() từng block của engine với kịch bản tải chuẩn (08 §1; chuẩn bị P1-34 / P3-21).
// [main] (CLI). Chạy kịch bản qua ScenarioRunner (tức đúng le_send / le_call + sim offline như app), runner đo
// steady_clock quanh MỖI lần render 1 block (RunOptions::blockTimesNs, không tính pump / JSON / kiểm expect).
//
//   le-engine-bench [--scenario file.json] [--blocks 128,256] [--seconds 20] [--warmup 0.5] [--max-p99 PCT]
//
// In bảng: trung bình / p50 / p99 / max của (thời gian xử lý / thời lượng block) · 100 %.
// --max-p99 PCT: mã thoát 1 nếu p99 của bất kỳ block size nào vượt PCT (bắt hồi quy hiệu năng). Lỗi chạy → mã 2.
// % của mỗi lần render = thời gian / thời lượng của CHÍNH nó (RunOptions::blockFrames): mảnh bị chia tại sự kiện
// giữa block không bị tính thấp.
// Số đo có ý nghĩa ở bản Release (preset mac-release). Số của máy chuẩn phải đo trên iPad 8.
// Máy đang bận (build song song…) làm p99/max nhảy: so sánh hồi quy nên chạy lúc máy rảnh, --seconds ≥ 20.
#include "sim/ScenarioRunner.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Stats {
    double mean = 0, p50 = 0, p99 = 0, max = 0;
    size_t count = 0;
};

Stats stats(std::vector<double> pct) {
    Stats s;
    s.count = pct.size();
    if (pct.empty()) return s;
    double sum = 0;
    for (double v : pct) sum += v;
    s.mean = sum / static_cast<double>(pct.size());
    std::sort(pct.begin(), pct.end());
    auto q = [&](double p) { return pct[std::min(pct.size() - 1, static_cast<size_t>(std::ceil(p * static_cast<double>(pct.size()))) - 1)]; };
    s.p50 = q(0.50);
    s.p99 = q(0.99);
    s.max = pct.back();
    return s;
}

std::vector<int> parseBlocks(const char* s) {
    std::vector<int> out;
    for (const auto& t : juce::StringArray::fromTokens(juce::String::fromUTF8(s), ",", "")) {
        const int b = t.getIntValue();
        if (b > 0) out.push_back(b);
    }
    return out;
}

void usage() {
    std::fprintf(stderr,
                 "usage: le-engine-bench [--scenario file.json] [--blocks 128,256] [--seconds 20] [--warmup 0.5] [--max-p99 PCT]\n");
}

} // namespace

int main(int argc, char** argv) {
    std::string scenario = std::string(LE_ENGINE_DIR) + "/tests/scenarios/standard_load.json";
    std::vector<int> blocks = {128, 256};
    double seconds = 20.0, warmup = 0.5, maxP99 = -1.0;
    for (int i = 1; i < argc; ++i) {
        const bool next = i + 1 < argc;
        if (std::strcmp(argv[i], "--scenario") == 0 && next) scenario = argv[++i];
        else if (std::strcmp(argv[i], "--blocks") == 0 && next) blocks = parseBlocks(argv[++i]);
        else if (std::strcmp(argv[i], "--seconds") == 0 && next) seconds = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--warmup") == 0 && next) warmup = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--max-p99") == 0 && next) maxP99 = std::atof(argv[++i]);
        else {
            usage();
            return 2;
        }
    }
    if (blocks.empty() || !(seconds > 0.0) || warmup < 0.0) {
        usage();
        return 2;
    }

    // Đọc kịch bản, thay độ dài render bằng --seconds (bỏ renderBeats / renderFrames / expect: chỉ đo)
    const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(scenario.c_str()));
    juce::var json = juce::JSON::parse(file);
    auto* obj = json.getDynamicObject();
    if (obj == nullptr) {
        std::fprintf(stderr, "không đọc được kịch bản: %s\n", file.getFullPathName().toRawUTF8());
        return 2;
    }
    obj->removeProperty("renderBeats");
    obj->removeProperty("renderFrames");
    obj->removeProperty("expect");
    obj->setProperty("renderSeconds", seconds);
    const std::string text = juce::JSON::toString(json).toStdString();
    const std::string baseDir = file.getParentDirectory().getParentDirectory().getFullPathName().toStdString();   // engine/tests

    std::printf("Kịch bản: %s  (%.1f s mỗi block size, bỏ %.2f s đầu)\n", file.getFileName().toRawUTF8(), seconds, warmup);
#ifndef NDEBUG
    std::printf("⚠️  Bản Debug: số đo chậm hơn Release nhiều. Đo thật bằng preset mac-release\n");
#endif
    std::printf("\n  block    ms/block   số block   TB %%    p50 %%    p99 %%    max %%\n");

    int exitCode = 0;
    for (int b : blocks) {
        std::vector<double> ns;
        std::vector<int> frames;
        le::sim::RunOptions opt;
        opt.blockSize = b;
        opt.baseDir = baseDir;
        opt.checkExpectations = false;
        opt.blockTimesNs = &ns;
        opt.blockFrames = &frames;   // n thật của từng lần render (runner chia block tại sự kiện giữa block)
        const auto r = le::sim::runScenarioJson(text, opt);
        if (!r.error.empty()) {
            std::fprintf(stderr, "lỗi chạy kịch bản (block %d): %s\n", b, r.error.c_str());
            return 2;
        }
        if (frames.size() != ns.size()) {
            std::fprintf(stderr, "runner trả %zu thời gian nhưng %zu độ dài block\n", ns.size(), frames.size());
            return 2;
        }
        const double blockNs = static_cast<double>(b) * 1.0e9 / r.sampleRate;
        const auto warmFrames = static_cast<long long>(warmup * r.sampleRate);
        std::vector<double> pct;
        long long elapsed = 0;
        size_t split = 0;
        for (size_t i = 0; i < ns.size(); ++i) {
            const int n = frames[i];
            if (n < b && i + 1 < ns.size()) ++split;
            const bool warm = elapsed >= warmFrames;
            elapsed += n;
            if (!warm || n <= 0) continue;
            pct.push_back(100.0 * ns[i] / (static_cast<double>(n) * 1.0e9 / r.sampleRate));   // % của CHÍNH block đó
        }
        if (pct.empty()) {
            std::fprintf(stderr, "không có block nào để đo (block %d: %zu block)\n", b, ns.size());
            return 2;
        }
        if (split > 0)
            std::printf("  (block %d: %zu block bị chia tại sự kiện — %% tính theo số frame thật của từng mảnh)\n", b, split);
        const Stats s = stats(pct);
        const bool over = maxP99 > 0.0 && s.p99 > maxP99;
        if (over) exitCode = 1;
        std::printf("  %5d   %8.3f   %8zu   %6.2f   %6.2f   %6.2f   %6.2f%s\n", b, blockNs / 1.0e6, s.count, s.mean, s.p50,
                    s.p99, s.max, over ? "   ← p99 VƯỢT ngưỡng" : "");
    }
    if (maxP99 > 0.0) std::printf("\nNgưỡng p99: %.2f %% → %s\n", maxP99, exitCode == 0 ? "ĐẠT" : "KHÔNG ĐẠT");
    return exitCode;
}
