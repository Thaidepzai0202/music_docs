#pragma once
// Các kênh giữa audio thread và phần còn lại (03 §2: SPSC queue, atomic, StatePublisher).
// rigtorp::SPSCQueue: 1 thread push, 1 thread pop, không lock. Bộ nhớ cấp phát 1 lần trong constructor
// (trên main), sau đó try_push / front / pop không cấp phát.
#include <cstdint>

#include <rigtorp/SPSCQueue.h>

#include "le/engine_api.h"

namespace le::core {

constexpr std::size_t kRtCommandCapacity = 1024;
constexpr std::size_t kRtToNrtCapacity = 1024;
constexpr std::size_t kMidiToRtCapacity = 512;

// UI (main) → RT
using CommandQueue = rigtorp::SPSCQueue<LeCommand>;

struct GraphSnapshot;

// RT → main: event cho Dart, hoặc snapshot đã hết người dùng để ReleasePool (main) delete.
struct RtMessage {
    enum Kind : std::int32_t { Event = 1, Retire = 2, TakeFinished = 3, MidiTakeFinished = 4, MidiNote = 5, MidiOverdubNote = 6,
                              OverdubFinished = 7, MidiOverdubFinished = 8, CaptureFinished = 9,
                              JamStopped = 10, MidiLearned = 11, MappedChange = 12, LoopButton = 13,
                              UndoOverdub = 14 };
    Kind kind = Event;
    std::int32_t type = 0;   // LeEventType (Event)
    std::int32_t a = 0;
    std::int32_t b = 0;
    double value = 0.0;
    GraphSnapshot* snapshot = nullptr;   // Retire: main sở hữu từ lúc pop
    // TakeFinished: a = track, b = slot, value = lengthBeats, value2 = bpm lúc thu,
    //               i0 = offset trong buffer thu, i1 = số frame (độ dài + đuôi)
    // MidiTakeFinished: a, b, value = lengthBeats. MidiNote / MidiOverdubNote: a, b, value = beat (trong take / trong
    //               clip), i0 = pitch, i1 = velocity (1..127, 0 = note-off)
    // OverdubFinished: a = track, b = slot, i0 = session của vé, i1 = 1 nếu đã ghi
    // MidiOverdubFinished: a = track, b = slot, value = vị trí (beat trong clip) lúc lượt kết thúc, value2 = lengthBeats
    // CaptureFinished: a = id lượt capture, i0 = số frame đã ghi, i1 = 1 nếu tự dừng vì đầy (maxSeconds)
    // JamStopped: a = id ring ghi jam — RT không còn con trỏ tới ring (main nhả được)
    // MidiLearned (P4-04): a = source (thiết bị), b = LearnKind (0 nốt, 1 CC), i0 = kênh 0..15, i1 = số 0..127
    // UndoOverdub: mapping {kind:"undoOverdub"} được nhấn, a = track đang chọn → main hoàn tác lớp overdub của ô đang phát
    // LoopButton (P1-39): mapping MIDI {kind:"loopButton"} được nhấn, a = track đang chọn → main chạy LE_CMD_LOOP_BUTTON
    // MappedChange (P4-04): mapping MIDI đổi state RT mà model phải biết. a = 0 gain dB / 1 mute / 2 FX param,
    //               b = track (−1 master), i0 = slot FX, i1 = paramId, value = giá trị
    double value2 = 0.0;
    std::int64_t i0 = 0, i1 = 0;
    std::uint32_t projectEpoch = 0, cellEpoch = 0;   // TakeFinished: epoch lúc bắt đầu thu (main bỏ take cũ)
};
using RtToNrtQueue = rigtorp::SPSCQueue<RtMessage>;

// CoreMIDI thread → RT (P4). Message thô + timestamp để RT đổi ra frame offset (04 §13).
struct MidiInMessage {
    std::int64_t hostTimeNs = 0;
    std::uint8_t data[3] = {0, 0, 0};
    std::uint8_t size = 0;
    std::uint16_t source = 0;   // chỉ số thiết bị
};
using MidiQueue = rigtorp::SPSCQueue<MidiInMessage>;

} // namespace le::core
