#include "core/Recorder.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace le::core {

void Recorder::prepare(double sampleRate) noexcept {
    tail_ = std::max(1, (int) std::ceil(kTailSeconds * sampleRate));
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        rec_[t] = Rec{};
        live_[t] = nullptr;
        liveSlot_[t] = -1;
        // head_ / pending_ giữ nguyên: main có thể còn đang copy take cũ
    }
}

void Recorder::armOverdub(int t, const OverdubTicket& ticket) noexcept [[clang::nonblocking]] {
    Overdub& od = od_[t];
    od = Overdub{};
    od.armed = true;
    od.slot = ticket.slot;
    od.session = ticket.session;
    od.target = ticket.target;
}

void Recorder::returnTicket(int t, const OverdubTicket& ticket, RtToNrtQueue& out) noexcept [[clang::nonblocking]] {
    RtMessage m;
    m.kind = RtMessage::OverdubFinished;
    m.a = t;
    m.b = ticket.slot;
    m.i0 = ticket.session;
    m.i1 = 0;   // không ghi gì
    if (!out.try_push(m)) ++lost_;
}

// Trả vé của lượt (đã ghi hoặc chưa) → main nhả target. Sau đó RT KHÔNG còn con trỏ nào tới target.
void Recorder::finishOverdub(int t, bool wrote, RtToNrtQueue& out) noexcept [[clang::nonblocking]] {
    Overdub& od = od_[t];
    RtMessage m;
    m.kind = RtMessage::OverdubFinished;
    m.a = t;
    m.b = od.slot;
    m.i0 = od.session;
    m.i1 = wrote ? 1 : 0;
    if (!out.try_push(m)) ++lost_;   // queue đầy: main không bao giờ nhả target (rò bộ nhớ, không crash)
    od = Overdub{};
}

void Recorder::processOverdub(const Transport& tr, const ClipScheduler& sch, const GraphSnapshot& snap,
                              std::int64_t chunkStart, const float* in0, const float* in1, int n, int latencySamples,
                              RtToNrtQueue& out) noexcept [[clang::nonblocking]] {
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        Overdub& od = od_[t];
        if (!od.armed && !od.active) continue;
        const int slot = sch.playingSlot(t);
        const bool overdubbing = slot >= 0 && slot == od.slot && sch.cellState(t, slot) == LE_CLIP_OVERDUBBING;

        if (od.armed) {   // có vé: mở lượt khi snapshot đã chứa target (trễ tối đa 1 block sau lệnh)
            if (!overdubbing) {   // tắt / dừng clip trước khi kịp mở lượt → trả vé
                finishOverdub(t, false, out);
                continue;
            }
            const ClipSnapshot& c = snap.clips[t][slot];
            if (c.audio.get() != od.target) continue;
            const bool oneToOne = std::fabs(c.audio->sampleRate() - tr.sampleRate()) < 0.5 &&
                                  (c.originalBpm <= 0.0 || std::fabs(c.originalBpm - tr.bpm()) < 1e-6);
            if (!oneToOne) {   // BPM vừa đổi giữa lệnh bật và lúc mở lượt (RtEngine đã kiểm lúc bật)
                finishOverdub(t, false, out);
                continue;
            }
            od.armed = false;
            od.active = true;
            od.start = chunkStart;   // lệnh áp dụng ở đầu khối
            od.latency = std::max(0, latencySamples);
            od.launchSample = tr.sampleAtBeat(sch.launchBeat(t));
            od.lenFrames = std::min<std::int64_t>((std::int64_t) std::llround(c.lengthBeats * tr.samplesPerBeat()), od.target->numFrames());
        }
        if (od.end < 0 && !overdubbing) od.end = chunkStart;   // tắt overdub / dừng clip / transport stop

        // Input tại thời điểm t là tiếng người chơi nghe ở t − L → ghi vào vị trí vòng lặp của t − L.
        const std::int64_t from = std::max(chunkStart, od.start + od.latency);
        const std::int64_t to = std::min<std::int64_t>(chunkStart + n, od.end >= 0 ? od.end + od.latency : chunkStart + n);
        if (tr.playing() && od.lenFrames > 0) {
            float* w0 = od.target->writePointer(0);
            float* w1 = od.target->numChannels() > 1 ? od.target->writePointer(1) : nullptr;
            for (std::int64_t time = from; time < to; ++time) {
                std::int64_t p = (time - od.latency - od.launchSample) % od.lenFrames;
                if (p < 0) p += od.lenFrames;
                const int i = (int) (time - chunkStart);
                const float x0 = in0 != nullptr ? in0[i] : 0.0f;
                w0[p] += x0;
                if (w1 != nullptr) w1[p] += in1 != nullptr ? in1[i] : x0;
            }
        }
        if (od.end >= 0 && (!tr.playing() || chunkStart + n >= od.end + od.latency)) finishOverdub(t, true, out);
    }
}

