#pragma once
// Mixer + Limiter + PeakMeter (P1-13/14, 04 §11) [RT] trừ prepare().
// - Mỗi track: gain (dB, ≤ -120 = -inf, tối đa +6), pan luật -3 dB (công suất không đổi: giữa = 0.707 mỗi kênh),
//   mute/solo (ramp 5 ms, có solo thì track không solo bị câm). Gain/pan trượt tuyến tính 20 ms.
// - Master: gain → EQ3 (MasterEq, P3-15) → Limiter → output.
// - Limiter KHÔNG lookahead (không thêm độ trễ, quan trọng với looper và mốc thời gian đúng sample):
//   gain g(n) = min(ceiling/|x(n)|, g nhả dần về 1 với τ 50 ms) → ra luôn ≤ ceiling (-0.3 dBFS). Đổi lại khi quá tải
//   nặng có méo nhẹ ở đỉnh — đây là limiter an toàn, không phải công cụ làm to.
// - Meter: peak THÔ theo cửa sổ 25 ms (đủ dài để UI đọc 60 Hz không sót đỉnh). Ballistics làm ở UI (04 §11).
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "le/engine_api.h"

namespace le::core {

// Trượt tuyến tính tới giá trị đích trong N sample (không có exp/pow trên RT).
struct LinearRamp {
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int remaining = 0;
    void set(float t, int samples) noexcept [[clang::nonblocking]] {
        target = t;
        if (samples <= 0) {
            current = t;
            remaining = 0;
            return;
        }
        step = (t - current) / (float) samples;
        remaining = samples;
    }
    void snap(float t) noexcept [[clang::nonblocking]] { current = target = t; remaining = 0; }
    float next() noexcept [[clang::nonblocking]] {
        if (remaining > 0 && --remaining == 0) current = target;
        else if (remaining > 0) current += step;
        return current;
    }
};

// Peak thô theo cửa sổ: value() = max(cửa sổ đang chạy, cửa sổ vừa xong).
struct PeakMeter {
    int window = 1200;
    int count = 0;
    float running = 0.0f, last = 0.0f;
    void prepare(double sr) noexcept { window = std::max(1, (int) std::lround(0.025 * sr)); count = 0; running = last = 0.0f; }
    void push(const float* x, int n) noexcept [[clang::nonblocking]] {
        for (int i = 0; i < n; ++i) {
            running = std::max(running, std::fabs(x[i]));
            if (++count >= window) { last = running; running = 0.0f; count = 0; }
        }
    }
    float value() const noexcept [[clang::nonblocking]] { return std::max(running, last); }
};

class Limiter {
public:
    static constexpr float kMinCeilingDb = -12.0f, kMaxCeilingDb = 0.0f;
    static constexpr float kMinReleaseMs = 1.0f, kMaxReleaseMs = 1000.0f;

