// P1-27 — SfzLoader: 3 file SFZ mẫu (drum kit 06 §4, piano 2 lớp velocity, pad có loop), kế thừa
// <global>/<group>/<region>, tên nốt, cảnh báo opcode lạ, lỗi rõ ràng khi thiếu sample.
// Sample tự tổng hợp qua loader giả: không cần file LFS.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (giá trị đọc từ text phải khớp chính xác).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"
#include "io/SfzLoader.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace le;
using io::DecodeResult;

namespace {

// "Thư viện" giả: đường dẫn → AudioData tổng hợp. Ghi lại mọi đường dẫn được yêu cầu.
struct FakeLibrary {
    std::map<std::string, dsp::AudioDataPtr> files;
    std::vector<std::string> requests;

    void add(const std::string& path, int64_t frames = 4800, float value = 0.5f, int channels = 1) {
        auto d = std::make_shared<dsp::AudioData>(channels, frames, 48000.0);
        for (int c = 0; c < channels; ++c) std::fill(d->writePointer(c), d->writePointer(c) + frames, value);
        files[path] = d;
    }
    io::SfzLoadOptions options() {
        io::SfzLoadOptions o;
        o.loadSample = [this](const std::string& p) {
            requests.push_back(p);
            const auto it = files.find(p);
            if (it == files.end()) return DecodeResult{LE_ERR_FILE_NOT_FOUND, "không có file", nullptr};
            return DecodeResult{LE_OK, "", it->second};
        };
        return o;
    }
};

bool hasWarning(const std::vector<std::string>& w, const std::string& needle) {
    return std::any_of(w.begin(), w.end(), [&](const std::string& s) { return s.find(needle) != std::string::npos; });
}

// ── 3 file mẫu ──────────────────────────────────────────────────────────────
// (1) Ví dụ drum kit trong docs/06 §4, giữ nguyên từng ký tự
const char* kDrumKit =
    "<control> default_path=samples/\n"
    "<global> loop_mode=one_shot ampeg_release=0.05\n"
    "<region> key=36 sample=kick.flac\n"
    "<region> key=38 sample=snare.flac\n"
    "<region> key=42 sample=hat_closed.flac group=1 off_by=2\n"
    "<region> key=46 sample=hat_open.flac   group=2 off_by=1\n";

// (2) Piano nhỏ: 2 lớp velocity, tên nốt, đường dẫn có dấu cách và dấu '\' kiểu Windows, comment, CRLF
const char* kPiano =
    "// Piano nhỏ: 2 lớp velocity\r\n"
    "<control> default_path=samples\\piano\\\r\n"
    "<global> ampeg_release=0.4 ampeg_sustain=80 volume=-3\r\n"
    "<group> lovel=1 hivel=63 volume=-6\r\n"
    "<region> sample=C3 soft.wav lokey=c3 hikey=f#3 pitch_keycenter=c3\r\n"
    "<region> sample=A3 soft.wav lokey=g3 hikey=c#4 pitch_keycenter=a3 tune=-12\r\n"
    "/* lớp mạnh:\r\n"
    "   dùng lại sample C3 */\r\n"
    "<group> lovel=64 hivel=127\r\n"
    "<region> sample=C3 soft.wav lokey=c3 hikey=f#3 pitch_keycenter=c3 volume=0\r\n"
    "<region> sample=A3 loud.wav lokey=g3 hikey=c#4 pitch_keycenter=a3 pan=-50\r\n";

// (3) Pad có loop + đủ thứ không hỗ trợ (phải cảnh báo, không fail)
const char* kPad =
    "<control> default_path=./\n"
    "<global> loop_mode=loop_continuous ampeg_attack=0.2 ampeg_release=1.5 cutoff=1200\n"
    "<region> sample=pad.wav key=60 loop_start=100 loop_end=899 fil_type=lpf cutoff=800\n"
    "<region> sample=pad.wav lokey=61 hikey=72 pitch_keycenter=66 loop_mode=loop_sustain\n"
    "<curve> curve_index=1 v000=0\n"
    "<region> sample=../shared/noise.wav key=a-1 loop_mode=no_loop\n"
    "#define $VEL 100\n"
    "<region> lokey=10 hikey=5 sample=bad.wav\n"
    "<region> key=20\n";

} // namespace

