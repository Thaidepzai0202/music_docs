#include "core/ClipScheduler.h"

#include <algorithm>
#include <cmath>

namespace le::core {

void ClipScheduler::log(double beat, int t, int s, LaunchKind k) noexcept [[clang::nonblocking]] {
    if (log_ == nullptr) return;
    LaunchEvent e;
    e.beat = beat;
    e.track = (std::int8_t) t;
    e.slot = (std::int8_t) s;
    e.kind = k;
    if (!log_->try_push(e)) ++droppedLog_;   // main đọc chậm: bỏ, không chờ
}

// Huỷ lệnh đang chờ của track, trả các ô về trạng thái trước khi có lệnh.
void ClipScheduler::cancelPending(int t) noexcept [[clang::nonblocking]] {
    TrackState& ts = tracks_[t];
    const int ps = ts.pendingSlot;
    switch (ts.pending) {
        case Pending::Launch:
            if (ps != ts.playing) cell_[t][ps] = hasClip_[t][ps] ? LE_CLIP_STOPPED : LE_CLIP_EMPTY;
            break;
        case Pending::Record: cell_[t][ps] = LE_CLIP_EMPTY; break;
        case Pending::None:
        case Pending::Stop: break;
    }
    if (ts.playing >= 0) cell_[t][ts.playing] = ts.soundState;   // bỏ dấu QueuedStop/QueuedPlay (retrigger)
    ts.pending = Pending::None;
    ts.pendingSlot = -1;
}

void ClipScheduler::launchAt(int t, int s, double due) noexcept [[clang::nonblocking]] {
    if (!hasClip_[t][s]) {   // ô trống = dừng track (04 §3.1)
        stopAt(t, due);
        return;
    }
    cancelPending(t);
    TrackState& ts = tracks_[t];
    ts.pending = Pending::Launch;
    ts.pendingSlot = (std::int8_t) s;
    ts.dueBeat = due;
    cell_[t][s] = LE_CLIP_QUEUED_PLAY;   // retrigger (s == playing) cũng hiện QueuedPlay tới ranh giới
    if (ts.playing >= 0 && ts.playing != s && ts.soundState != LE_CLIP_RECORDING) cell_[t][ts.playing] = LE_CLIP_QUEUED_STOP;
}

void ClipScheduler::stopAt(int t, double due) noexcept [[clang::nonblocking]] {
    cancelPending(t);   // QueuedPlay chưa tới ranh giới → Stopped
    TrackState& ts = tracks_[t];
    if (ts.playing < 0) return;
    ts.pending = Pending::Stop;
    ts.dueBeat = due;
    if (ts.soundState != LE_CLIP_RECORDING) cell_[t][ts.playing] = LE_CLIP_QUEUED_STOP;
}

void ClipScheduler::launch(const Transport& tr, int track, int slot) noexcept [[clang::nonblocking]] {
    launchAt(track, slot, tr.boundary(tr.beatNow()));
}

void ClipScheduler::stop(const Transport& tr, int track) noexcept [[clang::nonblocking]] {
    stopAt(track, tr.boundary(tr.beatNow()));
}

void ClipScheduler::sceneLaunch(const Transport& tr, int scene) noexcept [[clang::nonblocking]] {
    const double due = tr.boundary(tr.beatNow());   // mọi track dùng CÙNG targetBeat (04 §3.3)
    for (int t = 0; t < LE_MAX_TRACKS; ++t) launchAt(t, scene, due);   // ô trống → stopAt
    pendingScene_ = scene;
    sceneDue_ = due;
}

void ClipScheduler::stopAll(const Transport& tr) noexcept [[clang::nonblocking]] {
    const double due = tr.boundary(tr.beatNow());
    for (int t = 0; t < LE_MAX_TRACKS; ++t) stopAt(t, due);
}

void ClipScheduler::record(const Transport& tr, int track, int slot, int bars) noexcept [[clang::nonblocking]] {
    if (hasClip_[track][slot] || cell_[track][slot] != LE_CLIP_EMPTY) return;   // ô có clip: dùng OVERDUB_TOGGLE
    cancelPending(track);
    TrackState& ts = tracks_[track];
    const double start = tr.boundary(tr.beatNow());
    ts.pending = Pending::Record;
    ts.pendingSlot = (std::int8_t) slot;
    ts.countInStart = start;
    ts.dueBeat = start + (double) countInBars_ * tr.beatsPerBar();   // thu sau count-in (04 §2.3)
    ts.recordBars = bars;
    cell_[track][slot] = LE_CLIP_QUEUED_RECORD;
    if (ts.playing >= 0 && ts.soundState != LE_CLIP_RECORDING) cell_[track][ts.playing] = LE_CLIP_QUEUED_STOP;
}

void ClipScheduler::recordStop(const Transport& tr, int track) noexcept [[clang::nonblocking]] {
    TrackState& ts = tracks_[track];
    if (ts.pending == Pending::Record) {   // chưa bắt đầu thu → huỷ
        cancelPending(track);
        return;
    }
    if (ts.playing < 0 || ts.soundState != LE_CLIP_RECORDING || ts.recordEnd >= 0.0) return;
    // Take tự do: làm tròn LÊN tới bội số quantize, tối thiểu 1 bar (04 §5.2).
    const double unit = tr.quantizeLength() > 0.0 ? tr.quantizeLength() : 1.0;
    double len = std::ceil((tr.beatNow() - ts.recordStart - 1e-9) / unit) * unit;
    len = std::max(len, (double) tr.beatsPerBar());
    ts.recordEnd = ts.recordStart + len;
}

bool ClipScheduler::recordNow(const Transport& tr, int track, int slot) noexcept [[clang::nonblocking]] {
    if (hasClip_[track][slot] || cell_[track][slot] != LE_CLIP_EMPTY) return false;
    record(tr, track, slot, 0);
    TrackState& ts = tracks_[track];
    ts.countInStart = ts.dueBeat = tr.beatNow();   // không quantize, không count-in
    return true;
}

void ClipScheduler::recordStopExact(const Transport& tr, int track) noexcept [[clang::nonblocking]] {
    TrackState& ts = tracks_[track];
    if (ts.playing < 0 || ts.soundState != LE_CLIP_RECORDING || ts.recordEnd >= 0.0) return;
    ts.recordEnd = tr.beatNow();
}

void ClipScheduler::recordStopMultiple(const Transport& tr, int track, double unit) noexcept [[clang::nonblocking]] {
    TrackState& ts = tracks_[track];
    if (ts.pending == Pending::Record || unit <= 0.0) {
        recordStop(tr, track);
        return;
    }
    if (ts.playing < 0 || ts.soundState != LE_CLIP_RECORDING || ts.recordEnd >= 0.0) return;
    const double len = std::max(1.0, std::ceil((tr.beatNow() - ts.recordStart - 1e-9) / unit)) * unit;
    ts.recordEnd = ts.recordStart + len;
}

void ClipScheduler::overdubToggle(int track) noexcept [[clang::nonblocking]] {
    TrackState& ts = tracks_[track];
    if (ts.playing < 0) return;
    if (ts.soundState == LE_CLIP_PLAYING) ts.soundState = LE_CLIP_OVERDUBBING;
    else if (ts.soundState == LE_CLIP_OVERDUBBING) ts.soundState = LE_CLIP_PLAYING;
    else return;
    if (cell_[track][ts.playing] != LE_CLIP_QUEUED_STOP && cell_[track][ts.playing] != LE_CLIP_QUEUED_PLAY)
        cell_[track][ts.playing] = ts.soundState;
}

// Take kết thúc tại `endBeat` → thành clip Playing, vòng lặp nối tiếp liền mạch (launchBeat = recordStart).
void ClipScheduler::finishRecording(int t, double endBeat) noexcept [[clang::nonblocking]] {
    TrackState& ts = tracks_[t];
    const int s = ts.playing;
    const double len = endBeat - ts.recordStart;
    ts.recordEnd = -1.0;
    if (len <= 1e-9) {   // chưa thu được gì
        cell_[t][s] = LE_CLIP_EMPTY;
        hasClip_[t][s] = false;
        ts.playing = -1;
        ts.soundState = LE_CLIP_STOPPED;
        return;
    }
    hasClip_[t][s] = true;
    rtOwned_[t][s] = true;   // P1-21: main thêm clip vào model khi nhận RECORDING_FINISHED
    ownedEpoch_[t][s] = cellEpoch_[t][s];
    length_[t][s] = len;
    ts.soundState = LE_CLIP_PLAYING;
    ts.launchBeat = ts.recordStart;
    if (cell_[t][s] == LE_CLIP_RECORDING) cell_[t][s] = LE_CLIP_PLAYING;
}

void ClipScheduler::stopPlayingNow(int t, double beat) noexcept [[clang::nonblocking]] {
    TrackState& ts = tracks_[t];
    if (ts.playing < 0) return;
    if (ts.soundState == LE_CLIP_RECORDING) finishRecording(t, beat);
    if (ts.playing < 0) return;
    cell_[t][ts.playing] = hasClip_[t][ts.playing] ? LE_CLIP_STOPPED : LE_CLIP_EMPTY;
    ts.playing = -1;
    ts.soundState = LE_CLIP_STOPPED;
}

// 04 §3.4 (đã chốt): Recording → BỎ take dở (ô về Empty); Overdubbing → kết thúc overdub, GIỮ phần đã chồng
// (clip về Stopped); Queued* → huỷ lệnh chờ; còn lại → Stopped.
void ClipScheduler::transportStopping(const Transport& tr) noexcept [[clang::nonblocking]] {
    const double beat = tr.beatNow();
    pendingScene_ = -1;
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        cancelPending(t);
        TrackState& ts = tracks_[t];
        if (ts.playing < 0) continue;
        const int s = ts.playing;
        if (ts.soundState == LE_CLIP_RECORDING) {   // bỏ take dở: ghi thu chỉ vào ô trống nên ô về Empty
            cell_[t][s] = LE_CLIP_EMPTY;
            hasClip_[t][s] = false;
            rtOwned_[t][s] = false;
            length_[t][s] = 0.0;
            ts.recordEnd = -1.0;
            ts.playing = -1;
            ts.soundState = LE_CLIP_STOPPED;
        } else {
            stopPlayingNow(t, beat);   // Overdubbing: phần đã chồng nằm trong clip (P1-22), chỉ dừng
        }
        log(beat, t, -1, LaunchKind::Stop);
    }
}

