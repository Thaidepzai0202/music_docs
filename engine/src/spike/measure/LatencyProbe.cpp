#include "spike/measure/LatencyProbe.h"

#include <algorithm>
#include <cmath>

namespace le::spike {

namespace {
constexpr double kPi = 3.14159265358979323846;

int64_t msToSamples(double ms, double sr) {
    return static_cast<int64_t>(std::llround(ms * sr / 1000.0));
}
} // namespace

// [main]
void LatencyProbe::prepare(double sampleRate, int /*maxBlock*/, const Config& cfg) {
    cfg_ = cfg;
    sampleRate_ = sampleRate;

    // Chirp tuyến tính f0 → f1, nhân cửa sổ Hann cho hai đầu mềm (không click, correlation ít "gợn").
    const int len = std::max<int>(64, static_cast<int>(msToSamples(cfg.chirpMs, sampleRate)));
    const double f0 = cfg.chirpLoHz;
    const double f1 = std::min(cfg.chirpHiHz, 0.45 * sampleRate);
    const double T = len / sampleRate;
    chirp_.assign(static_cast<size_t>(len), 0.0f);
    for (int k = 0; k < len; ++k) {
        const double t = k / sampleRate;
        const double phase = 2.0 * kPi * (f0 * t + 0.5 * (f1 - f0) / T * t * t);
        const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * k / (len - 1));
        chirp_[static_cast<size_t>(k)] = static_cast<float>(w * std::sin(phase));
    }

    preRoll_  = msToSamples(cfg.preRollMs, sampleRate);
    interval_ = std::max<int64_t>(msToSamples(cfg.intervalMs, sampleRate), 2 * len);
    // Cửa sổ tìm của lượt r là [emit_r, emit_r + maxLag + len), phải kết thúc trước emit_{r+1}
    // để không bắt nhầm tiếng vọng của lượt kế tiếp.
    maxLag_ = static_cast<int32_t>(interval_ - len - 1);

    recording_.assign(static_cast<size_t>(preRoll_ + LatencyResult::kRuns * interval_), 0.0f);

    pos_ = 0;
    active_ = false;
    activeGen_ = requestGen_.load(std::memory_order_relaxed);
    doneGen_.store(activeGen_, std::memory_order_relaxed);
}

// [main]
void LatencyProbe::start() noexcept {
    requestGen_.fetch_add(1, std::memory_order_acq_rel);
}

// [RT] Chỉ toán + ghi vào buffer có sẵn: không cấp phát, không lock, không log.
void LatencyProbe::processRt(const float* in, float* const* out, int numCh, int n) noexcept [[clang::nonblocking]] {
    const uint32_t req = requestGen_.load(std::memory_order_acquire);
    if (req != activeGen_) {        // có yêu cầu đo mới → bắt đầu lại từ đầu
        activeGen_ = req;
        pos_ = 0;
        active_ = !recording_.empty();
        if (!active_) doneGen_.store(req, std::memory_order_release);   // chưa prepare: báo xong luôn
    }
    if (!active_) return;

    const int64_t total = static_cast<int64_t>(recording_.size());
    const int64_t len = static_cast<int64_t>(chirp_.size());
    float* rec = recording_.data();
    const float* ch = chirp_.data();

    for (int i = 0; i < n; ++i) {
        const int64_t p = pos_ + i;
        if (p < total) rec[p] = (in != nullptr) ? in[i] : 0.0f;

        float s = 0.0f;
        if (p >= preRoll_) {
            const int64_t rel = p - preRoll_;
            const int64_t run = rel / interval_;
            const int64_t off = rel - run * interval_;
            if (run < LatencyResult::kRuns && off < len) s = cfg_.gain * ch[off];
        }
        for (int c = 0; c < numCh; ++c)
            if (out != nullptr && out[c] != nullptr) out[c][i] = s;
    }

    pos_ += n;
    if (pos_ >= total) {
        active_ = false;
        doneGen_.store(activeGen_, std::memory_order_release);  // công bố: buffer đã đầy đủ
    }
}

