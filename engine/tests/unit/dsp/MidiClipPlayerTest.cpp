// P1-29 (lõi) — MidiClipPlayer: nốt đúng từng sample, giống hệt nhau với mọi block size, cắt ở điểm loop,
// bấm lại cao độ đang giữ, không có nốt treo, đổi clip lúc đang phát.
#include <catch2/catch_test_macros.hpp>

#include "dsp/MidiClipPlayer.h"

#include <algorithm>
#include <map>
#include <random>
#include <tuple>
#include <vector>

using namespace le::dsp;

namespace {
constexpr double kSpb120 = 24000.0;   // 120 BPM @ 48 kHz

struct AbsEvent {
    int64_t sample;
    int note, vel;
    bool on;
    bool operator==(const AbsEvent& o) const { return std::tie(sample, note, vel, on) == std::tie(o.sample, o.note, o.vel, o.on); }
};

MidiClip clip(std::vector<MidiNote> notes, double len) {
    MidiClip c;
    c.notes = std::move(notes);
    c.lengthBeats = len;
    return c;
}

// Chạy như RtEngine: transport neo ở sample 0, segment = block (có thể cắt thêm tại stopAt).
// launch tại sample launchAt (đầu một segment), dừng tại sample stopAt (−1 = không dừng).
std::vector<AbsEvent> run(const MidiClip& c, int64_t total, int block, double spb = kSpb120,
                          int64_t launchAt = 0, int64_t stopAt = -1) {
    MidiClipPlayer p;
    p.prepare(48000.0, block);
    p.setClip(&c, 1);
    MidiEventList list;
    std::vector<AbsEvent> out;
    for (int64_t s0 = 0; s0 < total;) {
        int64_t end = std::min<int64_t>(total, s0 + block);
        if (launchAt > s0 && launchAt < end) end = launchAt;   // launch/stop luôn ở đầu segment
        if (stopAt > s0 && stopAt < end) end = stopAt;
        const int n = static_cast<int>(end - s0);
        list.clear();
        if (s0 == launchAt) p.start(static_cast<double>(s0) / spb);
        if (s0 == stopAt) p.stop(list, 0);
        p.process(static_cast<double>(s0) / spb, n, spb, list);
        for (const MidiEvent& e : list) {
            REQUIRE(e.offset >= 0);
            REQUIRE(e.offset < n);
            out.push_back({s0 + e.offset, e.note, e.velocity, e.on});
        }
        s0 = end;
    }
    return out;
}

// Mọi note-on đều có note-off SAU nó (hoặc cùng sample nếu là bấm lại), không có on chồng on cùng cao độ.
bool balanced(const std::vector<AbsEvent>& ev, bool mustEndSilent) {
    std::map<int, bool> held;
    for (const auto& e : ev) {
        if (e.on) {
            if (held[e.note]) return false;
            held[e.note] = true;
        } else {
            if (!held[e.note]) return false;
            held[e.note] = false;
        }
    }
    if (mustEndSilent)
        for (const auto& [n, h] : held) if (h) return false;
    return true;
}
// Phát clip a từ sample 0, đổi sang b ĐÚNG tại sample swapAt (segment được cắt tại đó), dừng ở total.
std::vector<AbsEvent> runSwap(const MidiClip& a, const MidiClip& b, int64_t swapAt, int64_t total, int block) {
    MidiClipPlayer p;
    p.prepare(48000.0, block);
    p.setClip(&a, 1);
    MidiEventList list;
    std::vector<AbsEvent> out;
    for (int64_t s0 = 0; s0 < total;) {
        int64_t end = std::min<int64_t>(total, s0 + block);
        if (swapAt > s0 && swapAt < end) end = swapAt;
        const int n = static_cast<int>(end - s0);
        list.clear();
        if (s0 == 0) p.start(0.0);
        if (s0 == swapAt) p.setClip(&b, 2);
        p.process(static_cast<double>(s0) / kSpb120, n, kSpb120, list);
        for (const MidiEvent& e : list) {
            REQUIRE(e.offset >= 0);
            REQUIRE(e.offset < n);
            out.push_back({s0 + e.offset, e.note, e.velocity, e.on});
        }
        s0 = end;
    }
    list.clear();
    p.stop(list, 0);
    for (const MidiEvent& e : list) out.push_back({total, e.note, e.velocity, e.on});
    return out;
}

bool has(const std::vector<AbsEvent>& ev, int note, bool on, int64_t sample) {
    return std::any_of(ev.begin(), ev.end(), [&](const AbsEvent& e) { return e.note == note && e.on == on && e.sample == sample; });
}
int countOn(const std::vector<AbsEvent>& ev, int note) {
    return static_cast<int>(std::count_if(ev.begin(), ev.end(), [&](const AbsEvent& e) { return e.on && e.note == note; }));
}

// Giống hệt nhau với mọi block size, cân bằng on/off, không nốt treo
std::vector<AbsEvent> swapAllBlocks(const MidiClip& a, const MidiClip& b, int64_t swapAt, int64_t total) {
    const auto ref = runSwap(a, b, swapAt, total, 128);
    for (const int block : {64, 1000, 4096}) {
        CAPTURE(block);
        CHECK(runSwap(a, b, swapAt, total, block) == ref);
    }
    CHECK(balanced(ref, true));
    return ref;
}
} // namespace

