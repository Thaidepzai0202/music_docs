// SampleImport — biến bộ sample THU THẬT (Salamander Grand Piano, VSCO-2 CE…) thành nhạc cụ SFZ gọn cho app
// (P2-35 đợt B, 06 §4). [worker] — hàm thuần, cấp phát thoải mái, KHÔNG dùng trên audio thread.
//
// Mỗi sample nguồn đi qua:
//   1. trộn mono (hoặc giữ stereo nếu spec.stereo) · cắt khoảng lặng đầu (onset −40 dB so với đỉnh, lùi 2 ms)
//   2. đo cao độ bằng Yin trên phần ổn định (âm tắt dần: 100–700 ms — dây gảy lúc đầu hơi cao; âm ngân: 150 ms–1.2 s; thử thêm cửa sổ khác nếu
//      Yin không chắc) → phím gốc + `tune` (cent). Tên file chỉ dùng để suy QUY ƯỚC quãng tám
//      (VSCO gọi C3 = MIDI 60, Salamander gọi C4 = 60): độ lệch quãng tám = trung vị (đo − tên) của cả nhạc cụ.
//      Tên lệch đúng ±1 nửa cung (Yin chắc, dư < 30 cent) → coi là nguồn đặt tên sai, sửa phím gốc theo Yin (ghi chú).
//   3a. Sustain (dây / kèn / sáo): tìm vòng loop trong đoạn ổn định, kết thúc trước maxSustainSec, dài ≥ minLoopSec,
//       bằng tương quan quanh hai
//       điểm nối, rồi CROSSFADE: đoạn ngay trước loopEnd được trộn dần sang đoạn ngay trước loopStart → mẫu cuối
//       loop nối tiếp mẫu đầu loop như tín hiệu liền. File cắt ngay tại loopEnd (release do ampeg_release lo).
//   3b. Decay (piano / harp / pizz): giữ tới khi mức (RMS 50 ms) xuống dưới đỉnh −60 dB hoặc tối đa maxSec (nốt
//       trầm dài hơn nốt cao), fade-out cosin ở 25 % cuối (≥ 0.3 s).
//   4. chuẩn hoá đỉnh −1 dBFS, làm tròn 24-bit; cân loudness như kit (volume kéo về sample nhỏ nhất, ≤ 12 dB).
// Zone: phím gốc chung cho mọi lớp, cách nhau ≥ minSpacing nửa cung (nguồn dày hơn thì bỏ bớt); biên zone ở giữa hai
// phím gốc kề nhau, zone đầu / cuối phủ tới phím 0 / 127 (fullKeyboard) → mọi (phím, velocity) đúng một zone. Lớp velocity xếp
// hạng TRONG từng nốt (nhẹ nhất … mạnh nhất); nốt chỉ có 1 sample thì các lớp dùng chung file (loader nạp 1 lần).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace le::render::import {

struct SourceSample {
    std::string name;              // tên file nguồn (báo cáo, file đích)
    int nameNote = -1;             // nốt đọc từ tên theo quy ước "C4 = 60" (có thể lệch quãng tám)
    int layer = 0;                 // hạng velocity trong nguồn (nhỏ = nhẹ); chỉ cần đúng thứ tự
    std::vector<float> left, right;   // right rỗng = mono
    double sampleRate = 48000.0;
};

enum class Mode : uint8_t { Sustain, Decay };

