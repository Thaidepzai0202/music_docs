#pragma once
// GraphSnapshot (P1-06, 03 §4): bản "đóng băng" của EngineModel mà audio thread đọc.
//
// Ownership (vì sao an toàn):
// - Main/worker dựng snapshot, sau đó KHÔNG AI sửa nữa (bất biến).
// - Snapshot SỞ HỮU dữ liệu nặng qua shared_ptr (AudioData, Instrument, nốt MIDI). Audio thread chỉ MƯỢN con trỏ
//   thô (`.get()`): không bao giờ copy hay huỷ shared_ptr trên RT (08 §4).
// - Khi có snapshot mới, bản cũ vào mảng retiring[4] của RtEngine. Chỉ khi không còn voice/clip player nào đọc
//   `generation` của nó (GenerationUser), RtEngine mới gửi Retire về main qua rtToNrt. Main (ReleasePool) delete →
//   các shared_ptr được huỷ trên main.
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "dsp/AudioData.h"
#include "dsp/Instrument.h"
#include "dsp/MidiClip.h"
#include "dsp/Processor.h"
#include "le/engine_api.h"
#include "midi/MidiLearnMap.h"

namespace le::core {

enum class ClipKind : std::uint8_t { None, Audio, Midi };
enum class WarpMode : std::uint8_t { Repitch, Stretch };
enum class TrackKind : std::uint8_t { Audio, Instrument };

struct ClipSnapshot {
    ClipKind kind = ClipKind::None;
    double lengthBeats = 0.0;
    double originalBpm = 0.0;    // audio
    float gainDb = 0.0f;
    WarpMode warp = WarpMode::Repitch;
    dsp::AudioDataPtr audio;     // kind == Audio
    dsp::MidiClipPtr midi;       // kind == Midi (kiểu chung với MidiClipPlayer của 80)
    // P3-08: bản co giãn giữ cao độ tới stretchedBpm (WarpRenderer). nullptr = chưa có → Re-Pitch.
    // AudioClipPlayer tự chuyển sang bản này ở ranh giới bar khi BPM hiện tại khớp (P3-10).
    dsp::AudioDataPtr stretched;
    double stretchedBpm = 0.0;
    bool present() const noexcept [[clang::nonblocking]] { return kind != ClipKind::None; }
};

inline constexpr int kFxSlots = 3;       // FxChain mỗi track (04 §9)
inline constexpr std::uint16_t kAnyMidiSource = 0xFFFF;   // mapping "device": ""
inline constexpr std::uint8_t kAnyMidiChannel = 16;       // mapping "channel": -1
inline constexpr int kMaxFxParams = 8;   // paramId 0..7 (FX nhiều tham số nhất hiện có 5)

// Một slot FX (P3-12). Processor do EngineModel và snapshot cùng sở hữu. CÙNG instance đi qua mọi snapshot sau đó
// (snapshot đổi vì lý do khác không tạo lại FX) → đuôi reverb/delay không bị cắt. RT chỉ mượn con trỏ thô.
struct FxSlotSnapshot {
    std::shared_ptr<dsp::Processor> proc;   // nullptr = slot trống
    std::uint32_t instanceId = 0;           // main cấp, không lặp lại trong đời engine; 0 = trống
    bool bypass = false;                    // giá trị ĐẦU của instance mới; sau đó RT theo LE_CMD_FX_BYPASS
};

struct TrackSnapshot {
    TrackKind kind = TrackKind::Audio;
    dsp::InstrumentPtr instrument;   // kind == Instrument
    FxSlotSnapshot fx[kFxSlots];
};

struct GraphSnapshot {
    std::uint32_t generation = 0;    // tăng mỗi lần build; voice/clip player ghi lại generation đang đọc
    // Epoch: main tăng khi reset project (project.open/close) hoặc khi sửa một ô (clip.clear/setMidi/setAudio).
    // RT so epoch để bỏ take nó đang giữ (thu xong nhưng model chưa có) khi main đã xoá/thay ô đó.
    std::uint32_t projectEpoch = 0;
    std::uint32_t cellEpoch[LE_MAX_TRACKS][LE_MAX_SCENES] = {};
    int beatsPerBar = 4;             // transport.setTimeSignature
    int beatUnit = 4;
    TrackSnapshot tracks[LE_MAX_TRACKS];
    ClipSnapshot clips[LE_MAX_TRACKS][LE_MAX_SCENES];
    // P4-04: MIDI → hành động (learn + preset Launchpad). nullptr = không có mapping. Khoá có wildcard:
    // source = kAnyMidiSource (mọi thiết bị), channel = kAnyMidiChannel (mọi kênh).
    std::shared_ptr<const midi::MidiLearnMap> learnMap;

    GraphSnapshot() { liveCount().fetch_add(1, std::memory_order_relaxed); }
    ~GraphSnapshot() { liveCount().fetch_sub(1, std::memory_order_relaxed); }   // [main] luôn
    GraphSnapshot(const GraphSnapshot&) = delete;
    GraphSnapshot& operator=(const GraphSnapshot&) = delete;

    // Số snapshot đang sống (test kiểm không leak).
    static std::atomic<int>& liveCount() {
        static std::atomic<int> n{0};
        return n;
    }
};

// Thứ trên RT đọc dữ liệu của snapshot lâu hơn 1 block (voice đang release, clip player đang crossfade).
// RtEngine hỏi trước khi thu hồi snapshot cũ. Mọi method [RT].
class GenerationUser {
public:
    virtual ~GenerationUser() = default;
    virtual bool usesGeneration(std::uint32_t generation) const noexcept [[clang::nonblocking]] = 0;
    // retiring đầy: fade nhanh (3 ms) mọi thứ còn đọc generation này để trả nó sớm.
    virtual void fastReleaseGeneration(std::uint32_t generation) noexcept [[clang::nonblocking]] = 0;
};

// Kênh main → RT cho snapshot mới (03 §4.2).
class SnapshotExchange {
public:
    SnapshotExchange() = default;
    SnapshotExchange(const SnapshotExchange&) = delete;
    SnapshotExchange& operator=(const SnapshotExchange&) = delete;
    ~SnapshotExchange() { delete pending_.exchange(nullptr, std::memory_order_acq_rel); }   // [main] khi audio đã dừng

    // [main] Đưa snapshot mới lên. Nếu RT chưa kịp lấy bản trước thì bản đó được trả lại để main xoá (RT chưa
    // từng thấy nó nên xoá an toàn).
    std::unique_ptr<GraphSnapshot> publish(std::unique_ptr<GraphSnapshot> s) noexcept {
        // release: mọi ghi vào *s xảy ra trước khi RT thấy con trỏ.
        return std::unique_ptr<GraphSnapshot>(pending_.exchange(s.release(), std::memory_order_acq_rel));
    }

    // [RT] Lấy snapshot mới nếu có (acquire: thấy đủ nội dung main đã ghi).
    GraphSnapshot* take() noexcept [[clang::nonblocking]] { return pending_.exchange(nullptr, std::memory_order_acq_rel); }

    bool hasPending() const noexcept { return pending_.load(std::memory_order_acquire) != nullptr; }
    // [main, audio dừng] Bản đang chờ (không lấy ra) — RtEngine::prepare cần prepare lại FX trong đó.
    GraphSnapshot* peekPending() const noexcept { return pending_.load(std::memory_order_acquire); }

private:
    std::atomic<GraphSnapshot*> pending_{nullptr};
    static_assert(std::atomic<GraphSnapshot*>::is_always_lock_free);
};

} // namespace le::core
