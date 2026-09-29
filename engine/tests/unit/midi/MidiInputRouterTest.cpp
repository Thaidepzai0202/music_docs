// P4-01 (lõi) — MidiInputRouter: timestamp → frame offset, tới sớm kẹp 0, tương lai để block sau, lọc SysEx/clock,
// queue đầy, 2 thread không mất message; MidiLearnMap tra đúng, thay/xoá, đầy.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "midi/MidiInputRouter.h"
#include "midi/MidiLearnMap.h"

#include <atomic>
#include <random>
#include <thread>
#include <vector>

using namespace le::midi;
using le::core::MidiQueue;

namespace {
constexpr double kSr = 48000.0;
constexpr int64_t kT0 = 5'000'000'000;        // host time đầu block (ns)
constexpr int64_t kMs = 1'000'000;

bool push(MidiQueue& q, int64_t t, uint8_t s, uint8_t d1 = 60, uint8_t d2 = 100, uint16_t src = 0) {
    const uint8_t b[3] = {s, d1, d2};
    return MidiInputRouter::enqueue(q, t, b, 3, src);
}
} // namespace

TEST_CASE("MidiInputRouter: timestamp → offset; tới sớm kẹp 0; tương lai sang block sau (giữ thứ tự)", "[midi]") {
    MidiQueue q(le::core::kMidiToRtCapacity);
    MidiInputRouter router;
    REQUIRE(push(q, kT0 - 5 * kMs, 0x90, 60));    // đã qua đầu block 5 ms
    REQUIRE(push(q, kT0 + 1 * kMs, 0x90, 62));    // 1 ms = 48 sample
    REQUIRE(push(q, kT0 + 2 * kMs, 0x80, 62, 0));
    REQUIRE(push(q, kT0 + 3 * kMs, 0x90, 64));    // ngoài block 128 sample (2.667 ms)

    MidiInputEventList out;
    CHECK(router.drain(q, kT0, kSr, 128, out) == 3);
    REQUIRE(out.size() == 3);
    CHECK(out[0].offset == 0);
    CHECK(out[0].data1 == 60);
    CHECK(out[1].offset == 48);
    CHECK(out[2].offset == 96);
    CHECK(out[2].status == 0x80);

    out.clear();                                   // block kế tiếp bắt đầu ở kT0 + 128 sample
    const int64_t t1 = kT0 + static_cast<int64_t>(128.0 / kSr * 1e9);
    CHECK(router.drain(q, t1, kSr, 128, out) == 1);
    CHECK(out[0].data1 == 64);
    CHECK(out[0].offset == 16);                    // (3 ms − 2.667 ms) · 48 = 16
    CHECK(q.front() == nullptr);
}

TEST_CASE("MidiInputRouter: độ trễ lập lịch 1 block giữ đúng khoảng cách giữa các nốt", "[midi]") {
    MidiQueue q(64);
    MidiInputRouter router;
    const int64_t block = static_cast<int64_t>(128.0 / kSr * 1e9);
    router.setScheduleLatencyNs(block);
    // Hai nốt tới trong block TRƯỚC, cách nhau 1 ms → trong block này vẫn cách nhau đúng 48 sample
    push(q, kT0 - block + 0 * kMs, 0x90, 60);
    push(q, kT0 - block + 1 * kMs, 0x90, 61);
    MidiInputEventList out;
    router.drain(q, kT0, kSr, 128, out);
    REQUIRE(out.size() == 2);
    CHECK(out[0].offset == 0);
    CHECK(out[1].offset == 48);
}