    void prepare(double sr, float ceilingDb = -0.3f, float releaseSec = 0.05f) noexcept {
        sr_ = sr > 0.0 ? sr : 48000.0;
        setCeilingDb(ceilingDb);
        setReleaseSeconds(releaseSec);
        gain_ = 1.0f;
    }
    // [RT] P3-15. Đổi trần tức thì là an toàn: hạ trần → gain giảm ngay (vốn tấn công tức thì), nâng trần → nhả dần.
    void setCeilingDb(float db) noexcept [[clang::nonblocking]] {
        ceiling_ = std::pow(10.0f, std::clamp(db, kMinCeilingDb, kMaxCeilingDb) / 20.0f);
    }
    void setReleaseSeconds(float sec) noexcept [[clang::nonblocking]] {
        const float s = std::clamp(sec, kMinReleaseMs * 0.001f, kMaxReleaseMs * 0.001f);
        releaseCoef_ = 1.0f - std::exp(-1.0f / (s * (float) sr_));
    }
    // [RT] Xử lý tại chỗ 2 kênh.
    void process(float* L, float* R, int n) noexcept [[clang::nonblocking]] {
        const float c = ceiling_;
        float g = gain_;
        for (int i = 0; i < n; ++i) {
            const float peak = std::max(std::fabs(L[i]), std::fabs(R[i]));
            const float need = peak > c ? c / peak : 1.0f;
            g = need < g ? need : g + (1.0f - g) * releaseCoef_;   // tấn công tức thì, nhả mũ về 1
            if (g > need) g = need;
            L[i] = std::clamp(L[i] * g, -c, c);   // kẹp an toàn cho sai số float
            R[i] = std::clamp(R[i] * g, -c, c);
        }
        gain_ = g;
    }
    float gainReduction() const noexcept { return gain_; }
    float ceiling() const noexcept { return ceiling_; }

private:
    double sr_ = 48000.0;
    float ceiling_ = 0.966f;
    float releaseCoef_ = 0.0004f;
    float gain_ = 1.0f;
};

class Mixer {
public:
    // [main]
    void prepare(double sr) noexcept {
        rampGain_ = std::max(1, (int) std::lround(0.020 * sr));
        rampMute_ = std::max(1, (int) std::lround(0.005 * sr));
        for (auto& s : strips_) {
            s.gain.snap(dbToLin(s.gainDb));
            s.panL.snap(panLeft(s.pan));
            s.panR.snap(panRight(s.pan));
            s.audible.snap(audibleTarget(s));
            s.meterL.prepare(sr);
            s.meterR.prepare(sr);
        }
        master_.snap(dbToLin(masterDb_));
        limiter_.prepare(sr, limiterCeilingDb_, limiterReleaseMs_ * 0.001f);   // giữ tham số qua lần restart device
        masterMeterL_.prepare(sr);
        masterMeterR_.prepare(sr);
    }

    // ── Lệnh [RT] ──
    void setGainDb(int t, float db) noexcept [[clang::nonblocking]] {
        strips_[t].gainDb = db;
        strips_[t].gain.set(dbToLin(db), rampGain_);
    }
    void setPan(int t, float pan) noexcept [[clang::nonblocking]] {
        strips_[t].pan = std::clamp(pan, -1.0f, 1.0f);
        strips_[t].panL.set(panLeft(strips_[t].pan), rampGain_);
        strips_[t].panR.set(panRight(strips_[t].pan), rampGain_);
    }
    void setMute(int t, bool on) noexcept [[clang::nonblocking]] { strips_[t].mute = on; updateAudible(); }
    void setSolo(int t, bool on) noexcept [[clang::nonblocking]] { strips_[t].solo = on; updateAudible(); }
    void setMasterGainDb(float db) noexcept [[clang::nonblocking]] {
        masterDb_ = db;
        master_.set(dbToLin(db), rampGain_);
    }
    // LE_CMD_FX_PARAM track -1, slot 1: id 0 = trần (dB, -12..0), id 1 = release (ms, 1..1000).
    void setLimiterParam(int id, float v) noexcept [[clang::nonblocking]] {
        if (id == 0) {
            limiterCeilingDb_ = std::clamp(v, Limiter::kMinCeilingDb, Limiter::kMaxCeilingDb);
            limiter_.setCeilingDb(limiterCeilingDb_);
        } else if (id == 1) {
            limiterReleaseMs_ = std::clamp(v, Limiter::kMinReleaseMs, Limiter::kMaxReleaseMs);
            limiter_.setReleaseSeconds(limiterReleaseMs_ * 0.001f);
        }
    }

    // [RT] Cộng bus của từng track (đã render) vào out, qua gain/pan/mute/solo. Đo peak sau fader.
    void mixTrack(int t, const float* inL, const float* inR, float* outL, float* outR, float* scratchL, float* scratchR,
                  int n) noexcept [[clang::nonblocking]] {
        Strip& s = strips_[t];
        for (int i = 0; i < n; ++i) {
            const float g = s.gain.next() * s.audible.next();
            scratchL[i] = inL[i] * g * s.panL.next();
            scratchR[i] = inR[i] * g * s.panR.next();
            outL[i] += scratchL[i];
            outR[i] += scratchR[i];
        }
        s.meterL.push(scratchL, n);
        s.meterR.push(scratchR, n);
    }

