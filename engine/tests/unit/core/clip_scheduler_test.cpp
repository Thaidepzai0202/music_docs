// P1-15/16/17: ClipScheduler — MỌI mũi tên của sơ đồ 04 §3.1, quantize, scene, stop all, transport stop,
// count-in, remap, LaunchLog.
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "core/ClipScheduler.h"

using namespace le::core;

namespace {
// Chạy scheduler như RtEngine: mỗi block thêm ranh giới → chia segment → applyDue ở đầu từng segment.
struct Rig {
    Transport tr;
    ClipScheduler sch;
    BlockSplitter sp;
    Segment segs[BlockSplitter::kMaxSegments];
    LaunchLogQueue log{kLaunchLogCapacity};
    std::unique_ptr<GraphSnapshot> snap = std::make_unique<GraphSnapshot>();

    Rig() {
        tr.prepare(48000.0);
        tr.setQuantize(LE_Q_1_BAR);
        sch.setLaunchLog(&log);
    }
    void clip(int t, int s, double len = 4.0) {
        snap->clips[t][s].kind = ClipKind::Midi;
        snap->clips[t][s].lengthBeats = len;
        sch.remap(*snap);
    }
    void noClip(int t, int s) {
        snap->clips[t][s] = ClipSnapshot{};
        sch.remap(*snap);
    }
    void play() { tr.play(); }
    // Render tới khi transport đạt sample `target` (block 128, chia segment như RtEngine).
    void runTo(std::int64_t target, int block = 128) {
        while (tr.samplePos() < target) {
            const int n = (int) std::min<std::int64_t>(block, target - tr.samplePos());
            const std::int64_t start = tr.samplePos();
            sp.begin(n);
            sch.addBoundaries(tr, start, n, sp);
            const int ns = sp.split(tr, start, segs);
            for (int i = 0; i < ns; ++i) sch.applyDue(tr, segs[i].startSample);
            tr.advance(n);
        }
    }
    // Trạng thái SAU KHI đã render tới sample s (applyDue ở đầu segment chứa s−1 đã chạy).
    std::uint8_t at(int t, int s) const { return sch.cellState(t, s); }
    std::vector<LaunchEvent> drain() {
        std::vector<LaunchEvent> v;
        while (const auto* e = log.front()) { v.push_back(*e); log.pop(); }
        return v;
    }
};
constexpr std::int64_t kBar = 96000;   // 4 beat @ 120 BPM / 48 kHz
} // namespace

TEST_CASE("Scheduler: Empty ↔ Stopped theo snapshot (remap)", "[core][scheduler]") {
    Rig r;
    REQUIRE(r.at(0, 0) == LE_CLIP_EMPTY);
    r.clip(0, 0);
    REQUIRE(r.at(0, 0) == LE_CLIP_STOPPED);
    r.noClip(0, 0);
    REQUIRE(r.at(0, 0) == LE_CLIP_EMPTY);
}

TEST_CASE("Scheduler: Stopped → launch(q) → QueuedPlay → ranh giới → Playing (đúng sample)", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0);
    r.play();
    r.runTo(31200);   // beat 1.3
    r.sch.launch(r.tr, 0, 0);
    REQUIRE(r.at(0, 0) == LE_CLIP_QUEUED_PLAY);
    r.runTo(kBar);
    REQUIRE(r.at(0, 0) == LE_CLIP_QUEUED_PLAY);   // sample 95999 đã render, chưa tới ranh giới
    r.runTo(kBar + 1);
    REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    REQUIRE(r.sch.playingSlot(0) == 0);
    REQUIRE(r.sch.launchBeat(0) == 4.0);
    const auto log = r.drain();
    REQUIRE(log.size() == 1);
    REQUIRE(log[0].kind == LaunchKind::Launch);
    REQUIRE(log[0].beat == 4.0);
}

TEST_CASE("Scheduler: Playing loop — vẫn Playing, progress quay vòng", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0, 2.0);   // 2 beat
    r.play();
    r.sch.launch(r.tr, 0, 0);   // beat 0 = ranh giới → phát ngay
    r.runTo(1);
    REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    r.runTo(24000);   // beat 1 → 50%
    REQUIRE(r.sch.progress(r.tr, 0) == 0.5f);
    r.runTo(48000 * 3);   // beat 6 → đầu vòng thứ 4
    REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    REQUIRE(r.sch.progress(r.tr, 0) == 0.0f);
}

