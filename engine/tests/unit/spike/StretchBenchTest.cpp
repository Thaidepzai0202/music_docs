// P0-09 — StretchBench: 13 zone, độ dài đúng, thẳng hàng, đúng cao độ, formant on/off có tác dụng.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (deterministic / đọc lại float32 phải đúng từng bit).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_test_macros.hpp>

#include "render/Yin.h"
#include "spike/measure/StretchBench.h"
#include "spike/measure/WavIO.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using le::spike::StretchBench;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSr = 48000.0;

std::vector<float> sineAm(double hz, int64_t n) {
    std::vector<float> x(static_cast<size_t>(n));
    for (int64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kSr;
        x[static_cast<size_t>(i)] = static_cast<float>(0.5 * std::sin(2 * kPi * hz * t) * (0.7 + 0.3 * std::sin(2 * kPi * 3 * t)));
    }
    return x;
}

// "Nguyên âm" tổng hợp: hoạ âm của f0, biên độ theo đường bao có 2 formant (800 Hz, 2500 Hz).
double vowelEnvelope(double f) {
    return std::exp(-std::pow((f - 800.0) / 250.0, 2)) + 0.5 * std::exp(-std::pow((f - 2500.0) / 300.0, 2));
}
std::vector<float> vowel(double f0, int64_t n) {
    std::vector<float> x(static_cast<size_t>(n), 0.0f);
    for (int h = 1; f0 * h < 6000.0; ++h) {
        const double a = 0.3 * vowelEnvelope(f0 * h);
        for (int64_t i = 0; i < n; ++i)
            x[static_cast<size_t>(i)] += static_cast<float>(a * std::sin(2 * kPi * f0 * h * static_cast<double>(i) / kSr + 0.7 * h));
    }
    return x;
}

double nullDb(const std::vector<float>& ref, const std::vector<float>& y, size_t a, size_t b) {
    double e = 0.0, d = 0.0;
    for (size_t i = a; i < b; ++i) {
        e += static_cast<double>(ref[i]) * ref[i];
        d += static_cast<double>(y[i] - ref[i]) * (y[i] - ref[i]);
    }
    return 10.0 * std::log10((d + 1e-20) / (e + 1e-20));
}

// Biên độ DFT (cửa sổ Hann) tại tần số f, frame [start, start + N).
double dftMag(const std::vector<float>& x, size_t start, int N, double f) {
    // Phasor quay dần (nhân số phức) thay cho cos/sin mỗi sample → nhanh hơn nhiều.
    const double dw = 2 * kPi / (N - 1), df = -2 * kPi * f / kSr;
    const double cwr = std::cos(dw), cwi = std::sin(dw), cfr = std::cos(df), cfi = std::sin(df);
    double wr = 1.0, wi = 0.0;   // e^{i·dw·k}: cửa sổ Hann = 0.5 − 0.5·Re
    double pr = 1.0, pi = 0.0;   // e^{i·df·k}
    double re = 0.0, im = 0.0;
    for (int k = 0; k < N; ++k) {
        const double v = (0.5 - 0.5 * wr) * x[start + static_cast<size_t>(k)];
        re += v * pr;
        im += v * pi;
        const double nwr = wr * cwr - wi * cwi; wi = wr * cwi + wi * cwr; wr = nwr;
        const double npr = pr * cfr - pi * cfi; pi = pr * cfi + pi * cfr; pr = npr;
    }
    return std::sqrt(re * re + im * im);
}

// Tần số của đỉnh phổ mạnh nhất trong khoảng expected·(1 ± 3%), quét lưới 0.02%.
double peakHzNear(const std::vector<float>& x, size_t start, double expected) {
    const int N = 32768;
    double bestF = 0.0, bestM = -1.0;
    for (double d = -0.03; d <= 0.03; d += 0.0002) {
        const double f = expected * (1.0 + d);
        const double m = dftMag(x, start, N, f);
        if (m > bestM) { bestM = m; bestF = f; }
    }
    return bestF;
}