void ClipScheduler::remap(const GraphSnapshot& snap) noexcept [[clang::nonblocking]] {
    const bool projectReset = snap.projectEpoch != projectEpoch_;   // project.open/close → bỏ mọi take RT đang giữ
    projectEpoch_ = snap.projectEpoch;
    for (int t = 0; t < LE_MAX_TRACKS; ++t)
        for (int s = 0; s < LE_MAX_SCENES; ++s) {
            cellEpoch_[t][s] = snap.cellEpoch[t][s];
            if (rtOwned_[t][s] && (projectReset || ownedEpoch_[t][s] != cellEpoch_[t][s])) rtOwned_[t][s] = false;
        }
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        TrackState& ts = tracks_[t];
        for (int s = 0; s < LE_MAX_SCENES; ++s) {
            const ClipSnapshot& c = snap.clips[t][s];
            if (c.present()) {
                hasClip_[t][s] = true;
                rtOwned_[t][s] = false;   // model đã có clip → snapshot là nguồn sự thật
                length_[t][s] = c.lengthBeats;
                if (cell_[t][s] == LE_CLIP_EMPTY) cell_[t][s] = LE_CLIP_STOPPED;
                continue;
            }
            if (rtOwned_[t][s]) continue;   // take vừa thu, main chưa kịp đưa vào model
            hasClip_[t][s] = false;
            length_[t][s] = 0.0;
            const std::uint8_t st = cell_[t][s];
            if (st == LE_CLIP_QUEUED_RECORD || st == LE_CLIP_RECORDING) continue;   // đang thu vào ô trống
            if (ts.pending == Pending::Launch && ts.pendingSlot == s) {
                ts.pending = Pending::None;
                ts.pendingSlot = -1;
            }
            if (ts.playing == s) {   // clip bị xoá khi đang phát → dừng ngay (clip player tự fade)
                ts.playing = -1;
                ts.soundState = LE_CLIP_STOPPED;
                if (ts.pending == Pending::Stop) ts.pending = Pending::None;
            }
            cell_[t][s] = LE_CLIP_EMPTY;
        }
    }
}

