// P3-01 — Yin: sine 110/220/440/880 Hz sai số < 5 cent, nhiễu trắng confidence < 0.3,
// giọng hát (tổng hợp + fixture thật nếu có) ra đúng nốt.
#include <catch2/catch_test_macros.hpp>

#include "render/Yin.h"
#include "spike/measure/WavIO.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <regex>
#include <string>
#include <vector>

using le::PitchEstimate;
using le::Yin;

namespace {
constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine(double hz, double sr, double seconds, double amp = 0.5) {
    std::vector<float> x(static_cast<size_t>(seconds * sr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = static_cast<float>(amp * std::sin(2 * kPi * hz * static_cast<double>(i) / sr));
    return x;
}

double centsError(float got, double expected) { return 1200.0 * std::log2(got / expected); }

// Tiếng "la" tổng hợp: nguồn thanh môn + bức xạ môi (hoạ âm giảm ~6 dB/oct, biên độ 1/h) qua 3 formant
// nguyên âm "a" (730/1090/2440 Hz), vibrato 5.5 Hz ±30 cent, attack/release, hơi thở (nhiễu −35 dB so với RMS).
std::vector<float> synthVoice(double f0, double sr, double seconds, uint32_t seed = 3) {
    const size_t n = static_cast<size_t>(seconds * sr);
    std::vector<float> x(n, 0.0f);
    auto formant = [](double f) {
        return std::exp(-std::pow((f - 730.0) / 120.0, 2)) + 0.6 * std::exp(-std::pow((f - 1090.0) / 150.0, 2)) +
               0.25 * std::exp(-std::pow((f - 2440.0) / 250.0, 2)) + 0.15;
    };
    double phase = 0.0, energy = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        const double vib = std::pow(2.0, 0.30 / 12.0 * std::sin(2 * kPi * 5.5 * t));
        const double f = f0 * vib;
        phase += 2 * kPi * f / sr;
        double s = 0.0;
        for (int h = 1; f0 * h < 5000.0; ++h) s += formant(f * h) / h * std::sin(h * phase);
        const double env = std::min({1.0, t / 0.05, (seconds - t) / 0.1});
        x[i] = static_cast<float>(env * s);
        energy += static_cast<double>(x[i]) * x[i];
    }
    // Chuẩn hoá về RMS −18 dBFS, rồi thêm hơi thở −35 dB so với tín hiệu
    const double gain = std::pow(10.0, -18.0 / 20.0) / std::sqrt(energy / static_cast<double>(n) + 1e-30);
    const double noiseRms = std::pow(10.0, (-18.0 - 35.0) / 20.0);
    std::mt19937 rng(seed);
    std::normal_distribution<double> gauss(0.0, 1.0);
    for (auto& v : x) v = static_cast<float>(v * gain + noiseRms * gauss(rng));
    return x;
}
} // namespace

TEST_CASE("Yin: sine 110/220/440/880 Hz sai số < 5 cent, đúng nốt, confidence cao", "[yin]") {
    struct Case { double hz; int note; };
    for (double sr : {48000.0, 44100.0}) {
        for (const Case c : {Case{110.0, 45}, Case{220.0, 57}, Case{440.0, 69}, Case{880.0, 81}}) {
            CAPTURE(sr, c.hz);
            Yin yin;
            const auto x = sine(c.hz, sr, 1.0);
            const PitchEstimate p = yin.analyze(x.data(), static_cast<int64_t>(x.size()), sr);
            REQUIRE(p.ok);
            CHECK(std::fabs(centsError(p.hz, c.hz)) < 5.0);
            CHECK(p.rootNote == c.note);
            CHECK(std::fabs(p.cents) < 5.0f);
            CHECK(p.confidence > 0.9f);
        }
    }
}

TEST_CASE("Yin: tần số lẻ và biên của dải đo", "[yin]") {
    Yin yin;
    struct Case { double hz; int note; double cents; };
    // 446 Hz = A4 +23.4 cent; 261.63 = C4; 55 Hz = A1 (gần mức thấp 50 Hz); 1400 Hz = F6 +3.8 cent (gần mức cao 1500 Hz)
    for (const Case c : {Case{446.0, 69, 23.44}, Case{261.63, 60, 0.0}, Case{55.0, 33, 0.0}, Case{1400.0, 89, 3.84}}) {
        CAPTURE(c.hz);
        const auto x = sine(c.hz, 48000.0, 1.0);
        const PitchEstimate p = yin.analyze(x.data(), static_cast<int64_t>(x.size()), 48000.0);
        REQUIRE(p.ok);
        CHECK(p.rootNote == c.note);
        CHECK(std::fabs(p.cents - c.cents) < 3.0);
    }
}

TEST_CASE("Yin: tín hiệu nhiều hoạ âm không bị lỗi quãng tám", "[yin]") {
    // Răng cưa 10 hoạ âm (1/h) và "xung vuông" chỉ hoạ âm lẻ ở 196 Hz (G3 = 55)
    for (bool oddOnly : {false, true}) {
        CAPTURE(oddOnly);
        std::vector<float> x(48000, 0.0f);
        for (int h = 1; h <= 10; ++h) {
            if (oddOnly && h % 2 == 0) continue;
            for (size_t i = 0; i < x.size(); ++i)
                x[i] += static_cast<float>(0.3 / h * std::sin(2 * kPi * 196.0 * h * static_cast<double>(i) / 48000.0));
        }
        Yin yin;
        const PitchEstimate p = yin.analyze(x.data(), static_cast<int64_t>(x.size()), 48000.0);
        REQUIRE(p.ok);
        CHECK(p.rootNote == 55);
        CHECK(std::fabs(centsError(p.hz, 196.0)) < 5.0);
    }
}

TEST_CASE("Yin: nhiễu trắng → confidence < 0.3; im lặng → không dò được", "[yin]") {
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> uni(-0.5f, 0.5f);
    std::vector<float> noise(48000);
    for (auto& v : noise) v = uni(rng);
    Yin yin;
    const PitchEstimate pn = yin.analyze(noise.data(), static_cast<int64_t>(noise.size()), 48000.0);
    CHECK(pn.confidence < 0.3f);
    CHECK(pn.analyzedFrames > 80);

    std::vector<float> gaussNoise(48000);
    std::normal_distribution<float> g(0.0f, 0.1f);
    for (auto& v : gaussNoise) v = g(rng);
    CHECK(yin.analyze(gaussNoise.data(), static_cast<int64_t>(gaussNoise.size()), 48000.0).confidence < 0.3f);

    std::vector<float> silence(48000, 0.0f);
    const PitchEstimate ps = yin.analyze(silence.data(), static_cast<int64_t>(silence.size()), 48000.0);
    CHECK_FALSE(ps.ok);
    CHECK(ps.confidence < 1e-6f);
    CHECK(ps.analyzedFrames == 0);

    CHECK_FALSE(yin.analyze(nullptr, 100, 48000.0).ok);
    CHECK_FALSE(yin.analyze(silence.data(), 0, 48000.0).ok);
}

TEST_CASE("Yin: giọng hát tổng hợp (vibrato, formant, hơi thở) ra đúng nốt", "[yin]") {
    struct Case { double f0; int note; };
    // G3 (nam), A3, E4, C5 (nữ)
    for (const Case c : {Case{196.0, 55}, Case{220.0, 57}, Case{329.63, 64}, Case{523.25, 72}}) {
        CAPTURE(c.f0);
        const auto x = synthVoice(c.f0, 48000.0, 2.0);
        Yin yin;
        const PitchEstimate p = yin.analyze(x.data(), static_cast<int64_t>(x.size()), 48000.0);
        REQUIRE(p.ok);
        CHECK(p.rootNote == c.note);
        CHECK(std::fabs(centsError(p.hz, c.f0)) < 10.0);   // vibrato ±30 cent: median phải về gần tâm
        CHECK(p.confidence > 0.6f);                          // ngưỡng hỏi người dùng ở 04 §8
    }
}

TEST_CASE("Yin: đoạn im lặng đầu/cuối không làm sai kết quả, buffer ngắn hơn 1 frame vẫn chạy", "[yin]") {
    auto x = synthVoice(220.0, 48000.0, 1.0);
    std::vector<float> padded(24000, 0.0f);
    padded.insert(padded.end(), x.begin(), x.end());
    padded.insert(padded.end(), 24000, 0.0f);
    Yin yin;
    const PitchEstimate p = yin.analyze(padded.data(), static_cast<int64_t>(padded.size()), 48000.0);
    REQUIRE(p.ok);
    CHECK(p.rootNote == 57);
    CHECK(p.analyzedFrames < p.totalFrames);   // frame im lặng bị bỏ qua
    CHECK(p.confidence > 0.6f);

    const auto shortSine = sine(440.0, 48000.0, 0.03);   // 1440 sample < 2048
    const PitchEstimate ps = yin.analyze(shortSine.data(), static_cast<int64_t>(shortSine.size()), 48000.0);
    CHECK(ps.totalFrames == 1);
    REQUIRE(ps.ok);
    CHECK(ps.rootNote == 69);
}

TEST_CASE("Yin::analyzeFrame: sine sạch có aperiodicity gần 0, nhiễu thì cao", "[yin]") {
    Yin yin;
    const auto x = sine(440.0, 48000.0, 0.1);
    const auto f = yin.analyzeFrame(x.data(), 48000.0);
    CHECK(f.voiced);
    CHECK(f.aperiodicity < 0.02f);
    CHECK(std::fabs(centsError(f.hz, 440.0)) < 2.0);

    std::mt19937 rng(5);
    std::uniform_real_distribution<float> uni(-0.5f, 0.5f);
    std::vector<float> noise(2048);
    for (auto& v : noise) v = uni(rng);
    const auto fn = yin.analyzeFrame(noise.data(), 48000.0);
    CHECK(fn.aperiodicity > 0.3f);
}

// Fixture giọng thật (DoD "giọng hát mẫu ra đúng nốt"): đặt file vào tests/fixtures/ với tên
//   voice_<mô tả>_<Nốt><Quãng>.wav    ví dụ voice_la_A3.wav, voice_ooh_Cs4.wav (s = thăng)
// Không có file nào → test chỉ nhắc, không fail.
TEST_CASE("Yin: fixture giọng hát thật (tests/fixtures/voice_*_<nốt>.wav)", "[yin][fixture]") {
#ifdef LE_TEST_FIXTURES_DIR
    const std::filesystem::path dir = LE_TEST_FIXTURES_DIR;
#else
    const std::filesystem::path dir = "tests/fixtures";
#endif
    const std::regex re(R"(voice_.*_([A-G])(s?)(-?\d)\.wav)");
    int found = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        std::smatch m;
        const std::string name = entry.path().filename().string();
        if (!std::regex_match(name, m, re)) continue;
        ++found;
        static const int kSemis[] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
        const int expected = 12 * (std::stoi(m[3].str()) + 1) + kSemis[m[1].str()[0] - 'A'] + (m[2].str() == "s" ? 1 : 0);
        std::vector<float> x;
        double sr = 0.0;
        std::string err;
        CAPTURE(name);
        REQUIRE(le::spike::readAudioMono(entry.path().string(), x, sr, &err));
        Yin yin;
        const PitchEstimate p = yin.analyze(x.data(), static_cast<int64_t>(x.size()), sr);
        INFO("dò được nốt " << p.rootNote << " " << p.cents << " cent, confidence " << p.confidence);
        REQUIRE(p.ok);
        CHECK(p.rootNote == expected);
    }
    if (found == 0) WARN("Chưa có fixture giọng thật trong " << dir.string() << " (voice_la_A3.wav …): bước người dùng");
}

TEST_CASE("Yin: thời gian phân tích mẫu 4 giây (thông tin)", "[yin][.perf]") {
    const auto x = synthVoice(220.0, 48000.0, 4.0);
    Yin yin;
    const auto t0 = std::chrono::steady_clock::now();
    const PitchEstimate p = yin.analyze(x.data(), static_cast<int64_t>(x.size()), 48000.0);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    REQUIRE(p.ok);
    WARN("Yin 4 s @48k: " << ms << " ms (" << p.totalFrames << " frame)");
}