// Trọng tâm phổ (Hz) trong 50..8000 Hz của một frame 8192 sample có cửa sổ Hann.
double spectralCentroid(const std::vector<float>& x, size_t start) {
    double num = 0.0, den = 0.0;
    for (double f = 50.0; f <= 8000.0; f += 20.0) {
        const double mag = dftMag(x, start, 8192, f);
        num += f * mag;
        den += mag;
    }
    return num / den;
}
} // namespace

TEST_CASE("StretchBench: 13 zone từ -18 tới +18, output dài đúng bằng input", "[spike][stretch]") {
    const auto in = sineAm(440.0, 48000);
    StretchBench::Config cfg;
    cfg.keepAudio = true;
    const auto r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);

    REQUIRE(r.ok);
    REQUIRE(r.semitones.size() == 13);
    CHECK(r.semitones.front() == -18);
    CHECK(r.semitones[6] == 0);
    CHECK(r.semitones.back() == 18);
    REQUIRE(r.msPerZone.size() == 13);
    REQUIRE(r.audio.size() == 13);
    CHECK(r.files.empty());           // không có outDir → không ghi file
    CHECK(r.msTotal > 0.0);
    for (const auto& zone : r.audio) {
        REQUIRE(zone.size() == in.size());
        for (float v : zone) REQUIRE(std::isfinite(v));
    }
}

TEST_CASE("StretchBench: zone 0 nửa cung gần như trùng input (thẳng hàng, không trễ)", "[spike][stretch]") {
    const auto in = sineAm(330.0, 48000);
    StretchBench::Config cfg;
    cfg.semitoneMin = cfg.semitoneMax = 0;
    cfg.keepAudio = true;
    const auto r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    REQUIRE(r.ok);
    const auto& y = r.audio.at(0);
    CHECK(nullDb(in, y, 0, 2880) < -30.0);                       // đầu: không có pre-roll/lệch
    CHECK(nullDb(in, y, 10000, 38000) < -60.0);                  // giữa
    CHECK(nullDb(in, y, in.size() - 2880, in.size()) < -30.0);  // đuôi: flush đúng cách
}

TEST_CASE("StretchBench: cao độ từng zone đúng 440·2^(k/12)", "[spike][stretch]") {
    // Formant off: sine trần không có "đường bao phổ" nên bù formant trên sine là vô nghĩa
    // (năng lượng bị dồn sang chỗ khác). Formant on được kiểm cao độ bằng Yin trên nguyên âm (YinTest).
    // Sine biên độ KHÔNG đổi: tremolo tạo dải biên ±3 Hz làm lệch đỉnh phổ ở zone thấp.
    //
    // GIỚI HẠN ĐÃ BIẾT của Signalsmith (xem tools/docs/signalsmith-notes.md §4): phase vocoder chuyển
    // độ lệch tần số so với tâm bin sang output mà không nhân tỉ lệ, nên cao độ lệch ~1 Hz tuyệt đối.
    // Đo ở P0-09: preset mặc định (block 120 ms) lệch tới ~13 cent ở zone -18 của 440 Hz;
    // block 200/50 ms còn < 8 cent. Ngưỡng dưới đây khoá lại mức hiện tại để phát hiện thoái lui.
    std::vector<float> in(4 * 48000);
    for (size_t i = 0; i < in.size(); ++i) in[i] = static_cast<float>(0.5 * std::sin(2 * kPi * 440.0 * static_cast<double>(i) / kSr));

    struct Case { double blockMs, intervalMs, maxCents; };
    for (const Case c : {Case{0.0, 0.0, 15.0}, Case{200.0, 50.0, 8.0}}) {
        CAPTURE(c.blockMs);
        StretchBench::Config cfg;
        cfg.keepAudio = true;
        cfg.blockMs = c.blockMs;
        cfg.intervalMs = c.intervalMs;
        const auto r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
        REQUIRE(r.ok);
        for (size_t z = 0; z < r.semitones.size(); ++z) {
            const int k = r.semitones[z];
            CAPTURE(k);
            const double expected = 440.0 * std::pow(2.0, k / 12.0);
            const double got = peakHzNear(r.audio[z], 40000, expected);
            CHECK(std::fabs(1200.0 * std::log2(got / expected)) < c.maxCents);
        }
    }
}