void ClipScheduler::addBoundaries(const Transport& tr, std::int64_t chunkStart, int n, BlockSplitter& sp) const noexcept
    [[clang::nonblocking]] {
    if (!tr.playing()) return;
    auto add = [&](double beat) noexcept [[clang::nonblocking]] {
        const std::int64_t f = tr.sampleAtBeat(beat) - chunkStart;
        if (f > 0 && f < n) sp.add((int) f);
    };
    if (pendingScene_ >= 0) add(sceneDue_);
    const double chunkBeat = tr.beatAt(chunkStart);
    const double chunkEndBeat = tr.beatAt(chunkStart + n);
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        const TrackState& ts = tracks_[t];
        if (ts.pending != Pending::None) {
            add(ts.dueBeat);
            if (ts.pending == Pending::Record && ts.countInStart < ts.dueBeat) add(ts.countInStart);
        }
        if (ts.playing < 0) continue;
        if (ts.soundState == LE_CLIP_RECORDING) {
            if (ts.recordEnd >= 0.0) add(ts.recordEnd);
            continue;
        }
        // Điểm loop của clip đang phát (04 §2.2), tối đa 4 điểm mỗi khối.
        const double len = length_[t][ts.playing];
        if (len <= 0.0) continue;
        double k = std::floor((chunkBeat - ts.launchBeat) / len) + 1.0;
        for (int i = 0; i < 4; ++i, k += 1.0) {
            const double b = ts.launchBeat + k * len;
            if (b >= chunkEndBeat) break;
            add(b);
        }
    }
}