TEST_CASE("Scheduler: Playing → stop(q) → QueuedStop → ranh giới → Stopped", "[core][scheduler]") {
    Rig r;
    r.clip(1, 2);
    r.play();
    r.sch.launch(r.tr, 1, 2);
    r.runTo(50000);
    r.sch.stop(r.tr, 1);
    REQUIRE(r.at(1, 2) == LE_CLIP_QUEUED_STOP);
    REQUIRE(r.sch.playingSlot(1) == 2);   // vẫn đang kêu tới ranh giới
    r.runTo(kBar);
    REQUIRE(r.at(1, 2) == LE_CLIP_QUEUED_STOP);
    r.runTo(kBar + 1);
    REQUIRE(r.at(1, 2) == LE_CLIP_STOPPED);
    REQUIRE(r.sch.playingSlot(1) == -1);
}

TEST_CASE("Scheduler: QueuedPlay → stop trước ranh giới → Stopped", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0);
    r.play();
    r.runTo(1000);
    r.sch.launch(r.tr, 0, 0);
    REQUIRE(r.at(0, 0) == LE_CLIP_QUEUED_PLAY);
    r.sch.stop(r.tr, 0);
    REQUIRE(r.at(0, 0) == LE_CLIP_STOPPED);
    r.runTo(kBar * 2);
    REQUIRE(r.at(0, 0) == LE_CLIP_STOPPED);
    REQUIRE(r.drain().empty());
}

TEST_CASE("Scheduler: launch clip khác cùng track → A QueuedStop, B QueuedPlay, đổi cùng ranh giới", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0);
    r.clip(0, 1);
    r.play();
    r.sch.launch(r.tr, 0, 0);
    r.runTo(10000);
    r.sch.launch(r.tr, 0, 1);
    REQUIRE(r.at(0, 0) == LE_CLIP_QUEUED_STOP);
    REQUIRE(r.at(0, 1) == LE_CLIP_QUEUED_PLAY);
    r.runTo(kBar + 1);
    REQUIRE(r.at(0, 0) == LE_CLIP_STOPPED);
    REQUIRE(r.at(0, 1) == LE_CLIP_PLAYING);
    REQUIRE(r.sch.playingSlot(0) == 1);
}

TEST_CASE("Scheduler: launch lại clip đang phát → retrigger từ đầu tại ranh giới", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0, 3.0);
    r.play();
    r.sch.launch(r.tr, 0, 0);
    r.runTo(40000);
    r.sch.launch(r.tr, 0, 0);
    REQUIRE(r.at(0, 0) == LE_CLIP_QUEUED_PLAY);
    REQUIRE(r.sch.playingSlot(0) == 0);   // vẫn kêu
    r.runTo(kBar + 1);
    REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    REQUIRE(r.sch.launchBeat(0) == 4.0);
}

TEST_CASE("Scheduler: launch ô trống = dừng track theo quantize", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0);
    r.play();
    r.sch.launch(r.tr, 0, 0);
    r.runTo(5000);
    r.sch.launch(r.tr, 0, 7);   // trống
    REQUIRE(r.at(0, 0) == LE_CLIP_QUEUED_STOP);
    REQUIRE(r.at(0, 7) == LE_CLIP_EMPTY);
    r.runTo(kBar + 1);
    REQUIRE(r.at(0, 0) == LE_CLIP_STOPPED);
}

TEST_CASE("Scheduler: quantize None → áp dụng ngay đầu block", "[core][scheduler]") {
    Rig r;
    r.tr.setQuantize(LE_Q_NONE);
    r.clip(0, 0);
    r.play();
    r.runTo(12345);
    r.sch.launch(r.tr, 0, 0);
    r.runTo(12346);
    REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    REQUIRE(r.sch.launchBeat(0) == r.tr.beatAt(12345));
}

