#pragma once
// Recorder (P1-19/20, 04 §5) [RT] trừ setBuffer / releaseTake / prepare.
//
// Bù latency (04 §5.3): người chơi khớp với tiếng họ NGHE, nhưng tiếng tới engine muộn đúng round-trip L.
//   Take bắt đầu ở ranh giới B (sample) → dữ liệu của take = input(B + L … E + L). RT ghi input tại thời điểm
//   t vào buffer[base + (t − (B + L))], ghi tới E + L + đuôi 3 ms (player cần ≥ 2 ms sau điểm loop để crossfade).
//
// Ownership (vì sao an toàn):
// - Buffer thu do MAIN cấp phát (TRACK_ARM / CLIP_RECORD, trước khi push lệnh) và giữ tới khi engine huỷ. RT nhận
//   con trỏ thô qua atomic (release/acquire), chỉ ghi vào đó. Không cấp phát, không huỷ trên RT.
// - Take xong → RT gửi TakeFinished {offset, frames} qua rtToNrt và tăng pending_. Main copy vùng đó ra AudioData
//   đúng kích thước, rồi releaseTake() (giảm pending_). Chỉ khi pending_ == 0 (và không track nào còn phát thẳng
//   từ buffer) RT mới ghi lại từ đầu buffer; take nối tiếp nhau (chưa copy xong) ghi vào vùng kế tiếp (arena).
// - Vòng đầu tiên sau khi thu: take có offset 0 được phát THẲNG từ buffer (liveTake) trong lúc main còn đang copy.
//   Đọc vị trí p ở thời điểm E + p cần dữ liệu ghi lúc B + L + p < E + p ⇔ L < độ dài take → luôn đúng.
#include <atomic>
#include <cstdint>

#include "core/ClipScheduler.h"
#include "core/GraphSnapshot.h"
#include "core/RtQueues.h"
#include "core/Transport.h"
#include "dsp/AudioData.h"

namespace le::core {

class Recorder {
public:
    static constexpr double kTailSeconds = 0.003;

    // [main] Khi audio chưa chạy.
    void prepare(double sampleRate) noexcept;

    // [main] Buffer thu của track (main sở hữu, sống tới khi engine huỷ). Gọi TRƯỚC khi push lệnh thu.
    void setBuffer(int t, dsp::AudioData* b) noexcept { buf_[t].store(b, std::memory_order_release); }
    // [main] Đã copy xong take của track t.
    void releaseTake(int t) noexcept { pending_[t].fetch_sub(1, std::memory_order_acq_rel); }

    // P1-22 + R1 (rt-review 2026-09-29): "vé" cho MỘT lượt overdub audio. Main tạo vé (bản copy `target` đã nằm
    // trong model + snapshot), giữ vé và target sống tới khi RT TRẢ vé qua OverdubFinished cùng `session`.
    // Trao vé bằng atomic exchange ở CẢ HAI phía → tại mọi thời điểm chỉ một bên giữ vé:
    //   main offerOverdub(t, vé mới) → nhận lại vé cũ RT chưa lấy (main huỷ được ngay), nullptr = RT đã lấy.
    //   RT takeOverdubTicket(t) khi bật overdub → vé dùng cho đúng một lượt; không dùng được thì trả ngay.
    // RT chỉ đọc vé lúc lấy (copy các trường), sau đó không đụng tới nữa.
    struct OverdubTicket {
        std::uint32_t session = 0;
        int slot = -1;
        dsp::AudioData* target = nullptr;
    };
    OverdubTicket* offerOverdub(int t, OverdubTicket* ticket) noexcept {   // [main]
        return odTicket_[t].exchange(ticket, std::memory_order_acq_rel);
    }
    OverdubTicket* takeOverdubTicket(int t) noexcept [[clang::nonblocking]] {   // [RT]
        return odTicket_[t].exchange(nullptr, std::memory_order_acq_rel);
    }
    // [RT] Vé hợp lệ: chờ snapshot chứa target rồi mở lượt ở processOverdub.
    void armOverdub(int t, const OverdubTicket& ticket) noexcept [[clang::nonblocking]];
    // [RT] Trả vé không dùng (OverdubFinished, không ghi gì) → main huỷ.
    void returnTicket(int t, const OverdubTicket& ticket, RtToNrtQueue& out) noexcept [[clang::nonblocking]];
    bool overdubBusy(int t) const noexcept [[clang::nonblocking]] { return od_[t].armed || od_[t].active; }

