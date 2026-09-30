// P2-30 / P2-35 — kiểm THƯ VIỆN THẬT trong content/ (đúng file app sẽ bundle) qua đường nạp của engine
// (SfzLoader + io::decodeAudioFile + Sampler): mọi .sfz nạp không cảnh báo, mọi region có sample FLAC 48 kHz mono,
// kit đủ 16 pad 36–51, nhạc cụ phủ kín dải phím × velocity, loop nối liền, cao độ ở phím gốc đúng (Yin), tổng dung
// lượng mẫu thật ≤ 150 MB (06 §4). Không có content/ (checkout thiếu LFS…) → SKIP.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"
#include "io/AudioFileIO.h"
#include "io/SfzLoader.h"
#include "render/Yin.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace le;
namespace fs = std::filesystem;

namespace {

fs::path contentDir() { return fs::path(LE_TEST_FIXTURES_DIR).parent_path().parent_path().parent_path() / "content"; }

std::vector<fs::path> itemDirs(const fs::path& d) {
    std::vector<fs::path> v;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(d, ec))
        if (e.is_directory()) v.push_back(e.path());
    std::sort(v.begin(), v.end());
    return v;
}

std::string readText(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

io::SfzLoadResult loadItem(const fs::path& dir) {
    io::SfzLoadOptions o;
    o.loadSample = &io::decodeAudioFile;
    return io::loadSfzFile(fs::absolute(dir / (dir.filename().string() + ".sfz")).string(), o);
}

// "range": [a, b] trong <id>.json
bool rangeOf(const std::string& json, int& lo, int& hi) {
    const size_t p = json.find("\"range\"");
    if (p == std::string::npos) return false;
    const size_t b = json.find('[', p);
    if (b == std::string::npos) return false;
    char* end = nullptr;
    lo = static_cast<int>(std::strtol(json.c_str() + b + 1, &end, 10));
    hi = static_cast<int>(std::strtol(end + 1, nullptr, 10));
    return true;
}

} // namespace

TEST_CASE("Thư viện content/: kit — nạp bằng engine, 16 pad 36–51 ở mọi velocity, FLAC 48 kHz mono, manifest", "[content][kits]") {
    const fs::path root = contentDir() / "kits";
    if (!fs::is_directory(root)) SKIP("không có " << root.string());
    const auto dirs = itemDirs(root);
    CHECK(dirs.size() >= 9);   // 8 kit tổng hợp + kit_acoustic
    for (const fs::path& d : dirs) {
        CAPTURE(d.filename().string());
        const std::string json = readText(d / (d.filename().string() + ".json"));
        CHECK(json.find("\"category\": \"Drums\"") != std::string::npos);
        CHECK(json.find("\"id\": \"" + d.filename().string() + "\"") != std::string::npos);
        const auto r = loadItem(d);
        INFO(r.message);
        REQUIRE(r.ok);
        CHECK(r.warnings.empty());
        REQUIRE(r.regions >= 16);   // kit mẫu thật: 2 lớp velocity mỗi pad
        for (int key = 36; key <= 51; ++key) {
            CAPTURE(key);
            for (int vel : {1, 64, 95, 96, 127}) {   // pad kêu ở mọi velocity
                CAPTURE(vel);
                CHECK(r.instrument->findZone(key, vel) != nullptr);
            }
            const dsp::Zone* z = r.instrument->findZone(key, 100);
            REQUIRE(z != nullptr);
            REQUIRE(z->data != nullptr);
            CHECK(z->data->sampleRate() == 48000.0);
            CHECK(z->data->numChannels() == 1);
            CHECK(z->loopMode == dsp::LoopMode::OneShot);
            CHECK(!z->label.empty());
        }
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(d / "samples", ec)) CHECK(e.path().extension() == ".flac");   // không còn WAV
        // Choke: hat đóng / chân (42, 44) chặn hat mở (46) nếu kit có hat
        const dsp::Zone* hc = r.instrument->findZone(42, 100);
        const dsp::Zone* ho = r.instrument->findZone(46, 100);
        if (d.filename() != "kit_perc") {
            REQUIRE(hc != nullptr);
            REQUIRE(ho != nullptr);
            CHECK(hc->group != 0);
            CHECK(ho->offBy == hc->group);
        }
    }
    // kit_acoustic (Big Rusty Drums): tom cao độ tăng dần theo map GM (tom 14" dùng lại cho 48 / 50 với tune +3 / +6 nửa cung)
    const fs::path ac = root / "kit_acoustic";
    if (fs::is_directory(ac)) {
        const auto r = loadItem(ac);
        REQUIRE(r.ok);
        const dsp::Zone* t47 = r.instrument->findZone(47, 100);
        const dsp::Zone* t48 = r.instrument->findZone(48, 100);
        const dsp::Zone* t50 = r.instrument->findZone(50, 100);
        REQUIRE(t47 != nullptr);
        REQUIRE(t48 != nullptr);
        REQUIRE(t50 != nullptr);
        CHECK(t48->data == t47->data);
        CHECK(t48->tuneCents == Catch::Approx(300.0f));
        CHECK(t50->tuneCents == Catch::Approx(600.0f));
        CHECK(r.instrument->findZone(39, 100)->label == "Rim Click");
    }
}