TEST_CASE("Scheduler: Empty → record(q) → QueuedRecord → Recording → đủ độ dài → Playing", "[core][scheduler][record]") {
    Rig r;
    r.play();
    r.runTo(10000);
    r.sch.record(r.tr, 3, 0, 1);   // 1 bar
    REQUIRE(r.at(3, 0) == LE_CLIP_QUEUED_RECORD);
    r.runTo(kBar + 1);
    REQUIRE(r.at(3, 0) == LE_CLIP_RECORDING);
    REQUIRE(r.sch.anyRecording());
    REQUIRE(r.sch.progress(r.tr, 3) >= 0.0f);
    r.runTo(2 * kBar);
    REQUIRE(r.at(3, 0) == LE_CLIP_RECORDING);
    r.runTo(2 * kBar + 1);
    REQUIRE(r.at(3, 0) == LE_CLIP_PLAYING);   // tự chuyển sang phát, vòng nối liền
    REQUIRE(r.sch.hasClip(3, 0));
    REQUIRE(r.sch.clipLength(3, 0) == 4.0);
    REQUIRE(r.sch.launchBeat(3) == 4.0);
    REQUIRE_FALSE(r.sch.anyRecording());
    const auto log = r.drain();
    REQUIRE(log.size() == 1);
    REQUIRE(log[0].kind == LaunchKind::Record);
}

TEST_CASE("Scheduler: record tự do → recordStop → làm tròn LÊN theo quantize, tối thiểu 1 bar", "[core][scheduler][record]") {
    Rig r;
    r.play();
    r.sch.record(r.tr, 0, 0, 0);   // tự do, bắt đầu beat 0
    r.runTo(1);
    REQUIRE(r.at(0, 0) == LE_CLIP_RECORDING);
    r.runTo(24000 * 5 + 100);   // beat 5.0…
    r.sch.recordStop(r.tr, 0);  // → làm tròn lên 8 beat (2 bar)
    REQUIRE(r.at(0, 0) == LE_CLIP_RECORDING);
    r.runTo(2 * kBar + 1);
    REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    REQUIRE(r.sch.clipLength(0, 0) == 8.0);

    Rig r2;   // dừng sớm < 1 bar → vẫn tối thiểu 1 bar
    r2.play();
    r2.tr.setQuantize(LE_Q_1_4);
    r2.sch.record(r2.tr, 0, 0, 0);
    r2.runTo(24000);
    r2.sch.recordStop(r2.tr, 0);
    r2.runTo(kBar + 1);
    REQUIRE(r2.sch.clipLength(0, 0) == 4.0);
}

TEST_CASE("Scheduler: count-in 1 bar — thu bắt đầu sau ranh giới + 1 bar, metronome được báo", "[core][scheduler][record]") {
    Rig r;
    r.sch.setCountIn(1);
    r.play();
    r.runTo(10000);
    r.sch.record(r.tr, 0, 0, 1);
    REQUIRE_FALSE(r.sch.countingIn(r.tr, 10000));
    r.runTo(kBar + 1);
    REQUIRE(r.at(0, 0) == LE_CLIP_QUEUED_RECORD);   // đang count-in
    REQUIRE(r.sch.countingIn(r.tr, kBar));
    REQUIRE(r.sch.countingIn(r.tr, 2 * kBar - 1));
    REQUIRE_FALSE(r.sch.countingIn(r.tr, 2 * kBar));
    r.runTo(2 * kBar + 1);
    REQUIRE(r.at(0, 0) == LE_CLIP_RECORDING);
}

TEST_CASE("Scheduler: huỷ record trước khi bắt đầu → Empty; record vào ô có clip bị bỏ qua", "[core][scheduler][record]") {
    Rig r;
    r.clip(0, 1);
    r.play();
    r.runTo(100);
    r.sch.record(r.tr, 0, 0, 1);
    r.sch.recordStop(r.tr, 0);
    REQUIRE(r.at(0, 0) == LE_CLIP_EMPTY);
    r.sch.record(r.tr, 0, 1, 1);
    REQUIRE(r.at(0, 1) == LE_CLIP_STOPPED);
}