TEST_CASE("MidiClipPlayer: nốt ở beat 0.5 → đúng sample 12000 (@120 BPM, 48 kHz), mọi block size", "[dsp][midi]") {
    const MidiClip c = clip({{0.5, 0.25, 60, 100}}, 4.0);
    const auto ref = run(c, 30000, 128);
    REQUIRE(ref.size() == 2);
    CHECK(ref[0] == AbsEvent{12000, 60, 100, true});
    CHECK(ref[1] == AbsEvent{18000, 60, 0, false});       // 0.5 + 0.25 beat
    for (int block : {1, 64, 333, 1024, 12000, 12001}) {
        CAPTURE(block);
        CHECK(run(c, 30000, block) == ref);
    }
    // 100 BPM: 28800 sample/beat → beat 0.5 = sample 14400
    const auto slow = run(c, 30000, 256, 28800.0);
    REQUIRE_FALSE(slow.empty());
    CHECK(slow[0].sample == 14400);
}

TEST_CASE("MidiClipPlayer: lặp vòng, nốt vượt điểm cuối bị cắt tại điểm loop", "[dsp][midi][loop]") {
    // Vòng 2 beat. Nốt 36 ở beat 0 (0.5 beat); nốt 40 ở beat 1.5 dài 1 beat → vượt điểm loop 2.0
    const MidiClip c = clip({{0.0, 0.5, 36, 90}, {1.5, 1.0, 40, 80}}, 2.0);
    const auto ev = run(c, 3 * 48000, 128);
    const std::vector<AbsEvent> expect = {
        {0, 36, 90, true},      {12000, 36, 0, false},  {36000, 40, 80, true},
        {48000, 40, 0, false},  {48000, 36, 90, true},  // cắt tại điểm loop, cùng sample với nốt đầu vòng 2
        {60000, 36, 0, false},  {84000, 40, 80, true},
        {96000, 40, 0, false},  {96000, 36, 90, true},  {108000, 36, 0, false}, {132000, 40, 80, true},
    };
    CHECK(ev == expect);
    for (int block : {1, 77, 1024}) {
        CAPTURE(block);
        CHECK(run(c, 3 * 48000, block) == expect);
    }
}

