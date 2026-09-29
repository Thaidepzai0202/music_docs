#pragma once
// Transport (P1-08, 04 §2.1): đồng hồ nhạc. [RT] trừ prepare().
//
// Vì sao không trôi: vị trí beat KHÔNG được cộng dồn từng block. Nó luôn tính lại từ số sample nguyên
// `samplePos` và một điểm neo (anchorSample, anchorBeat):
//     beat(s) = anchorBeat + (s − anchorSample) / samplesPerBeat
// Khi BPM đổi thì neo lại tại vị trí hiện tại, nên beat liên tục, không nhảy.
// "Beat" = một nốt mẫu số của nhịp (4/4 → nốt đen). beatsPerBar = tử số.
#include <cmath>
#include <cstdint>

#include "le/engine_api.h"

namespace le::core {

class Transport {
public:
    static constexpr double kMinBpm = 20.0, kMaxBpm = 300.0;

    // [main] Khi audio chưa chạy (device start/restart). Đổi SR thì giữ nguyên beat hiện tại.
    void prepare(double sampleRate) noexcept {
        const double beat = beatNow();
        sampleRate_ = sampleRate > 0 ? sampleRate : 48000.0;
        anchorSample_ = samplePos_;
        anchorBeat_ = beat;
        updateSpb();
    }

    // ── Lệnh [RT], áp dụng tại đầu block (samplePos hiện tại) ──
    void play() noexcept [[clang::nonblocking]] {
        if (playing_) return;
        playing_ = true;
        samplePos_ = 0;
        anchorSample_ = 0;
        anchorBeat_ = 0.0;   // luôn bắt đầu ở beat 0 (04 §3.2)
    }
    void stop() noexcept [[clang::nonblocking]] {
        playing_ = false;
        samplePos_ = 0;       // 04 §3.4: reset vị trí
        anchorSample_ = 0;
        anchorBeat_ = 0.0;
    }
    void setBpm(double bpm) noexcept [[clang::nonblocking]] {
        const double b = beatNow();
        anchorSample_ = samplePos_;
        anchorBeat_ = b;
        bpm_ = bpm < kMinBpm ? kMinBpm : (bpm > kMaxBpm ? kMaxBpm : bpm);
        updateSpb();
    }
    // [RT] Pedal mode (04 §2.5): vòng đầu dài `samplePos()` sample = `beats` beat. Neo beat 0 tại sample 0 (sample đầu
    // của take) và đặt samplesPerBeat = samplePos / beats CHÍNH XÁC → beatNow() == beats ngay lúc này.
    void retimeFromLength(double beats) noexcept [[clang::nonblocking]] {
        if (beats <= 0.0 || samplePos_ <= 0) return;
        anchorSample_ = 0;
        anchorBeat_ = 0.0;
        spb_ = (double) samplePos_ / beats;
        bpm_ = 60.0 * sampleRate_ / spb_;
    }
    void setTimeSignature(int num, int den) noexcept [[clang::nonblocking]] {
        beatsPerBar_ = num >= 1 && num <= 32 ? num : 4;
        beatUnit_ = (den == 2 || den == 4 || den == 8 || den == 16) ? den : 4;
    }
    void setQuantize(int q) noexcept [[clang::nonblocking]] { quantize_ = q >= LE_Q_NONE && q <= LE_Q_4_BAR ? q : LE_Q_1_BAR; }

    // [RT] Sau khi render xong n frame.
    void advance(int n) noexcept [[clang::nonblocking]] {
        if (playing_) samplePos_ += n;
    }

    // ── Truy vấn [RT] ──
    bool playing() const noexcept [[clang::nonblocking]] { return playing_; }
    double bpm() const noexcept [[clang::nonblocking]] { return bpm_; }
    double sampleRate() const noexcept [[clang::nonblocking]] { return sampleRate_; }
    double samplesPerBeat() const noexcept [[clang::nonblocking]] { return spb_; }
    int beatsPerBar() const noexcept [[clang::nonblocking]] { return beatsPerBar_; }
    int beatUnit() const noexcept [[clang::nonblocking]] { return beatUnit_; }
    int quantize() const noexcept [[clang::nonblocking]] { return quantize_; }
    std::int64_t samplePos() const noexcept [[clang::nonblocking]] { return samplePos_; }   // sample kể từ Play

    double beatAt(std::int64_t s) const noexcept [[clang::nonblocking]] {
        return anchorBeat_ + (double) (s - anchorSample_) / spb_;
    }
    double beatNow() const noexcept [[clang::nonblocking]] { return beatAt(samplePos_); }

    // Sample đầu tiên mà beat ≥ b. Trừ ε = 1e-6 sample để số nguyên chính xác (VD 4 beat × 24000 = 96000)
    // không bị làm tròn lên 96001 do sai số dấu phẩy động.
    std::int64_t sampleAtBeat(double b) const noexcept [[clang::nonblocking]] {
        return anchorSample_ + (std::int64_t) std::ceil((b - anchorBeat_) * spb_ - 1e-6);
    }

    // Độ dài (beat) của một mức quantize (04 §3.2). 1/16 = beatUnit/16 beat (4/4 → 0.25).
    double quantizeLength(int q) const noexcept [[clang::nonblocking]] {
        switch (q) {
            case LE_Q_1_16: return beatUnit_ / 16.0;
            case LE_Q_1_8: return beatUnit_ / 8.0;
            case LE_Q_1_4: return beatUnit_ / 4.0;
            case LE_Q_1_2: return beatUnit_ / 2.0;
            case LE_Q_1_BAR: return beatsPerBar_;
            case LE_Q_2_BAR: return 2.0 * beatsPerBar_;
            case LE_Q_4_BAR: return 4.0 * beatsPerBar_;
            default: return 0.0;
        }
    }
    double quantizeLength() const noexcept [[clang::nonblocking]] { return quantizeLength(quantize_); }

    // Ranh giới quantize kế tiếp tính từ `beat` (đứng đúng ranh giới thì dùng luôn).
    static double boundary(double beat, double q) noexcept [[clang::nonblocking]] {
        if (q <= 0.0) return beat;
        return std::ceil((beat - 1e-9) / q) * q;
    }
    double boundary(double beat) const noexcept [[clang::nonblocking]] { return boundary(beat, quantizeLength()); }

private:
    void updateSpb() noexcept [[clang::nonblocking]] { spb_ = 60.0 * sampleRate_ / bpm_; }

    double sampleRate_ = 48000.0;
    double bpm_ = 120.0;
    double spb_ = 24000.0;
    int beatsPerBar_ = 4;
    int beatUnit_ = 4;
    int quantize_ = LE_Q_1_BAR;
    bool playing_ = false;
    std::int64_t samplePos_ = 0;
    std::int64_t anchorSample_ = 0;
    double anchorBeat_ = 0.0;
};

} // namespace le::core