TEST_CASE("Scheduler: Playing → overdub → Overdubbing → overdub → Playing", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0);
    r.play();
    r.sch.launch(r.tr, 0, 0);
    r.runTo(10);
    r.sch.overdubToggle(0);
    REQUIRE(r.at(0, 0) == LE_CLIP_OVERDUBBING);
    r.sch.overdubToggle(0);
    REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    r.sch.overdubToggle(5);   // track không phát gì: bỏ qua
    REQUIRE(r.at(5, 0) == LE_CLIP_EMPTY);
}

TEST_CASE("Scheduler: scene với ô trống — track đó dừng, cùng ranh giới", "[core][scheduler][scene]") {
    Rig r;
    r.clip(0, 0);
    r.clip(1, 0);
    r.clip(0, 1);
    r.play();
    r.sch.sceneLaunch(r.tr, 0);
    r.runTo(1);
    REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    REQUIRE(r.at(1, 0) == LE_CLIP_PLAYING);
    r.runTo(36000);
    r.sch.sceneLaunch(r.tr, 1);
    r.runTo(kBar + 1);
    REQUIRE(r.at(0, 1) == LE_CLIP_PLAYING);
    REQUIRE(r.at(0, 0) == LE_CLIP_STOPPED);
    REQUIRE(r.at(1, 0) == LE_CLIP_STOPPED);
    REQUIRE(r.sch.playingSlot(1) == -1);
    const auto log = r.drain();
    // scene 0 (beat 0): Scene, Launch t0, Launch t1 · scene 1 (beat 4): Scene, Launch t0, Stop t1
    REQUIRE(log.size() == 6);
    REQUIRE(log[0].kind == LaunchKind::Scene);
    REQUIRE(log[3].kind == LaunchKind::Scene);
    REQUIRE(log[3].beat == 4.0);
    REQUIRE(log[5].kind == LaunchKind::Stop);
}

TEST_CASE("Scheduler: stop all theo quantize", "[core][scheduler][scene]") {
    Rig r;
    for (int t = 0; t < 8; ++t) r.clip(t, 0);
    r.play();
    r.sch.sceneLaunch(r.tr, 0);
    r.runTo(kBar + 5000);
    r.sch.stopAll(r.tr);
    for (int t = 0; t < 8; ++t) REQUIRE(r.at(t, 0) == LE_CLIP_QUEUED_STOP);
    r.runTo(2 * kBar + 1);
    for (int t = 0; t < 8; ++t) REQUIRE(r.at(t, 0) == LE_CLIP_STOPPED);
}

TEST_CASE("Scheduler: transport stop — Recording bỏ take, Overdubbing giữ clip, Queued* huỷ (04 §3.4)", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0);
    r.clip(1, 0);
    r.clip(3, 0);
    r.play();
    r.sch.launch(r.tr, 0, 0);
    r.sch.launch(r.tr, 3, 0);
    r.sch.record(r.tr, 2, 0, 0);
    r.runTo(10);
    r.sch.overdubToggle(3);
    r.runTo(kBar + 30000);        // track 2 đã thu 1.25 bar
    REQUIRE(r.at(2, 0) == LE_CLIP_RECORDING);
    REQUIRE(r.at(3, 0) == LE_CLIP_OVERDUBBING);
    r.sch.launch(r.tr, 1, 0);     // đang chờ (QueuedPlay)
    r.sch.record(r.tr, 4, 0, 1);  // đang chờ (QueuedRecord)
    r.sch.transportStopping(r.tr);
    r.tr.stop();
    REQUIRE(r.at(0, 0) == LE_CLIP_STOPPED);
    REQUIRE(r.at(1, 0) == LE_CLIP_STOPPED);   // lệnh chờ bị huỷ
    REQUIRE(r.at(2, 0) == LE_CLIP_EMPTY);     // take dở bị bỏ
    REQUIRE_FALSE(r.sch.hasClip(2, 0));
    REQUIRE(r.at(3, 0) == LE_CLIP_STOPPED);   // overdub kết thúc, clip giữ lại
    REQUIRE(r.sch.hasClip(3, 0));
    REQUIRE(r.at(4, 0) == LE_CLIP_EMPTY);     // QueuedRecord bị huỷ
    for (int t = 0; t < 5; ++t) REQUIRE(r.sch.playingSlot(t) == -1);
    REQUIRE_FALSE(r.sch.anyRecording());
}

