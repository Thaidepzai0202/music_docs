// P3-18/19 (lõi) — ExportWriter: WAV 24-bit TPDF (sai số ≤ 1.5 LSB, trung bình 0, RMS ≈ 0.5 LSB), đúng độ dài /
// sample rate, deterministic, kẹp khi vượt 0 dBFS, ghi an toàn qua .tmp; M4A/AAC 256k đọc lại được bằng JUCE
// (CoreAudioFormat), độ dài lệch ≤ 1 frame AAC, nội dung giống nguồn.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "io/AudioFileIO.h"
#include "io/ExportWriter.h"
#include "le/engine_api.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <vector>

using le::dsp::AudioData;
using namespace le::io;

namespace {
constexpr double kPi = 3.14159265358979323846;

struct TempDir {
    std::filesystem::path path;
    TempDir() {
        std::random_device rd;
        char name[64];
        std::snprintf(name, sizeof(name), "le-export-test-%08x%08x", rd(), rd());
        path = std::filesystem::temp_directory_path() / name;
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    std::string file(const char* n) const { return (path / n).string(); }
};

std::shared_ptr<AudioData> music(int channels, int64_t n, double sr, float amp = 0.9f) {
    auto d = std::make_shared<AudioData>(channels, n, sr);
    std::mt19937 rng(5);
    std::uniform_real_distribution<float> u(-0.05f, 0.05f);
    for (int c = 0; c < channels; ++c)
        for (int64_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / sr;
            d->writePointer(c)[i] = static_cast<float>((amp - 0.05) * (0.6 * std::sin(2 * kPi * (220.0 + 110.0 * c) * t) +
                                                                       0.4 * std::sin(2 * kPi * 1733.0 * t))) + u(rng);
        }
    return d;
}

std::vector<char> bytesOf(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
} // namespace

TEST_CASE("ExportWriter WAV 24-bit: đọc lại đúng độ dài/sample rate, sai số TPDF ≤ 1.5 LSB, trung bình 0, RMS ≈ 0.5 LSB",
          "[io][export]") {
    TempDir tmp;
    const auto src = music(2, 72000, 48000.0);
    const std::string path = tmp.file("out/scene.wav");          // thư mục con chưa có → tự tạo
    const ExportResult r = exportAudio(path, *src);
    INFO(r.message);
    REQUIRE(r.ok);
    CHECK(r.frames == 72000);
    CHECK(r.clippedSamples == 0);
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
    CHECK(std::filesystem::file_size(path) == 44u + 72000u * 2u * 3u);

    const DecodeResult d = decodeAudioFile(path);
    INFO(d.message);
    REQUIRE(d.error == LE_OK);
    REQUIRE(d.data->numFrames() == 72000);
    CHECK(d.data->sampleRate() == Catch::Approx(48000.0));
    REQUIRE(d.data->numChannels() == 2);
    double maxErr = 0.0, sum = 0.0, sq = 0.0;
    int64_t count = 0;
    for (int c = 0; c < 2; ++c)
        for (int64_t i = 0; i < 72000; ++i) {
            const double e = (static_cast<double>(d.data->channel(c)[i]) - src->channel(c)[i]) * 8388608.0;   // theo LSB
            maxErr = std::max(maxErr, std::fabs(e));
            sum += e;
            sq += e * e;
            ++count;
        }
    const double mean = sum / static_cast<double>(count), rms = std::sqrt(sq / static_cast<double>(count));
    INFO("sai số: max " << maxErr << " LSB, trung bình " << mean << ", RMS " << rms);
    CHECK(maxErr <= 1.5 + 1e-6);          // dither tam giác ±1 LSB + làm tròn ±0.5 LSB
    CHECK(std::fabs(mean) < 0.02);         // không lệch DC
    CHECK(rms == Catch::Approx(0.5).margin(0.03));   // TPDF lý thuyết: √(1/6 + 1/12) = 0.5 LSB
}

TEST_CASE("ExportWriter WAV: mono 44.1 kHz số frame lẻ, kẹp khi vượt 0 dBFS, deterministic theo seed", "[io][export]") {
    TempDir tmp;
    auto src = music(1, 44101, 44100.0);
    src->writePointer(0)[10] = 1.5f;
    src->writePointer(0)[11] = -2.0f;
    const std::string a = tmp.file("a.wav"), b = tmp.file("b.wav"), c = tmp.file("c.wav");
    const ExportResult r = exportAudio(a, *src);
    REQUIRE(r.ok);
    CHECK(r.clippedSamples == 2);
    const DecodeResult d = decodeAudioFile(a);
    REQUIRE(d.error == LE_OK);
    CHECK(d.data->numFrames() == 44101);                        // có byte đệm chẵn cuối chunk
    CHECK(d.data->sampleRate() == Catch::Approx(44100.0));
    CHECK(d.data->channel(0)[10] == Catch::Approx(8388607.0 / 8388608.0));
    CHECK(d.data->channel(0)[11] == Catch::Approx(-1.0));

    REQUIRE(exportAudio(b, *src).ok);
    ExportOptions other;
    other.ditherSeed = 12345;
    REQUIRE(exportAudio(c, *src, other).ok);
    CHECK(bytesOf(a) == bytesOf(b));                             // cùng seed → cùng từng byte
    CHECK(bytesOf(a) != bytesOf(c));
}

TEST_CASE("ExportWriter: ghi an toàn — chưa finish thì không có file thật, file cũ giữ nguyên", "[io][export]") {
    TempDir tmp;
    const std::string path = tmp.file("jam.wav");
    { std::ofstream(path) << "file cũ"; }
    const auto src = music(2, 4800, 48000.0);
    const float* ch[2] = {src->channel(0), src->channel(1)};
    {
        ExportWriter w;
        REQUIRE(w.open(path, 48000.0, 2) == LE_OK);
        REQUIRE(std::filesystem::exists(path + ".tmp"));
        REQUIRE(w.write(ch, 4800) == LE_OK);
        CHECK(w.framesWritten() == 4800);
    }                                                            // huỷ khi chưa finish → abort
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
    CHECK(bytesOf(path) == std::vector<char>{'f', 'i', 'l', 'e', ' ', 'c', '\xC5', '\xA9'});   // "file cũ" (UTF-8)

    ExportWriter w;
    REQUIRE(w.open(path, 48000.0, 2) == LE_OK);
    REQUIRE(w.write(ch, 4800) == LE_OK);
    REQUIRE(w.finish() == LE_OK);
    CHECK(decodeAudioFile(path).data->numFrames() == 4800);     // thay thế file cũ sau khi xong

    ExportWriter bad;
    CHECK(bad.open("", 48000.0, 2) == LE_ERR_INVALID_ARG);
    CHECK(bad.open(tmp.file("x.wav"), 0.0, 2) == LE_ERR_INVALID_ARG);
    CHECK(bad.open(tmp.file("x.wav"), 48000.0, 3) == LE_ERR_INVALID_ARG);
    CHECK(bad.write(ch, 10) == LE_ERR_INVALID_ARG);             // chưa open
    CHECK(bad.finish() == LE_ERR_INVALID_ARG);
    CHECK(std::string(exportExtension(ExportFormat::M4aAac)) == "m4a");
}

#if defined(__APPLE__)
TEST_CASE("ExportWriter M4A/AAC 256k: JUCE đọc lại được, độ dài lệch ≤ 1 frame AAC, nội dung giống nguồn", "[io][export][aac]") {
    TempDir tmp;
    for (int channels : {2, 1}) {
        CAPTURE(channels);
        const auto src = music(channels, 96000, 48000.0, 0.7f);
        const std::string path = tmp.file(channels == 2 ? "scene.m4a" : "mono.m4a");
        ExportOptions opt;
        opt.format = ExportFormat::M4aAac;
        const ExportResult r = exportAudio(path, *src, opt);
        INFO(r.message);
        REQUIRE(r.ok);
        CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
        CHECK(r.bitrate > 0);
        CHECK(r.bitrate <= 256000);
        if (channels == 2) CHECK(r.bitrate == 256000);
        WARN("AAC " << channels << " kênh: bitrate thực " << r.bitrate << ", file " << std::filesystem::file_size(path) << " byte");

        const DecodeResult d = decodeAudioFile(path);
        INFO(d.message);
        REQUIRE(d.error == LE_OK);
        CHECK(d.data->sampleRate() == Catch::Approx(48000.0));
        CHECK(d.data->numChannels() == channels);
        CHECK(std::llabs(d.data->numFrames() - 96000) <= 1024);

        // Tìm độ lệch (do priming) trong ±2200 sample rồi so nội dung: tương quan > 0.99
        const float* x = src->channel(0);
        const float* y = d.data->channel(0);
        const int64_t m = std::min<int64_t>(96000, d.data->numFrames()) - 4400;
        double best = -1.0;
        for (int lag = -2200; lag <= 2200; ++lag) {
            double sxy = 0, sxx = 0, syy = 0;
            for (int64_t i = 2200; i < m; i += 4) {
                const double a = x[i], b = y[i + lag];
                sxy += a * b; sxx += a * a; syy += b * b;
            }
            best = std::max(best, sxy / std::sqrt(sxx * syy + 1e-30));
        }
        INFO("tương quan tốt nhất " << best);
        CHECK(best > 0.99);
    }
}
#endif

// Tên project / file tiếng Việt + emoji (UTF-8, đi qua std::filesystem và CFURL): ghi an toàn + đọc lại được.
TEST_CASE("ExportWriter: đường dẫn có dấu / emoji — WAV và M4A ghi, đổi tên .tmp, đọc lại đúng", "[io][export][utf8]") {
    TempDir tmp;
    const std::filesystem::path dir = tmp.path / "Dự án 🎵" / "Bài hát – thử";
    std::filesystem::create_directories(dir);
    const auto src = music(2, 48000, 48000.0, 0.5f);
    for (const ExportFormat f : {ExportFormat::Wav24, ExportFormat::M4aAac}) {
        const std::string path = (dir / (std::string("xuất bản 1 🥁.") + exportExtension(f))).string();
        CAPTURE(path);
        ExportOptions opt;
        opt.format = f;
        const ExportResult r = exportAudio(path, *src, opt);
        INFO(r.message);
        REQUIRE(r.ok);
        CHECK(std::filesystem::exists(std::filesystem::path(path)));
        CHECK_FALSE(std::filesystem::exists(std::filesystem::path(path + ".tmp")));
        const DecodeResult d = decodeAudioFile(path);
        INFO(d.message);
        REQUIRE(d.error == LE_OK);
        CHECK(d.data->numChannels() == 2);
        CHECK(std::llabs(d.data->numFrames() - 48000) <= 1024);
    }
}
