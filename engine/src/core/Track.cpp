#include "core/Track.h"

#include <algorithm>
#include <cmath>

#include "core/Mixer.h"

namespace le::core {

void Track::prepare(double sampleRate, int maxBlock) {
    busL_.assign((size_t) maxBlock, 0.0f);
    busR_.assign((size_t) maxBlock, 0.0f);
    sampler_.prepare(sampleRate, maxBlock);
    player_.prepare(sampleRate, maxBlock);
    midi_.prepare(sampleRate, maxBlock);
    lastSlot_ = -1;
    lastLaunch_ = -1.0;
    // R8: đợi hết crossfade đổi data của player (kSwapFadeSec) + lề Hermite trước khi nhả buffer thu.
    liveFadeLen_ = (int) std::ceil(dsp::AudioClipPlayer::kSwapFadeSec * sampleRate) + 8;
    liveBuf_ = nullptr;   // restart device: buffer thu có thể được cấp lại (R7) → không giữ con trỏ cũ
    liveFade_ = 0;
}

// Nạp clip của ô `slot` (hoặc gỡ nếu ô không có audio) vào player với generation hiện tại.
void Track::loadClip(const GraphSnapshot& s, int index, int slot) noexcept [[clang::nonblocking]] {
    const ClipSnapshot* c = slot >= 0 ? &s.clips[index][slot] : nullptr;
    const bool audio = c != nullptr && c->kind == ClipKind::Audio && c->audio != nullptr;
    const bool midi = c != nullptr && c->kind == ClipKind::Midi && c->midi != nullptr;
    if (audio) player_.setClip(c->audio.get(), c->lengthBeats, c->originalBpm, Mixer::dbToLin(c->gainDb), s.generation);
    else player_.setClip(nullptr, 0.0, 0.0, 0.0f, s.generation);
    // P3-10: SAU setClip. Player chuyển Re-Pitch → Stretched ở ranh giới bar khi BPM khớp, quay lại ngay khi lệch.
    player_.setStretched(audio ? c->stretched.get() : nullptr, audio ? c->stretchedBpm : 0.0, s.generation);
    midi_.setClip(midi ? c->midi.get() : nullptr, s.generation);
    hasData_ = audio || midi;
}

void Track::remap(const GraphSnapshot& s, int index, const ClipScheduler& sch) noexcept [[clang::nonblocking]] {
    generation_ = s.generation;
    player_.setBeatsPerBar(s.beatsPerBar);
    const TrackSnapshot& ts = s.tracks[index];
    sampler_.setInstrument(ts.kind == TrackKind::Instrument ? ts.instrument.get() : nullptr, s.generation);
    // Luôn gọi setClip mỗi lần remap: player giữ con trỏ data kể cả khi im lặng (usesGeneration), nên phải
    // chuyển nó sang generation mới (cùng data → rẻ) hoặc gỡ (ô trống) để snapshot cũ được thu hồi.
    // Cùng ô mà data đổi (thay take) → player tự crossfade cùng pha.
    const int slot = sch.playingSlot(index);
    const bool snapHasAudio = slot >= 0 && s.clips[index][slot].kind == ClipKind::Audio && s.clips[index][slot].audio != nullptr;
    if (liveBuf_ != nullptr && slot == lastSlot_ && !snapHasAudio) {
        // Snapshot mới (do lệnh khác) chưa có take → tiếp tục phát từ buffer thu, chỉ cập nhật generation.
        player_.setClip(liveBuf_, liveLength_, 0.0, 1.0f, s.generation);
        player_.setStretched(nullptr, 0.0, s.generation);
        midi_.setClip(nullptr, s.generation);
        return;
    }
    if (liveBuf_ != nullptr) {   // bản copy của take đã vào snapshot → crossfade cùng pha, đợi fade xong mới nhả buffer
        liveBuf_ = nullptr;
        liveFade_ = liveFadeLen_;
    }
    loadClip(s, index, slot);
}

void Track::beginChunk(int n, int blockOffset) noexcept [[clang::nonblocking]] {
    chunkBase_ = blockOffset;
    std::fill(busL_.begin(), busL_.begin() + n, 0.0f);
    std::fill(busR_.begin(), busR_.begin() + n, 0.0f);
}

void Track::renderSegment(const GraphSnapshot& s, int index, const ClipScheduler& sch, const Recorder& rec,
                          const Segment& seg, double samplesPerBeat) noexcept [[clang::nonblocking]] {
    const int slot = sch.playingSlot(index);
    const double launch = sch.launchBeat(index);
    events_.clear();
    if (slot != lastSlot_ || (slot >= 0 && std::fabs(launch - lastLaunch_) > 1e-12)) {   // retrigger = launchBeat mới
        if (slot < 0) {
            player_.stop();          // fade-out 5 ms (clip stop, transport stop, clip bị xoá)
            midi_.stop(events_, 0);  // note-off mọi nốt đang giữ, ngay đầu segment
        } else {
            if (slot != lastSlot_) {   // clip khác: A fade-out ở đầu đọc cũ, B vào từ đầu
                player_.stop();
                midi_.stop(events_, 0);
                loadClip(s, index, slot);
            }
            player_.start(launch);   // retrigger cùng ô cũng đi đường này
            midi_.start(launch);
        }
        if (slot != lastSlot_) liveBuf_ = nullptr;
        lastSlot_ = slot;
        lastLaunch_ = launch;
    }
    // Take vừa thu xong (Recording → Playing) mà snapshot chưa có bản copy → phát vòng đầu thẳng từ buffer thu.
    if (slot >= 0 && !hasData_ && liveBuf_ == nullptr && !sch.isRecording(index)) {
        if (const dsp::AudioData* live = rec.liveTake(index, slot)) {
            liveBuf_ = live;
            liveLength_ = sch.clipLength(index, slot);
            player_.setClip(live, liveLength_, 0.0, 1.0f, generation_);   // originalBpm ≤ 0 → tốc độ gốc
            // Gốc pha = beat hiện tại (E = start + len → cùng pha với recordStart) để player coi đây là lần start
            // mới (chỉ fade-in), không phải điểm vòng lặp (sẽ crossfade/dip thêm và làm sample đầu = 0).
            player_.start(seg.startBeat);
            hasData_ = true;
        }
    }
    if (liveFade_ > 0) liveFade_ = std::max(0, liveFade_ - seg.numFrames);
    float* out[2] = {busL_.data(), busR_.data()};
    player_.render(out, 2, seg.startFrame, seg.numFrames, seg.startBeat, samplesPerBeat);

    // MIDI clip → nốt có offset; nốt live (P4-02) cũng có offset. Sampler render từng đoạn giữa các sự kiện, trộn
    // hai nguồn theo thời gian (cùng offset: nốt của clip trước) → đúng sample, không phụ thuộc block.
    midi_.process(seg.startBeat, seg.numFrames, samplesPerBeat, events_);
    const int segBase = chunkBase_ + seg.startFrame;   // frame đầu segment trong block thiết bị
    constexpr int kNone = 1 << 30;
    int pos = 0, ci = 0;
    for (;;) {
        const int co = ci < events_.size() ? events_[ci].offset : kNone;
        int lo = kNone;
        if (liveCursor_ < numLive_ && live_[liveCursor_].offset < segBase + seg.numFrames)
            lo = std::max(0, live_[liveCursor_].offset - segBase);
        const int o = std::min(co, lo);
        if (o == kNone) break;
        if (o > pos) {
            sampler_.render(out, 2, seg.startFrame + pos, o - pos);
            pos = o;
        }
        if (co <= lo) {
            const dsp::MidiEvent& e = events_[ci++];
            if (e.on) sampler_.noteOn(e.note, (float) e.velocity / 127.0f);
            else sampler_.noteOff(e.note);
        } else {
            const LiveNote& n = live_[liveCursor_++];
            if (n.on) sampler_.noteOn(n.note, n.velocity);
            else sampler_.noteOff(n.note);
        }
    }
    sampler_.render(out, 2, seg.startFrame + pos, seg.numFrames - pos);
}

} // namespace le::core
