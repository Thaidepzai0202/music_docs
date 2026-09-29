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

const std::vector<float> LatencyProbe::kEmpty;

// [main] Dựng Tables mới (không đụng bản worker có thể đang đọc), rồi thay con trỏ.
void LatencyProbe::prepare(double sampleRate, int /*maxBlock*/, const Config& cfg) {
    auto t = std::make_shared<Tables>();
    t->cfg = cfg;
    t->sampleRate = sampleRate;

    // Chirp tuyến tính f0 → f1, nhân cửa sổ Hann cho hai đầu mềm (không click, correlation ít "gợn").
    const int len = std::max<int>(64, static_cast<int>(msToSamples(cfg.chirpMs, sampleRate)));
    const double f0 = cfg.chirpLoHz;
    const double f1 = std::min(cfg.chirpHiHz, 0.45 * sampleRate);
    const double T = len / sampleRate;
    t->chirp.assign(static_cast<size_t>(len), 0.0f);
    for (int k = 0; k < len; ++k) {
        const double tt = k / sampleRate;
        const double phase = 2.0 * kPi * (f0 * tt + 0.5 * (f1 - f0) / T * tt * tt);
        const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * k / (len - 1));
        t->chirp[static_cast<size_t>(k)] = static_cast<float>(w * std::sin(phase));
    }

    t->preRoll  = msToSamples(cfg.preRollMs, sampleRate);
    t->interval = std::max<int64_t>(msToSamples(cfg.intervalMs, sampleRate), 2 * len);
    // Cửa sổ tìm của lượt r là [emit_r, emit_r + maxLag + len), phải kết thúc trước emit_{r+1}
    // để không bắt nhầm tiếng vọng của lượt kế tiếp.
    t->maxLag = static_cast<int32_t>(t->interval - len - 1);
    t->recording.assign(static_cast<size_t>(t->preRoll + LatencyResult::kRuns * t->interval), 0.0f);
    t->epoch = epoch_.load(std::memory_order_relaxed) + 1;

    {
        const std::lock_guard<std::mutex> lock(mutex_);
        tables_ = t;    // bản cũ (nếu worker còn giữ) tự huỷ khi worker thả shared_ptr
    }
    rt_ = t.get();
    epoch_.store(t->epoch, std::memory_order_release);

    // Lượt đang chờ (requestGen_ ≠ doneGen_) → RT bắt đầu lại lượt đó với bảng mới; không chờ gì → RT rảnh.
    pos_ = 0;
    active_ = false;
    activeGen_ = doneGen_.load(std::memory_order_relaxed);
}

// [main / worker]
void LatencyProbe::start() noexcept {
    requestGen_.fetch_add(1, std::memory_order_acq_rel);
}

std::shared_ptr<const LatencyProbe::Tables> LatencyProbe::current() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return tables_;
}

double LatencyProbe::sampleRate() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return tables_ != nullptr ? tables_->sampleRate : 0.0;
}

int64_t LatencyProbe::totalSamples() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return tables_ != nullptr ? static_cast<int64_t>(tables_->recording.size()) : 0;
}

int64_t LatencyProbe::emitStart(int run) const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return tables_ != nullptr ? tables_->preRoll + static_cast<int64_t>(run) * tables_->interval : 0;
}

const std::vector<float>& LatencyProbe::chirp() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return tables_ != nullptr ? tables_->chirp : kEmpty;
}

// [RT] Chỉ toán + ghi vào buffer có sẵn: không cấp phát, không lock, không log.
void LatencyProbe::processRt(const float* in, float* const* out, int numCh, int n) noexcept [[clang::nonblocking]] {
    const uint32_t req = requestGen_.load(std::memory_order_acquire);
    Tables* t = rt_;
    if (req != activeGen_) {        // có yêu cầu đo mới → bắt đầu lại từ đầu
        activeGen_ = req;
        pos_ = 0;
        active_ = t != nullptr && !t->recording.empty();
        if (!active_) doneGen_.store(req, std::memory_order_release);   // chưa prepare: báo xong luôn
    }
    if (!active_) return;

    const int64_t total = static_cast<int64_t>(t->recording.size());
    const int64_t len = static_cast<int64_t>(t->chirp.size());
    float* rec = t->recording.data();
    const float* ch = t->chirp.data();
    const float gain = t->cfg.gain;

    for (int i = 0; i < n; ++i) {
        const int64_t p = pos_ + i;
        if (p < total) rec[p] = (in != nullptr) ? in[i] : 0.0f;

        float s = 0.0f;
        if (p >= t->preRoll) {
            const int64_t rel = p - t->preRoll;
            const int64_t run = rel / t->interval;
            const int64_t off = rel - run * t->interval;
            if (run < LatencyResult::kRuns && off < len) s = gain * ch[off];
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
    const std::shared_ptr<const Tables> t = current();   // giữ bảng sống suốt lúc phân tích
    if (t == nullptr) return r;
    r.sampleRate = t->sampleRate;
    r.epoch = t->epoch;
    if (!isDone() || t->recording.empty()) return r;

    const std::vector<float>& rec = t->recording;
    const int64_t total = static_cast<int64_t>(rec.size());
    for (float v : rec) r.inputPeak = std::max(r.inputPeak, std::fabs(v));
    const int64_t pre = std::min<int64_t>(t->preRoll, total);
    if (pre > 0) {
        double e = 0.0;
        for (int64_t i = 0; i < pre; ++i) e += static_cast<double>(rec[static_cast<size_t>(i)]) * rec[static_cast<size_t>(i)];
        r.noiseRms = static_cast<float>(std::sqrt(e / static_cast<double>(pre)));
    }

    std::array<int32_t, LatencyResult::kRuns> valid{};
    for (int run = 0; run < LatencyResult::kRuns; ++run) {
        float score = 0.0f;
        const int32_t lag = findLag(t->chirp.data(), static_cast<int>(t->chirp.size()), rec.data(), total,
                                    t->preRoll + static_cast<int64_t>(run) * t->interval, t->maxLag, &score);
        r.score[static_cast<size_t>(run)] = score;
        if (score >= t->cfg.minScore) {
            r.runs[static_cast<size_t>(run)] = lag;
            valid[static_cast<size_t>(r.validRuns++)] = lag;
        }
    }
    if (r.validRuns == 0) return r;

    std::sort(valid.begin(), valid.begin() + r.validRuns);
    const int m = r.validRuns / 2;
    r.measuredSamples = (r.validRuns % 2 == 1) ? valid[static_cast<size_t>(m)]
                                               : (valid[static_cast<size_t>(m - 1)] + valid[static_cast<size_t>(m)]) / 2;
    r.measuredMs = r.measuredSamples * 1000.0 / t->sampleRate;
    r.spreadSamples = valid[static_cast<size_t>(r.validRuns - 1)] - valid[0];
    r.ok = r.validRuns >= 3;
    return r;
}

} // namespace le::spike