TEST_CASE("SfzLoader: tên nốt SFZ → MIDI", "[io][sfz]") {
    CHECK(io::parseSfzNote("60") == 60);
    CHECK(io::parseSfzNote("c4") == 60);
    CHECK(io::parseSfzNote("C4") == 60);
    CHECK(io::parseSfzNote("c#4") == 61);
    CHECK(io::parseSfzNote("db4") == 61);
    CHECK(io::parseSfzNote("a-1") == 9);
    CHECK(io::parseSfzNote("b3") == 59);
    CHECK(io::parseSfzNote("bb3") == 58);
    CHECK(io::parseSfzNote("g9") == 127);
    CHECK(io::parseSfzNote(" f#3 ") == 54);
    CHECK(io::parseSfzNote("g#9") == -1);   // 128: ngoài dải
    CHECK(io::parseSfzNote("128") == -1);
    CHECK(io::parseSfzNote("h4") == -1);
    CHECK(io::parseSfzNote("") == -1);
    CHECK(io::parseSfzNote("c") == -1);
}

TEST_CASE("SfzLoader: file mẫu 1 — drum kit của 06 §4", "[io][sfz]") {
    FakeLibrary lib;
    for (const char* f : {"kick", "snare", "hat_closed", "hat_open"}) lib.add(std::string("/lib/kits/808/samples/") + f + ".flac");
    const auto r = io::loadSfzText(kDrumKit, "/lib/kits/808", "808", lib.options());
    INFO(r.message);
    REQUIRE(r.ok);
    CHECK(r.error == LE_OK);
    CHECK(r.warnings.empty());
    REQUIRE(r.regions == 4);
    CHECK(r.samplesLoaded == 4);
    const dsp::Instrument& inst = *r.instrument;
    CHECK(inst.name == "808");
    REQUIRE(inst.zones.size() == 4);

    const dsp::Zone& kick = inst.zones[0];
    CHECK(kick.loKey == 36);
    CHECK(kick.hiKey == 36);
    CHECK(kick.rootKey == 36);
    CHECK(kick.loopMode == dsp::LoopMode::OneShot);             // kế thừa từ <global>
    CHECK(kick.env.release == Catch::Approx(0.05f));
    CHECK(kick.data == lib.files["/lib/kits/808/samples/kick.flac"].get());

    const dsp::Zone& closed = inst.zones[2];
    const dsp::Zone& open = inst.zones[3];
    CHECK(closed.loKey == 42);
    CHECK(closed.group == 1);
    CHECK(closed.offBy == 2);
    CHECK(open.loKey == 46);
    CHECK(open.group == 2);
    CHECK(open.offBy == 1);
    CHECK(inst.findZone(38, 100) == &inst.zones[1]);
    CHECK(inst.findZone(40, 100) == nullptr);
}

TEST_CASE("SfzLoader: file mẫu 2 — piano 2 lớp velocity, kế thừa global/group/region", "[io][sfz]") {
    FakeLibrary lib;
    const std::string dir = "/lib/instruments/piano/samples/piano/";
    lib.add(dir + "C3 soft.wav");
    lib.add(dir + "A3 soft.wav");
    lib.add(dir + "A3 loud.wav", 4800, 0.5f, 2);
    const auto r = io::loadSfzText(kPiano, "/lib/instruments/piano", "piano", lib.options());
    INFO(r.message);
    REQUIRE(r.ok);
    CHECK(r.warnings.empty());
    REQUIRE(r.regions == 4);
    CHECK(r.samplesLoaded == 3);
    CHECK(lib.requests.size() == 3);                             // "C3 soft.wav" dùng 2 lần, nạp 1 lần

    const auto& z = r.instrument->zones;
    // Sắp theo loKey, giữ thứ tự trong file khi bằng nhau: C3 nhẹ, C3 mạnh, A3 nhẹ, A3 mạnh
    CHECK(z[0].loKey == 48);  CHECK(z[0].hiKey == 54);  CHECK(z[0].rootKey == 48);
    CHECK(z[0].hiVel == 63);  CHECK(z[0].gainDb == -6.0f);      // <group> ghi đè <global>
    CHECK(z[1].loKey == 48);  CHECK(z[1].loVel == 64);  CHECK(z[1].gainDb == 0.0f);   // <region> ghi đè
    CHECK(z[2].loKey == 55);  CHECK(z[2].hiKey == 61);  CHECK(z[2].rootKey == 57);
    CHECK(z[2].tuneCents == -12.0f);
    CHECK(z[3].gainDb == -3.0f);                                 // group 2 không có volume → của <global>
    CHECK(z[3].pan == Catch::Approx(-0.5f));
    CHECK(z[3].data->isStereo());
    for (const auto& zone : z) {
        CHECK(zone.env.release == Catch::Approx(0.4f));
        CHECK(zone.env.sustain == Catch::Approx(0.8f));         // ampeg_sustain 80 % → 0.8
        CHECK(zone.loopMode == dsp::LoopMode::NoLoop);
    }
    CHECK(z[0].data == z[1].data);
    CHECK(r.instrument->findZone(50, 30) == &z[0]);
    CHECK(r.instrument->findZone(50, 100) == &z[1]);
    CHECK(r.instrument->findZone(60, 127) == &z[3]);
}

