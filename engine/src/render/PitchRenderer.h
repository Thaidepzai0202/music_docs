// PitchRenderer — biến tiếng tự thu (mono) thành nhạc cụ sampler 13 zone (04 §8, P3-04 lõi). [worker]
// Chạy trên worker thread (cấp phát, Signalsmith). 68 bọc thành job `instrument.createFromRecording`.
//
// Pipeline:
//   1) Nốt gốc: Yin (frame 2048, hop 512) → rootNote + cents + confidence. Có Config::rootNote (người
//      dùng chọn trên UI) thì dùng giá trị đó. Yin không chắc (confidence < 0.6) mà không có rootNote
//      → LE_ERR_PITCH_NOT_DETECTED (UI hỏi nốt, 04 §8).
//   2) Mỗi zone k ∈ {−18, −15, …, +18}: Signalsmith offline, setTransposeSemitones(k − cents/100) → zone ra
//      ĐÚNG nốt tuyệt đối root + k. Giữ formant: setFormantFactor(1, compensatePitch) + setFormantBase(f0/sr)
//      (đã chốt ở P0-09, engine/tools/docs/signalsmith-notes.md §2).
//   3) Đo lại bằng Yin (~1 s giữa zone): lệch so với cao độ mong muốn = cao độ Yin của input · 2^(shift/12)
//      → Zone::tuneCents = −lệch (Sampler bù khi phát). Signalsmith có thể lệch vài cent (notes §4).
//   4) Chuẩn hoá âm lượng: Zone::gainDb = RMS(zone gốc k = 0) − RMS(zone k), kẹp ±maxGainDb
//      (giữ formant làm zone cao nhỏ đi tới −12 dB, notes §5). Dữ liệu zone KHÔNG bị sửa.
//   5) Zone k phủ phím [root+k−1, root+k+1]; zone thấp nhất kéo xuống phím 0, cao nhất lên 127.
//      Instrument mode Natural, classicZone = zone k = 0.
// Deterministic (Signalsmith seed cố định, Yin không ngẫu nhiên). Có cancel (kiểm mỗi khúc ~8192 sample)
// và progress 0..1.
#pragma once

#include "dsp/Instrument.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace le::render {

struct PitchRenderConfig {
    std::vector<int> semitones = {-18, -15, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18};
    int   rootNote = -1;             // ≥ 0: người dùng chọn → bỏ qua Yin khi quyết định nốt gốc
    float rootCents = 0.0f;          // đi kèm rootNote (thường 0)
    float minConfidence = 0.6f;      // 04 §8
    bool  formant = true;
    double blockMs = 0.0, intervalMs = 0.0;   // 0 = presetDefault của Signalsmith
    bool  measureTune = true;        // bước 3
    bool  normalizeRms = true;       // bước 4
    float maxGainDb = 12.0f;
    dsp::AdsrParams env{0.005f, 0.2f, 0.8f, 0.3f};   // mặc định như userInstruments trong 06 §2
    std::string name = "Tiếng thu";

    const std::atomic<bool>* cancel = nullptr;
    std::atomic<float>*      progress = nullptr;
};

struct PitchRenderResult {
    bool ok = false;
    int32_t error = 0;               // LeError: OK | INVALID_ARG | PITCH_NOT_DETECTED | JOB_CANCELLED
    std::string message;
    dsp::InstrumentPtr instrument;

    int   rootNote = -1;             // nốt gốc đã dùng
    float rootCents = 0.0f;
    float detectedHz = 0.0f;         // Yin của input (0 nếu không dò được)
    float confidence = 0.0f;
    bool  rootFromUser = false;

    std::vector<int>   semitones;    // theo thứ tự zone (tăng dần)
    std::vector<float> zoneTuneCents, zoneGainDb, zoneMeasuredErrorCents;
    double msTotal = 0.0;
};

PitchRenderResult renderPitchInstrument(const float* mono, int64_t numSamples, double sampleRate,
                                        const PitchRenderConfig& cfg);

} // namespace le::render