    // [RT] Master gain → limiter → meter (tại chỗ). RtEngine gọi 2 nửa riêng để chèn MasterEq ở giữa.
    void processMaster(float* L, float* R, int n) noexcept [[clang::nonblocking]] {
        applyMasterGain(L, R, n);
        limitAndMeter(L, R, n);
    }
    void applyMasterGain(float* L, float* R, int n) noexcept [[clang::nonblocking]] {
        for (int i = 0; i < n; ++i) {
            const float g = master_.next();
            L[i] *= g;
            R[i] *= g;
        }
    }
    void limitAndMeter(float* L, float* R, int n) noexcept [[clang::nonblocking]] {
        // R2: NaN / Inf (FX hỏng, chia 0…) KHÔNG được ra loa: mẫu không hữu hạn → 0, đếm lại (engine.info).
        std::uint32_t bad = 0;
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(L[i])) {
                L[i] = 0.0f;
                ++bad;
            }
            if (!std::isfinite(R[i])) {
                R[i] = 0.0f;
                ++bad;
            }
        }
        if (bad != 0) nonFinite_.fetch_add(bad, std::memory_order_relaxed);
        limiter_.process(L, R, n);
        masterMeterL_.push(L, n);
        masterMeterR_.push(R, n);
    }

    float trackPeak(int t, int ch) const noexcept [[clang::nonblocking]] {
        return ch == 0 ? strips_[t].meterL.value() : strips_[t].meterR.value();
    }
    float masterPeak(int ch) const noexcept [[clang::nonblocking]] { return ch == 0 ? masterMeterL_.value() : masterMeterR_.value(); }
    const Limiter& limiter() const noexcept { return limiter_; }
    // [RT / test] Giá trị đích đã đặt.
    float gainDb(int t) const noexcept [[clang::nonblocking]] { return strips_[t].gainDb; }
    float pan(int t) const noexcept [[clang::nonblocking]] { return strips_[t].pan; }
    bool muted(int t) const noexcept [[clang::nonblocking]] { return strips_[t].mute; }
    bool soloed(int t) const noexcept [[clang::nonblocking]] { return strips_[t].solo; }
    float masterGainDb() const noexcept [[clang::nonblocking]] { return masterDb_; }
    // [any] Số mẫu NaN/Inf đã bị đổi thành 0 ở master (R2).
    std::uint32_t nonFiniteSamples() const noexcept { return nonFinite_.load(std::memory_order_relaxed); }

    static float dbToLin(float db) noexcept [[clang::nonblocking]] { return db <= -120.0f ? 0.0f : std::pow(10.0f, db / 20.0f); }
    static float panLeft(float p) noexcept [[clang::nonblocking]] { return std::cos((p + 1.0f) * 0.25f * 3.14159265f); }
    static float panRight(float p) noexcept [[clang::nonblocking]] { return std::sin((p + 1.0f) * 0.25f * 3.14159265f); }

private:
    struct Strip {
        float gainDb = 0.0f, pan = 0.0f;
        bool mute = false, solo = false;
        LinearRamp gain, panL, panR, audible;
        PeakMeter meterL, meterR;
        Strip() {
            gain.snap(1.0f);
            panL.snap(0.70710678f);
            panR.snap(0.70710678f);
            audible.snap(1.0f);
        }
    };
    float audibleTarget(const Strip& s) const noexcept [[clang::nonblocking]] {
        bool anySolo = false;
        for (const auto& x : strips_) anySolo |= x.solo;
        return (!s.mute && (!anySolo || s.solo)) ? 1.0f : 0.0f;
    }
    void updateAudible() noexcept [[clang::nonblocking]] {
        for (auto& s : strips_) s.audible.set(audibleTarget(s), rampMute_);
    }

    Strip strips_[LE_MAX_TRACKS];
    LinearRamp master_;
    float masterDb_ = 0.0f;
    float limiterCeilingDb_ = -0.3f, limiterReleaseMs_ = 50.0f;
    int rampGain_ = 960, rampMute_ = 240;
    Limiter limiter_;
    PeakMeter masterMeterL_, masterMeterR_;
    std::atomic<std::uint32_t> nonFinite_{0};
};

} // namespace le::core
