// MidiLearnMap — bảng tra cố định "message MIDI → hành động" cho MIDI learn (04 §13, P4-01/P4-04 lõi).
// [main] dựng bảng (set/remove) rồi đưa vào snapshot (bất biến). [RT] find() tra nhị phân, không cấp phát.
// Khoá = {thiết bị, loại (nốt/CC), kênh, số}. Tối đa 256 mapping (std::array sắp theo khoá).
#pragma once

#include <array>
#include <cstdint>

namespace le::midi {

struct MidiInputEvent;

enum class LearnKind : uint8_t { Note = 0, CC = 1 };   // khớp LE_EVT_MIDI_LEARNED a = kind

struct LearnKey {
    uint16_t source = 0;       // chỉ số thiết bị (68 ánh xạ từ tên thiết bị trong project, 06 §2)
    LearnKind kind = LearnKind::Note;
    uint8_t channel = 0;       // 0..15
    uint8_t number = 0;        // 0..127

    uint64_t packed() const noexcept [[clang::nonblocking]] {
        return (static_cast<uint64_t>(source) << 24) | (static_cast<uint64_t>(kind) << 16) |
               (static_cast<uint64_t>(channel) << 8) | number;
    }
};

// Tạo khoá từ event: note-on/note-off → Note, control change → CC. Loại khác → false.
bool learnKeyFor(const MidiInputEvent& e, LearnKey& out) noexcept [[clang::nonblocking]];

enum class LearnAction : uint8_t {
    None = 0,
    ClipLaunch,      // track, slot
    SceneLaunch,     // slot = scene
    TransportPlay,
    TransportStop,
    TransportToggle,
    StopAll,
    TrackGain,       // track; giá trị CC → dB trong [minValue, maxValue]
    TrackMute,       // track; bật/tắt
    FxParam,         // track (−1 = master), slot = fx, paramId; CC → [minValue, maxValue]
    LoopButton,      // nút looper của track ĐANG CHỌN: xoay vòng record → stop → overdub (P1-39). RT chỉ báo main, main
                     // thực thi qua Engine::send (overdub cần vé do main tạo) → mapLearnValue không dùng giá trị này.
    TrackStop,       // footswitch: dừng clip của track ĐANG CHỌN (CLIP_STOP theo quantize). Như LoopButton: RT báo main.
    UndoOverdub,     // footswitch: hoàn tác lớp overdub của ô đang phát trên track ĐANG CHỌN (như clip.undoOverdub; không
                     // có lớp undo → bỏ qua). Như LoopButton: RT báo main, mapLearnValue không dùng.
};

struct LearnTarget {
    LearnAction action = LearnAction::None;
    int8_t  track = -1;
    int8_t  slot = -1;
    int16_t paramId = -1;
    float   minValue = 0.0f, maxValue = 1.0f;
};

// Giá trị MIDI 0..127 → [minValue, maxValue] tuyến tính (cho TrackGain / FxParam).
inline float mapLearnValue(const LearnTarget& t, uint8_t midiValue) noexcept [[clang::nonblocking]] {
    return t.minValue + (t.maxValue - t.minValue) * static_cast<float>(midiValue & 0x7F) / 127.0f;
}

class MidiLearnMap {
public:
    static constexpr int kCapacity = 256;

    // [main] Thêm hoặc thay mapping của key. false nếu đã đầy (key mới).
    bool set(const LearnKey& key, const LearnTarget& target) noexcept;
    // [main] Xoá mapping của key. false nếu không có.
    bool remove(const LearnKey& key) noexcept;
    void clear() noexcept { size_ = 0; }

    // [RT] Mapping của key, hoặc nullptr.
    const LearnTarget* find(const LearnKey& key) const noexcept [[clang::nonblocking]];
    int size() const noexcept [[clang::nonblocking]] { return size_; }

private:
    struct Entry {
        uint64_t key = 0;
        LearnTarget target;
    };
    int lowerBound(uint64_t key) const noexcept [[clang::nonblocking]];

    std::array<Entry, kCapacity> entries_{};
    int size_ = 0;
};

} // namespace le::midi
