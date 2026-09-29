// StatePublisher (seqlock): 2 thread × 10⁶ lần publish/read, không đọc phải bản rách (P0-03 DoD).
// Chạy dưới TSan: scripts/test_engine.sh mac-tsan
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstring>
#include <thread>

#include "core/StatePublisher.h"
#include "le/engine_api.h"

using le::core::SeqLockPublisher;

namespace {

// Mọi trường đều suy ra từ k → nếu đọc lẫn 2 lần publish khác nhau thì sẽ lệch.
void fill(LeState& s, uint32_t k) {
    std::memset(&s, 0, sizeof(s));
    s.publishCounter = k;
    s.playing = (uint8_t) (k & 0xFF);
    s.beat = (double) k * 0.5;
    s.bpm = (double) k;
    s.sampleRate = (double) k + 1.0;
    s.bufferSize = (int32_t) k;
    s.xrunCount = k;
    s.cpuLoad = (float) (k % 1000);
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        s.trackPeak[t][0] = s.trackPeak[t][1] = (float) (k % 4096);
        s.trackClipProgress[t] = (float) (k % 2048);
        s.trackPlayingSlot[t] = (int8_t) (k % 7);
        for (int c = 0; c < LE_MAX_SCENES; ++c) s.clipState[t][c] = (uint8_t) (k % 251);
    }
}

bool consistent(const LeState& s) {
    LeState expect;
    if (s.publishCounter == 0) std::memset(&expect, 0, sizeof(expect));   // chưa publish lần nào: toàn 0
    else fill(expect, s.publishCounter);
    return std::memcmp(&s, &expect, sizeof(LeState)) == 0;
}

} // namespace

TEST_CASE("SeqLockPublisher: đọc trước khi publish trả toàn 0", "[core][publisher]") {
    SeqLockPublisher<LeState> p;
    LeState s;
    std::memset(&s, 0xAB, sizeof(s));
    p.read(s);
    LeState zero{};
    std::memset(&zero, 0, sizeof(zero));
    REQUIRE(std::memcmp(&s, &zero, sizeof(LeState)) == 0);
    REQUIRE(p.publishCount() == 0);
}

TEST_CASE("SeqLockPublisher: 1 thread publish rồi read", "[core][publisher]") {
    SeqLockPublisher<LeState> p;
    LeState in, out;
    fill(in, 42);
    p.publish(in);
    p.read(out);
    REQUIRE(std::memcmp(&in, &out, sizeof(LeState)) == 0);
    REQUIRE(p.publishCount() == 1);
}

TEST_CASE("SeqLockPublisher: 2 thread × 10^6, không rách, counter đơn điệu", "[core][publisher][stress]") {
    constexpr uint32_t kIterations = 1'000'000;
    SeqLockPublisher<LeState> p;
    std::atomic<bool> go{false};

    std::thread writer([&] {
        while (!go.load(std::memory_order_acquire)) {}
        LeState s;
        for (uint32_t k = 1; k <= kIterations; ++k) {
            fill(s, k);
            p.publish(s);
        }
    });

    uint32_t torn = 0, backwards = 0, last = 0, distinct = 0;
    go.store(true, std::memory_order_release);
    LeState s;
    for (uint32_t i = 0; i < kIterations; ++i) {
        p.read(s);
        if (!consistent(s)) ++torn;
        if (s.publishCounter < last) ++backwards;
        if (s.publishCounter != last) ++distinct;
        last = s.publishCounter;
    }
    writer.join();

    p.read(s);
    REQUIRE(torn == 0);
    REQUIRE(backwards == 0);
    REQUIRE(s.publishCounter == kIterations);
    REQUIRE(p.publishCount() == kIterations);
    INFO("reader thấy " << distinct << " bản khác nhau");
    REQUIRE(distinct > 0);
}