TEST_CASE("MidiClipPlayer: bấm lại cao độ đang giữ → note-off rồi note-on cùng sample", "[dsp][midi]") {
    const MidiClip c = clip({{0.0, 1.0, 60, 100}, {0.5, 1.0, 60, 70}}, 4.0);
    const auto ev = run(c, 48000, 256);
    const std::vector<AbsEvent> expect = {
        {0, 60, 100, true}, {12000, 60, 0, false}, {12000, 60, 70, true}, {36000, 60, 0, false}};
    CHECK(ev == expect);
    CHECK(balanced(ev, true));
}

TEST_CASE("MidiClipPlayer: stop → note-off mọi nốt đang giữ, không có nốt treo", "[dsp][midi]") {
    const MidiClip c = clip({{0.0, 3.0, 48, 100}, {0.0, 3.0, 52, 100}, {1.0, 0.5, 55, 100}}, 4.0);
    // Dừng ở sample 30000 (beat 1.25): 48, 52, 55 đang giữ → 3 note-off tại 30000
    const auto ev = run(c, 60000, 128, kSpb120, 0, 30000);
    CHECK(balanced(ev, true));
    int offsAtStop = 0;
    for (const auto& e : ev) if (!e.on && e.sample == 30000) ++offsAtStop;
    CHECK(offsAtStop == 3);
    for (const auto& e : ev) CHECK(e.sample <= 30000);    // sau stop không còn gì

    // Launch ở giữa: beat 4 = sample 96000 → nốt 55 (beat 1.0 trong clip) ở sample 120000
    const auto late = run(c, 130000, 100, kSpb120, 96000);
    REQUIRE(late.size() >= 3);
    CHECK(late[0].sample == 96000);
    CHECK(std::any_of(late.begin(), late.end(), [](const AbsEvent& e) { return e.on && e.note == 55 && e.sample == 120000; }));
}

TEST_CASE("MidiClipPlayer: clip dày ngẫu nhiên — cân bằng on/off, giống hệt nhau mọi block size", "[dsp][midi]") {
    std::mt19937 rng(2024);
    std::uniform_real_distribution<double> start(0.0, 8.0), dur(0.01, 3.0);
    std::uniform_int_distribution<int> pitch(40, 52), vel(1, 127);
    std::vector<MidiNote> notes;
    for (int i = 0; i < 200; ++i)
        notes.push_back({start(rng), dur(rng), static_cast<uint8_t>(pitch(rng)), static_cast<uint8_t>(vel(rng))});
    notes.push_back({3.0, 0.0, 60, 100});                     // nốt dài 0 → vẫn có on/off (tối thiểu 1 sample)
    std::stable_sort(notes.begin(), notes.end(), [](const MidiNote& a, const MidiNote& b) { return a.startBeat < b.startBeat; });
    const MidiClip c = clip(notes, 8.0);

    const int64_t total = 5 * 8 * 24000 + 12345;
    const auto ref = run(c, total, 128, kSpb120, 0, total - 777);
    CHECK(balanced(ref, true));
    CHECK(ref.size() > 1000);
    for (int block : {1, 63, 500, 1024}) {
        CAPTURE(block);
        CHECK(run(c, total, block, kSpb120, 0, total - 777) == ref);
    }
}

