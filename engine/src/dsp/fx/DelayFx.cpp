#include "dsp/fx/DelayFx.h"

#include "dsp/Interpolators.h"

#include <algorithm>
#include <cmath>

namespace le::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kMinDelay = 4.0;          // ≥ 4 sample: Hermite không đọc vào vùng chưa ghi
constexpr double kFeedbackLpHz = 6000.0;
}

DelayFx::DelayFx() {
    for (const ParamInfo& p : paramInfo(FxType::Delay)) params_[static_cast<size_t>(p.id)] = p.defaultValue;
}

// [main] Cấp phát buffer 4 s cho mỗi kênh.
void DelayFx::prepare(double sampleRate, int /*maxBlock*/) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    len_ = static_cast<int64_t>(std::ceil(kMaxSeconds * sampleRate_)) + 8;
    for (auto& b : buf_) b.assign(static_cast<size_t>(len_), 0.0f);
    time_.prepare(sampleRate_, 0.100);
    feedback_.prepare(sampleRate_);
    mix_.prepare(sampleRate_);
    lpCoef_ = static_cast<float>(1.0 - std::exp(-2.0 * kPi * kFeedbackLpHz / sampleRate_));
    for (int id = 0; id < 4; ++id) setParam(id, params_[static_cast<size_t>(id)]);
    reset();
}

// Xoá buffer (~0.8 MB mỗi kênh @48k): 68 chỉ gọi lúc tạo trên main / project.open.
void DelayFx::reset() noexcept [[clang::nonblocking]] {
    for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.0f);
    write_ = 0;
    lpState_.fill(0.0f);
    feedback_.snap();
    mix_.snap();
    needSnapTime_ = true;
}

void DelayFx::setParam(int id, float value) noexcept [[clang::nonblocking]] {
    if (id < 0 || id > 3) return;
    const float v = clampParam(FxType::Delay, id, value);
    params_[static_cast<size_t>(id)] = v;
    if (id == 1) feedback_.setTarget(v);
    else if (id == 2) mix_.setTarget(v);
    // id 0 (nhịp) và 3 (ping-pong) đọc trực tiếp trong process()
}

float DelayFx::getParam(int id) const noexcept [[clang::nonblocking]] {
    return (id >= 0 && id <= 3) ? params_[static_cast<size_t>(id)] : 0.0f;
}

double DelayFx::delaySamplesFor(double bpm) const noexcept [[clang::nonblocking]] {
    const auto idx = static_cast<size_t>(std::clamp(static_cast<int>(params_[0] + 0.5f), 0, static_cast<int>(kNoteBeats.size()) - 1));
    const double d = kNoteBeats[idx] * 60.0 / std::max(1.0, bpm) * sampleRate_;
    // Kẹp đúng 4 s (buffer có thêm vài sample lề cho các điểm Hermite, không dùng để kéo dài delay)
    return std::clamp(d, kMinDelay, std::min(kMaxSeconds * sampleRate_, static_cast<double>(len_ - 4)));
}

// Đọc buffer vòng tại (write_ − delay) bằng Hermite; chỉ số lấy vòng quanh len_.
float DelayFx::readTap(const float* buf, double delay) const noexcept [[clang::nonblocking]] {
    double pos = static_cast<double>(write_) - delay;
    if (pos < 0.0) pos += static_cast<double>(len_);
    const int64_t i = floorToInt(pos);
    const auto t = static_cast<float>(pos - static_cast<double>(i));
    auto at = [&](int64_t k) noexcept [[clang::nonblocking]] {   // i ∈ [0, len), lệch −1..+2 → vòng tối đa 1 lần
        if (k < 0) k += len_;
        else if (k >= len_) k -= len_;
        return buf[k];
    };
    return hermite4(t, at(i - 1), at(i), at(i + 1), at(i + 2));
}

// [RT]
void DelayFx::process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] {
    if (len_ == 0) {   // chưa prepare: không xử lý, nhưng vẫn không cho NaN đi qua
        guardOutput(io, numCh, ctx.numFrames);
        return;
    }
    const double target = delaySamplesFor(ctx.bpm);
    if (needSnapTime_) {
        time_.setTarget(static_cast<float>(target));
        time_.snap();
        lastTarget_ = target;
        needSnapTime_ = false;
    } else if (std::fabs(target - lastTarget_) > 0.5) {   // BPM hoặc nhịp đổi → trượt 100 ms
        time_.setTarget(static_cast<float>(target));
        lastTarget_ = target;
    }

    const bool stereo = numCh >= 2;
    const bool pingPong = stereo && params_[3] >= 0.5f;
    float* L = io[0];
    float* R = stereo ? io[1] : nullptr;
    float* bl = buf_[0].data();
    float* br = buf_[1].data();

    for (int s = 0; s < ctx.numFrames; ++s) {
        const double d = time_.next();
        const float fb = feedback_.next();
        const float mix = mix_.next();
        const float xl = L[s];
        const float xr = stereo ? R[s] : 0.0f;
        const float dl = readTap(bl, d);
        const float dr = stereo ? readTap(br, d) : 0.0f;

        // Lọc thông thấp 1 cực trong vòng feedback
        lpState_[0] += lpCoef_ * (dl - lpState_[0]);
        lpState_[1] += lpCoef_ * (dr - lpState_[1]);
        if (pingPong) {                   // vào trái, vọng chéo trái ↔ phải
            bl[write_] = 0.5f * (xl + xr) + fb * lpState_[1];
            br[write_] = fb * lpState_[0];
        } else {
            bl[write_] = xl + fb * lpState_[0];
            if (stereo) br[write_] = xr + fb * lpState_[1];
        }
        L[s] = xl * (1.0f - mix) + dl * mix;
        if (stereo) R[s] = xr * (1.0f - mix) + dr * mix;
        if (++write_ >= len_) write_ = 0;
    }
    guardOutput(io, numCh, ctx.numFrames);   // NaN / Inf → 0 + reset (Processor.h)
}

} // namespace le::dsp