// [worker]
int32_t LatencyProbe::findLag(const float* ref, int refLen, const float* rec, int64_t recLen,
                              int64_t start, int32_t maxLag, float* outScore) {
    // Cross-correlation: c(lag) = Σ_k ref[k] · rec[start + lag + k].
    // Khi lag đúng bằng độ trễ thật, chirp thu được "chồng khít" lên chirp gốc nên tổng lớn nhất.
    // Lấy |c| để chịu được trường hợp loa/mic đảo pha.
    double bestAbs = -1.0;
    int32_t bestLag = 0;
    for (int32_t lag = 0; lag <= maxLag; ++lag) {
        const int64_t base = start + lag;
        if (base >= recLen) break;
        const int64_t avail = std::min<int64_t>(refLen, recLen - base);
        const float* x = rec + base;
        float acc = 0.0f;
        {
            // Cho phép compiler đổi thứ tự cộng để vector hoá (NEON), sai số float không đáng kể ở đây.
#pragma clang fp reassociate(on)
            for (int64_t k = 0; k < avail; ++k) acc += ref[k] * x[k];
        }
        const double a = std::fabs(static_cast<double>(acc));
        if (a > bestAbs) { bestAbs = a; bestLag = lag; }
    }

    if (outScore != nullptr) {
        // Normalized correlation tại lag tốt nhất: |c| / (‖ref‖·‖đoạn thu‖) ∈ [0, 1].
        // ≈1: đoạn thu giống hệt chirp (chỉ khác biên độ). ≈0: không liên quan (nhiễu, im lặng).
        const int64_t base = start + bestLag;
        const int64_t avail = std::max<int64_t>(0, std::min<int64_t>(refLen, recLen - base));
        double eRef = 0.0, eRec = 0.0;
        for (int64_t k = 0; k < avail; ++k) {
            eRef += static_cast<double>(ref[k]) * ref[k];
            eRec += static_cast<double>(rec[base + k]) * rec[base + k];
        }
        const double denom = std::sqrt(eRef * eRec);
        *outScore = denom > 1e-20 ? static_cast<float>(bestAbs / denom) : 0.0f;
    }
    return bestLag;
}

// [worker]
LatencyResult LatencyProbe::analyze() const {
    LatencyResult r;
    r.runs.fill(-1);
    r.score.fill(0.0f);
    if (!isDone() || recording_.empty()) return r;

    const int64_t total = static_cast<int64_t>(recording_.size());
    for (float v : recording_) r.inputPeak = std::max(r.inputPeak, std::fabs(v));

    std::array<int32_t, LatencyResult::kRuns> valid{};
    for (int run = 0; run < LatencyResult::kRuns; ++run) {
        float score = 0.0f;
        const int32_t lag = findLag(chirp_.data(), static_cast<int>(chirp_.size()), recording_.data(),
                                    total, emitStart(run), maxLag_, &score);
        r.score[static_cast<size_t>(run)] = score;
        if (score >= cfg_.minScore) {
            r.runs[static_cast<size_t>(run)] = lag;
            valid[static_cast<size_t>(r.validRuns++)] = lag;
        }
    }
    if (r.validRuns == 0) return r;

    std::sort(valid.begin(), valid.begin() + r.validRuns);
    const int m = r.validRuns / 2;
    r.measuredSamples = (r.validRuns % 2 == 1) ? valid[static_cast<size_t>(m)]
                                               : (valid[static_cast<size_t>(m - 1)] + valid[static_cast<size_t>(m)]) / 2;
    r.measuredMs = r.measuredSamples * 1000.0 / sampleRate_;
    r.spreadSamples = valid[static_cast<size_t>(r.validRuns - 1)] - valid[0];
    r.ok = r.validRuns >= 3;
    return r;
}

} // namespace le::spike
