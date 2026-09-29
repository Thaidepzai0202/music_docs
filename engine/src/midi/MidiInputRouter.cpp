#include "midi/MidiInputRouter.h"

#include <cmath>

namespace le::midi {

ParsedMidi parseMidi(uint8_t status, uint8_t d1, uint8_t d2) noexcept [[clang::nonblocking]] {
    ParsedMidi p;
    p.channel = static_cast<uint8_t>(status & 0x0F);
    p.number = static_cast<uint8_t>(d1 & 0x7F);
    p.value = static_cast<uint8_t>(d2 & 0x7F);
    switch (status & 0xF0) {
        case 0x80: p.kind = ParsedMidi::Kind::NoteOff; break;
        case 0x90: p.kind = p.value == 0 ? ParsedMidi::Kind::NoteOff : ParsedMidi::Kind::NoteOn; break;
        case 0xA0: p.kind = ParsedMidi::Kind::PolyPressure; break;
        case 0xB0: p.kind = ParsedMidi::Kind::ControlChange; break;
        case 0xC0: p.kind = ParsedMidi::Kind::ProgramChange; p.value = 0; break;
        case 0xD0: p.kind = ParsedMidi::Kind::ChannelPressure; p.value = p.number; p.number = 0; break;
        case 0xE0:
            p.kind = ParsedMidi::Kind::PitchBend;
            p.bend = static_cast<int16_t>(((d2 & 0x7F) << 7 | (d1 & 0x7F)) - 8192);
            p.number = 0;
            break;
        default: p.kind = ParsedMidi::Kind::Other; break;
    }
    return p;
}

// Chỉ nhận message kênh (status 0x80–0xEF) đủ số byte. SysEx (0xF0…), MIDI clock (0xF8), active sensing (0xFE)…
// bị bỏ: clock 24 xung/beat sẽ làm ngập queue, còn đồng bộ clock không nằm trong MVP.
bool MidiInputRouter::accepts(const uint8_t* bytes, int size) noexcept [[clang::nonblocking]] {
    if (bytes == nullptr || size < 1 || size > 3) return false;
    const uint8_t s = bytes[0];
    if (s < 0x80 || s >= 0xF0) return false;
    const int need = ((s & 0xF0) == 0xC0 || (s & 0xF0) == 0xD0) ? 2 : 3;
    return size >= need;
}

// [thread MIDI] rigtorp::SPSCQueue::try_push không cấp phát (buffer có sẵn) nhưng không gắn nonblocking.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfunction-effects"

bool MidiInputRouter::enqueue(core::MidiQueue& q, int64_t hostTimeNs, const uint8_t* bytes, int size,
                              uint16_t source) noexcept [[clang::nonblocking]] {
    if (!accepts(bytes, size)) return false;
    core::MidiInMessage m;
    m.hostTimeNs = hostTimeNs;
    m.size = static_cast<uint8_t>(size);
    for (int i = 0; i < size; ++i) m.data[i] = bytes[i];
    m.source = source;
    return q.try_push(m);
}

// [RT] front()/pop() của rigtorp: chỉ đọc/ghi 2 chỉ số atomic, không lock, không cấp phát.
int MidiInputRouter::drain(core::MidiQueue& q, int64_t blockHostTimeNs, double sampleRate, int numFrames,
                           MidiInputEventList& out) const noexcept [[clang::nonblocking]] {
    if (numFrames <= 0 || !(sampleRate > 0.0)) return 0;
    int taken = 0;
    const double framesPerNs = sampleRate / 1.0e9;
    while (const core::MidiInMessage* m = q.front()) {
        const int64_t dt = m->hostTimeNs + latencyNs_ - blockHostTimeNs;
        int64_t offset = std::llround(static_cast<double>(dt) * framesPerNs);
        if (offset >= numFrames) {
            if (dt <= kMaxFutureNs) break;   // thuộc block sau → để lại (thứ tự tới được giữ)
            offset = 0;                      // đồng hồ lệch bất thường: phát ngay, đừng chặn queue
        }
        if (offset < 0) offset = 0;          // đã qua đầu block → phát ở sample đầu tiên
        MidiInputEvent e;
        e.offset = static_cast<int32_t>(offset);
        e.status = m->data[0];
        e.data1 = m->size > 1 ? m->data[1] : 0;
        e.data2 = m->size > 2 ? m->data[2] : 0;
        e.size = m->size;
        e.source = m->source;
        if (!out.push(e)) break;             // list đầy: phần còn lại chờ block sau
        q.pop();
        ++taken;
    }
    return taken;
}

#pragma clang diagnostic pop

} // namespace le::midi
