// P1-24 — PeakBuilder: min/max đúng với sine và dữ liệu đã biết, 3 mức khớp nhau, maxPairs nhỏ
// → trả đúng số cặp, cache .peaks đọc lại đúng từng bit, file hỏng/lệch nguồn bị từ chối.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (peak phải đúng bằng giá trị sample, cache đúng từng bit).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "le/engine_api.h"
#include "render/PeakBuilder.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <vector>

using le::dsp::AudioData;
using le::render::Peaks;

namespace {
constexpr double kPi = 3.14159265358979323846;

std::shared_ptr<AudioData> ramp(int64_t n, int channels = 1) {
    auto d = std::make_shared<AudioData>(channels, n, 48000.0);
    for (int c = 0; c < channels; ++c)
        for (int64_t i = 0; i < n; ++i) d->writePointer(c)[i] = static_cast<float>(i) / static_cast<float>(n) * (c == 0 ? 1.0f : -1.0f);
    return d;
}

struct TempDir {
    std::filesystem::path path;
    TempDir() {
        std::random_device rd;
        char name[64];
        std::snprintf(name, sizeof(name), "le-peaks-test-%08x%08x", rd(), rd());
        path = std::filesystem::temp_directory_path() / name;
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};
} // namespace

TEST_CASE("PeakBuilder: sine 1 kHz → mọi điểm có min ≈ −0.5, max ≈ +0.5", "[render][peaks]") {
    auto d = std::make_shared<AudioData>(1, 48000, 48000.0);
    for (int i = 0; i < 48000; ++i) d->writePointer(0)[i] = 0.5f * static_cast<float>(std::sin(2 * kPi * 1000.0 * i / 48000.0));
    const Peaks p = le::render::buildPeaks(*d);
    CHECK(p.numFrames == 48000);
    CHECK(p.numPoints(0) == 188);        // ceil(48000 / 256)
    CHECK(p.numPoints(1) == 24);         // ceil(48000 / 2048)
    CHECK(p.numPoints(2) == 3);          // ceil(48000 / 16384)
    for (int lv = 0; lv < Peaks::kLevels; ++lv)
        for (int64_t k = 0; k < p.numPoints(lv); ++k) {
            CAPTURE(lv, k);
            CHECK(p.levels[static_cast<size_t>(lv)][static_cast<size_t>(2 * k)] == Catch::Approx(-0.5f).margin(0.01f));
            CHECK(p.levels[static_cast<size_t>(lv)][static_cast<size_t>(2 * k + 1)] == Catch::Approx(0.5f).margin(0.01f));
        }
}

TEST_CASE("PeakBuilder: giá trị chính xác, điểm cuối lẻ, gộp kênh, mức trên khớp tính thẳng", "[render][peaks]") {
    const int64_t n = 50000;                 // không chia hết 256/2048/16384
    auto d = ramp(n, 2);                     // kênh 0: 0 → +1, kênh 1: 0 → −1
    const Peaks p = le::render::buildPeaks(*d);
    for (int lv = 0; lv < Peaks::kLevels; ++lv) {
        const int spp = Peaks::kSamplesPerPoint[static_cast<size_t>(lv)];
        REQUIRE(p.numPoints(lv) == (n + spp - 1) / spp);
        for (int64_t k = 0; k < p.numPoints(lv); ++k) {
            const int64_t a = k * spp, b = std::min<int64_t>(n, a + spp) - 1;
            // min = kênh 1 ở frame cuối của đoạn, max = kênh 0 ở frame cuối (ramp đơn điệu)
            CAPTURE(lv, k);
            REQUIRE(p.levels[static_cast<size_t>(lv)][static_cast<size_t>(2 * k)] == d->channel(1)[b]);
            REQUIRE(p.levels[static_cast<size_t>(lv)][static_cast<size_t>(2 * k + 1)] == d->channel(0)[b]);
            (void) a;
        }
    }
    CHECK(p.numChannels == 2);
    CHECK(p.sampleRate == 48000.0);

    const Peaks empty = le::render::buildPeaks(AudioData(1, 0, 48000.0));
    CHECK(empty.numPoints(0) == 0);
    CHECK(empty.numPoints(2) == 0);
}

TEST_CASE("PeakBuilder: copyTo — maxPairs nhỏ hơn dữ liệu trả đúng số cặp; tham số sai → INVALID_ARG", "[render][peaks]") {
    const Peaks p = le::render::buildPeaks(*ramp(10000));
    REQUIRE(p.numPoints(0) == 40);
    std::vector<float> buf(200, -7.0f);
    CHECK(p.copyTo(0, buf.data(), 10) == 10);
    CHECK(buf[0] == p.levels[0][0]);
    CHECK(buf[19] == p.levels[0][19]);
    CHECK(buf[20] == -7.0f);                  // không ghi lố
    CHECK(p.copyTo(0, buf.data(), 100) == 40);   // xin nhiều hơn có → trả số có
    CHECK(p.copyTo(0, buf.data(), 5, 38) == 2);  // từ điểm 38 chỉ còn 2
    CHECK(buf[0] == p.levels[0][76]);
    CHECK(p.copyTo(0, buf.data(), 5, 40) == 0);
    CHECK(p.copyTo(2, buf.data(), 10) == 1);
    CHECK(p.copyTo(3, buf.data(), 10) == LE_ERR_INVALID_ARG);
    CHECK(p.copyTo(-1, buf.data(), 10) == LE_ERR_INVALID_ARG);
    CHECK(p.copyTo(0, nullptr, 10) == LE_ERR_INVALID_ARG);
    CHECK(p.copyTo(0, buf.data(), -1) == LE_ERR_INVALID_ARG);
}

TEST_CASE("PeakBuilder: cache .peaks ghi/đọc đúng từng bit, file hỏng hoặc lệch nguồn bị từ chối", "[render][peaks]") {
    TempDir tmp;
    const auto src = ramp(123457, 2);
    const Peaks p = le::render::buildPeaks(*src);
    const std::string path = (tmp.path / "cache" / "c_1.peaks").string();   // thư mục cache/ chưa có → tự tạo
    std::string err;
    REQUIRE(le::render::writePeaksFile(path, p, &err));
    CHECK_FALSE(std::filesystem::exists(path + ".tmp"));
    const auto size = std::filesystem::file_size(path);
    CHECK(size == 64u + 8u * static_cast<uintmax_t>(p.numPoints(0) + p.numPoints(1) + p.numPoints(2)));

    Peaks back;
    REQUIRE(le::render::readPeaksFile(path, back, &err, src->numFrames(), 48000.0));
    CHECK(back.numFrames == p.numFrames);
    CHECK(back.numChannels == 2);
    for (int lv = 0; lv < Peaks::kLevels; ++lv) CHECK(back.levels[static_cast<size_t>(lv)] == p.levels[static_cast<size_t>(lv)]);

    Peaks junk;
    CHECK_FALSE(le::render::readPeaksFile(path, junk, &err, 999));          // file nguồn đã đổi độ dài
    CHECK_FALSE(le::render::readPeaksFile(path, junk, &err, -1, 44100.0));  // đổi sample rate
    CHECK_FALSE(le::render::readPeaksFile((tmp.path / "khong-co.peaks").string(), junk, &err));

    auto corrupt = [&](size_t offset, char value) {
        const std::string bad = (tmp.path / "bad.peaks").string();
        std::filesystem::copy_file(path, bad, std::filesystem::copy_options::overwrite_existing);
        std::fstream f(bad, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(static_cast<std::streamoff>(offset));
        f.put(value);
        f.close();
        Peaks q;
        return le::render::readPeaksFile(bad, q, &err);
    };
    CHECK_FALSE(corrupt(0, 'X'));             // magic
    CHECK_FALSE(corrupt(4, 9));               // version
    CHECK_FALSE(corrupt(48, 1));              // numPoints
    CHECK(corrupt(100, 1));                   // chỉ đổi dữ liệu → header vẫn hợp lệ (không có checksum)

    const std::string truncated = (tmp.path / "short.peaks").string();
    std::filesystem::copy_file(path, truncated);
    std::filesystem::resize_file(truncated, size - 8);
    CHECK_FALSE(le::render::readPeaksFile(truncated, junk, &err));
    INFO(err);
}

TEST_CASE("PeakBuilder: huỷ giữa chừng trả rỗng; thời gian 60 s stereo (thông tin)", "[render][peaks]") {
    auto d = std::make_shared<AudioData>(2, 60 * 48000, 48000.0);
    std::atomic<bool> cancel{true};
    CHECK(le::render::buildPeaks(*d, &cancel).numFrames == 0);

    const auto t0 = std::chrono::steady_clock::now();
    const Peaks p = le::render::buildPeaks(*d);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CHECK(p.numPoints(0) == (60 * 48000 + 255) / 256);
    WARN("buildPeaks 60 s stereo: " << ms << " ms");
}
