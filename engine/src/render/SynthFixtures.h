// SynthFixtures — tổng hợp bằng code bộ trống + nhạc cụ dùng cho test/golden (P1-28). [worker]/[tool]
// KHÔNG dùng sample có license: mọi âm thanh sinh từ công thức toán, DETERMINISTIC (nhiễu dùng
// xorshift tự viết, không phụ thuộc std::*_distribution vốn có thể khác giữa các phiên bản thư viện).
// Tool `le-gen-fixtures` (engine/tools/) ghi ra engine/tests/fixtures/{kit_synth,inst_synth}/.
//
// kit_synth (4 hit, one-shot, 48 kHz mono):
//   kick  (key 36) sine quét cao độ 150 → 45 Hz + tiếng "tách" 2 ms, 0.5 s
//   snare (key 38) tone 185 + 330 Hz + nhiễu lọc cao, 0.35 s
//   hat đóng (key 42) nhiễu lọc cao ~7 kHz, tắt nhanh 40 ms, 0.15 s, group=1 off_by=2
//   hat mở  (key 46) nhiễu lọc cao ~7 kHz, tắt chậm 400 ms, 0.8 s, group=2 off_by=1
// inst_synth (3 root × 2 lớp velocity, loop_continuous):
//   tone có hoạ âm ở C3 / C4 / C5. Tần số NGUYÊN Hz (131 / 262 / 523) → mọi đoạn dài nguyên giây là
//   vòng lặp liền mạch tuyệt đối; `tune` trong SFZ bù phần lệch so với nốt chuẩn (≈ ±2.5 cent).
//   Lớp nhẹ (vel 1–63): biên độ 0.25, 4 hoạ âm (tối). Lớp mạnh (vel 64–127): 0.6, 10 hoạ âm (sáng).
//   File 1.5 s, loop [0.25 s, 1.25 s) → còn 0.25 s "đuôi" sau điểm loop.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace le::render::fixtures {

struct SynthFile {
    std::string relativePath;     // so với thư mục của bộ fixture, ví dụ "samples/kick.wav"
    std::vector<float> samples;   // mono
};

struct SynthSet {
    std::string name;             // "kit_synth" / "inst_synth"
    std::string sfzFileName;      // "kit_synth.sfz"
    std::string sfzText;
    double sampleRate = 48000.0;
    std::vector<SynthFile> files;
};

SynthSet makeDrumKit(double sampleRate = 48000.0);
SynthSet makeInstrument(double sampleRate = 48000.0);

// Thông tin để test đối chiếu (nhạc cụ)
struct ToneZoneInfo { int rootKey; int loKey, hiKey; double hz; double tuneCents; };
std::vector<ToneZoneInfo> instrumentZones();

// Mã băm FNV-1a 64-bit của dữ liệu float (kiểm tra deterministic / file ghi ra không đổi).
uint64_t fnv1a(const std::vector<float>& x);

} // namespace le::render::fixtures
