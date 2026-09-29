// MidiClipPlayer — lõi phát MIDI clip lặp theo beat (04 §7, P1-29). [RT] trừ prepare().
// Mỗi segment, player sinh danh sách note-on/off kèm OFFSET (sample tính từ đầu segment). Caller
// (Track của 68) chia segment tại các offset đó rồi gọi Sampler::noteOn/noteOff (04 §2.2: nốt là ranh giới).
//
// Chính xác từng sample:
//   sample của nốt = sample đầu tiên có beat ≥ beat của nốt
//   offset = ceil((beatNốt − segStartBeat) · samplesPerBeat − 1e−6)      (giống Transport::sampleAtBeat)
//   Nốt ở beat 0.5 @120 BPM, 48 kHz → sample 12000, bất kể block chia thế nào.
// Mỗi nốt phát ĐÚNG MỘT LẦN: player giữ con trỏ "nốt kế tiếp" (chỉ số + số vòng đã lặp) và chỉ tiến lên,
// không quét lại theo cửa sổ beat → ranh giới segment rơi vào đâu cũng không mất / trùng nốt.
// Note-off: bảng cố định 128 phần tử (1 ô mỗi cao độ) lưu beat tắt. Nốt dài quá điểm cuối vòng bị CẮT
// tại điểm loop. Bấm lại cao độ đang giữ → note-off rồi note-on cùng offset. stop() → note-off tất cả.
//
// An toàn RT: không cấp phát (mảng cố định), chỉ đọc MidiClip bất biến qua con trỏ thô.
#pragma once

#include "dsp/MidiClip.h"

#include <array>
#include <cstdint>

namespace le::dsp {

struct MidiEvent {
    int32_t offset = 0;      // sample tính từ đầu segment, 0 ≤ offset < n
    uint8_t note = 0;
    uint8_t velocity = 0;    // 1..127 với note-on; 0 với note-off
    bool    on = false;
};

// Danh sách sự kiện cố định dung lượng. Đầy thì bỏ bớt và đếm (không bao giờ cấp phát).
class MidiEventList {
public:
    static constexpr int kCapacity = 512;

    void clear() noexcept [[clang::nonblocking]] { count_ = 0; }
    bool push(const MidiEvent& e) noexcept [[clang::nonblocking]] {
        if (count_ >= kCapacity) { ++dropped_; return false; }
        events_[static_cast<size_t>(count_++)] = e;
        return true;
    }
    int size() const noexcept [[clang::nonblocking]] { return count_; }
    const MidiEvent& operator[](int i) const noexcept [[clang::nonblocking]] { return events_[static_cast<size_t>(i)]; }
    const MidiEvent* begin() const noexcept [[clang::nonblocking]] { return events_.data(); }
    const MidiEvent* end() const noexcept [[clang::nonblocking]] { return events_.data() + count_; }
    uint32_t dropped() const noexcept [[clang::nonblocking]] { return dropped_; }

    // Sắp theo offset; cùng offset thì note-off trước note-on (để bấm lại cao độ đang giữ hoạt động đúng).
    void sort() noexcept [[clang::nonblocking]];

private:
    std::array<MidiEvent, kCapacity> events_{};
    int count_ = 0;
    uint32_t dropped_ = 0;
};

class MidiClipPlayer {
public:
    // [main]
    void prepare(double sampleRate, int maxBlock);

    // [RT] Gán clip (gọi mỗi lần remap). Cùng con trỏ → chỉ cập nhật generation. nullptr → như stop() ở segment kế.
    // Khác con trỏ khi đang phát (sửa bằng piano roll, overdub MIDI, 04 §7) → áp dụng ở process() kế tiếp, cùng pha:
    //   • nốt đang giữ cao độ p: clip MỚI còn nốt p BAO TRÙM vị trí hiện tại (start ≤ pos < start + length, toạ độ
    //     clip) → ngân tiếp, giờ tắt = cuối nốt đó (cắt ở điểm loop). Overdub: chính nốt cũ → giờ tắt không đổi;
    //     kéo dài / thu ngắn nốt đang kêu → theo nội dung mới. Không còn → note-off ở offset 0 (Sampler release).
    //   • nốt kế tiếp định vị theo vị trí hiện tại trong clip mới: startBeat < pos → vòng sau (không bật giữa chừng);
    //     startBeat ≥ pos → đúng giờ trong vòng này. Nốt đã phát không phát lại → không treo, không trùng.
    void setClip(const MidiClip* clip, uint32_t generation) noexcept [[clang::nonblocking]];

    // [RT] Phát từ đầu clip, gốc pha tại launchBeat (thường = beat đầu segment hiện tại).
    void start(double launchBeat) noexcept [[clang::nonblocking]];

    // [RT] Dừng: note-off mọi nốt đang giữ tại `offset` (all-notes-off của track, 04 §7).
    void stop(MidiEventList& out, int offset = 0) noexcept [[clang::nonblocking]];

    // [RT] Sinh sự kiện của segment gồm n sample bắt đầu ở segStartBeat, THÊM vào out (đã sort).
    void process(double segStartBeat, int n, double samplesPerBeat, MidiEventList& out) noexcept
        [[clang::nonblocking]];

    bool usesGeneration(uint32_t generation) const noexcept [[clang::nonblocking]] {
        return clip_ != nullptr && generation_ == generation;
    }
    bool isPlaying() const noexcept [[clang::nonblocking]] { return playing_; }
    int  heldNotes() const noexcept [[clang::nonblocking]];
    float progress(double beat) const noexcept [[clang::nonblocking]];

private:
    static int32_t offsetOf(double beat, double segStartBeat, double spb) noexcept [[clang::nonblocking]];
    void seek(double beat, MidiEventList& out) noexcept [[clang::nonblocking]];

    const MidiClip* clip_ = nullptr;
    uint32_t generation_ = 0;
    bool     playing_ = false;
    bool     needSeek_ = false;
    double   launchBeat_ = 0.0;
    int64_t  loop_ = 0;           // vòng hiện tại của con trỏ (0 = vòng đầu sau launch)
    size_t   next_ = 0;           // chỉ số nốt kế tiếp trong vòng loop_
    std::array<double, 128> offBeat_{};   // beat tắt của từng cao độ; < 0 = không giữ
};

} // namespace le::dsp
