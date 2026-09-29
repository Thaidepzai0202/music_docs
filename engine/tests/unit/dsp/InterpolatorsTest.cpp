// Interpolators: Hermite 4 điểm / linear, đọc lố biên nhờ vùng đệm, đọc vòng lặp liền mạch.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (giá trị phải đúng chính xác, ví dụ 0 trong vùng đệm).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/AudioData.h"
#include "dsp/Interpolators.h"

#include <cmath>
#include <memory>

using namespace le::dsp;

namespace {
constexpr double kPi = 3.14159265358979323846;

std::shared_ptr<AudioData> sineData(double hz, double sr, int64_t n) {
    auto d = std::make_shared<AudioData>(1, n, sr);
    float* w = d->writePointer(0);
    for (int64_t i = 0; i < n; ++i) w[i] = static_cast<float>(std::sin(2 * kPi * hz * static_cast<double>(i) / sr));
    return d;
}
} // namespace

TEST_CASE("Interpolators: t = 0 trả đúng sample gốc; hàm tuyến tính được tái tạo chính xác", "[dsp][interp]") {
    CHECK(hermite4(0.0f, 3.0f, -1.5f, 7.0f, 2.0f) == Catch::Approx(-1.5f));
    CHECK(linear2(0.25f, 0.0f, 4.0f) == Catch::Approx(1.0f));

    auto d = std::make_shared<AudioData>(1, 64, 48000.0);
    for (int i = 0; i < 64; ++i) d->writePointer(0)[i] = static_cast<float>(i);   // đường thẳng x[i] = i
    for (double pos = 1.0; pos < 61.0; pos += 0.37) {
        CAPTURE(pos);
        CHECK(readHermite(d->channel(0), pos) == Catch::Approx(pos).margin(1e-4));
        CHECK(readLinear(d->channel(0), pos) == Catch::Approx(pos).margin(1e-4));
    }
}

TEST_CASE("Interpolators: Hermite sai số nhỏ hơn linear trên sine 2 kHz", "[dsp][interp]") {
    const double sr = 48000.0, hz = 2000.0;
    auto d = sineData(hz, sr, 4800);
    double errH = 0.0, errL = 0.0;
    for (double pos = 10.0; pos < 4700.0; pos += 0.731) {
        const double truth = std::sin(2 * kPi * hz * pos / sr);
        errH = std::max(errH, std::fabs(readHermite(d->channel(0), pos) - truth));
        errL = std::max(errL, std::fabs(readLinear(d->channel(0), pos) - truth));
    }
    INFO("sai số lớn nhất: Hermite " << errH << ", linear " << errL);
    CHECK(errH < 0.01);
    CHECK(errH < errL / 3.0);
}

TEST_CASE("Interpolators: đọc lố đầu/cuối nằm trong vùng đệm 0 (an toàn, êm dần về 0)", "[dsp][interp]") {
    auto d = std::make_shared<AudioData>(2, 16, 48000.0);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 16; ++i) d->writePointer(c)[i] = 1.0f;
    const AudioData& cd = *d;
    CHECK(cd.channel(0)[-AudioData::kPadFrames] == 0.0f);
    CHECK(cd.channel(1)[15 + AudioData::kPadFrames] == 0.0f);
    CHECK(readHermite(cd.channel(0), -1.0) == Catch::Approx(0.0f).margin(1e-6));   // dùng x[-2..1]
    CHECK(readHermite(cd.channel(0), 17.0) == Catch::Approx(0.0f).margin(1e-6));   // dùng x[16..19]
    CHECK(readHermite(cd.channel(0), 8.5) == Catch::Approx(1.0f));
    CHECK(cd.sampleAt(0, -100) == 0.0f);
    CHECK(cd.sampleAt(0, 1000) == 0.0f);
    CHECK(cd.channelOrMono(1) == cd.channel(1));

    auto mono = std::make_shared<AudioData>(1, 8, 44100.0);
    CHECK(mono->channelOrMono(1) == mono->channel(0));
    CHECK(mono->durationSeconds() == Catch::Approx(8.0 / 44100.0));
    CHECK(AudioData(5, 10, 48000.0).numChannels() == AudioData::kMaxChannels);   // kẹp 1..2
}

TEST_CASE("Interpolators: readHermiteLoop nối vòng liền mạch", "[dsp][interp]") {
    // Vòng [100, 100 + 480) chứa đúng 10 chu kỳ sine 1 kHz → đọc qua mép vòng phải khớp sine lý tưởng.
    const double sr = 48000.0, hz = 1000.0;
    auto d = sineData(hz, sr, 1000);
    const int64_t ls = 100, le = 580;
    double err = 0.0;
    for (double pos = static_cast<double>(le) - 3.0; pos < static_cast<double>(le); pos += 0.1) {
        const double truth = std::sin(2 * kPi * hz * pos / sr);
        err = std::max(err, std::fabs(readHermiteLoop(d->channel(0), pos, ls, le) - truth));
    }
    CHECK(err < 0.01);
    CHECK(floorToInt(-0.5) == -1);
    CHECK(floorToInt(-1.0) == -1);
    CHECK(floorToInt(2.99) == 2);
}
