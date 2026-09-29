#pragma once
// ClipScheduler (P1-15/16/17, 04 §3) [RT]: máy trạng thái của 8×8 ô clip + quantize + scene + LaunchLog.
//
//            launch(q)                 boundary                 loop...
//  Stopped ──────────────► QueuedPlay ─────────────► Playing ◄──────────┐
//     ▲                        │ stop trước khi tới      │ └──────────────┘
//     │ boundary               ▼                         │ stop(q) / launch clip khác cùng track
//  QueuedStop ◄──────────────────────────────────────────┘
//  Empty ──record(q)──► QueuedRecord ──boundary(+countIn)──► Recording ──đủ độ dài / stop──► Playing
//  Playing ──overdub──► Overdubbing ──overdub lần nữa──► Playing
//
// - Mỗi track chỉ có 1 clip Playing/Recording/Overdubbing. Launch B khi A đang phát: A → QueuedStop,
//   B → QueuedPlay, đổi cùng một ranh giới.
// - Lệnh ghi `dueBeat = boundary(beat lúc nhận lệnh)`. Ranh giới được đổi sang SAMPLE bằng Transport,
//   BlockSplitter chia block tại đúng sample đó → launch rơi đúng sample bất kể blockSize.
// - Launch vào ô trống = dừng track (theo quantize). Launch lại clip đang phát = phát lại từ đầu tại ranh giới.
// - Người gọi (RtEngine) bảo đảm transport đang chạy trước launch/scene/record (transport dừng → play ở beat 0).
// Mọi mảng cố định, không cấp phát. Chỉ audio thread gọi (trừ setLaunchLog).
#include <cstdint>

#include <rigtorp/SPSCQueue.h>

#include "core/BlockSplitter.h"
#include "core/GraphSnapshot.h"
#include "core/Transport.h"
#include "le/engine_api.h"

namespace le::core {

// LaunchLog (04 §3.5): mọi launch/stop/record/scene THỰC SỰ xảy ra, theo beat. RT → main qua SPSC.
enum class LaunchKind : std::uint8_t { Launch = 0, Stop = 1, Record = 2, Scene = 3 };
struct LaunchEvent {
    double beat = 0.0;
    std::int8_t track = -1;   // -1 với Scene
    std::int8_t slot = -1;    // -1 với Stop
    LaunchKind kind = LaunchKind::Launch;
};
constexpr std::size_t kLaunchLogCapacity = 4096;
using LaunchLogQueue = rigtorp::SPSCQueue<LaunchEvent>;

class ClipScheduler {
public:
    // [main, audio chưa chạy]
    void setLaunchLog(LaunchLogQueue* q) noexcept { log_ = q; }

    // ── Lệnh [RT], gọi ở đầu block (beat = tr.beatNow()) ──
    void launch(const Transport& tr, int track, int slot) noexcept [[clang::nonblocking]];
    void stop(const Transport& tr, int track) noexcept [[clang::nonblocking]];
    void sceneLaunch(const Transport& tr, int scene) noexcept [[clang::nonblocking]];
    void stopAll(const Transport& tr) noexcept [[clang::nonblocking]];
    void record(const Transport& tr, int track, int slot, int bars) noexcept [[clang::nonblocking]];
    void recordStop(const Transport& tr, int track) noexcept [[clang::nonblocking]];
    void overdubToggle(int track) noexcept [[clang::nonblocking]];
    // Pedal mode (04 §2.5). recordNow: bắt đầu thu NGAY tại beat hiện tại (không quantize, không count-in); false nếu
    // ô đã có clip. recordStopExact: chốt take tự do đúng beat hiện tại (không làm tròn). recordStopMultiple: làm tròn
    // LÊN bội số `unit` beat tính từ đầu take (vòng sau của pedal mode: bội số vòng đầu).
    bool recordNow(const Transport& tr, int track, int slot) noexcept [[clang::nonblocking]];
    void recordStopExact(const Transport& tr, int track) noexcept [[clang::nonblocking]];
    void recordStopMultiple(const Transport& tr, int track, double unit) noexcept [[clang::nonblocking]];
    // Trạng thái TIẾNG của clip đang phát (PLAYING / OVERDUBBING), khác cellState khi ô đang mang dấu QueuedStop.
    int soundState(int track) const noexcept [[clang::nonblocking]] {
        return tracks_[track].playing >= 0 ? tracks_[track].soundState : LE_CLIP_STOPPED;
    }
    void setCountIn(int bars) noexcept [[clang::nonblocking]] { countInBars_ = bars < 0 ? 0 : (bars > 2 ? 2 : bars); }
    int countInBars() const noexcept [[clang::nonblocking]] { return countInBars_; }
    // Transport dừng NGAY (04 §3.4): Recording → bỏ take (Empty), Overdubbing → giữ phần đã chồng rồi Stopped,
    // Queued* → huỷ, còn lại → Stopped. Gọi TRƯỚC Transport::stop().
    void transportStopping(const Transport& tr) noexcept [[clang::nonblocking]];
    // Snapshot mới: cập nhật ô có clip / độ dài. Clip bị xoá khi đang phát → dừng ngay.
    void remap(const GraphSnapshot& s) noexcept [[clang::nonblocking]];