TEST_CASE("MidiClipPlayer: đổi clip lúc đang phát (overdub) → phát tiếp cùng pha, không trùng/mất nốt", "[dsp][midi]") {
    const MidiClip a = clip({{0.0, 0.25, 60, 100}, {1.0, 0.25, 62, 100}}, 4.0);
    const MidiClip b = clip({{0.0, 0.25, 60, 100}, {1.0, 0.25, 62, 100}, {1.5, 0.25, 64, 100}}, 4.0);
    MidiClipPlayer p;
    p.prepare(48000.0, 128);
    p.setClip(&a, 1);
    MidiEventList list;
    std::vector<AbsEvent> ev;
    for (int64_t s0 = 0; s0 < 48000; s0 += 128) {
        if (s0 == 0) p.start(0.0);
        if (s0 == 24064) p.setClip(&b, 2);                     // ngay sau beat 1.0 (sample 24000) đã phát
        list.clear();
        p.process(static_cast<double>(s0) / kSpb120, 128, kSpb120, list);
        for (const MidiEvent& e : list) ev.push_back({s0 + e.offset, e.note, e.velocity, e.on});
    }
    int count62 = 0;
    for (const auto& e : ev) if (e.on && e.note == 62) ++count62;
    CHECK(count62 == 1);                                        // nốt ở beat 1.0 chỉ phát 1 lần
    CHECK(std::any_of(ev.begin(), ev.end(), [](const AbsEvent& e) { return e.on && e.note == 64 && e.sample == 36000; }));
    CHECK(balanced(ev, true));
    CHECK_FALSE(p.usesGeneration(1));
    CHECK(p.usesGeneration(2));

    // Gỡ clip khi đang giữ nốt → note-off ngay đầu segment kế tiếp
    const MidiClip longNote = clip({{0.0, 3.0, 70, 100}}, 4.0);
    MidiClipPlayer q;
    q.prepare(48000.0, 128);
    q.setClip(&longNote, 1);
    q.start(0.0);
    list.clear();
    q.process(0.0, 128, kSpb120, list);
    CHECK(q.heldNotes() == 1);
    q.setClip(nullptr, 2);
    list.clear();
    q.process(128.0 / kSpb120, 128, kSpb120, list);
    REQUIRE(list.size() == 1);
    CHECK_FALSE(list[0].on);
    CHECK(list[0].offset == 0);
    CHECK(q.heldNotes() == 0);
    CHECK_FALSE(q.isPlaying());
}

TEST_CASE("MidiEventList: đầy thì bỏ và đếm; sort ổn định, off trước on", "[dsp][midi]") {
    MidiEventList l;
    for (int i = 0; i < 600; ++i) l.push({i % 7, 60, 100, (i % 2) == 0});
    CHECK(l.size() == MidiEventList::kCapacity);
    CHECK(l.dropped() == 600 - MidiEventList::kCapacity);
    l.clear();
    l.push({5, 60, 100, true});
    l.push({5, 60, 0, false});
    l.push({2, 61, 0, false});
    l.push({5, 62, 0, false});
    l.sort();
    CHECK(l[0].offset == 2);
    CHECK((!l[1].on && l[1].note == 60));
    CHECK((!l[2].on && l[2].note == 62));                     // off giữ thứ tự ban đầu
    CHECK(l[3].on);
}

TEST_CASE("MidiClipPlayer: sửa clip lúc đang phát (piano roll) — xoá / giữ / kéo dài / thu ngắn nốt đang kêu", "[dsp][midi][edit]") {
    const MidiClip a = clip({{0.0, 3.0, 60, 100}, {0.0, 1.0, 64, 100}, {2.0, 1.0, 67, 100}}, 4.0);
    SECTION("xoá nốt đang kêu → note-off ngay tại chỗ đổi; nốt còn lại giữ giờ tắt cũ") {
        const MidiClip b = clip({{0.0, 1.0, 64, 100}, {2.0, 1.0, 67, 100}}, 4.0);   // bỏ 60
        const auto ev = swapAllBlocks(a, b, 12000, 96000);                          // đổi ở beat 0.5
        CHECK(has(ev, 60, true, 0));
        CHECK(has(ev, 60, false, 12000));
        CHECK(has(ev, 64, false, 24000));
        CHECK(has(ev, 67, true, 48000));
        CHECK(countOn(ev, 60) == 1);
    }
    SECTION("overdub: clip mới chứa mọi nốt cũ + nốt mới → không nốt nào bị cắt sớm") {
        const MidiClip b = clip({{0.0, 3.0, 60, 100}, {0.0, 1.0, 64, 100}, {1.5, 0.25, 62, 90}, {2.0, 1.0, 67, 100}}, 4.0);
        const auto ev = swapAllBlocks(a, b, 12000, 96000);
        CHECK(has(ev, 60, false, 72000));   // beat 3: đúng giờ cũ
        CHECK(has(ev, 64, false, 24000));
        CHECK(has(ev, 62, true, 36000));    // nốt thêm ở beat 1.5 (sau vị trí đổi) → kêu ngay vòng này
    }
    SECTION("kéo dài nốt đang kêu → tắt theo độ dài mới; thu ngắn tới trước vị trí hiện tại → tắt ngay") {
        const MidiClip longer = clip({{0.0, 3.5, 60, 100}, {0.0, 2.0, 64, 100}, {2.0, 1.0, 67, 100}}, 4.0);
        auto ev = swapAllBlocks(a, longer, 12000, 96000);
        CHECK(has(ev, 60, false, 84000));   // beat 3.5
        CHECK(has(ev, 64, false, 48000));   // beat 2
        const MidiClip shorter = clip({{0.0, 0.25, 60, 100}, {0.0, 1.0, 64, 100}, {2.0, 1.0, 67, 100}}, 4.0);
        ev = swapAllBlocks(a, shorter, 12000, 96000);
        CHECK(has(ev, 60, false, 12000));   // kết thúc ở beat 0.25 < 0.5 → tắt ngay
        CHECK(has(ev, 64, false, 24000));
    }
}