TEST_CASE("StretchBench: formant on giữ đường bao phổ, formant off thì đường bao trượt theo cao độ", "[spike][stretch]") {
    const double f0 = 150.0;
    const auto in = vowel(f0, 48000);
    const double c0 = spectralCentroid(in, 20000);

    StretchBench::Config cfg;
    cfg.semitoneMin = cfg.semitoneMax = 12;
    cfg.keepAudio = true;
    cfg.formantBaseHz = f0;

    cfg.formant = false;
    const auto plain = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    cfg.formant = true;
    const auto keep = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    REQUIRE(plain.ok);
    REQUIRE(keep.ok);

    const double cPlain = spectralCentroid(plain.audio[0], 20000);
    const double cKeep = spectralCentroid(keep.audio[0], 20000);
    INFO("centroid gốc " << c0 << " Hz, +12 plain " << cPlain << " Hz, +12 formant " << cKeep << " Hz");
    CHECK(cPlain > 1.5 * c0);              // formant off: đường bao bị kéo lên (lý tưởng 2×)
    CHECK(cKeep < 1.2 * c0);               // formant on: gần như giữ nguyên
    CHECK(cKeep > 0.8 * c0);
}

TEST_CASE("StretchBench + Yin: mọi zone (formant on/off) của nguyên âm ra đúng nốt", "[spike][stretch][yin]") {
    // Nguyên âm G3 (196 Hz = nốt 55). Zone k phải ra nốt 55 + k. Sai số cent bị giới hạn bởi Signalsmith
    // (notes §4: preset mặc định lệch tới ~22 cent), nên ở đây chỉ khoá "đúng nốt" và < 30 cent.
    const auto in = vowel(196.0, 2 * 48000);
    le::Yin yin;
    for (bool formant : {false, true}) {
        CAPTURE(formant);
        StretchBench::Config cfg;
        cfg.formant = formant;
        cfg.formantBaseHz = 196.0;
        cfg.keepAudio = true;
        const auto r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
        REQUIRE(r.ok);
        for (size_t z = 0; z < r.semitones.size(); ++z) {
            const int k = r.semitones[z];
            CAPTURE(k);
            const auto p = yin.analyze(r.audio[z].data(), static_cast<int64_t>(r.audio[z].size()), kSr);
            REQUIRE(p.ok);
            const double cents = 1200.0 * std::log2(p.hz / (196.0 * std::pow(2.0, k / 12.0)));
            INFO("Yin: nốt " << p.rootNote << ", lệch " << cents << " cent, confidence " << p.confidence);
            CHECK(p.rootNote == 55 + k);
            CHECK(std::fabs(cents) < 30.0);
        }
    }
}

TEST_CASE("StretchBench: output lặp lại y hệt giữa hai lần chạy (deterministic)", "[spike][stretch]") {
    const auto in = vowel(220.0, 24000);
    StretchBench::Config cfg;
    cfg.semitoneMin = -6;
    cfg.semitoneMax = 6;
    cfg.semitoneStep = 6;
    cfg.formant = true;
    cfg.keepAudio = true;
    const auto a = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    const auto b = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    REQUIRE(a.ok);
    REQUIRE(b.ok);
    CHECK(a.audio == b.audio);
}