TEST_CASE("SfzLoader: file mẫu 3 — loop, đường dẫn tương đối, cảnh báo thứ không hỗ trợ", "[io][sfz]") {
    FakeLibrary lib;
    lib.add("/lib/instruments/pad/pad.wav", 1000);
    lib.add("/lib/instruments/shared/noise.wav", 1000);
    const auto r = io::loadSfzText(kPad, "/lib/instruments/pad", "pad", lib.options());
    INFO(r.message);
    REQUIRE(r.ok);
    REQUIRE(r.regions == 3);
    const auto& z = r.instrument->zones;
    // Sắp theo loKey: noise (a-1 = 9), pad key 60, pad 61..72
    CHECK(z[0].loKey == 9);
    CHECK(z[0].loopMode == dsp::LoopMode::NoLoop);               // region ghi đè loop_continuous của global
    CHECK(z[0].data == lib.files["/lib/instruments/shared/noise.wav"].get());   // "../" được chuẩn hoá
    CHECK(z[1].loKey == 60);
    CHECK(z[1].loopMode == dsp::LoopMode::LoopContinuous);
    CHECK(z[1].loopStart == 100);
    CHECK(z[1].loopEnd == 900);                                  // SFZ loop_end GỒM sample 899 → end = 900
    CHECK(z[1].env.attack == Catch::Approx(0.2f));
    CHECK(z[2].rootKey == 66);
    CHECK(z[2].loopMode == dsp::LoopMode::LoopContinuous);       // loop_sustain → loop_continuous
    CHECK(z[2].loopEnd == -1);                                   // không đặt → hết sample
    CHECK(r.samplesLoaded == 2);

    const auto& w = r.warnings;
    INFO("cảnh báo:\n" << [&] { std::string s; for (const auto& x : w) s += "  " + x + "\n"; return s; }());
    CHECK(hasWarning(w, "'cutoff' (2 lần, lần đầu ở dòng 2)"));
    CHECK(hasWarning(w, "'fil_type'"));
    CHECK(hasWarning(w, "<curve>"));
    CHECK(hasWarning(w, "#define"));
    CHECK(hasWarning(w, "loop_sustain"));
    CHECK(hasWarning(w, "dòng 8: lokey > hikey"));
    CHECK(hasWarning(w, "dòng 9 không có sample"));
    CHECK_FALSE(hasWarning(w, "curve_index"));                   // opcode của header lạ: đã gộp vào cảnh báo header
}

