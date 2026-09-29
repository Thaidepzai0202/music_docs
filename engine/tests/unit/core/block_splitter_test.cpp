// P1-09 BlockSplitter: ranh giới ở frame 0, frame cuối, nhiều ranh giới, block 1 frame, gộp ≤ 1 sample, tối đa 16.
#include <catch2/catch_test_macros.hpp>

#include "core/BlockSplitter.h"

using namespace le::core;

namespace {
struct Fixture {
    Transport tr;
    BlockSplitter sp;
    Segment segs[BlockSplitter::kMaxSegments];
    Fixture() {
        tr.prepare(48000.0);
        tr.play();
    }
    int run(int n, std::initializer_list<int> bounds, std::int64_t start = 0) {
        sp.begin(n);
        for (int b : bounds) sp.add(b);
        return sp.split(tr, start, segs);
    }
    void requireContiguous(int count, int n) {
        int f = 0;
        for (int i = 0; i < count; ++i) {
            REQUIRE(segs[i].startFrame == f);
            REQUIRE(segs[i].numFrames > 0);
            f += segs[i].numFrames;
        }
        REQUIRE(f == n);
    }
};
} // namespace

TEST_CASE("BlockSplitter: không có ranh giới → 1 segment cả block", "[core][splitter]") {
    Fixture fx;
    REQUIRE(fx.run(128, {}) == 1);
    REQUIRE(fx.segs[0].numFrames == 128);
    REQUIRE(fx.segs[0].startBeat == 0.0);
    REQUIRE(fx.segs[0].endBeat == 128.0 / 24000.0);
}

TEST_CASE("BlockSplitter: ranh giới ở frame 0 không tách (áp dụng đầu segment đầu)", "[core][splitter]") {
    Fixture fx;
    REQUIRE(fx.run(128, {0}) == 1);
    REQUIRE(fx.run(128, {-5, 128, 500}) == 1);   // ngoài block bị bỏ
}

TEST_CASE("BlockSplitter: ranh giới ở frame cuối", "[core][splitter]") {
    Fixture fx;
    REQUIRE(fx.run(128, {127}) == 2);
    REQUIRE(fx.segs[0].numFrames == 127);
    REQUIRE(fx.segs[1].startFrame == 127);
    REQUIRE(fx.segs[1].numFrames == 1);
}

TEST_CASE("BlockSplitter: nhiều ranh giới, không theo thứ tự, trùng nhau", "[core][splitter]") {
    Fixture fx;
    const int n = fx.run(256, {200, 50, 50, 120});
    REQUIRE(n == 4);
    fx.requireContiguous(n, 256);
    REQUIRE(fx.segs[1].startFrame == 50);
    REQUIRE(fx.segs[2].startFrame == 120);
    REQUIRE(fx.segs[3].startFrame == 200);
    // beat của segment tính từ sample tuyệt đối
    REQUIRE(fx.segs[2].startSample == 120);
    REQUIRE(fx.segs[2].startBeat == fx.tr.beatAt(120));
}

TEST_CASE("BlockSplitter: gộp ranh giới cách nhau ≤ 1 sample về cái sớm hơn", "[core][splitter]") {
    Fixture fx;
    const int n = fx.run(128, {60, 61, 62, 64});
    // 60, 61 (gộp vào 60), 62 (cách 60 hai sample → giữ), 64
    REQUIRE(n == 4);
    REQUIRE(fx.segs[1].startFrame == 60);
    REQUIRE(fx.segs[2].startFrame == 62);
    REQUIRE(fx.segs[3].startFrame == 64);
}

TEST_CASE("BlockSplitter: block 1 frame", "[core][splitter]") {
    Fixture fx;
    REQUIRE(fx.run(1, {0, 1}) == 1);
    REQUIRE(fx.segs[0].numFrames == 1);
}

TEST_CASE("BlockSplitter: quá 16 segment → gộp cặp gần nhau nhất", "[core][splitter]") {
    Fixture fx;
    fx.sp.begin(1024);
    for (int b = 10; b < 1024; b += 30) fx.sp.add(b);   // 34 ranh giới
    fx.sp.add(15);                                        // cặp gần nhất (10, 15)
    const int n = fx.sp.split(fx.tr, 0, fx.segs);
    REQUIRE(n == BlockSplitter::kMaxSegments);
    fx.requireContiguous(n, 1024);
}

TEST_CASE("BlockSplitter: transport dừng → beat đứng yên", "[core][splitter]") {
    Fixture fx;
    fx.tr.stop();
    REQUIRE(fx.run(128, {64}) == 2);
    REQUIRE(fx.segs[1].startBeat == 0.0);
    REQUIRE(fx.segs[1].endBeat == 0.0);
}

TEST_CASE("BlockSplitter: segment bắt đầu ở block giữa timeline", "[core][splitter]") {
    Fixture fx;
    fx.tr.advance(96000);
    const int n = fx.run(128, {32}, 96000);
    REQUIRE(n == 2);
    REQUIRE(fx.segs[0].startBeat == 4.0);
    REQUIRE(fx.segs[1].startSample == 96032);
}
