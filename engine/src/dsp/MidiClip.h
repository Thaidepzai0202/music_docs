// MidiClip — dữ liệu bất biến của một MIDI clip (04 §7), nằm trong snapshot của 68 giống AudioData.
// [NRT] tạo (clip.setMidi / thu MIDI P1-30) → std::shared_ptr<const MidiClip> trong snapshot → RT chỉ đọc
// qua con trỏ thô (MidiClipPlayer).
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace le::dsp {

struct MidiNote {
    double  startBeat = 0.0;     // toạ độ TRONG clip, 0 ≤ startBeat < MidiClip::lengthBeats
    double  lengthBeats = 0.25;  // > 0
    uint8_t pitch = 60;          // 0..127
    uint8_t velocity = 100;      // 1..127
};

struct MidiClip {
    std::vector<MidiNote> notes; // PHẢI sắp tăng dần theo startBeat (cùng startBeat: giữ thứ tự)
    double lengthBeats = 4.0;    // độ dài vòng lặp
};

using MidiClipPtr = std::shared_ptr<const MidiClip>;

} // namespace le::dsp