TEST_CASE("StretchBench: huỷ, progress, input không hợp lệ", "[spike][stretch]") {
    const auto in = sineAm(440.0, 12000);
    StretchBench::Config cfg;

    std::atomic<bool> cancel{true};
    cfg.cancel = &cancel;
    auto r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    CHECK_FALSE(r.ok);
    CHECK(r.cancelled);
    CHECK(r.msPerZone.empty());

    cancel = false;
    std::atomic<float> progress{0.0f};
    cfg.progress = &progress;
    r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    CHECK(r.ok);
    CHECK(progress.load() == 1.0f);

    // Danh sách nửa cung cụ thể (le_call "spike.stretchBench" truyền vào): giữ đúng thứ tự
    StretchBench::Config listCfg;
    listCfg.semitones = {12, -12, 0};
    listCfg.keepAudio = true;
    r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, listCfg);
    REQUIRE(r.ok);
    CHECK(r.semitones == std::vector<int>{12, -12, 0});
    CHECK(r.audio.size() == 3);
    listCfg.semitones = {60};
    CHECK_FALSE(StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, listCfg).ok);
    StretchBench::Config badRange;
    badRange.semitoneStep = 0;
    CHECK_FALSE(StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, badRange).ok);

    CHECK_FALSE(StretchBench::run(nullptr, 100, kSr, {}).ok);
    CHECK_FALSE(StretchBench::run(in.data(), 0, kSr, {}).ok);
    CHECK_FALSE(StretchBench::run(in.data(), 100, 0.0, {}).ok);

    // Input ngắn hơn độ trễ của Signalsmith (~60 ms) vẫn chạy, output đúng độ dài
    StretchBench::Config shortCfg;
    shortCfg.keepAudio = true;
    r = StretchBench::run(in.data(), 500, kSr, shortCfg);
    REQUIRE(r.ok);
    CHECK(r.audio.at(0).size() == 500);
}

// Thư mục tạm RIÊNG cho mỗi lượt chạy: nhiều build (mac-debug/tsan/asan, nhiều agent) có thể chạy
// le-tests cùng lúc. create_directory chỉ trả true khi chính process này vừa tạo nó → không trùng.
// Tự xoá khi ra khỏi scope, kể cả khi REQUIRE fail giữa chừng.
namespace {
struct UniqueTempDir {
    std::filesystem::path path;
    explicit UniqueTempDir(const std::string& prefix) {
        std::random_device rd;
        for (;;) {
            char name[96];
            std::snprintf(name, sizeof(name), "%s-%08x%08x", prefix.c_str(), rd(), rd());
            path = std::filesystem::temp_directory_path() / name;
            if (std::filesystem::create_directory(path)) break;
        }
    }
    ~UniqueTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    UniqueTempDir(const UniqueTempDir&) = delete;
    UniqueTempDir& operator=(const UniqueTempDir&) = delete;
};
} // namespace

TEST_CASE("StretchBench: ghi 13 file WAV float32, đọc lại đúng nội dung", "[spike][stretch][wav]") {
    const UniqueTempDir tmp("le-stretch-bench-test");
    const auto dir = tmp.path / "out";   // thư mục con chưa có → kiểm luôn việc tự tạo thư mục

    const auto in = sineAm(440.0, 24000);
    StretchBench::Config cfg;
    cfg.formant = true;
    cfg.outDir = dir.string();
    cfg.keepAudio = true;
    const auto r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
    REQUIRE(r.ok);
    REQUIRE(r.files.size() == 13);
    CHECK(r.files[0].find("z00_-18st_formant.wav") != std::string::npos);
    CHECK(r.files[6].find("z06_+00st_formant.wav") != std::string::npos);
    CHECK(r.files[12].find("z12_+18st_formant.wav") != std::string::npos);

    for (size_t z = 0; z < r.files.size(); ++z) {
        CAPTURE(r.files[z]);
        std::vector<float> back;
        double sr = 0.0;
        std::string err;
        REQUIRE(le::spike::readAudioMono(r.files[z], back, sr, &err));
        CHECK(sr == kSr);
        REQUIRE(back.size() == in.size());
        CHECK(back == r.audio[z]);   // float32 → không mất dữ liệu
    }
}

TEST_CASE("StretchBench: thời gian 13 zone cho mẫu 4 giây (thông tin)", "[spike][stretch][.perf]") {
    // Chạy tay: le-tests "[.perf]". DoD trên iPad 8: < 3 giây cho 13 zone.
    const auto in = vowel(196.0, 4 * 48000);
    for (bool formant : {false, true}) {
        StretchBench::Config cfg;
        cfg.formant = formant;
        const auto r = StretchBench::run(in.data(), static_cast<int64_t>(in.size()), kSr, cfg);
        REQUIRE(r.ok);
        WARN("formant " << (formant ? "on " : "off") << ": tổng " << r.msTotal << " ms, setup " << r.msSetup << " ms");
    }
}