struct ImportSpec {
    std::string id;                // "inst_violin"
    std::string title;             // dòng mô tả trong .sfz
    std::string credit;            // ghi công trong .sfz (CC-BY bắt buộc)
    Mode mode = Mode::Sustain;
    int maxLayers = 2;             // lấy tối đa N lớp (nhẹ nhất … mạnh nhất, cách đều)
    int minSpacing = 3;            // nửa cung tối thiểu giữa hai phím gốc
    bool stereo = false;
    double minLoopSec = 0.8;       // Sustain: loop tối thiểu
    double maxSustainSec = 3.2;    // Sustain: loopEnd không quá mốc này (tính từ onset) → file ngắn, vừa ngân sách
    double maxSecLow = 8.0, maxSecHigh = 3.0;   // Decay: độ dài tối đa ở nốt thấp nhất / cao nhất (nội suy)
    double attack = 0.003, release = 0.3;       // ampeg_attack / ampeg_release (s)
    bool measureTune = true;       // false: giữ cao độ gốc (tune 0) — piano: stretch tuning là CÓ CHỦ Ý, Yin lại kém
                                   // tin ở piano (không hài hoà, 3 dây / nốt lệch nhau); vẫn dùng Yin để sửa tên sai
    int extendBelow = 2, extendAbove = 2;       // dải tự nhiên (manifest "range") = phím gốc thấp nhất − 2 .. cao nhất + 2
    bool fullKeyboard = true;      // zone đầu / cuối kéo tới phím 0 / 127 (P2-35 B2: mọi phím đều kêu, ngoài dải = repitch)
};

struct ImportedZone {
    int root = 60, loKey = 60, hiKey = 60, loVel = 1, hiVel = 127;
    int layer = 0;                 // 0 = nhẹ nhất trong các lớp đã chọn
    float tuneCents = 0.0f;        // SFZ tune (bù độ lệch đo được)
    float measuredCents = 0.0f;    // cao độ đo được so với root (cent, trước khi bù)
    bool pitchMeasured = false;    // false: Yin không chắc → dùng nốt theo tên, tune 0
    float volumeDb = 0.0f;
    double loudnessDb = 0.0;
    int64_t loopStart = -1, loopEnd = -1;   // Sustain: [start, end), end == số frame
    std::string file;              // "<prefix>_<root>_<lớp>.flac"
    std::string source;            // tên file nguồn
    std::vector<float> left, right;   // rỗng nếu shared
    double sampleRate = 48000.0;
    bool shared = false;           // cùng file với zone lớp khác (nốt chỉ có 1 sample nguồn) → không ghi lại
};

struct ImportResult {
    bool ok = false;
    std::string error;
    std::string sfzText;
    int rangeLo = 0, rangeHi = 127;    // dải tự nhiên (không tính phần repitch tới 0 / 127)
    std::vector<ImportedZone> zones;   // theo lớp rồi theo root
    std::vector<std::string> notes;    // cảnh báo / ghi chú (cao độ không đo được, tên lệch…)
};

ImportResult importInstrument(const ImportSpec& spec, const std::vector<SourceSample>& sources);

// ── Kit trống từ mẫu thật (P2-35 đợt C: kit_acoustic) ──
// Mỗi pad: 1–2 lớp velocity (nhẹ → mạnh, đã trộn các micro thành mono), cắt đầu, cắt đuôi ở −60 dB hoặc maxSec, fade
// cosin 25 % cuối, đỉnh −1 dBFS, 24-bit. Âm gõ thật có đỉnh transient nhọn → chuẩn hoá đỉnh làm thân âm nhỏ: pad
// nào nhỏ hơn TRUNG VỊ loudness của kit được nén đỉnh (limitPeaks, tối đa kMaxLimitDb) cho gần trung vị, rồi mới cân
// loudness cả kit (volume ≤ 0) về PHÂN VỊ 25 % loudness của kit — âm quá "nhọn" (rimshot) có thể nhỏ hơn mức đích vài
// dB thay vì kéo cả kit nhỏ đi. `sameAs` = dùng lại file của pad key
// khác và dịch cao độ bằng SFZ `tune` (VD bộ chỉ có 4 tom mà map GM cần 6) — file chỉ ghi / nạp một lần.
struct KitLayerSource {
    std::string name;              // mô tả nguồn (báo cáo)
    std::vector<float> mono;
    double sampleRate = 48000.0;
};
struct KitPadSpec {
    int key = 36;
    std::string label;             // region_label
    std::string file;              // tiền tố file đích, VD "kick" → kick_soft.flac / kick_hard.flac
    int group = 0, offBy = 0;
    float tuneCents = 0.0f;
    double maxSec = 2.0;
    int sameAs = -1;               // key của pad dùng chung file (−1 = pad có nguồn riêng)
    std::vector<KitLayerSource> layers;   // nhẹ → mạnh (rỗng nếu sameAs)
};
struct KitRegion {
    int key = 36, loVel = 1, hiVel = 127;
    std::string file, label;
    int group = 0, offBy = 0;
    float tuneCents = 0.0f, volumeDb = 0.0f;
    double loudnessDb = 0.0;
    bool shared = false;           // file của pad khác (sameAs) → không ghi lại
    std::vector<float> samples;
    double sampleRate = 48000.0;
};
struct KitImportResult {
    bool ok = false;
    std::string error, sfzText;
    std::vector<KitRegion> regions;
    std::vector<std::string> notes;    // pad đã nén đỉnh bao nhiêu dB
};
inline constexpr double kMaxLimitDb = 6.0;
KitImportResult importKit(const std::string& id, const std::string& title, const std::string& credit,
                          const std::vector<KitPadSpec>& pads);

