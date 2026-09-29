// P3-09 (lõi) — WarpRenderer: độ dài output đúng từng sample, giữ cao độ, nhịp (onset) co giãn đúng,
// cancel < 100 ms + progress, deterministic, cache stretched/<clipId>@<bpm>.caf.
// So sánh float bằng == ở đây là CÓ CHỦ ĐÍCH (deterministic / cache đọc lại đúng từng bit).
#pragma clang diagnostic ignored "-Wfloat-equal"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "le/engine_api.h"
#include "render/WarpRenderer.h"
#include "render/Yin.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>
#include <thread>
#include <vector>

using le::dsp::AudioData;
using le::render::WarpConfig;

namespace {
constexpr double kSr = 48000.0;
constexpr double kPi = 3.14159265358979323846;

// Tone có hoạ âm (cao độ rõ cho Yin)
std::shared_ptr<AudioData> tone(double hz, int64_t n, int channels = 1) {
    auto d = std::make_shared<AudioData>(channels, n, kSr);
    for (int c = 0; c < channels; ++c)
        for (int64_t i = 0; i < n; ++i) {
            double s = 0.0;
            for (int h = 1; h <= 6; ++h) s += std::sin(2 * kPi * hz * h * static_cast<double>(i) / kSr + 0.3 * h) / h;
            d->writePointer(c)[i] = static_cast<float>(0.3 * s);
        }
    return d;
}

// Click 5 ms ở đầu mỗi beat (orig BPM)
std::shared_ptr<AudioData> clicks(double bpm, int beats) {
    const double spb = 60.0 * kSr / bpm;
    const auto n = static_cast<int64_t>(std::llround(beats * spb));
    auto d = std::make_shared<AudioData>(1, n, kSr);
    for (int b = 0; b < beats; ++b) {
        const auto s0 = static_cast<int64_t>(std::llround(b * spb));
        for (int i = 0; i < 240; ++i)
            d->writePointer(0)[s0 + i] = static_cast<float>(0.8 * std::sin(2 * kPi * 2000.0 * i / kSr) * (1.0 - i / 240.0));
    }
    return d;
}

std::vector<int64_t> onsets(const AudioData& d, float thr, int64_t minGap) {
    std::vector<int64_t> out;
    int64_t last = -minGap;
    for (int64_t i = 0; i < d.numFrames(); ++i)
        if (std::fabs(d.channel(0)[i]) > thr && i - last >= minGap) {
            out.push_back(i);
            last = i;
        } else if (std::fabs(d.channel(0)[i]) > thr) {
            last = i;
        }
    return out;
}
} // namespace

TEST_CASE("WarpRenderer: độ dài output = round(inLen · orig/new) CHÍNH XÁC từng sample", "[render][warp]") {
    CHECK(le::render::warpedLength(115200, 100.0, 120.0) == 96000);
    CHECK(le::render::warpedLength(96000, 120.0, 100.0) == 115200);
    CHECK(le::render::warpedLength(48001, 97.0, 131.0) == std::llround(48001.0 * 97.0 / 131.0));
    CHECK(le::render::warpedLength(0, 100.0, 120.0) == 0);
    CHECK(le::render::warpedLength(1000, 0.0, 120.0) == 1000);
    struct Case { int64_t n; double orig, nw; int ch; };
    for (const Case c : {Case{115200, 100.0, 120.0, 1}, Case{96000, 120.0, 100.0, 2}, Case{48001, 97.0, 131.0, 1},
                         Case{30000, 120.0, 180.0, 2}, Case{500, 120.0, 90.0, 1}}) {
        CAPTURE(c.n, c.orig, c.nw, c.ch);
        const auto src = tone(220.0, c.n, c.ch);
        WarpConfig cfg;
        cfg.originalBpm = c.orig;
        cfg.newBpm = c.nw;
        const auto r = le::render::renderWarp(*src, cfg);
        REQUIRE(r.ok);
        CHECK(r.data->numFrames() == le::render::warpedLength(c.n, c.orig, c.nw));
        CHECK(r.data->numChannels() == c.ch);
        CHECK(r.data->sampleRate() == kSr);
        CHECK(r.ratio == Catch::Approx(c.orig / c.nw));
        for (int ch = 0; ch < c.ch; ++ch)
            for (int64_t i = 0; i < r.data->numFrames(); ++i) REQUIRE(std::isfinite(r.data->channel(ch)[i]));
    }
}

TEST_CASE("WarpRenderer: giữ cao độ khi co giãn (Yin < 5 cent)", "[render][warp]") {
    const auto src = tone(220.0, 115200);
    for (double nw : {80.0, 120.0, 150.0}) {
        CAPTURE(nw);
        WarpConfig cfg;
        cfg.originalBpm = 100.0;
        cfg.newBpm = nw;
        const auto r = le::render::renderWarp(*src, cfg);
        REQUIRE(r.ok);
        le::Yin yin;
        const auto p = yin.analyze(r.data->channel(0) + 12000, 48000, kSr);
        REQUIRE(p.ok);
        CHECK(std::fabs(1200.0 * std::log2(p.hz / 220.0)) < 5.0);
    }
}