void ClipScheduler::applyDue(const Transport& tr, std::int64_t segStart) noexcept [[clang::nonblocking]] {
    if (!tr.playing()) return;
    if (pendingScene_ >= 0 && tr.sampleAtBeat(sceneDue_) <= segStart) {
        log(sceneDue_, -1, pendingScene_, LaunchKind::Scene);
        pendingScene_ = -1;
    }
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        TrackState& ts = tracks_[t];
        // 1) Take có độ dài cố định / đã chốt tới lúc kết thúc → Playing
        if (ts.playing >= 0 && ts.soundState == LE_CLIP_RECORDING && ts.recordEnd >= 0.0 &&
            tr.sampleAtBeat(ts.recordEnd) <= segStart)
            finishRecording(t, ts.recordEnd);

        // 2) Lệnh đang chờ tới hạn
        if (ts.pending == Pending::None || tr.sampleAtBeat(ts.dueBeat) > segStart) continue;
        const Pending p = ts.pending;
        const int s = ts.pendingSlot;
        const double due = ts.dueBeat;
        ts.pending = Pending::None;
        ts.pendingSlot = -1;
        switch (p) {
            case Pending::Launch:
                if (ts.playing >= 0 && ts.playing != s) stopPlayingNow(t, due);   // A → Stopped cùng ranh giới
                if (!hasClip_[t][s]) break;                                     // clip bị xoá trong lúc chờ
                ts.playing = (std::int8_t) s;
                ts.soundState = LE_CLIP_PLAYING;
                ts.launchBeat = due;   // luôn phát từ đầu (04 §3.2)
                cell_[t][s] = LE_CLIP_PLAYING;
                log(due, t, s, LaunchKind::Launch);
                break;
            case Pending::Stop:
                if (ts.playing >= 0) {
                    stopPlayingNow(t, due);
                    log(due, t, -1, LaunchKind::Stop);
                }
                break;
            case Pending::Record:
                if (ts.playing >= 0) stopPlayingNow(t, due);
                ts.playing = (std::int8_t) s;
                ts.soundState = LE_CLIP_RECORDING;
                ts.launchBeat = ts.recordStart = due;
                ts.recordEnd = ts.recordBars > 0 ? due + (double) ts.recordBars * tr.beatsPerBar() : -1.0;
                cell_[t][s] = LE_CLIP_RECORDING;
                log(due, t, s, LaunchKind::Record);
                break;
            case Pending::None: break;
        }
    }
}

float ClipScheduler::progress(const Transport& tr, int t) const noexcept [[clang::nonblocking]] {
    const TrackState& ts = tracks_[t];
    if (ts.playing < 0) return 0.0f;
    const double beat = tr.beatNow();
    if (ts.soundState == LE_CLIP_RECORDING) {
        if (ts.recordEnd > ts.recordStart) return (float) std::clamp((beat - ts.recordStart) / (ts.recordEnd - ts.recordStart), 0.0, 1.0);
        const double bars = (beat - ts.recordStart) / tr.beatsPerBar();
        return (float) (bars - std::floor(bars));
    }
    const double len = length_[t][ts.playing];
    if (len <= 0.0) return 0.0f;
    const double x = std::fmod(beat - ts.launchBeat, len);
    return (float) ((x < 0.0 ? x + len : x) / len);
}

bool ClipScheduler::anyRecording() const noexcept [[clang::nonblocking]] {
    for (const auto& ts : tracks_)
        if (ts.playing >= 0 && ts.soundState == LE_CLIP_RECORDING) return true;
    return false;
}

bool ClipScheduler::countingIn(const Transport& tr, std::int64_t s) const noexcept [[clang::nonblocking]] {
    if (!tr.playing()) return false;
    for (const auto& ts : tracks_)
        if (ts.pending == Pending::Record && ts.countInStart < ts.dueBeat && tr.sampleAtBeat(ts.countInStart) <= s &&
            s < tr.sampleAtBeat(ts.dueBeat))
            return true;
    return false;
}

} // namespace le::core
