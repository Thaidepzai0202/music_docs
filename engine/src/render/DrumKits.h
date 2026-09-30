// DrumKits — bộ trống 16 pad TỔNG HỢP BẰNG CODE (P2-30 / P2-35, 06 §4: map General MIDI 36–51, khớp pad 4×4). [worker]
// Không dùng sample bên thứ ba → không vướng license (content/LICENSES/SYNTHESIZED.md). Tool `le-gen-kits` ghi ra
// content/kits/<id>/<id>.sfz + samples/*.flac (FLAC 24-bit 48 kHz mono) + <id>.json (mục manifest gợi ý).
// Khối dựng dùng chung: render/KitSynth.h. 808 / 909 / Percussion ở DrumKits.cpp, các kit P2-35 ở DrumKitsMore.cpp.
//
//   kit_808  kick sine quét + decay dài · rim · snare tone+noise · clap 4 burst + đuôi · snare 2 · 6 tom sine quét
//            (41 < 43 < 45 < 47 < 48 < 50) · hat đóng / chân / mở từ 6 oscillator vuông kiểu 808 + bandpass cao
//            (42, 44 choke 46) · crash · ride
//   kit_909  cùng map, chất 909: kick có click, snare nhiều noise, hat sáng hơn, clap 909
//   kit_perc conga / bongo thấp-cao · cowbell (2 vuông 540/800 Hz + bandpass) · tambourine · shaker · clave ·
//            triangle tắt / mở (44 choke 45) · agogô thấp-cao · guiro · cabasa · timbale thấp-cao
//   kit_trap 36 "808 Kick" dài có glide (110 → 45 Hz) · 37 "Perc" · snare / clap giòn · 40 "Snap" · 808 tom ·
//            hat đóng ~12 ms (roll 1/32 vẫn tách) · crash · 51 "Riser" (nhiễu quét 300 Hz → 9 kHz, thay ride)
//   kit_lofi boom-bap: kick ấm, snare dày, hat mềm; mọi âm bitcrush 12 bit @ 24 kHz + lọc 9–10 kHz + hiss /
//            lách tách đĩa than (tất định theo seed)
//   kit_606  TR-606: kick ngắn chặt, snare mỏng nhiều noise, hat kim loại mảnh, cymbal
//   kit_707  TR-707: tổng hợp kiểu acoustic rồi sample lại 8-bit μ-law @ 25 kHz (cymbal 6-bit)
//   kit_linn LinnDrum: 8-bit μ-law @ 28 kHz, kick dày, snare có đuôi phòng, clap 4 burst, tom sâu; 37 "Side Stick"
//
// Mọi âm: tất định (nhiễu xorshift32 tự viết, không std::*_distribution), bão hoà mềm tanh cho âm ngắn (snare,
// clap, hat, rim… "đặc" hơn như mạch analog → loudness cao hơn ở cùng đỉnh), fade-out cosin ở 30 % cuối thời lượng
// → đuôi xuống dưới −80 dBFS rồi CẮT; đỉnh chuẩn hoá đúng −1 dBFS. Cân loudness trong kit bằng `volume` của SFZ:
// mọi âm bị giảm về loudness của âm NHỎ NHẤT kit (không tăng được âm nhỏ vì file đã ở đỉnh −1 dBFS), không giảm
// âm nào quá kMaxCutDb → mọi pad nghe to như nhau (±0.1 dB do làm tròn). Loudness = K-weighting BS.1770 (shelf
// +4 dB @1.68 kHz, HPF 38 Hz), max trung bình bình phương cửa sổ 100 ms (drum ngắn: cửa sổ 400 ms của BS.1770
// thiên vị âm dài).
#pragma once

#include <string>
#include <vector>

namespace le::render::kits {

inline constexpr float kPeakDb = -1.0f;    // đỉnh mỗi file
inline constexpr float kTailDb = -80.0f;   // cắt đuôi dưới mức này (dBFS)
inline constexpr float kMaxCutDb = -12.0f; // giảm loudness tối đa

struct KitPad {
    int key = 36;                  // 36..51
    std::string label;             // SFZ region_label (tên trên pad, tiếng Anh ngắn)
    std::string file;              // tên file trong samples/, VD "kick.flac"
    int group = 0, offBy = 0;      // choke (SFZ group / off_by)
    float volumeDb = 0.0f;         // cân loudness (≤ 0, bước 0.1 dB) → SFZ volume
    double loudnessDb = 0.0;       // loudness của file (trước volume)
    std::vector<float> samples;    // mono
};

struct Kit {
    std::string id;                // "kit_808" / "kit_909" / "kit_perc"
    std::string sfzFileName;       // "<id>.sfz"
    std::string sfzText;
    double sampleRate = 48000.0;
    std::vector<KitPad> pads;      // đúng 16, key 36..51 theo thứ tự
};

Kit make808(double sampleRate = 48000.0);
Kit make909(double sampleRate = 48000.0);
Kit makePerc(double sampleRate = 48000.0);
Kit makeTrap(double sampleRate = 48000.0);
Kit makeLofi(double sampleRate = 48000.0);
Kit make606(double sampleRate = 48000.0);
Kit make707(double sampleRate = 48000.0);
Kit makeLinn(double sampleRate = 48000.0);
// Thứ tự cố định: 808, 909, perc, trap, lofi, 606, 707, linn.
std::vector<Kit> makeAllKits(double sampleRate = 48000.0);

// Làm tròn về lưới 24-bit (k / 2^23, kẹp [−2^23, 2^23 − 1]): đúng giá trị file FLAC 24-bit chứa và decode ra.
// Mọi pad đã qua bước này (trước khi đo loudness) → buffer trong bộ nhớ == dữ liệu engine nạp từ file.
void quantize24(std::vector<float>& x);

// Loudness (dB, thang LUFS-like) của một âm mono — dùng cho cân kit và test.
double loudnessDb(const std::vector<float>& x, double sampleRate);

} // namespace le::render::kits
