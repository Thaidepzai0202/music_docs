// Instrument / Zone / AdsrParams — dữ liệu bất biến của một nhạc cụ sampler (04 §6.1).
// HỢP ĐỒNG NỘI BỘ 80 ↔ 68 (docs/11 §6): snapshot của 68 giữ `std::shared_ptr<const Instrument>`,
// Sampler (80) nhận `const Instrument*`.
//
// Nguồn tạo:  SfzLoader (drum kit, nhạc cụ thư viện) · PitchRenderer (tiếng tự thu, P3) — đều ở [worker].
// Ownership:  Instrument SỞ HỮU các sample qua `samples` (shared_ptr). Zone::data là con trỏ thô
//             trỏ vào một phần tử của `samples` → hợp lệ chừng nào Instrument còn sống.
//             Copy Instrument vẫn an toàn (copy shared_ptr, Zone::data vẫn trỏ đúng AudioData).
//
// Khác 04 §6.1 (có chủ đích, để khớp SFZ ở 06 §4): envelope, loop mode (one-shot) và choke
// (group/off_by) nằm ở TỪNG ZONE, vì SFZ cho phép mỗi region một giá trị (<global>/<group> chỉ là
// giá trị mặc định kế thừa). Instrument tự thu (P3) thì mọi zone dùng chung một envelope.
#pragma once

#include "dsp/AudioData.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace le::dsp {

// Tham số ADSR, đơn vị giây (sustain là mức tuyến tính 0..1). Khớp opcode SFZ ampeg_* (ampeg_sustain
// tính %, SfzLoader chia 100). Adsr (P1-26) tự đặt sàn attack ≥ 0.5 ms, release ≥ 5 ms để không click.
struct AdsrParams {
    float attack = 0.0f;
    float decay = 0.0f;
    float sustain = 1.0f;
    float release = 0.005f;
};

enum class LoopMode : uint8_t {
    NoLoop,          // phát tới hết sample (hoặc tới khi envelope release xong)
    OneShot,         // như NoLoop nhưng BỎ QUA note-off: luôn phát hết (drum)
    LoopContinuous,  // lặp [loopStart, loopEnd) cho tới khi release xong
};

struct Zone {
    // Chọn zone: loKey ≤ note ≤ hiKey và loVel ≤ velocity ≤ hiVel (velocity MIDI 1..127)
    int16_t loKey = 0, hiKey = 127;
    int16_t loVel = 1, hiVel = 127;

    int16_t rootKey = 60;      // SFZ pitch_keycenter: phím phát đúng cao độ gốc của sample
    float   tuneCents = 0.0f;  // SFZ tune: > 0 nâng cao độ khi phát. Sample đo được lệch +x cent → đặt −x
    float   gainDb = 0.0f;     // SFZ volume
    float   pan = 0.0f;        // −1 (trái) .. +1 (phải); SFZ pan −100..100 chia 100

    LoopMode loopMode = LoopMode::NoLoop;
    int64_t  loopStart = 0;    // frame, [loopStart, loopEnd) — end KHÔNG gồm (SFZ loop_end gồm → +1)
    int64_t  loopEnd = -1;     // −1 = tới hết sample

    AdsrParams env{};

    // Choke (SFZ group / off_by): nốt mới của zone có group = g sẽ tắt nhanh (5 ms) mọi voice
    // đang kêu mà zone của nó có offBy = g. 0 = không thuộc nhóm nào.
    int32_t group = 0;
    int32_t offBy = 0;

    const AudioData* data = nullptr;   // MƯỢN từ Instrument::samples. nullptr = zone hỏng, bị bỏ qua
};

struct Instrument {
    enum class Mode : uint8_t {
        Natural,   // mỗi phím dùng zone khớp (bình thường)
        Classic,   // mọi phím dùng zones[classicZone] rồi resample (tiếng tự thu, 04 §6.2)
    };

    std::string name;                       // [NRT] để log/debug
    std::vector<Zone> zones;                // sắp theo loKey (ổn định: cùng loKey giữ thứ tự trong file)
    std::vector<AudioDataPtr> samples;      // sở hữu dữ liệu mà Zone::data trỏ tới
    Mode mode = Mode::Natural;
    int32_t classicZone = 0;

    // [any] Zone đầu tiên khớp (note, velocity 1..127), hoặc nullptr. Classic: luôn zones[classicZone].
    const Zone* findZone(int note, int velocity) const noexcept [[clang::nonblocking]] {
        if (mode == Mode::Classic) {
            return (classicZone >= 0 && static_cast<size_t>(classicZone) < zones.size() &&
                    zones[static_cast<size_t>(classicZone)].data != nullptr)
                       ? &zones[static_cast<size_t>(classicZone)]
                       : nullptr;
        }
        for (const Zone& z : zones) {
            if (z.loKey > note) break;   // đã sắp theo loKey
            if (note <= z.hiKey && velocity >= z.loVel && velocity <= z.hiVel && z.data != nullptr) return &z;
        }
        return nullptr;
    }
};

using InstrumentPtr = std::shared_ptr<const Instrument>;   // [NRT] chủ sở hữu (snapshot); RT dùng get()

} // namespace le::dsp
