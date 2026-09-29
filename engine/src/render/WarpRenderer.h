// WarpRenderer — co giãn thời gian clip audio theo BPM mới, GIỮ cao độ (04 §10, P3-09 lõi). [worker]
// 68 làm debounce 300 ms, job (cancel khi BPM lại đổi), đưa kết quả vào snapshot; AudioClipPlayer chuyển
// Re-Pitch → Stretched ở ranh giới bar (P3-10).
//
//   ratio = originalBpm / newBpm     (BPM mới nhanh hơn → clip ngắn lại)
//   độ dài output = round(inFrames · originalBpm / newBpm)   — CHÍNH XÁC từng sample, không pre-roll
// Signalsmith offline (seek → process → flush, cách đã chốt ở P0-09), render theo khúc 8192 sample output để
// kiểm cancel (dừng < 100 ms) và báo progress. Deterministic (seed cố định). Mono/stereo giữ nguyên số kênh
// và sample rate của nguồn.
//
// Cache: <project>/cache/stretched/<clipId>@<bpm 2 chữ số thập phân>.caf  (06 §1), float32 qua
// io::writeCafFloat32 / io::decodeAudioFile. Đọc cache kiểm độ dài + sample rate khớp → sai thì coi như không có.
#pragma once

#include "dsp/AudioData.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace le::render {

// Preset STFT (04 §10). Phase vocoder làm nhoè transient ra quanh nó trong khoảng ~nửa block: block mặc định 120 ms
// → pre-echo −27 dB ở 5 ms và −50 dB ở 10 ms trước mỗi click (đo 29/09). Preset percussive:
//   1) block ngắn 40/10 ms: nhoè xa (15–30 ms trước onset) giảm ~23 dB. Block ≤ 40 ms làm hỏng âm trầm (nguyên âm 110 Hz
//      mất cao độ) → chỉ dùng cho clip percussive; không xuống thấp hơn 40 ms.
//   2) nén pre-echo: trong 25 ms trước mỗi onset (vị trí ánh xạ từ nguồn), đường bao output bị kẹp về ≤ 2 × đường bao
//      nguồn (+6 dB) theo khung 2.5 ms, chừa 1 ms sát onset (không đụng attack). Chỉ GIẢM gain → không thêm gì, tất định,
//      độ dài giữ nguyên. Nền liên tục (hi-hat, bass, nốt đang ngân) giữ nguyên vì nguồn cũng có mức đó.
// Auto (analyzeTransients): onset dày (≥ 1/s) → nén pre-echo, KỂ CẢ clip có cao độ (click có thân sine, tom, pluck: an toàn vì
//   chỉ cắt phần vượt đường bao nguồn); thêm block ngắn CHỈ khi percussive (onset dày + không có cao độ).
// Tonal: preset mặc định, không nén. Percussive: block ngắn + nén.
enum class WarpPreset : uint8_t { Auto, Tonal, Percussive };
inline constexpr double kPercussiveBlockMs = 40.0, kPercussiveIntervalMs = 10.0;

struct WarpConfig {
    double originalBpm = 120.0;
    double newBpm = 120.0;
    WarpPreset preset = WarpPreset::Auto;
    double blockMs = 0.0, intervalMs = 0.0;      // > 0 cả hai: ghi đè preset (bench / thử nghiệm)
    const std::atomic<bool>* cancel = nullptr;
    std::atomic<float>*      progress = nullptr;
};

struct WarpResult {
    bool ok = false;
    int32_t error = 0;                // LeError: OK | INVALID_ARG | JOB_CANCELLED
    std::string message;
    dsp::AudioDataPtr data;           // khi ok: numFrames == warpedLength(...)
    double ratio = 1.0;               // originalBpm / newBpm
    double msTotal = 0.0;
    bool   percussive = false;        // block ngắn (40/10) đã được dùng
    int    suppressedOnsets = 0;      // số onset đã được nén pre-echo (0 = không nén / không có gì để nén)
    double onsetsPerSecond = 0.0;     // từ analyzeTransients (Auto)
    float  periodicFraction = 0.0f;
    int    blockSamples = 0, intervalSamples = 0;
};

// Clip "percussive" (loop trống) hay "tonal"? Tất định, chỉ đọc dữ liệu. [worker]
//   onset: năng lượng khung 5 ms (bước 2.5 ms, trộn mono) TĂNG ≥ 10 dB so với mức thấp nhất trong 10 ms trước, không thấp
//          hơn đỉnh clip quá 50 dB, cách onset trước ≥ 50 ms. onsetsPerSecond = số onset / độ dài clip.
//   periodicFraction: tỉ lệ khung Yin (2048 sample, tối đa 24 khung rải đều) có cao độ trong các khung không im lặng.
//   dense = onsetsPerSecond ≥ 1 (→ nén pre-echo). percussive = dense VÀ periodicFraction < 0.5 (→ thêm block ngắn):
//   arpeggio / tiếng gảy có cao độ không bị block ngắn làm hỏng âm trầm.
struct TransientStats {
    double onsetsPerSecond = 0.0;
    float  periodicFraction = 0.0f;
    int    onsets = 0;
    bool   dense = false;
    bool   percussive = false;
    std::vector<int64_t> onsetSamples;   // vị trí attack (sample nguồn): mẫu đầu tiên ≥ 10 % đỉnh của khung onset
};
TransientStats analyzeTransients(const dsp::AudioData& source);

// round(inFrames · originalBpm / newBpm), ≥ 1 khi inFrames ≥ 1. BPM ≤ 0 → trả inFrames.
int64_t warpedLength(int64_t inFrames, double originalBpm, double newBpm) noexcept;

// [worker]
WarpResult renderWarp(const dsp::AudioData& source, const WarpConfig& cfg);

// Tên file cache: "c_1a2b@120.00.caf". Đường dẫn đầy đủ: projectDir/cache/stretched/<tên>.
std::string stretchedCacheFileName(const std::string& clipId, double bpm);
std::string stretchedCachePath(const std::string& projectDir, const std::string& clipId, double bpm);

// [worker] Ghi / đọc cache. read trả nullptr nếu thiếu, hỏng, hoặc không khớp (expectFrames, expectSampleRate).
bool writeStretchedCache(const std::string& path, const dsp::AudioData& data, std::string* error = nullptr);
dsp::AudioDataPtr readStretchedCache(const std::string& path, int64_t expectFrames, double expectSampleRate,
                                     std::string* error = nullptr);

} // namespace le::render