    // ── Mỗi block / segment [RT] ──
    // Thêm ranh giới (frame trong khối [chunkStart, chunkStart+n)) cho mọi lệnh đang chờ, kết thúc thu, điểm loop.
    void addBoundaries(const Transport& tr, std::int64_t chunkStart, int n, BlockSplitter& sp) const noexcept
        [[clang::nonblocking]];
    // Áp dụng mọi thứ đến hạn tại đầu segment (sample tuyệt đối `segStart`).
    void applyDue(const Transport& tr, std::int64_t segStart) noexcept [[clang::nonblocking]];

    // ── Trạng thái [RT] (RtEngine đưa vào LeState) ──
    std::uint8_t cellState(int t, int s) const noexcept [[clang::nonblocking]] { return cell_[t][s]; }
    bool hasClip(int t, int s) const noexcept [[clang::nonblocking]] { return hasClip_[t][s]; }
    double clipLength(int t, int s) const noexcept [[clang::nonblocking]] { return length_[t][s]; }
    // Slot đang phát ra tiếng (Playing / QueuedStop / Overdubbing / Recording), -1 nếu không.
    int playingSlot(int t) const noexcept [[clang::nonblocking]] { return tracks_[t].playing; }
    double launchBeat(int t) const noexcept [[clang::nonblocking]] { return tracks_[t].launchBeat; }
    float progress(const Transport& tr, int t) const noexcept [[clang::nonblocking]];
    // Recorder (P1-19): track đang ở trạng thái Recording, beat bắt đầu / kết thúc (−1 = tự do, chưa chốt).
    bool isRecording(int t) const noexcept [[clang::nonblocking]] {
        return tracks_[t].playing >= 0 && tracks_[t].soundState == LE_CLIP_RECORDING;
    }
    double recordStartBeat(int t) const noexcept [[clang::nonblocking]] { return tracks_[t].recordStart; }
    double recordEndBeat(int t) const noexcept [[clang::nonblocking]] { return tracks_[t].recordEnd; }
    bool anyRecording() const noexcept [[clang::nonblocking]];
    // Đang count-in tại sample `s` (metronome phải kêu). count-in = [ranh giới quantize, bắt đầu thu).
    bool countingIn(const Transport& tr, std::int64_t s) const noexcept [[clang::nonblocking]];
    std::uint32_t droppedLogEvents() const noexcept { return droppedLog_; }

private:
    enum class Pending : std::uint8_t { None, Launch, Stop, Record };
    struct TrackState {
        std::int8_t playing = -1;
        std::uint8_t soundState = LE_CLIP_STOPPED;   // trạng thái "thật" của ô đang phát: PLAYING/OVERDUBBING/RECORDING
        double launchBeat = 0.0;
        Pending pending = Pending::None;
        std::int8_t pendingSlot = -1;
        double dueBeat = 0.0;
        double countInStart = 0.0;   // Record: ranh giới quantize (count-in bắt đầu)
        int recordBars = 0;          // Record: 0 = tự do
        double recordStart = 0.0;
        double recordEnd = -1.0;     // < 0: tự do, chưa chốt
    };

    void launchAt(int t, int s, double due) noexcept [[clang::nonblocking]];
    void stopAt(int t, double due) noexcept [[clang::nonblocking]];
    void cancelPending(int t) noexcept [[clang::nonblocking]];
    void finishRecording(int t, double endBeat) noexcept [[clang::nonblocking]];
    void stopPlayingNow(int t, double beat) noexcept [[clang::nonblocking]];
    void log(double beat, int t, int s, LaunchKind k) noexcept [[clang::nonblocking]];
    bool sounding(std::uint8_t st) const noexcept [[clang::nonblocking]] {
        return st == LE_CLIP_PLAYING || st == LE_CLIP_QUEUED_STOP || st == LE_CLIP_OVERDUBBING || st == LE_CLIP_RECORDING;
    }

    std::uint8_t cell_[LE_MAX_TRACKS][LE_MAX_SCENES] = {};   // LeClipState
    bool hasClip_[LE_MAX_TRACKS][LE_MAX_SCENES] = {};
    bool rtOwned_[LE_MAX_TRACKS][LE_MAX_SCENES] = {};       // take vừa thu trên RT, snapshot chưa có (P1-21)
    std::uint32_t ownedEpoch_[LE_MAX_TRACKS][LE_MAX_SCENES] = {};   // cellEpoch lúc tạo take
    std::uint32_t cellEpoch_[LE_MAX_TRACKS][LE_MAX_SCENES] = {};    // từ snapshot gần nhất
    std::uint32_t projectEpoch_ = 0;
    double length_[LE_MAX_TRACKS][LE_MAX_SCENES] = {};
    TrackState tracks_[LE_MAX_TRACKS];
    int countInBars_ = 0;
    int pendingScene_ = -1;
    double sceneDue_ = 0.0;
    LaunchLogQueue* log_ = nullptr;
    std::uint32_t droppedLog_ = 0;
};

} // namespace le::core