TEST_CASE("SfzLoader: lỗi rõ ràng, không crash", "[io][sfz]") {
    SECTION("thiếu file sample → FILE_NOT_FOUND, nêu tên sample và số dòng") {
        FakeLibrary lib;
        lib.add("/k/samples/kick.flac");                         // thiếu snare
        const auto r = io::loadSfzText(kDrumKit, "/k", "kit", lib.options());
        CHECK_FALSE(r.ok);
        CHECK(r.error == LE_ERR_FILE_NOT_FOUND);
        CHECK(r.instrument == nullptr);
        CHECK(r.message.find("snare.flac") != std::string::npos);
        CHECK(r.message.find("dòng 4") != std::string::npos);
        CHECK(r.message.find("không có file") != std::string::npos);   // lý do từ loader
    }
    SECTION("loader báo sai định dạng → giữ nguyên mã lỗi") {
        io::SfzLoadOptions o;
        o.loadSample = [](const std::string&) { return DecodeResult{LE_ERR_FILE_FORMAT, "không phải WAV", nullptr}; };
        CHECK(io::loadSfzText(kDrumKit, "/k", "kit", o).error == LE_ERR_FILE_FORMAT);
    }
    SECTION("loader trả file rỗng → FILE_FORMAT") {
        io::SfzLoadOptions o;
        o.loadSample = [](const std::string&) {
            return DecodeResult{LE_OK, "", std::make_shared<dsp::AudioData>(1, 0, 48000.0)};
        };
        CHECK(io::loadSfzText(kDrumKit, "/k", "kit", o).error == LE_ERR_FILE_FORMAT);
    }
    SECTION("không có loader → INVALID_ARG") {
        CHECK(io::loadSfzText(kDrumKit, "/k", "kit", {}).error == LE_ERR_INVALID_ARG);
    }
    SECTION("header thiếu '>' → FILE_FORMAT có số dòng") {
        FakeLibrary lib;
        const auto r = io::loadSfzText("<global> volume=1\n<region key=1 sample=a.wav\n", "/k", "x", lib.options());
        CHECK(r.error == LE_ERR_FILE_FORMAT);
        CHECK(r.message.find("Dòng 2") != std::string::npos);
    }
    SECTION("không có region dùng được → FILE_FORMAT") {
        FakeLibrary lib;
        CHECK(io::loadSfzText("// trống\n<global> volume=-3\n", "/k", "x", lib.options()).error == LE_ERR_FILE_FORMAT);
        CHECK(io::loadSfzText("", "/k", "x", lib.options()).error == LE_ERR_FILE_FORMAT);
    }
    SECTION("giá trị hỏng → cảnh báo, dùng mặc định") {
        FakeLibrary lib;
        lib.add("/k/a.wav");
        const auto r = io::loadSfzText("<region> sample=a.wav lokey=xyz volume=loud\n", "/k", "x", lib.options());
        REQUIRE(r.ok);
        CHECK(r.instrument->zones[0].loKey == 0);
        CHECK(r.instrument->zones[0].gainDb == 0.0f);
        CHECK(hasWarning(r.warnings, "lokey=xyz"));
        CHECK(hasWarning(r.warnings, "volume=loud"));
    }
    SECTION("huỷ giữa chừng → JOB_CANCELLED") {
        FakeLibrary lib;
        for (const char* f : {"kick", "snare", "hat_closed", "hat_open"}) lib.add(std::string("/k/samples/") + f + ".flac");
        std::atomic<bool> cancel{true};
        auto o = lib.options();
        o.cancel = &cancel;
        CHECK(io::loadSfzText(kDrumKit, "/k", "kit", o).error == LE_ERR_JOB_CANCELLED);
        CHECK(lib.requests.empty());
    }
    SECTION("loop vượt độ dài sample → cảnh báo và kẹp lại") {
        FakeLibrary lib;
        lib.add("/k/short.wav", 500);
        const auto r = io::loadSfzText("<region> sample=short.wav loop_mode=loop_continuous loop_start=100 loop_end=999\n",
                                       "/k", "x", lib.options());
        REQUIRE(r.ok);
        CHECK(r.instrument->zones[0].loopEnd == 500);
        CHECK(hasWarning(r.warnings, "loop vượt quá"));
    }
}

