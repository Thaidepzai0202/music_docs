#pragma once
// [main] Kiểm tra LeCommand trước khi đẩy vào queue RT (P1-04). Audio thread tin mọi lệnh nó nhận
// đã hợp lệ, nên không phải kiểm lại trên RT.
#include <cmath>

#include "core/GraphSnapshot.h"
#include "le/engine_api.h"

namespace le::core {

inline bool validTrack(int t) { return t >= 0 && t < LE_MAX_TRACKS; }
inline bool validSlot(int s) { return s >= 0 && s < LE_MAX_SCENES; }
inline bool inRange(int v, int lo, int hi) { return v >= lo && v <= hi; }
inline bool finite(double v) { return std::isfinite(v); }

// true nếu lệnh hợp lệ theo 05 §2 (dải của track/slot/i0/f0/d0 cho từng loại).
inline bool isValidCommand(const LeCommand& c) {
    if (!finite(c.f0) || !finite(c.f1) || !finite(c.d0)) return false;
    if (c.track < -1 || c.track >= LE_MAX_TRACKS || c.slot < -1 || c.slot >= LE_MAX_SCENES) return false;
    switch (c.type) {
        case LE_CMD_TRANSPORT_PLAY:
        case LE_CMD_TRANSPORT_STOP:
        case LE_CMD_STOP_ALL: return true;
        case LE_CMD_SET_BPM: return c.d0 >= 20.0 && c.d0 <= 300.0;
        case LE_CMD_SET_QUANTIZE: return inRange(c.i0, LE_Q_NONE, LE_Q_4_BAR);
        case LE_CMD_METRONOME: return inRange(c.i0, 0, 2) && c.f0 >= 0.0f && c.f0 <= 1.0f;
        case LE_CMD_SET_COUNT_IN: return inRange(c.i0, 0, 2);

        case LE_CMD_CLIP_LAUNCH: return validTrack(c.track) && validSlot(c.slot);
        case LE_CMD_CLIP_STOP:
        case LE_CMD_RECORD_STOP:
        case LE_CMD_OVERDUB_TOGGLE:
        case LE_CMD_SELECT_TRACK: return validTrack(c.track);
        case LE_CMD_SCENE_LAUNCH: return validSlot(c.slot);
        case LE_CMD_CLIP_RECORD: return validTrack(c.track) && validSlot(c.slot) && inRange(c.i0, 0, 64);
        case LE_CMD_LOOP_BUTTON: return validTrack(c.track) && (c.slot == -1 || validSlot(c.slot));   // slot −1 = tự chọn

        case LE_CMD_TRACK_GAIN: return validTrack(c.track) && c.f0 <= 6.0f;   // ≤ -120 = -inf
        case LE_CMD_TRACK_PAN: return validTrack(c.track) && c.f0 >= -1.0f && c.f0 <= 1.0f;
        case LE_CMD_TRACK_MUTE:
        case LE_CMD_TRACK_SOLO:
        case LE_CMD_TRACK_ARM: return validTrack(c.track) && inRange(c.i0, 0, 1);
        case LE_CMD_TRACK_MONITOR: return validTrack(c.track) && inRange(c.i0, 0, 2);

        case LE_CMD_NOTE_ON: return validTrack(c.track) && inRange(c.i0, 0, 127) && c.f0 > 0.0f && c.f0 <= 1.0f;
        case LE_CMD_NOTE_OFF: return validTrack(c.track) && inRange(c.i0, 0, 127);
        case LE_CMD_ALL_NOTES_OFF: return true;   // track -1 = mọi track

        // FX (P3-12/15): track 0..7 → slot 0..2, paramId 0..7 (main kiểm thêm theo loại FX trong model).
        // Master (track -1) cố định: slot 0 = EQ3 (p0..2 = low/mid/high dB), slot 1 = limiter (p0 trần dB, p1 release ms).
        case LE_CMD_FX_PARAM:
            if (c.track < 0) return (c.slot == 0 && inRange(c.i0, 0, 2)) || (c.slot == 1 && inRange(c.i0, 0, 1));
            return inRange(c.slot, 0, kFxSlots - 1) && inRange(c.i0, 0, kMaxFxParams - 1);
        case LE_CMD_FX_BYPASS:
            if (c.track < 0) return c.slot == 0 && inRange(c.i0, 0, 1);   // bật/tắt EQ3 master
            return inRange(c.slot, 0, kFxSlots - 1) && inRange(c.i0, 0, 1);
        case LE_CMD_MASTER_GAIN: return c.f0 <= 6.0f;

        case LE_CMD_SPIKE_SINE: return true;
        case LE_CMD_SPIKE_LOAD_VOICES: return inRange(c.i0, 0, 1024);   // RT tự kẹp về 128
        case LE_CMD_SPIKE_RECORD: return c.i0 >= 0;
        case LE_CMD_SPIKE_PLAY_RECORD:
        case LE_CMD_SPIKE_PASSTHROUGH: return inRange(c.i0, 0, 1);
        default: return false;
    }
}

} // namespace le::core
