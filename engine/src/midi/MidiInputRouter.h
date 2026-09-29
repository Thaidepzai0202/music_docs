// MidiInputRouter — MIDI từ controller vào audio thread với offset chính xác theo sample (04 §13, P4-01 lõi).
// 68 mở thiết bị (juce::MidiInput) và nối vào RtEngine; queue là core::MidiQueue (SPSC, 512) của 68.
//
//   [thread CoreMIDI]  enqueue(queue, hostTimeNs, bytes, size, source): lọc message kênh (0x80–0xEF, ≤ 3 byte),
//                      bỏ SysEx / clock / realtime, try_push (queue đầy → false, đếm ở caller). KHÔNG dùng
//                      juce::MidiMessageCollector (bên trong có lock).
//   [RT]               drain(queue, blockHostTimeNs, sampleRate, numFrames, out): đổi timestamp thành frame offset
//                          offset = round((t + latencyNs − blockHostTimeNs) · sampleRate / 1e9)
//                      offset < 0 (tới "sớm": trước đầu block, đã qua) → kẹp về 0;
//                      offset ≥ numFrames (thuộc tương lai) → để lại queue cho block sau;
//                      xa bất thường (> 1 s tương lai: đồng hồ lệch) → phát ở 0 để không chặn cả queue.
//   latencyNs = 0: độ trễ nhỏ nhất, jitter ≤ 1 block. Đặt = thời lượng 1 block: vị trí tương đối giữa các nốt
//   được giữ đúng (không jitter), đổi lại trễ thêm 1 block.
// Thời gian: hostTimeNs của message và của block phải CÙNG một đồng hồ (mach host time đổi ra ns).
#pragma once

#include "core/RtQueues.h"

#include <array>
#include <cstdint>

namespace le::midi {

struct MidiInputEvent {
    int32_t  offset = 0;       // 0 ≤ offset < numFrames
    uint8_t  status = 0, data1 = 0, data2 = 0;
    uint8_t  size = 0;
    uint16_t source = 0;       // chỉ số thiết bị (cho MidiLearnMap)
};

// Danh sách cố định dung lượng cho 1 block (không bao giờ cấp phát). Đầy thì drain dừng, phần còn lại ở queue.
class MidiInputEventList {
public:
    static constexpr int kCapacity = 512;
    void clear() noexcept [[clang::nonblocking]] { count_ = 0; }
    bool push(const MidiInputEvent& e) noexcept [[clang::nonblocking]] {
        if (count_ >= kCapacity) return false;
        events_[static_cast<size_t>(count_++)] = e;
        return true;
    }
    int size() const noexcept [[clang::nonblocking]] { return count_; }
    const MidiInputEvent& operator[](int i) const noexcept [[clang::nonblocking]] { return events_[static_cast<size_t>(i)]; }
    const MidiInputEvent* begin() const noexcept [[clang::nonblocking]] { return events_.data(); }
    const MidiInputEvent* end() const noexcept [[clang::nonblocking]] { return events_.data() + count_; }

private:
    std::array<MidiInputEvent, kCapacity> events_{};
    int count_ = 0;
};

// Message kênh đã tách nghĩa. Note-on velocity 0 = note-off (quy ước MIDI).
struct ParsedMidi {
    enum class Kind : uint8_t { NoteOn, NoteOff, ControlChange, ProgramChange, PitchBend, ChannelPressure, PolyPressure, Other };
    Kind    kind = Kind::Other;
    uint8_t channel = 0;       // 0..15
    uint8_t number = 0;        // nốt / số CC / program
    uint8_t value = 0;         // velocity / giá trị CC / pressure; PitchBend: 7 bit cao
    int16_t bend = 0;          // PitchBend: −8192..+8191
};
ParsedMidi parseMidi(uint8_t status, uint8_t data1, uint8_t data2) noexcept [[clang::nonblocking]];

class MidiInputRouter {
public:
    static constexpr int64_t kMaxFutureNs = 1'000'000'000;   // > 1 s tương lai → coi là đồng hồ lệch

    // [thread MIDI] true = đã vào queue. false = bị lọc (SysEx, clock…) hoặc queue đầy.
    static bool enqueue(core::MidiQueue& q, int64_t hostTimeNs, const uint8_t* bytes, int size, uint16_t source) noexcept
        [[clang::nonblocking]];
    static bool accepts(const uint8_t* bytes, int size) noexcept [[clang::nonblocking]];

    // [main] Độ trễ lập lịch (ns) cộng vào timestamp — xem comment đầu file.
    void setScheduleLatencyNs(int64_t ns) noexcept { latencyNs_ = ns; }

    // [RT] Lấy các message thuộc block này ra `out` (thêm vào sau, theo thứ tự tới). Trả số message đã lấy.
    int drain(core::MidiQueue& q, int64_t blockHostTimeNs, double sampleRate, int numFrames,
              MidiInputEventList& out) const noexcept [[clang::nonblocking]];

private:
    int64_t latencyNs_ = 0;
};

} // namespace le::midi