TEST_CASE("MidiInputRouter: lọc SysEx/clock/realtime; queue đầy trả false; đồng hồ lệch không chặn queue", "[midi]") {
    const uint8_t clock[1] = {0xF8}, sysex[3] = {0xF0, 0x7E, 0xF7}, pc[2] = {0xC3, 5}, shortNote[2] = {0x90, 60};
    CHECK_FALSE(MidiInputRouter::accepts(clock, 1));
    CHECK_FALSE(MidiInputRouter::accepts(sysex, 3));
    CHECK(MidiInputRouter::accepts(pc, 2));                  // program change 2 byte
    CHECK_FALSE(MidiInputRouter::accepts(shortNote, 2));     // note-on thiếu velocity
    CHECK_FALSE(MidiInputRouter::accepts(nullptr, 3));

    MidiQueue small(4);                                      // rigtorp: dung lượng 4 → chứa được 4
    int ok = 0;
    for (int i = 0; i < 10; ++i) ok += push(small, kT0, 0x90, static_cast<uint8_t>(i)) ? 1 : 0;
    CHECK(ok == 4);

    MidiQueue q(8);
    MidiInputRouter router;
    push(q, kT0 + 60'000 * kMs, 0x90, 70);                   // timestamp 60 s tương lai (đồng hồ sai)
    push(q, kT0, 0x90, 71);
    MidiInputEventList out;
    CHECK(router.drain(q, kT0, kSr, 128, out) == 2);         // không bị kẹt ở đầu queue
    CHECK(out[0].offset == 0);
}

TEST_CASE("parseMidi: note-on velocity 0 = note-off, CC, pitch bend, program change", "[midi]") {
    CHECK(parseMidi(0x93, 60, 100).kind == ParsedMidi::Kind::NoteOn);
    CHECK(parseMidi(0x93, 60, 100).channel == 3);
    CHECK(parseMidi(0x90, 60, 0).kind == ParsedMidi::Kind::NoteOff);
    CHECK(parseMidi(0x81, 60, 0).kind == ParsedMidi::Kind::NoteOff);
    const auto cc = parseMidi(0xB0, 7, 99);
    CHECK(cc.kind == ParsedMidi::Kind::ControlChange);
    CHECK(cc.number == 7);
    CHECK(cc.value == 99);
    CHECK(parseMidi(0xE0, 0x00, 0x40).bend == 0);            // giữa
    CHECK(parseMidi(0xE0, 0x7F, 0x7F).bend == 8191);
    CHECK(parseMidi(0xE0, 0x00, 0x00).bend == -8192);
    CHECK(parseMidi(0xC2, 12, 0).kind == ParsedMidi::Kind::ProgramChange);
}

TEST_CASE("MidiInputRouter: 1 thread đẩy (CoreMIDI) + 1 thread lấy (RT) → không mất, không trùng message", "[midi][stress]") {
    MidiQueue q(le::core::kMidiToRtCapacity);
    MidiInputRouter router;
    constexpr int kTotal = 20000;
    std::atomic<bool> done{false};
    std::thread producer([&] {
        for (int i = 0; i < kTotal;) {
            const uint8_t b[3] = {0x90, static_cast<uint8_t>(i & 0x7F), static_cast<uint8_t>((i >> 7) & 0x7F)};
            if (MidiInputRouter::enqueue(q, kT0, b, 3, 0)) ++i;
            else std::this_thread::yield();                    // queue đầy: đợi RT lấy bớt
        }
        done = true;
    });
    int received = 0;
    bool inOrder = true;
    MidiInputEventList out;
    while (!done.load() || q.front() != nullptr) {
        out.clear();
        router.drain(q, kT0, kSr, 128, out);
        for (const auto& e : out) {
            const int id = e.data1 | (e.data2 << 7);
            if (id != (received & 0x3FFF)) inOrder = false;
            ++received;
        }
    }
    producer.join();
    CHECK(received == kTotal);
    CHECK(inOrder);
}

TEST_CASE("MidiLearnMap: tra đúng, thay mapping, xoá, đầy 256; learnKeyFor; mapLearnValue", "[midi][learn]") {
    MidiLearnMap map;
    LearnTarget launch;
    launch.action = LearnAction::ClipLaunch;
    launch.track = 2;
    launch.slot = 5;
    LearnTarget gain;
    gain.action = LearnAction::TrackGain;
    gain.track = 1;
    gain.minValue = -60.0f;
    gain.maxValue = 6.0f;

    const LearnKey padKey{3, LearnKind::Note, 0, 81};          // Launchpad (thiết bị 3), nốt 81
    const LearnKey knobKey{3, LearnKind::CC, 0, 21};
    REQUIRE(map.set(padKey, launch));
    REQUIRE(map.set(knobKey, gain));
    CHECK(map.size() == 2);
    const LearnTarget* t = map.find(padKey);
    REQUIRE(t != nullptr);
    CHECK(t->action == LearnAction::ClipLaunch);
    CHECK(t->slot == 5);
    CHECK(map.find(LearnKey{3, LearnKind::Note, 1, 81}) == nullptr);   // khác kênh
    CHECK(map.find(LearnKey{4, LearnKind::Note, 0, 81}) == nullptr);   // khác thiết bị
    CHECK(map.find(LearnKey{3, LearnKind::CC, 0, 81}) == nullptr);     // khác loại

    launch.slot = 6;
    REQUIRE(map.set(padKey, launch));                        // thay, không thêm
    CHECK(map.size() == 2);
    CHECK(map.find(padKey)->slot == 6);
    CHECK(map.remove(padKey));
    CHECK_FALSE(map.remove(padKey));
    CHECK(map.find(padKey) == nullptr);

    // Event → khoá: note-on và note-off cùng khoá Note, CC → khoá CC, pitch bend → không có khoá
    MidiInputEvent e;
    e.status = 0xB0; e.data1 = 21; e.data2 = 64; e.source = 3;
    LearnKey k;
    REQUIRE(learnKeyFor(e, k));
    const LearnTarget* g = map.find(k);
    REQUIRE(g != nullptr);
    CHECK(mapLearnValue(*g, 127) == Catch::Approx(6.0f));
    CHECK(mapLearnValue(*g, 0) == Catch::Approx(-60.0f));
    e.status = 0xE0;
    CHECK_FALSE(learnKeyFor(e, k));

    // Đầy: 256 mapping ngẫu nhiên, tra lại được hết, cái thứ 257 bị từ chối
    MidiLearnMap full;
    std::mt19937 rng(9);
    std::vector<LearnKey> keys;
    for (int i = 0; i < MidiLearnMap::kCapacity; ++i) {
        const LearnKey key{static_cast<uint16_t>(rng() % 8), static_cast<LearnKind>(i % 2), static_cast<uint8_t>(i / 128),
                           static_cast<uint8_t>(i % 128)};
        LearnTarget tt;
        tt.action = LearnAction::SceneLaunch;
        tt.slot = static_cast<int8_t>(i % 8);
        if (full.set(key, tt)) keys.push_back(key);
    }
    for (const auto& key : keys) REQUIRE(full.find(key) != nullptr);
    if (full.size() == MidiLearnMap::kCapacity) CHECK_FALSE(full.set(LearnKey{99, LearnKind::Note, 15, 127}, launch));
}