void Recorder::finishCapture(bool full, RtToNrtQueue& out) noexcept [[clang::nonblocking]] {
    RtMessage m;
    m.kind = RtMessage::CaptureFinished;
    m.a = (std::int32_t) cap_.id;
    m.i0 = capPos_;
    m.i1 = full ? 1 : 0;
    if (!out.try_push(m)) ++lost_;   // queue đầy: main giữ buffer tới khi huỷ engine (không crash)
    cap_ = CaptureTicket{};
}

void Recorder::processCapture(const float* in0, const float* in1, int n, RtToNrtQueue& out) noexcept
    [[clang::nonblocking]] {
    if (CaptureTicket* tk = capTicket_.exchange(nullptr, std::memory_order_acq_rel)) {   // lượt mới
        if (cap_.buf != nullptr) finishCapture(false, out);
        cap_ = *tk;
        capPos_ = 0;
        capProgress_.store((std::uint64_t) (cap_.id & kCaptureIdMask) << 40, std::memory_order_release);
    }
    if (cap_.buf == nullptr) return;
    if (capStop_.load(std::memory_order_acquire) == cap_.id) {   // main đã stop (đã ghi file phần [0, pos))
        finishCapture(false, out);
        return;
    }
    const int m = (int) std::min<std::int64_t>(n, cap_.buf->numFrames() - capPos_);
    float* w0 = cap_.buf->writePointer(0);
    float* w1 = cap_.buf->numChannels() > 1 ? cap_.buf->writePointer(1) : nullptr;
    for (int i = 0; i < m; ++i) {
        const float x0 = in0 != nullptr ? in0[i] : 0.0f;
        w0[capPos_ + i] = x0;
        if (w1 != nullptr) w1[capPos_ + i] = in1 != nullptr ? in1[i] : x0;
    }
    capPos_ += m;
    capProgress_.store(((std::uint64_t) (cap_.id & kCaptureIdMask) << 40) | (std::uint64_t) capPos_, std::memory_order_release);
    if (capPos_ >= cap_.buf->numFrames()) finishCapture(true, out);   // maxSeconds
}

void Recorder::addNote(int track, double beat, int pitch, int velocity127, bool on) noexcept [[clang::nonblocking]] {
    if (numNotes_ >= kMaxNotesPerBlock) return;
    notes_[numNotes_++] = {beat, (std::int16_t) pitch, (std::int16_t) velocity127, (std::int8_t) track, on};
}

