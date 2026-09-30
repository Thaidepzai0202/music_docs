// P2-35 — spike::writeFlac24: FLAC 24-bit ghi bằng JUCE FlacAudioFormat, decode lại bằng io::decodeAudioFile (đường
// engine nạp thư viện) ra ĐÚNG từng bit với buffer trên lưới 24-bit; làm tròn / kẹp đúng; mono + stereo; đường dẫn
// UTF-8; cùng input → cùng từng byte file; nén thật (nhỏ hơn WAV float).
#include <catch2/catch_test_macros.hpp>

#include "io/AudioFileIO.h"
#include "render/DrumKits.h"
#include "spike/measure/WavIO.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace le;
namespace fs = std::filesystem;

namespace {

fs::path tmpDir() {
    const fs::path d = fs::temp_directory_path() / "le_flac_io_test";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

std::vector<char> bytesOf(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::vector<float> tone(size_t n, double hz, float amp) {
    std::vector<float> x(n);
    for (size_t i = 0; i < n; ++i)
        x[i] = amp * static_cast<float>(std::sin(6.283185307179586 * hz * static_cast<double>(i) / 48000.0));
    return x;
}

} // namespace

TEST_CASE("FLAC 24-bit: ghi rồi decode bằng io::decodeAudioFile khớp từng bit (mono, trên lưới 24-bit)", "[spike][flac]") {
    std::vector<float> x = tone(48000, 441.0, 0.9f);
    render::kits::quantize24(x);
    const fs::path p = tmpDir() / "mono.flac";
    std::string err;
    REQUIRE(spike::writeFlac24Mono(p.string(), x.data(), static_cast<int64_t>(x.size()), 48000.0, &err));
    const auto b = bytesOf(p);
    REQUIRE(b.size() > 4);
    CHECK(std::string(b.begin(), b.begin() + 4) == "fLaC");
    CHECK(b.size() < x.size() * 3);                           // nén thật: nhỏ hơn PCM 24-bit thô (WAV float = 4 byte/mẫu)

    const io::DecodeResult d = io::decodeAudioFile(fs::absolute(p).string());
    REQUIRE(d.error == LE_OK);
    REQUIRE(d.data != nullptr);
    CHECK(d.data->numChannels() == 1);
    CHECK(d.data->sampleRate() == 48000.0);
    REQUIRE(d.data->numFrames() == static_cast<int64_t>(x.size()));
    CHECK(std::equal(x.begin(), x.end(), d.data->channel(0)));   // == từng giá trị

    // readAudioMono (WavIO) cũng đọc ra đúng
    std::vector<float> back;
    double sr = 0.0;
    REQUIRE(spike::readAudioMono(p.string(), back, sr, &err));
    CHECK(back == x);
}

TEST_CASE("FLAC 24-bit: làm tròn gần nhất, kẹp ±1, NaN → 0, stereo, cùng input cùng byte", "[spike][flac]") {
    constexpr double q = 1.0 / 8388608.0;   // 1 LSB 24-bit
    const std::vector<float> L = {0.0f, static_cast<float>(0.4 * q), static_cast<float>(0.6 * q), static_cast<float>(-0.6 * q),
                                  1.0f, 1.5f, -1.0f, -2.0f, std::nanf(""), 0.25f};
    const std::vector<float> R = {0.5f, -0.5f, 0.125f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.25f};
    const float* ch[2] = {L.data(), R.data()};
    const fs::path p = tmpDir() / "stereo.flac", p2 = tmpDir() / "stereo2.flac";
    std::string err;
    REQUIRE(spike::writeFlac24(p.string(), ch, 2, static_cast<int64_t>(L.size()), 44100.0, 5, &err));
    REQUIRE(spike::writeFlac24(p2.string(), ch, 2, static_cast<int64_t>(L.size()), 44100.0, 5, &err));
    CHECK(bytesOf(p) == bytesOf(p2));

    const io::DecodeResult d = io::decodeAudioFile(fs::absolute(p).string());
    REQUIRE(d.error == LE_OK);
    REQUIRE(d.data->numChannels() == 2);
    CHECK(d.data->sampleRate() == 44100.0);
    const float* l = d.data->channel(0);
    const float* r = d.data->channel(1);
    const float top = static_cast<float>(1.0 - q);
    const std::vector<float> wantL = {0.0f, 0.0f, static_cast<float>(q), static_cast<float>(-q), top, top, -1.0f, -1.0f, 0.0f, 0.25f};
    for (size_t i = 0; i < L.size(); ++i) {
        CAPTURE(i, l[i], wantL[i]);
        CHECK(l[i] == wantL[i]);
        CHECK(r[i] == R[i]);
    }
}

TEST_CASE("FLAC 24-bit: đường dẫn UTF-8, tham số sai bị từ chối", "[spike][flac]") {
    std::vector<float> x = tone(4800, 1000.0, 0.5f);
    render::kits::quantize24(x);
    const fs::path p = tmpDir() / "Nhạc cụ 🎹" / "đàn.flac";
    std::string err;
    REQUIRE(spike::writeFlac24Mono(p.string(), x.data(), static_cast<int64_t>(x.size()), 48000.0, &err));
    const io::DecodeResult d = io::decodeAudioFile(fs::absolute(p).string());
    REQUIRE(d.error == LE_OK);
    CHECK(std::equal(x.begin(), x.end(), d.data->channel(0)));

    const float* ch[1] = {x.data()};
    CHECK_FALSE(spike::writeFlac24(p.string(), ch, 0, 10, 48000.0, 8, &err));
    CHECK_FALSE(spike::writeFlac24(p.string(), ch, 1, 10, 0.0, 8, &err));
    CHECK_FALSE(spike::writeFlac24(p.string(), ch, 1, 10, 48000.0, 9, &err));
    CHECK_FALSE(spike::writeFlac24(p.string(), ch, 1, 10, 48000.0, 0, &err));   // JUCE bỏ qua 0 → không cho dùng
    CHECK_FALSE(err.empty());
}