    // P3-02 capture (thu một MẪU cho sampler, không phải clip). Vé = buffer main cấp sẵn đủ maxSeconds. RT ghi
    // input từ đầu buffer, không phụ thuộc transport, tự dừng khi đầy; trả vé qua CaptureFinished. Tiến độ
    // (id, số frame đã ghi XONG) công bố qua MỘT atomic 64-bit (release) → main đọc đúng lượt của mình (acquire).
    static constexpr std::uint32_t kCaptureIdMask = 0xFFFFFFu;   // id 24 bit, frame 40 bit
    struct CaptureTicket {
        std::uint32_t id = 0;
        dsp::AudioData* buf = nullptr;
    };
    CaptureTicket* offerCapture(CaptureTicket* ticket) noexcept {   // [main] trả vé cũ RT chưa lấy
        return capTicket_.exchange(ticket, std::memory_order_acq_rel);
    }
    void stopCapture(std::uint32_t id) noexcept { capStop_.store(id, std::memory_order_release); }   // [main]
    std::int64_t captureFrames(std::uint32_t id) const noexcept {   // [main] 0 nếu RT chưa bắt đầu lượt `id`
        const std::uint64_t v = capProgress_.load(std::memory_order_acquire);
        return (std::uint32_t) (v >> 40) == (id & kCaptureIdMask) ? (std::int64_t) (v & ((1ull << 40) - 1)) : 0;
    }
    // [RT] Mỗi khối, sau process().
    void processCapture(const float* in0, const float* in1, int n, RtToNrtQueue& out) noexcept [[clang::nonblocking]];

    // [RT] Nốt (NOTE_ON/OFF) nhận ở đầu khối, beat = beat đầu khối. process() quyết định nốt nào thuộc take MIDI.
    void addNote(int track, double beat, int pitch, int velocity127, bool on) noexcept [[clang::nonblocking]];

    // [RT] Sau khi các segment của khối đã applyDue (scheduler biết take bắt đầu/kết thúc).
    // liveInUse[t]: track t còn đang phát thẳng từ buffer thu (không được ghi đè đầu buffer).
    // isMidi[t]: track instrument → take MIDI (không thu audio, không bù latency).
    void process(const Transport& tr, const ClipScheduler& sch, std::int64_t chunkStart, const float* in0,
                 const float* in1, int n, int latencySamples, const bool* liveInUse, const bool* isMidi,
                 std::uint32_t projectEpoch, const std::uint32_t (*cellEpoch)[LE_MAX_SCENES], RtToNrtQueue& out) noexcept
        [[clang::nonblocking]];
    // [RT] Overdub audio (04 §5.4): clip[p] += input đã bù latency, p = vị trí vòng lặp. Chỉ khi clip phát 1:1.
    void processOverdub(const Transport& tr, const ClipScheduler& sch, const GraphSnapshot& snap, std::int64_t chunkStart,
                        const float* in0, const float* in1, int n, int latencySamples, RtToNrtQueue& out) noexcept
        [[clang::nonblocking]];

    // [RT] Take của ô (t, slot) đang/đã thu vào ĐẦU buffer → phát thẳng được (nullptr nếu không).
    const dsp::AudioData* liveTake(int t, int slot) const noexcept [[clang::nonblocking]] {
        return liveSlot_[t] == slot ? live_[t] : nullptr;
    }
    bool capturing(int t) const noexcept [[clang::nonblocking]] { return rec_[t].active; }
    std::uint32_t lostTakes() const noexcept { return lost_; }

private:
    struct Note {
        double beat;
        std::int16_t pitch, velocity;
        std::int8_t track;
        bool on;
    };
    static constexpr int kMaxNotesPerBlock = 64;
    Note notes_[kMaxNotesPerBlock];
    int numNotes_ = 0;

    struct Rec {
        bool active = false;
        bool midi = false;
        int slot = -1;
        double startBeat = 0.0, lengthBeats = 0.0, bpm = 120.0;
        std::int64_t B = 0, E = -1, captureStart = 0, base = 0;
        int latency = 0;
        std::uint32_t projectEpoch = 0, cellEpoch = 0;
        dsp::AudioData* buf = nullptr;
    };

    Rec rec_[LE_MAX_TRACKS];
    std::int64_t head_[LE_MAX_TRACKS] = {};        // [RT] vùng trống kế tiếp trong buffer
    const dsp::AudioData* live_[LE_MAX_TRACKS] = {};
    int liveSlot_[LE_MAX_TRACKS] = {-1, -1, -1, -1, -1, -1, -1, -1};
    std::atomic<dsp::AudioData*> buf_[LE_MAX_TRACKS] = {};
    std::atomic<int> pending_[LE_MAX_TRACKS] = {};

    struct Overdub {
        bool armed = false;    // có vé, chờ snapshot chứa target
        bool active = false;   // đang ghi
        int slot = -1;
        std::uint32_t session = 0;
        dsp::AudioData* target = nullptr;
        std::int64_t start = 0, end = -1, launchSample = 0, lenFrames = 0;
        int latency = 0;
    };
    void finishOverdub(int t, bool wrote, RtToNrtQueue& out) noexcept [[clang::nonblocking]];
    Overdub od_[LE_MAX_TRACKS];
    std::atomic<OverdubTicket*> odTicket_[LE_MAX_TRACKS] = {};
    void finishCapture(bool full, RtToNrtQueue& out) noexcept [[clang::nonblocking]];
    std::atomic<CaptureTicket*> capTicket_{nullptr};
    std::atomic<std::uint32_t> capStop_{0};
    std::atomic<std::uint64_t> capProgress_{0};
    CaptureTicket cap_{};           // [RT] lượt đang thu (buf == nullptr = không)
    std::int64_t capPos_ = 0;
    int tail_ = 144;
    std::uint32_t lost_ = 0;
};

} // namespace le::core
