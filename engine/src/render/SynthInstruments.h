// SynthInstruments — nhạc cụ giai điệu TỔNG HỢP BẰNG CODE, nhiều zone (P2-35, 06 §4 "Mở rộng thư viện"). [worker]
// Tool `le-gen-instruments` ghi ra content/instruments/<id>/<id>.sfz + samples/*.flac (FLAC 24-bit 48 kHz mono)
// + <id>.json (mục manifest gợi ý).
//
//   inst_epiano  E-Piano kiểu Rhodes: FM 2 operator (tỉ lệ 1:1, chỉ số điều chế giảm dần sau tiếng gõ) + "tine"
//                tỉ lệ 7 lúc đầu + tremolo nhẹ 4 Hz (±6 %). Lớp mạnh: chỉ số FM cao hơn + bão hoà tanh ("bark").
//                Nốt ngân qua loop, tắt dần bằng ampeg_decay (thấp ~9 s, cao ~2.5 s), nhả phím: release 0.3 s.
//   inst_organ   Organ drawbar kiểu tonewheel: 16' 5⅓' 8' ở mức 8 (888000000); lớp mạnh thêm 4' mức 5 +
//                percussion bậc 3 (2⅔', tắt ~0.2 s) + tiếng click phím to hơn. Không tắt dần khi giữ phím.
//   inst_synth   Tone tổng hợp — bản THƯ VIỆN của fixture tests/fixtures/inst_synth (cùng công thức: hài 1/h, pha 0.7·h,
//                lớp nhẹ 4 hài / mạnh 10 hài, attack 10 ms, release 0.3 s) nhưng có zone THẬT C0–C8; nốt trầm được
//                thêm hài cho tới ≥ 250 Hz để loa nhỏ vẫn nghe ra cao độ. Không cân loudness theo K-weighting (bộ lọc
//                38 Hz sẽ kéo cả nhạc cụ nhỏ đi vì nốt C0 16 Hz) — mỗi sample đỉnh −1 dBFS, volume 0.
//
// Zone: mỗi 3 nửa cung (phím gốc ở giữa, phủ gốc −1 .. gốc +1 → dịch cao độ tối đa 1 nửa cung), 2 lớp velocity
// (1–95 nhẹ, 96–127 mạnh). Zone thấp nhất kéo xuống phím 0, zone cao nhất lên phím 127 (P2-35 B2: MỌI phím đều kêu,
// ngoài dải là repitch); `rangeLo..rangeHi` vẫn là dải tự nhiên (manifest "range" để UI đánh dấu).
// Mỗi sample = phần tấn công T giây + LOOP đúng 1 giây ở cuối file.
//
// Loop liền mạch TUYỆT ĐỐI: mọi thành phần trong phần loop có tần số NGUYÊN Hz (organ: bội của 2 Hz vì có 16' =
// f/2) và pha tính từ số nguyên (i·f mod sr) → mẫu thứ i + sr đúng bằng mẫu thứ i, từng bit. Phần tấn công (chỉ số
// FM, tine, percussion, click) về 0 đúng tại T. Tần số nguyên lệch nốt chuẩn ≤ ~15 cent → SFZ `tune` bù lại.
// Cân loudness như kit: mọi sample đỉnh −1 dBFS, `volume` kéo về loudness của sample nhỏ nhất (≤ 12 dB).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace le::render::instruments {

inline constexpr int kZoneStep = 3;      // nửa cung giữa hai phím gốc
inline constexpr int kSoftMaxVel = 95;   // lớp nhẹ: velocity 1..95, lớp mạnh: 96..127

struct InstSample {
    int root = 60;                 // SFZ pitch_keycenter
    int loKey = 59, hiKey = 61;
    int loVel = 1, hiVel = 127;
    std::string file;              // tên trong samples/, VD "ep_060_soft.flac"
    double f0Hz = 0.0;             // tần số thật (nguyên Hz) của sample
    float tuneCents = 0.0f;        // SFZ tune: bù f0Hz về nốt chuẩn của root
    float volumeDb = 0.0f;         // cân loudness (≤ 0, bước 0.1 dB)
    double loudnessDb = 0.0;
    int64_t loopStart = 0;         // frame, [loopStart, loopEnd) — SFZ loop_end = loopEnd − 1 (SFZ tính cả điểm cuối)
    int64_t loopEnd = 0;           // == samples.size()
    float decaySec = 0.0f;         // SFZ ampeg_decay (0 = không ghi)
    std::vector<float> samples;    // mono, đã làm tròn lưới 24-bit
};

struct SynthInstrument {
    std::string id;                // "inst_epiano" / "inst_organ" / "inst_synth"
    std::string sfzFileName;       // "<id>.sfz"
    std::string sfzText;
    double sampleRate = 48000.0;
    int rangeLo = 36, rangeHi = 96;   // dải tự nhiên (manifest "range"); zone vẫn phủ 0..127
    std::vector<InstSample> zones;    // theo root tăng dần, lớp nhẹ trước lớp mạnh
};

SynthInstrument makeEPiano(double sampleRate = 48000.0);
SynthInstrument makeOrgan(double sampleRate = 48000.0);
SynthInstrument makeSynthTone(double sampleRate = 48000.0);
// Thứ tự cố định: epiano, organ, synth.
std::vector<SynthInstrument> makeAllInstruments(double sampleRate = 48000.0);

} // namespace le::render::instruments