TEST_CASE("WarpRenderer: nhịp co giãn đúng — click mỗi beat @100 → mỗi 24000 sample @120 (±5 ms)", "[render][warp]") {
    const auto src = clicks(100.0, 8);
    WarpConfig cfg;
    cfg.originalBpm = 100.0;
    cfg.newBpm = 120.0;
    const auto r = le::render::renderWarp(*src, cfg);
    REQUIRE(r.ok);
    REQUIRE(r.data->numFrames() == 8 * 24000);
    const auto on = onsets(*r.data, 0.1f, 6000);
    INFO("onset:" << [&] { std::string s; for (auto o : on) s += " " + std::to_string(o); return s; }());
    REQUIRE(on.size() == 8);
    for (size_t b = 0; b < on.size(); ++b) CHECK(std::llabs(on[b] - static_cast<int64_t>(b) * 24000) <= 240);
}

TEST_CASE("WarpRenderer: cùng BPM → gần như nguyên vẹn; deterministic; tham số sai", "[render][warp]") {
    const auto src = tone(330.0, 48000, 2);
    WarpConfig same;
    same.originalBpm = same.newBpm = 120.0;
    const auto r = le::render::renderWarp(*src, same);
    REQUIRE(r.ok);
    REQUIRE(r.data->numFrames() == 48000);
    double e = 0.0, d = 0.0;
    for (int64_t i = 10000; i < 38000; ++i) {
        e += static_cast<double>(src->channel(0)[i]) * src->channel(0)[i];
        d += std::pow(static_cast<double>(r.data->channel(0)[i] - src->channel(0)[i]), 2);
    }
    CHECK(10.0 * std::log10(d / e) < -40.0);

    WarpConfig cfg;
    cfg.originalBpm = 100.0;
    cfg.newBpm = 123.0;
    const auto a = le::render::renderWarp(*src, cfg), b = le::render::renderWarp(*src, cfg);
    REQUIRE(a.ok);
    REQUIRE(b.ok);
    for (int c = 0; c < 2; ++c)
        CHECK(std::equal(a.data->channel(c), a.data->channel(c) + a.data->numFrames(), b.data->channel(c)));

    WarpConfig bad;
    bad.newBpm = 0.0;
    CHECK(le::render::renderWarp(*src, bad).error == LE_ERR_INVALID_ARG);
    CHECK(le::render::renderWarp(AudioData(1, 0, kSr), cfg).error == LE_ERR_INVALID_ARG);
}

TEST_CASE("WarpRenderer: cancel dừng < 100 ms, progress tới 1", "[render][warp]") {
    const auto src = tone(220.0, 30 * 48000, 2);   // 30 s stereo
    std::atomic<bool> cancel{false};
    std::atomic<float> progress{0.0f};
    WarpConfig cfg;
    cfg.originalBpm = 100.0;
    cfg.newBpm = 120.0;
    cfg.cancel = &cancel;
    cfg.progress = &progress;
    le::render::WarpResult r;
    std::chrono::steady_clock::time_point stopAt, doneAt;
    std::thread worker([&] {
        r = le::render::renderWarp(*src, cfg);
        doneAt = std::chrono::steady_clock::now();
    });
    while (progress.load() < 0.1f) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    stopAt = std::chrono::steady_clock::now();
    cancel = true;
    worker.join();
    const double ms = std::chrono::duration<double, std::milli>(doneAt - stopAt).count();
    INFO("dừng sau " << ms << " ms");
    CHECK(r.error == LE_ERR_JOB_CANCELLED);
    CHECK(ms < 100.0);

    cancel = false;
    const auto small = tone(220.0, 4 * 48000);
    const auto ok = le::render::renderWarp(*small, cfg);
    REQUIRE(ok.ok);
    CHECK(progress.load() == Catch::Approx(1.0f));
    WARN("Warp clip 4 s mono 100 → 120 BPM: " << ok.msTotal << " ms");
}

TEST_CASE("WarpRenderer: cache stretched/<clipId>@<bpm>.caf ghi/đọc đúng từng bit, sai lệch → không dùng", "[render][warp]") {
    CHECK(le::render::stretchedCacheFileName("c_1a2b", 120.0) == "c_1a2b@120.00.caf");
    CHECK(le::render::stretchedCacheFileName("c_x", 99.5) == "c_x@99.50.caf");
    CHECK(le::render::stretchedCacheFileName("c_x", 133.333) == "c_x@133.33.caf");

    std::random_device rd;
    char name[64];
    std::snprintf(name, sizeof(name), "le-warp-test-%08x%08x", rd(), rd());
    const auto proj = std::filesystem::temp_directory_path() / name;
    const std::string path = le::render::stretchedCachePath(proj.string(), "c_7", 120.0);
    CHECK(path.find("cache/stretched/c_7@120.00.caf") != std::string::npos);

    const auto src = tone(220.0, 24000, 2);
    WarpConfig cfg;
    cfg.originalBpm = 100.0;
    cfg.newBpm = 120.0;
    const auto r = le::render::renderWarp(*src, cfg);
    REQUIRE(r.ok);
    std::string err;
    REQUIRE(le::render::writeStretchedCache(path, *r.data, &err));
    const auto back = le::render::readStretchedCache(path, r.data->numFrames(), kSr, &err);
    INFO(err);
    REQUIRE(back != nullptr);
    for (int c = 0; c < 2; ++c)
        CHECK(std::equal(back->channel(c), back->channel(c) + back->numFrames(), r.data->channel(c)));
    CHECK(le::render::readStretchedCache(path, r.data->numFrames() + 1, kSr, &err) == nullptr);   // lệch độ dài
    CHECK(le::render::readStretchedCache(path, r.data->numFrames(), 44100.0, &err) == nullptr);   // lệch sample rate
    CHECK(le::render::readStretchedCache(path + ".khong-co", 1, kSr, &err) == nullptr);
    std::error_code ec;
    std::filesystem::remove_all(proj, ec);
}