TEST_CASE("SfzLoader: đọc file .sfz trên đĩa, đường dẫn tính theo thư mục của file", "[io][sfz]") {
    std::random_device rd;
    char dirName[64];
    std::snprintf(dirName, sizeof(dirName), "le-sfz-test-%08x%08x", rd(), rd());
    const auto dir = std::filesystem::temp_directory_path() / dirName;
    std::filesystem::create_directories(dir);
    const auto sfz = dir / "808.sfz";
    { std::ofstream(sfz) << kDrumKit; }

    FakeLibrary lib;
    const std::string base = std::filesystem::path(dir).lexically_normal().generic_string();
    for (const char* f : {"kick", "snare", "hat_closed", "hat_open"}) lib.add(base + "/samples/" + f + ".flac");
    const auto r = io::loadSfzFile(sfz.string(), lib.options());
    INFO(r.message);
    CHECK(r.ok);
    CHECK(r.instrument->name == "808");

    const auto missing = io::loadSfzFile((dir / "khong-co.sfz").string(), lib.options());
    CHECK(missing.error == LE_ERR_FILE_NOT_FOUND);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("SfzLoader + Sampler: drum kit nạp xong chơi được, choke hi-hat hoạt động", "[io][sfz][sampler]") {
    FakeLibrary lib;
    for (const char* f : {"kick", "snare", "hat_closed", "hat_open"}) lib.add(std::string("/k/samples/") + f + ".flac", 48000);
    const auto r = io::loadSfzText(kDrumKit, "/k", "kit", lib.options());
    REQUIRE(r.ok);

    dsp::Sampler s;
    s.prepare(48000.0, 128);
    s.setInstrument(r.instrument.get(), 1);
    std::vector<float> l(128), rr(128);
    float* ch[2] = {l.data(), rr.data()};
    s.noteOn(46, 1.0f);                  // hat mở
    s.noteOn(36, 1.0f);                  // kick one-shot
    s.noteOff(36);
    s.render(ch, 2, 0, 128);
    CHECK(s.activeVoices() == 2);
    s.noteOn(42, 1.0f);                  // hat đóng chặn hat mở
    for (int i = 0; i < 3; ++i) s.render(ch, 2, 0, 128);   // > 5 ms
    CHECK(s.activeVoices() == 2);        // kick (one-shot, vẫn kêu) + hat đóng
}

// Thư viện / sample tên tiếng Việt + emoji: đường dẫn UTF-8 giữ nguyên từng byte tới loadSample.
TEST_CASE("SfzLoader: thư mục và tên sample có dấu / emoji", "[io][sfz][utf8]") {
    std::random_device rd;
    char dirName[64];
    std::snprintf(dirName, sizeof(dirName), "le-sfz-utf8-%08x%08x", rd(), rd());
    const auto dir = std::filesystem::temp_directory_path() / dirName / "Nhạc cụ 🎹 Việt";
    std::filesystem::create_directories(dir);
    const auto sfz = dir / "trống đồng.sfz";
    { std::ofstream(sfz) << "<control> default_path=mẫu âm/\n<region> sample=trống cái 🥁.flac key=36\n<region> sample=chũm chọe.flac key=42\n"; }
    FakeLibrary lib;
    const std::string base = std::filesystem::path(dir).lexically_normal().generic_string();
    lib.add(base + "/mẫu âm/trống cái 🥁.flac");
    lib.add(base + "/mẫu âm/chũm chọe.flac");
    const auto r = io::loadSfzFile(sfz.string(), lib.options());
    INFO(r.message);
    REQUIRE(r.ok);
    CHECK(r.instrument->zones.size() == 2);
    CHECK(r.instrument->name == "trống đồng");
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::temp_directory_path() / dirName, ec);
}

// 06 §4: region_label = tên hiện trên pad / piano roll; giá trị có dấu cách, đọc tới opcode kế tiếp hoặc hết dòng.
TEST_CASE("SfzLoader: region_label (có dấu cách, UTF-8) vào Zone::label, không cảnh báo", "[io][sfz]") {
    FakeLibrary lib;
    lib.add("/lib/samples/kick.wav");
    lib.add("/lib/samples/tom.wav");
    lib.add("/lib/samples/hat.wav");
    const char* text = "<control> default_path=samples/\n"
                       "<global> loop_mode=one_shot\n"
                       "<region> key=36 sample=kick.wav region_label=Kick\n"
                       "<region> key=41 region_label=Floor Tom L sample=tom.wav volume=-3\n"
                       "<region> key=42 sample=hat.wav group=1 region_label=Hat đóng 🥁\n";
    const auto r = io::loadSfzText(text, "/lib", "kit", lib.options());
    INFO(r.message);
    REQUIRE(r.ok);
    CHECK(r.warnings.empty());
    REQUIRE(r.instrument->zones.size() == 3);
    CHECK(r.instrument->zones[0].label == "Kick");
    CHECK(r.instrument->zones[1].label == "Floor Tom L");   // dừng trước " sample="
    CHECK(r.instrument->zones[1].gainDb == -3.0f);
    CHECK(r.instrument->zones[2].label == "Hat đóng 🥁");
    CHECK(r.instrument->zones[2].group == 1);
}
