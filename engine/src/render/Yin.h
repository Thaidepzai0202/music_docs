// Yin — dò cao độ (f0) của tiếng thu mono, dùng cho pipeline "tiếng tự thu → sampler" (04 §8, P3-01).
// [worker] Chạy trên worker thread (analyze() cấp phát). Không gọi từ audio thread.
//
// Thuật toán YIN (de Cheveigné & Kawahara, 2002), mỗi frame 2048 sample:
//   1) Hàm hiệu d(τ) = Σ (x[j] − x[j+τ])²: tín hiệu lặp lại sau τ sample thì d(τ) ≈ 0.
//   2) Chuẩn hoá tích luỹ d'(τ) = d(τ) / trung bình(d(1..τ)): bỏ được "hố giả" ở τ nhỏ, d' ∈ [0, ~1+].
//   3) Ngưỡng tuyệt đối: τ ĐẦU TIÊN có d'(τ) < 0.12 (đi tiếp tới đáy của hố đó) → tránh lỗi quãng tám thấp.
//   4) Nội suy parabol quanh τ → chu kỳ lẻ sample → f0 = sampleRate / τ.
// Cả buffer: frame cách nhau 512 sample, bỏ frame im lặng, lấy median cao độ các frame "voiced",
// giữ các frame nằm trong ±1 nửa cung quanh median (loại lỗi quãng tám), median lần nữa → nốt + cent.
//
// Dùng:
//   le::Yin yin;                                    // cấu hình mặc định theo 04 §8
//   le::PitchEstimate p = yin.analyze(buf, n, sr);
//   if (!p.ok || p.confidence < 0.6f) → hỏi người dùng chọn nốt gốc (mặc định C4)
//   else  rootNote = p.rootNote; cents = p.cents;   // "A3 +12 cent"
#pragma once

#include <cstdint>
#include <vector>

namespace le {

struct YinConfig {
    int   frameSize = 2048;
    int   hopSize = 512;
    float threshold = 0.12f;    // ngưỡng d'(τ) để coi frame là có cao độ ("voiced")
    float minHz = 50.0f;
    float maxHz = 1500.0f;
    float silenceDb = -45.0f;   // frame có RMS dưới mức này bị bỏ qua (khớp SilenceTrimmer, 04 §8)
    float a4Hz = 440.0f;
};

struct YinFrame {
    float hz = 0.0f;            // 0 nếu không voiced
    float aperiodicity = 1.0f;  // d'(τ) tại τ đã chọn: ~0 = tuần hoàn hoàn hảo, ~1 = nhiễu
    float rmsDb = -200.0f;
    bool  voiced = false;
    bool  silent = false;
};

struct PitchEstimate {
    bool  ok = false;           // false: không đủ frame có cao độ (LE_ERR_PITCH_NOT_DETECTED)
    float hz = 0.0f;
    int   rootNote = -1;        // MIDI 0..127 (69 = A4)
    float cents = 0.0f;         // −50..+50 so với rootNote
    float confidence = 0.0f;    // 0..1 = (tỉ lệ frame ổn định) × (1 − aperiodicity trung vị)
    int   stableFrames = 0;     // frame voiced nằm trong ±1 nửa cung quanh median
    int   voicedFrames = 0;
    int   analyzedFrames = 0;   // frame không im lặng
    int   totalFrames = 0;
};

class Yin {
public:
    explicit Yin(const YinConfig& cfg = {});

    // [worker] Phân tích 1 frame dài cfg.frameSize sample. Không cấp phát.
    YinFrame analyzeFrame(const float* frame, double sampleRate);

    // [worker] Phân tích cả buffer mono. Buffer ngắn hơn 1 frame thì được đệm 0.
    PitchEstimate analyze(const float* mono, int64_t numSamples, double sampleRate);

    const YinConfig& config() const noexcept { return cfg_; }

    static float hzToMidi(float hz, float a4Hz = 440.0f);
    static float midiToHz(float midi, float a4Hz = 440.0f);

private:
    YinConfig cfg_;
    std::vector<float> diff_;    // d(τ)
    std::vector<float> cmnd_;    // d'(τ)
    std::vector<float> pad_;     // frame đệm khi buffer ngắn
    std::vector<float> midi_, aper_, tmp_;
};

} // namespace le
