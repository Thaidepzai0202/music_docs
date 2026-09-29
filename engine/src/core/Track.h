#pragma once
// Track (P1-13) [RT] trừ prepare(): nguồn âm của một cột grid → bus stereo riêng, Mixer cộng vào master.
// - Audio clip: dsp::AudioClipPlayer (lõi của 80, P1-12). Track khớp player với ClipScheduler ở đầu mỗi segment:
//   đổi slot → stop() + setClip(B) + start(launchBeat) (A fade-out, B fade-in từ đầu); retrigger → start().
// - Instrument: dsp::Sampler (80, P1-25). NOTE_ON/OFF áp dụng ngay (đầu block).
// - GenerationUser: còn voice / đầu đọc nào dùng dữ liệu của generation cũ thì RtEngine chưa thu hồi snapshot đó.
// - MIDI clip (P1-29): dsp::MidiClipPlayer (80) → danh sách nốt có offset trong segment → Sampler render tới offset
//   rồi mới noteOn/noteOff → nốt rơi đúng sample. Dừng / đổi clip → note-off mọi nốt đang giữ.
#include <cstdint>
#include <vector>

#include "core/BlockSplitter.h"
#include "core/ClipScheduler.h"
#include "core/GraphSnapshot.h"
#include "core/Recorder.h"
#include "dsp/AudioClipPlayer.h"
#include "dsp/MidiClipPlayer.h"
#include "dsp/Sampler.h"

namespace le::core {

class Track final : public GenerationUser {
public:
    // [main] Cấp phát bus + sampler + player.
    void prepare(double sampleRate, int maxBlock);

    // [RT] Snapshot mới (RtEngine::remap, sau ClipScheduler::remap).
    void remap(const GraphSnapshot& s, int index, const ClipScheduler& sch) noexcept [[clang::nonblocking]];
    // [RT] Đầu block thiết bị: bỏ nốt live của block trước.
    void beginBlock() noexcept [[clang::nonblocking]] { numLive_ = liveCursor_ = 0; }
    // [RT] Đầu khối (≤ maxBlock) bắt đầu ở frame `blockOffset` của block thiết bị: xoá bus.
    void beginChunk(int n, int blockOffset = 0) noexcept [[clang::nonblocking]];
    // [RT] Nốt live (bàn phím / pad trên màn hình / MIDI input, P4-02) tại frame `blockOffset` của block thiết bị.
    // Gọi theo thứ tự offset tăng dần, trước khi render block. Sampler nhận nốt ĐÚNG sample (trộn với nốt của clip MIDI).
    void addLiveNote(int blockOffset, int note, float velocity01, bool on) noexcept [[clang::nonblocking]] {
        if (numLive_ >= kMaxLiveNotes) return;
        live_[numLive_++] = {blockOffset, (std::int16_t) note, velocity01, on};
    }
    void dropPendingLiveNotes() noexcept [[clang::nonblocking]] { numLive_ = liveCursor_; }   // ALL_NOTES_OFF
    // [RT] Đầu segment: khớp player với trạng thái scheduler (vừa applyDue), rồi render segment vào bus.
    void renderSegment(const GraphSnapshot& s, int index, const ClipScheduler& sch, const Recorder& rec,
                       const Segment& seg, double samplesPerBeat) noexcept [[clang::nonblocking]];
    // [RT] Player còn đọc thẳng buffer thu (vòng đầu sau khi thu) → Recorder không được ghi đè đầu buffer.
    bool usingLiveTake() const noexcept [[clang::nonblocking]] { return liveBuf_ != nullptr || liveFade_ > 0; }

    void noteOn(int note, float velocity) noexcept [[clang::nonblocking]] { sampler_.noteOn(note, velocity); }
    void noteOff(int note) noexcept [[clang::nonblocking]] { sampler_.noteOff(note); }
    void allNotesOff(bool fast) noexcept [[clang::nonblocking]] { sampler_.allNotesOff(fast); }

    const float* busL() const noexcept { return busL_.data(); }
    const float* busR() const noexcept { return busR_.data(); }
    float* busLMutable() noexcept { return busL_.data(); }   // [RT] input monitoring cộng vào bus
    float* busRMutable() noexcept { return busR_.data(); }
    int activeVoices() const noexcept [[clang::nonblocking]] { return sampler_.activeVoices(); }
    float progress(double beat) const noexcept [[clang::nonblocking]] { return player_.progress(beat); }
    bool clipPlaying() const noexcept [[clang::nonblocking]] { return player_.isPlaying(); }
    bool clipStretched() const noexcept [[clang::nonblocking]] { return player_.isStretched(); }   // P3-10

    // GenerationUser
    bool usesGeneration(std::uint32_t g) const noexcept [[clang::nonblocking]] override {
        return sampler_.countVoicesUsing(g) > 0 || player_.usesGeneration(g) || midi_.usesGeneration(g);
    }
    void fastReleaseGeneration(std::uint32_t g) noexcept [[clang::nonblocking]] override { sampler_.fastReleaseGeneration(g); }

private:
    void loadClip(const GraphSnapshot& s, int index, int slot) noexcept [[clang::nonblocking]];

    dsp::Sampler sampler_;
    dsp::AudioClipPlayer player_;
    dsp::MidiClipPlayer midi_;
    dsp::MidiEventList events_;   // sự kiện nốt của segment đang render (cố định 512)
    struct LiveNote {
        int offset;           // frame trong block thiết bị
        std::int16_t note;
        float velocity;
        bool on;
    };
    static constexpr int kMaxLiveNotes = 256;
    LiveNote live_[kMaxLiveNotes] = {};
    int numLive_ = 0, liveCursor_ = 0;
    int chunkBase_ = 0;           // frame đầu khối hiện tại trong block thiết bị
    std::vector<float> busL_, busR_;   // [main] cấp phát trong prepare, RT chỉ ghi
    std::uint32_t generation_ = 0;
    int lastSlot_ = -1;
    double lastLaunch_ = -1.0;
    bool hasData_ = false;                      // player đang có data audio cho ô đang phát
    const dsp::AudioData* liveBuf_ = nullptr;   // đang phát thẳng từ buffer thu (chưa có bản trong snapshot)
    double liveLength_ = 0.0;
    int liveFade_ = 0;                          // sample còn lại của crossfade live → bản snapshot
    int liveFadeLen_ = 248;                     // R8: kSwapFadeSec · SR + 8
};

} // namespace le::core
