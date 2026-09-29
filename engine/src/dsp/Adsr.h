// Adsr — envelope Attack/Decay/Sustain/Release cho một voice sampler (04 §6.3, P1-26). [RT]
//
//   Attack : tăng TUYẾN TÍNH từ mức hiện tại lên 1 trong `attack` giây
//   Decay  : giảm theo HÀM MŨ từ 1 về `sustain`; sau `decay` giây còn cách sustain −60 dB (sàn 5 ms)
//   Sustain: giữ nguyên cho tới note-off (sustain = 0 → nốt tắt hẳn sau decay)
//   Release: giảm theo hàm mũ; sau `release` giây còn −60 dB so với lúc nhả phím, tắt hẳn ở −80 dB
//   Fast release (voice bị cướp 3 ms, choke 5 ms): giảm TUYẾN TÍNH về 0 trong đúng N sample → biết
//   chắc voice được giải phóng lúc nào.
//
// "Hệ số tính sẵn": vài phép exp chỉ chạy trong start() (1 lần mỗi nốt), next() mỗi sample chỉ có
// phép nhân/cộng. Toàn bộ là toán số → an toàn trên audio thread.
// Sàn chống click: attack ≥ 0.5 ms, release ≥ 5 ms (sample thường không bắt đầu/kết thúc đúng ở 0).
#pragma once

#include "dsp/Instrument.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace le::dsp {

class Adsr {
public:
    using Params = AdsrParams;
    enum class Stage : uint8_t { Idle, Attack, Decay, Sustain, Release, FastRelease };

    static constexpr float kMinAttack = 0.0005f;
    static constexpr float kMinDecay = 0.005f;
    static constexpr float kMinRelease = 0.005f;
    static constexpr float kOffLevel = 1.0e-4f;   // −80 dB: coi như im lặng

    // [RT] Nạp tham số (tính hệ số) và bắt đầu Attack TỪ MỨC HIỆN TẠI (voice mới = 0).
    void start(const Params& p, double sampleRate) noexcept [[clang::nonblocking]] {
        const double sr = std::max(1.0, sampleRate);
        const double a = std::max(static_cast<double>(p.attack), static_cast<double>(kMinAttack));
        const double r = std::max(static_cast<double>(p.release), static_cast<double>(kMinRelease));
        attackStep_ = static_cast<float>(1.0 / (a * sr));
        sustain_ = std::clamp(p.sustain, 0.0f, 1.0f);
        // −60 dB sau t giây: coef^(t·sr) = 10^−3  →  coef = exp(ln(10^−3) / (t·sr))
        // sustain = 1 thì không có pha decay. decay = 0 mà sustain < 1: dùng 5 ms để khỏi nhảy bậc (click).
        const double d = std::max(static_cast<double>(p.decay), static_cast<double>(kMinDecay));
        decayCoef_ = sustain_ < 1.0f ? static_cast<float>(std::exp(kLn1e3 / (d * sr))) : 0.0f;
        releaseCoef_ = static_cast<float>(std::exp(kLn1e3 / (r * sr)));
        stage_ = Stage::Attack;
    }

    // [RT] Nhả phím: vào Release từ mức hiện tại. Không làm gì nếu đã Idle hoặc đang fast release.
    void noteOff() noexcept [[clang::nonblocking]] {
        if (stage_ != Stage::Idle && stage_ != Stage::FastRelease) stage_ = Stage::Release;
    }

    // [RT] Tắt nhanh tuyến tính về 0 trong `seconds` (≥ 1 sample). Dùng cho voice bị cướp / choke.
    void fastRelease(float seconds, double sampleRate) noexcept [[clang::nonblocking]] {
        if (stage_ == Stage::Idle) return;
        // Làm tròn tới sample gần nhất: 0.003f · 48000 = 144.0000012 (sai số float) phải ra đúng 144.
        fastLeft_ = std::max<int32_t>(1, static_cast<int32_t>(static_cast<double>(seconds) * sampleRate + 0.5));
        fastStep_ = level_ / static_cast<float>(fastLeft_);
        stage_ = Stage::FastRelease;
    }

    // [RT] Tắt ngay (không fade). Chỉ dùng khi mức đã ~0 hoặc khi reset.
    void reset() noexcept [[clang::nonblocking]] {
        stage_ = Stage::Idle;
        level_ = 0.0f;
    }

    // [RT] Mức của sample kế tiếp.
    float next() noexcept [[clang::nonblocking]] {
        switch (stage_) {
            case Stage::Idle:
                return 0.0f;
            case Stage::Attack:
                level_ += attackStep_;
                if (level_ >= 1.0f) {
                    level_ = 1.0f;
                    stage_ = sustain_ < 1.0f ? Stage::Decay : Stage::Sustain;
                }
                break;
            case Stage::Decay:
                level_ = sustain_ + (level_ - sustain_) * decayCoef_;
                if (level_ - sustain_ < kOffLevel) {
                    level_ = sustain_;
                    stage_ = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                if (level_ < kOffLevel) {   // sustain = 0: nốt đã tắt sau decay
                    reset();
                    return 0.0f;
                }
                break;
            case Stage::Release:
                level_ *= releaseCoef_;
                if (level_ < kOffLevel) {
                    reset();
                    return 0.0f;
                }
                break;
            case Stage::FastRelease:
                level_ = std::max(0.0f, level_ - fastStep_);
                if (--fastLeft_ <= 0) {
                    reset();
                    return 0.0f;
                }
                break;
        }
        return level_;
    }

    Stage stage() const noexcept [[clang::nonblocking]] { return stage_; }
    float level() const noexcept [[clang::nonblocking]] { return level_; }
    bool  isActive() const noexcept [[clang::nonblocking]] { return stage_ != Stage::Idle; }
    bool  isReleasing() const noexcept [[clang::nonblocking]] {
        return stage_ == Stage::Release || stage_ == Stage::FastRelease;
    }

private:
    static constexpr double kLn1e3 = -6.907755278982137;   // ln(10^−3)

    Stage   stage_ = Stage::Idle;
    float   level_ = 0.0f;
    float   attackStep_ = 1.0f;
    float   decayCoef_ = 0.0f;
    float   sustain_ = 1.0f;
    float   releaseCoef_ = 0.0f;
    float   fastStep_ = 0.0f;
    int32_t fastLeft_ = 0;
};

} // namespace le::dsp
