// P1-04: queue RT và validate lệnh ở phía main.
// Stress 2 thread: không mất, không lặp, không rách (08 §3.1). Chạy dưới TSan: scripts/test_engine.sh mac-tsan
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <limits>
#include <thread>

#include "core/CommandValidation.h"
#include "core/RtQueues.h"

using namespace le::core;

namespace {
#if defined(__has_feature)
#if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
constexpr uint32_t kMessages = 1'000'000;   // sanitizer chậm ~10×
#else
constexpr uint32_t kMessages = 10'000'000;
#endif
#else
constexpr uint32_t kMessages = 10'000'000;
#endif

LeCommand make(uint32_t k) {   // mọi trường suy ra từ k → phát hiện được bản rách
    LeCommand c{};
    c.type = (uint16_t) (k & 0xFFFF);
    c.track = (int8_t) (k % 8);
    c.slot = (int8_t) (k % 7);
    c.i0 = (int32_t) k;
    c.f0 = (float) (k % 1000);
    c.f1 = (float) (k % 999);
    c.d0 = (double) k * 0.5;
    c.hostTimeNs = (int64_t) k * 3;
    return c;
}

bool same(const LeCommand& a, const LeCommand& b) {
    return a.type == b.type && a.track == b.track && a.slot == b.slot && a.i0 == b.i0 && a.f0 == b.f0 &&
           a.f1 == b.f1 && a.d0 == b.d0 && a.hostTimeNs == b.hostTimeNs;
}
} // namespace

TEST_CASE("CommandQueue: 2 thread, không mất / lặp / rách", "[core][queue][stress]") {
    CommandQueue q(kRtCommandCapacity);
    std::atomic<bool> go{false};
    uint32_t fullCount = 0;

    std::thread producer([&] {   // vai main: try_push, đầy thì thử lại (main thật trả false cho Dart)
        while (!go.load(std::memory_order_acquire)) {}
        for (uint32_t k = 0; k < kMessages; ++k) {
            const LeCommand c = make(k);
            while (!q.try_push(c)) ++fullCount;
        }
    });

    go.store(true, std::memory_order_release);
    uint32_t expected = 0, bad = 0;
    while (expected < kMessages) {   // vai RT: front / pop, không bao giờ chờ lock
        if (const LeCommand* c = q.front()) {
            if (!same(*c, make(expected))) ++bad;
            q.pop();
            ++expected;
        }
    }
    producer.join();
    REQUIRE(bad == 0);
    REQUIRE(expected == kMessages);
    REQUIRE(q.front() == nullptr);
    INFO("producer gặp queue đầy " << fullCount << " lần");
    SUCCEED();
}

TEST_CASE("RtToNrt / Midi queue: dung lượng theo 04", "[core][queue]") {
    RtToNrtQueue r(kRtToNrtCapacity);
    MidiQueue m(kMidiToRtCapacity);
    REQUIRE(r.capacity() == 1024);
    REQUIRE(m.capacity() == 512);
    for (int i = 0; i < 512; ++i) REQUIRE(m.try_push(MidiInMessage{}));
    REQUIRE_FALSE(m.try_push(MidiInMessage{}));   // đầy: trả false, không block
}

TEST_CASE("isValidCommand: dải tham số từng lệnh", "[core][validate]") {
    auto c = [](uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0, float f0 = 0, double d0 = 0) {
        LeCommand x{};
        x.type = type;
        x.track = (int8_t) track;
        x.slot = (int8_t) slot;
        x.i0 = i0;
        x.f0 = f0;
        x.d0 = d0;
        return x;
    };
    // Transport
    CHECK(isValidCommand(c(LE_CMD_TRANSPORT_PLAY)));
    CHECK(isValidCommand(c(LE_CMD_SET_BPM, -1, -1, 0, 0, 120.0)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_SET_BPM, -1, -1, 0, 0, 19.9)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_SET_BPM, -1, -1, 0, 0, 300.1)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_SET_BPM, -1, -1, 0, 0, std::numeric_limits<double>::quiet_NaN())));
    CHECK(isValidCommand(c(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_4_BAR)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_SET_QUANTIZE, -1, -1, 8)));
    CHECK(isValidCommand(c(LE_CMD_METRONOME, -1, -1, 2, 1.0f)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_METRONOME, -1, -1, 3, 0.5f)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_METRONOME, -1, -1, 1, 1.5f)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_SET_COUNT_IN, -1, -1, 3)));
    // Clip & scene
    CHECK(isValidCommand(c(LE_CMD_CLIP_LAUNCH, 7, 7)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_CLIP_LAUNCH, 8, 0)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_CLIP_LAUNCH, -1, 0)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_CLIP_LAUNCH, 0, -1)));
    CHECK(isValidCommand(c(LE_CMD_SCENE_LAUNCH, -1, 3)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_SCENE_LAUNCH, -1, -1)));
    CHECK(isValidCommand(c(LE_CMD_CLIP_RECORD, 0, 0, 4)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_CLIP_RECORD, 0, 0, -1)));
    // Track, nốt, FX
    CHECK(isValidCommand(c(LE_CMD_TRACK_GAIN, 0, -1, 0, -120.0f)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_TRACK_GAIN, 0, -1, 0, 6.5f)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_TRACK_PAN, 0, -1, 0, 1.5f)));
    CHECK(isValidCommand(c(LE_CMD_NOTE_ON, 0, -1, 60, 0.8f)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_NOTE_ON, 0, -1, 128, 0.8f)));
    CHECK_FALSE(isValidCommand(c(LE_CMD_NOTE_ON, 0, -1, 60, 0.0f)));   // vel 0 = note off → dùng NOTE_OFF
    CHECK(isValidCommand(c(LE_CMD_ALL_NOTES_OFF, -1)));
    CHECK(isValidCommand(c(LE_CMD_FX_PARAM, -1, 0, 1, 0.5f)));   // master
    CHECK_FALSE(isValidCommand(c(LE_CMD_FX_PARAM, 0, -1, 1, 0.5f)));
    // Không rõ loại
    CHECK_FALSE(isValidCommand(c(0)));
    CHECK_FALSE(isValidCommand(c(777)));
}