TEST_CASE("MidiClipPlayer: sửa clip lúc đang phát — nốt mới đã qua kêu từ vòng sau, nốt đúng vị trí đổi kêu đúng 1 lần", "[dsp][midi][edit]") {
    const MidiClip a = clip({{0.0, 0.25, 60, 100}, {1.0, 0.25, 62, 100}}, 4.0);
    // Đổi ĐÚNG tại beat 1.0 (sample 24000): nốt 62 ở beat 1.0 chưa phát → phát đúng 1 lần, không mất, không trùng.
    // Nốt mới 63 ở beat 0.5 (đã qua) → vòng sau (beat 4.5); nốt mới 65 ở beat 2 → vòng này.
    const MidiClip b = clip({{0.0, 0.25, 60, 100}, {0.5, 0.5, 63, 100}, {1.0, 0.25, 62, 100}, {2.0, 0.5, 65, 100}}, 4.0);
    const auto ev = swapAllBlocks(a, b, 24000, 8 * 24000);
    CHECK(countOn(ev, 62) == 2);          // beat 1 và beat 5
    CHECK(has(ev, 62, true, 24000));
    CHECK_FALSE(has(ev, 63, true, 12000));
    CHECK(has(ev, 63, true, 108000));     // beat 4.5
    CHECK(countOn(ev, 63) == 1);
    CHECK(has(ev, 65, true, 48000));
    CHECK(countOn(ev, 60) == 2);          // beat 0 và 4, không phát lại lúc đổi
}

TEST_CASE("MidiClipPlayer: sửa clip lúc đang phát — đổi độ dài vòng, nốt đang giữ cắt ở điểm loop mới", "[dsp][midi][edit]") {
    const MidiClip a = clip({{0.0, 4.0, 60, 100}}, 4.0);
    const MidiClip b = clip({{0.0, 2.0, 60, 100}}, 2.0);          // vòng còn 2 beat
    const auto ev = swapAllBlocks(a, b, 24000, 6 * 24000);        // đổi ở beat 1
    CHECK(has(ev, 60, false, 48000));     // điểm loop mới (beat 2) …
    CHECK(has(ev, 60, true, 48000));      // … rồi vòng mới bắt đầu cùng sample (off trước on)
    CHECK(has(ev, 60, true, 96000));      // beat 4
    CHECK(countOn(ev, 60) == 3);          // beat 0, 2, 4
    // Clip mới rỗng → mọi nốt đang giữ tắt ngay
    const MidiClip empty = clip({}, 4.0);
    const auto e2 = swapAllBlocks(a, empty, 24000, 4 * 24000);
    CHECK(has(e2, 60, false, 24000));
    CHECK(countOn(e2, 60) == 1);
}