TEST_CASE("Scheduler: remap — clip bị xoá khi đang phát → dừng ngay; take RT chưa vào model được giữ", "[core][scheduler]") {
    Rig r;
    r.clip(0, 0);
    r.play();
    r.sch.launch(r.tr, 0, 0);
    r.runTo(10);
    r.noClip(0, 0);
    REQUIRE(r.at(0, 0) == LE_CLIP_EMPTY);
    REQUIRE(r.sch.playingSlot(0) == -1);

    r.sch.record(r.tr, 1, 0, 1);
    r.runTo(kBar + 1);
    r.runTo(2 * kBar + 1);
    REQUIRE(r.at(1, 0) == LE_CLIP_PLAYING);
    r.sch.remap(*r.snap);   // snapshot chưa có take này
    REQUIRE(r.at(1, 0) == LE_CLIP_PLAYING);   // không bị xoá
}

TEST_CASE("Scheduler: ranh giới đúng sample với mọi block size", "[core][scheduler]") {
    for (int block : {1, 64, 128, 256, 1024, 777}) {
        INFO("block " << block);
        Rig r;
        r.clip(0, 0);
        r.play();
        r.runTo(31200, block);
        r.sch.launch(r.tr, 0, 0);
        r.runTo(kBar, block);
        REQUIRE(r.at(0, 0) == LE_CLIP_QUEUED_PLAY);
        r.runTo(kBar + 1, block);
        REQUIRE(r.at(0, 0) == LE_CLIP_PLAYING);
    }
}

TEST_CASE("LaunchLog: 100 lần launch → log đúng thứ tự, đúng beat (P1-17)", "[core][scheduler][launchlog]") {
    Rig r;
    for (int s = 0; s < 8; ++s) r.clip(s % 8, s);
    r.tr.setQuantize(LE_Q_1_4);
    r.play();
    std::vector<std::pair<int, int>> expected;
    for (int i = 0; i < 100; ++i) {
        const int t = i % 8, s = (i * 3) % 8;
        r.clip(t, s);
        r.sch.launch(r.tr, t, s);
        expected.push_back({t, s});
        r.runTo(r.tr.samplePos() + 24000);   // mỗi lần 1 beat → mỗi launch rơi đúng 1 ranh giới 1/4
    }
    const auto log = r.drain();
    std::vector<LaunchEvent> launches;
    for (const auto& e : log)
        if (e.kind == LaunchKind::Launch) launches.push_back(e);
    REQUIRE(launches.size() == 100);
    for (int i = 0; i < 100; ++i) {
        REQUIRE(launches[(size_t) i].track == expected[(size_t) i].first);
        REQUIRE(launches[(size_t) i].slot == expected[(size_t) i].second);
        REQUIRE(launches[(size_t) i].beat == (double) i);   // launch ở beat i (đầu block = ranh giới) → áp dụng tại beat i
        if (i > 0) REQUIRE(launches[(size_t) i].beat > launches[(size_t) i - 1].beat);
    }
}

TEST_CASE("Scheduler: project reset / sửa ô → bỏ take RT đang giữ (epoch)", "[core][scheduler]") {
    Rig r;
    r.play();
    r.sch.record(r.tr, 1, 0, 1);
    r.runTo(kBar + 1);
    r.runTo(2 * kBar + 1);
    REQUIRE(r.at(1, 0) == LE_CLIP_PLAYING);   // take trên RT
    r.sch.remap(*r.snap);                     // snapshot chưa có take, cùng epoch → giữ
    REQUIRE(r.at(1, 0) == LE_CLIP_PLAYING);
    r.snap->projectEpoch += 1;                // project.open
    r.sch.remap(*r.snap);
    REQUIRE(r.at(1, 0) == LE_CLIP_EMPTY);
    REQUIRE(r.sch.playingSlot(1) == -1);

    r.sch.record(r.tr, 2, 3, 1);
    r.runTo(4 * kBar + 1);
    r.runTo(5 * kBar + 1);
    REQUIRE(r.at(2, 3) == LE_CLIP_PLAYING);
    r.snap->cellEpoch[2][3] += 1;             // clip.clear trên ô đó
    r.sch.remap(*r.snap);
    REQUIRE(r.at(2, 3) == LE_CLIP_EMPTY);
}