TEST_CASE("Thư viện content/: nhạc cụ — phủ kín phím × velocity, loop nối liền, cao độ đúng, ≤ 150 MB mẫu thật", "[content][instruments]") {
    const fs::path root = contentDir() / "instruments";
    if (!fs::is_directory(root)) SKIP("không có " << root.string());
    const auto dirs = itemDirs(root);
    CHECK(dirs.size() >= 2);
    uintmax_t realBytes = 0;
    for (const fs::path& d : dirs) {
        const std::string id = d.filename().string();
        CAPTURE(id);
        const std::string json = readText(d / (id + ".json"));
        CHECK(json.find("\"category\": \"Instruments/") != std::string::npos);
        int lo = -1, hi = -1;
        REQUIRE(rangeOf(json, lo, hi));
        REQUIRE(lo <= hi);
        const bool synthesized = json.find("Self-synthesized") != std::string::npos;
        if (!synthesized) {
            std::error_code ec;
            for (const auto& e : fs::directory_iterator(d / "samples", ec)) realBytes += e.file_size(ec);
        }
        const auto r = loadItem(d);
        INFO(r.message);
        REQUIRE(r.ok);
        CHECK(r.warnings.empty());
        REQUIRE(r.regions > 0);

        // P2-35 B2: MỌI phím 0–127 × velocity 1–127 → đúng một zone (ngoài dải tự nhiên là repitch, không phím nào im)
        int bad = 0;
        for (int key = 0; key <= 127; ++key)
            for (int vel = 1; vel <= 127; ++vel) {
                int n = 0;
                for (const auto& z : r.instrument->zones) n += key >= z.loKey && key <= z.hiKey && vel >= z.loVel && vel <= z.hiVel ? 1 : 0;
                if (n != 1) {
                    if (bad < 5) UNSCOPED_INFO("  phím " << key << " vel " << vel << ": " << n << " zone");
                    ++bad;
                }
            }
        CHECK(bad == 0);
        // Trong dải tự nhiên × velocity biên các lớp → đúng một zone có sample
        for (int key = lo; key <= hi; ++key)
            for (int vel : {1, 60, 61, 95, 96, 100, 101, 127}) {
                int n = 0;
                for (const auto& z : r.instrument->zones) n += key >= z.loKey && key <= z.hiKey && vel >= z.loVel && vel <= z.hiVel ? 1 : 0;
                CAPTURE(key, vel);
                CHECK(n == 1);
            }
        // Sample: 48 kHz mono; loop nối liền (mẫu cuối loop ≈ mẫu ngay trước loopStart); tune hợp lý
        for (const auto& z : r.instrument->zones) {
            REQUIRE(z.data != nullptr);
            CAPTURE(z.rootKey, z.loVel);
            CHECK(z.data->sampleRate() == 48000.0);
            CHECK(z.data->numChannels() == 1);
            CHECK(std::fabs(z.tuneCents) <= 50.0f);
            if (z.loopMode == dsp::LoopMode::LoopContinuous) {
                REQUIRE(z.loopStart > 0);
                CHECK(z.loopEnd == z.data->numFrames());
                const float* x = z.data->channel(0);
                CHECK(std::fabs(x[z.loopEnd - 1] - x[z.loopStart - 1]) < 1e-4f);
            }
        }
        // Cao độ ở từng phím gốc 40 Hz – 3 kHz (lớp mạnh nhất) qua Sampler: ≥ 90 % zone đúng nốt ±15 cent
        // (organ 16' và vài nguồn nhiều hài bậc 2: cho phép Yin thấy thấp 1 quãng tám)
        int total = 0, good = 0;
        for (const auto& z : r.instrument->zones) {
            if (z.hiVel != 127) continue;
            const double f0 = 440.0 * std::pow(2.0, (z.rootKey - 69) / 12.0);
            if (f0 < 40.0 || f0 > 3000.0) continue;   // ngoài vùng Yin tin được (piano A0–D#1: cơ bản yếu, không hài hoà; C8)
            ++total;
            dsp::Sampler s;
            s.prepare(48000.0, 256);
            s.setInstrument(r.instrument.get(), 1);
            std::vector<float> L(33600, 0.0f), R(33600, 0.0f);
            float* ch[2] = {L.data(), R.data()};
            s.noteOn(z.rootKey, 1.0f);
            for (int i = 0; i < 33600; i += 256) s.render(ch, 2, i, std::min(256, 33600 - i));
            const double f = 440.0 * std::pow(2.0, (z.rootKey - 69) / 12.0);
            YinConfig cfg;
            cfg.minHz = static_cast<float>(0.3 * f);
            cfg.maxHz = static_cast<float>(1.6 * f);
            int n = 1024;
            while (n < 2 * (static_cast<int>(48000.0 / cfg.minHz) + 2) && n < 16384) n *= 2;
            cfg.frameSize = n;
            cfg.silenceDb = -70.0f;
            Yin yin(cfg);
            const auto p = yin.analyze(L.data() + 7200, 26400, 48000.0);   // 150–700 ms
            // Piano giữ stretch tuning gốc (bè cao lệch tới +40 cent là CÓ CHỦ Ý) → nới ±45 cent
            const float tol = id == "inst_piano" ? 45.0f : 15.0f;
            const bool ok = p.ok && (p.rootNote == z.rootKey || p.rootNote == z.rootKey - 12) && std::fabs(p.cents) < tol;
            if (!ok) UNSCOPED_INFO("  " << id << " phím " << z.rootKey << ": Yin " << p.hz << " Hz (nốt " << p.rootNote << ", " << p.cents << " cent, conf " << p.confidence << ")");
            good += ok ? 1 : 0;
        }
        CAPTURE(total, good);
        CHECK(good * 10 >= total * 9);
    }
    INFO("mẫu thật: " << static_cast<double>(realBytes) / 1048576.0 << " MB");
    // inst_synth (track bass của demo): C1 / C2 là zone thật, kêu đúng nốt
    const fs::path synthDir = root / "inst_synth";
    if (fs::is_directory(synthDir)) {
        const auto r = loadItem(synthDir);
        REQUIRE(r.ok);
        for (int key : {24, 36}) {
            const dsp::Zone* z = r.instrument->findZone(key, 100);
            REQUIRE(z != nullptr);
            CAPTURE(key, z->rootKey);
            CHECK(std::abs(z->rootKey - key) <= 1);
            dsp::Sampler s;
            s.prepare(48000.0, 256);
            s.setInstrument(r.instrument.get(), 1);
            std::vector<float> L(48000, 0.0f), R(48000, 0.0f);
            float* ch[2] = {L.data(), R.data()};
            s.noteOn(key, 0.8f);
            for (int i = 0; i < 48000; i += 256) s.render(ch, 2, i, std::min(256, 48000 - i));
            const double f = 440.0 * std::pow(2.0, (key - 69) / 12.0);
            YinConfig cfg;
            cfg.minHz = static_cast<float>(0.6 * f);
            cfg.maxHz = static_cast<float>(1.6 * f);
            cfg.frameSize = 8192;
            Yin yin(cfg);
            const auto p = yin.analyze(L.data() + 14400, 24000, 48000.0);
            CAPTURE(p.hz, p.cents);
            REQUIRE(p.ok);
            CHECK(p.rootNote == key);
            CHECK(std::fabs(p.cents) < 5.0f);
        }
    }
    CHECK(realBytes <= 150ull * 1024 * 1024);
}