// Âm gõ: cắt đầu (onset), cắt đuôi ở đỉnh −60 dB (RMS 20 ms) hoặc maxSec, fade cosin 25 % cuối (≥ 30 ms), đỉnh
// −1 dBFS, lưới 24-bit.
std::vector<float> prepareOneShot(const std::vector<float>& mono, double sr, double maxSec);

// Nén đỉnh có nhìn trước (lookahead 2 ms, nhả 40 ms): hệ số khuếch đại giảm mượt quanh transient để đỉnh ≤ đỉnh cũ
// − reductionDb, rồi chuẩn hoá lại đỉnh −1 dBFS (+ 24-bit) → thân âm to lên ~reductionDb. Tất định.
void limitPeaks(std::vector<float>& x, double sr, double reductionDb);

// ── khối dựng (public để test) ──

// Nốt MIDI từ tên kiểu "A#2", "Db4", "C-1" theo quy ước C4 = 60. Sai → −1.
int noteFromName(const std::string& token);

// Frame đầu tiên có |x| ≥ đỉnh · 10^(thresholdDb/20), lùi lại preRollSec (≥ 0).
int64_t findOnset(const std::vector<float>& x, double sr, double thresholdDb = -40.0, double preRollSec = 0.002);

// Vòng loop cho đoạn [searchStart, searchEnd): loopEnd gần searchEnd, loopStart sao cho độ dài ≥ minLen và hai điểm
// nối giống nhau nhất (tương quan chuẩn hoá cửa sổ ±win quanh mỗi điểm). score ∈ [−1, 1].
struct LoopPoints {
    int64_t start = -1, end = -1;
    double score = -1.0;
};
LoopPoints findLoop(const std::vector<float>& mono, double sr, int64_t searchStart, int64_t searchEnd, double minLenSec);

// Đổi tần số lấy mẫu (VD 44.1 → 48 kHz) bằng sinc cửa sổ Kaiser đa pha: srOut/srIn rút gọn thành L/M nguyên
// (48000/44100 = 160/147) → mỗi pha có đúng một bộ hệ số tính sẵn, không nội suy bảng → tất định. 32 tap mỗi phía,
// cắt ở 0.95 × Nyquist nhỏ hơn, suy hao dải chặn ≈ 90 dB. Tần số nguyên Hz bắt buộc (≤ 384 kHz).
std::vector<float> resample(const std::vector<float>& x, int srIn, int srOut);

// Crossfade để vòng [loopStart, loopEnd) liền mạch: x[loopEnd − xf + k] ← x[loopEnd − xf + k]·(1 − w) + x[loopStart − xf + k]·w,
// w: 0 → 1 (cosin nâng). Cần loopStart ≥ xf.
void crossfadeLoop(std::vector<float>& x, int64_t loopStart, int64_t loopEnd, int64_t xf);

} // namespace le::render::import
