// P1-03: chạy mọi tests/scenarios/*.json với block 64/128/256/1024.
// Mỗi lượt phải đạt kỳ vọng, và output phải GIỐNG NHAU giữa các kích thước block (08 §3.2).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "sim/ScenarioRunner.h"

namespace fs = std::filesystem;

namespace {
std::vector<fs::path> scenarioFiles() {
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(LE_TEST_SCENARIOS_DIR))
        if (e.path().extension() == ".json") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    return files;
}

std::string join(const std::vector<std::string>& v) {
    std::string s;
    for (const auto& x : v) s += "\n  - " + x;
    return s;
}
} // namespace

TEST_CASE("Scenario: mọi file trong tests/scenarios đạt ở block 64/128/256/1024", "[scenario]") {
    const auto files = scenarioFiles();
    REQUIRE_FALSE(files.empty());

    for (const auto& file : files) {
        std::vector<float> refL, refR;
        for (int block : {64, 128, 256, 1024}) {
            INFO("scenario " << file.filename().string() << " @ block " << block);
            le::sim::RunOptions opt;
            opt.blockSize = block;
            const auto r = le::sim::runScenarioFile(file.string(), opt);
            INFO("error: " << r.error << "\nfailures:" << join(r.failures) << "\nnotes:" << join(r.notes));
            REQUIRE(r.error.empty());
            CHECK(r.failures.empty());
            REQUIRE(r.ok);

            if (refL.empty()) {
                refL = r.left;
                refR = r.right;
                continue;
            }
            REQUIRE(r.left.size() == refL.size());
            size_t firstDiff = r.left.size();
            for (size_t i = 0; i < r.left.size(); ++i)
                if (std::fabs(r.left[i] - refL[i]) > 1e-7f || std::fabs(r.right[i] - refR[i]) > 1e-7f) {
                    firstDiff = i;
                    break;
                }
            INFO("sample đầu tiên khác block 64: " << firstDiff);
            REQUIRE(firstDiff == r.left.size());
        }
    }
}

TEST_CASE("ScenarioRunner: báo lỗi rõ ràng", "[scenario]") {
    SECTION("JSON hỏng") {
        const auto r = le::sim::runScenarioJson("{oops");
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("JSON") != std::string::npos);
    }
    SECTION("lệnh bị engine từ chối") {
        const auto r = le::sim::runScenarioJson(
            R"({"name":"x","timeline":[{"atBeat":0,"send":{"type":"TRANSPORT_PLAY"}}],"renderBeats":1})");
        REQUIRE_FALSE(r.ok);
        REQUIRE(r.error.find("từ chối") != std::string::npos);
    }
    SECTION("tên lệnh không có") {
        const auto r = le::sim::runScenarioJson(R"({"timeline":[{"atBeat":0,"send":{"type":"NOPE"}}],"renderBeats":1})");
        REQUIRE_FALSE(r.ok);
    }
    SECTION("thiếu độ dài render") {
        REQUIRE_FALSE(le::sim::runScenarioJson(R"({"timeline":[]})").ok);
    }
}

TEST_CASE("ScenarioRunner: kỳ vọng sai → failure có số sample cụ thể", "[scenario]") {
    const auto r = le::sim::runScenarioJson(R"({
        "timeline":[{"atSample":1000,"send":{"type":"SPIKE_SINE","f0":440,"f1":0.5}}],
        "renderFrames":4800,
        "expect":{"firstNonSilentSample":2000,"golden":"golden/khong_co.wav"}})");
    REQUIRE(r.error.empty());
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.failures.size() == 2);
    REQUIRE(r.failures[0].find("firstNonSilentSample = 1001") != std::string::npos);
    REQUIRE(r.failures[1].find("golden_update.sh") != std::string::npos);
}

TEST_CASE("ScenarioRunner: golden null test báo sample đầu tiên bị lệch", "[scenario]") {
    const std::string json = R"({"name":"g","timeline":[{"atSample":0,"send":{"type":"SPIKE_SINE","f0":440,"f1":0.5}}],
                                 "renderFrames":4800})";
    auto base = le::sim::runScenarioJson(json);
    REQUIRE(base.ok);

    const auto dir = fs::temp_directory_path() / "le-golden-test";
    fs::create_directories(dir);
    auto golden = base;
    golden.left[3000] += 0.01f;   // làm hỏng 1 sample
    REQUIRE(le::sim::writeWavStereo((dir / "g.wav").string(), golden.left, golden.right, 48000.0));

    le::sim::RunOptions opt;
    opt.baseDir = dir.string();
    const std::string withGolden = json.substr(0, json.size() - 1) + R"(,"expect":{"golden":"g.wav","nullTestMaxDb":-90}})";
    const auto r = le::sim::runScenarioJson(withGolden, opt);
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.failures.size() == 1);
    REQUIRE(r.failures[0].find("sample đầu tiên bị lệch: 3000") != std::string::npos);

    REQUIRE(le::sim::writeWavStereo((dir / "g.wav").string(), base.left, base.right, 48000.0));   // golden đúng
    REQUIRE(le::sim::runScenarioJson(withGolden, opt).ok);
    fs::remove_all(dir);
}