void Recorder::process(const Transport& tr, const ClipScheduler& sch, std::int64_t chunkStart, const float* in0,
                       const float* in1, int n, int latencySamples, const bool* liveInUse, const bool* isMidi,
                       std::uint32_t projectEpoch, const std::uint32_t (*cellEpoch)[LE_MAX_SCENES], RtToNrtQueue& out) noexcept
    [[clang::nonblocking]] {
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        Rec& r = rec_[t];
        const bool schedRec = sch.isRecording(t);

        // 1a) Take MIDI mới (track instrument): không cần buffer, không bù latency
        if (!r.active && schedRec && isMidi[t]) {
            r = Rec{};
            r.active = true;
            r.midi = true;
            r.slot = sch.playingSlot(t);
            r.startBeat = sch.recordStartBeat(t);
            r.B = tr.sampleAtBeat(r.startBeat);
            r.projectEpoch = projectEpoch;
            r.cellEpoch = cellEpoch[t][r.slot];
        }
        // 1b) Take audio mới (scheduler vừa chuyển sang Recording ở một segment của khối này)
        if (!r.active && schedRec) {
            dsp::AudioData* b = buf_[t].load(std::memory_order_acquire);
            if (b == nullptr) {   // Engine luôn cấp phát trước khi push CLIP_RECORD → không nên xảy ra
                ++lost_;
                continue;
            }
            if (pending_[t].load(std::memory_order_acquire) == 0 && !liveInUse[t]) {
                head_[t] = 0;   // mọi take cũ đã được main copy → dùng lại từ đầu
                live_[t] = nullptr;
                liveSlot_[t] = -1;
            }
            r = Rec{};
            r.active = true;
            r.buf = b;
            r.slot = sch.playingSlot(t);
            r.startBeat = sch.recordStartBeat(t);
            r.B = tr.sampleAtBeat(r.startBeat);
            r.latency = std::max(0, latencySamples);   // chốt L lúc bắt đầu take
            r.captureStart = r.B + r.latency;
            r.base = head_[t];
            r.bpm = tr.bpm();
            r.projectEpoch = projectEpoch;
            r.cellEpoch = cellEpoch[t][r.slot];
            if (r.base == 0) {   // vòng đầu phát thẳng từ buffer này (xem Recorder.h)
                live_[t] = b;
                liveSlot_[t] = r.slot;
            }
        }
        if (!r.active) continue;

        // 2) Take bị bỏ (transport stop, project.open, ô bị xoá…) → dừng, không báo main
        if (!schedRec && !sch.hasClip(t, r.slot)) {
            if (liveSlot_[t] == r.slot) {
                live_[t] = nullptr;
                liveSlot_[t] = -1;
            }
            r.active = false;
            continue;
        }

        // 3) Biết điểm kết thúc: take cố định / tự do đã chốt, hoặc scheduler đã chuyển sang Playing
        if (r.E < 0) {
            const double endBeat = schedRec ? sch.recordEndBeat(t) : r.startBeat + sch.clipLength(t, r.slot);
            if (endBeat >= 0.0) {
                r.E = tr.sampleAtBeat(endBeat);
                r.lengthBeats = endBeat - r.startBeat;
                r.bpm = tr.bpm();   // pedal mode: tempo chỉ có khi vòng đầu chốt (04 §2.5)
            }
        }

        // 4-MIDI) Nốt trong cửa sổ take [start, end) → main; tới E thì báo take xong (nốt gửi TRƯỚC, cùng queue)
        if (r.midi) {
            for (int k = 0; k < numNotes_; ++k) {
                const Note& nt = notes_[k];
                if (nt.track != t || nt.beat < r.startBeat - 1e-9) continue;
                if (r.E >= 0 && tr.sampleAtBeat(nt.beat) >= r.E) continue;
                RtMessage m;
                m.kind = RtMessage::MidiNote;
                m.a = t;
                m.b = r.slot;
                m.value = nt.beat - r.startBeat;
                m.i0 = nt.pitch;
                m.i1 = nt.on ? nt.velocity : 0;
                if (!out.try_push(m)) ++lost_;
            }
            if (r.E >= 0 && chunkStart + n >= r.E) {
                RtMessage m;
                m.kind = RtMessage::MidiTakeFinished;
                m.a = t;
                m.b = r.slot;
                m.value = r.lengthBeats;
                m.projectEpoch = r.projectEpoch;
                m.cellEpoch = r.cellEpoch;
                if (!out.try_push(m)) ++lost_;
                r.active = false;
            }
            continue;
        }

        // 4) Ghi input có thời điểm trong [captureStart, E + L + đuôi)
        const std::int64_t cap = r.buf->numFrames();
        const std::int64_t stop = r.E >= 0 ? r.E + r.latency + tail_ : std::numeric_limits<std::int64_t>::max();
        const std::int64_t from = std::max(chunkStart, r.captureStart);
        const std::int64_t to = std::min(chunkStart + n, stop);
        const int chs = r.buf->numChannels();
        float* w0 = r.buf->writePointer(0);
        float* w1 = chs > 1 ? r.buf->writePointer(1) : nullptr;
        for (std::int64_t time = from; time < to; ++time) {
            const std::int64_t idx = r.base + (time - r.captureStart);
            if (idx >= cap) break;   // hết buffer (take > 64 s): phần sau mất, take vẫn kết thúc bình thường
            const int i = (int) (time - chunkStart);
            const float x0 = in0 != nullptr ? in0[i] : 0.0f;
            w0[idx] = x0;
            if (w1 != nullptr) w1[idx] = in1 != nullptr ? in1[i] : x0;
        }

        // 5) Đã có đủ E + L + đuôi → báo main để copy thành clip
        if (r.E >= 0 && chunkStart + n >= stop) {
            const std::int64_t frames = std::min<std::int64_t>((r.E - r.B) + tail_, cap - r.base);
            RtMessage m;
            m.kind = RtMessage::TakeFinished;
            m.a = t;
            m.b = r.slot;
            m.value = r.lengthBeats;
            m.value2 = r.bpm;
            m.i0 = r.base;
            m.i1 = frames;
            m.projectEpoch = r.projectEpoch;
            m.cellEpoch = r.cellEpoch;
            pending_[t].fetch_add(1, std::memory_order_acq_rel);   // trước khi push: main không thể giảm trước
            if (!out.try_push(m)) {
                pending_[t].fetch_sub(1, std::memory_order_acq_rel);
                ++lost_;
            } else {
                head_[t] = r.base + frames;
            }
            r.active = false;
        }
    }
    numNotes_ = 0;
}

} // namespace le::core
