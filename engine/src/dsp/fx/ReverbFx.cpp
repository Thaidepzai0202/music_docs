#include "dsp/fx/ReverbFx.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>

namespace le::dsp {

ReverbFx::ReverbFx() : reverb_(std::make_unique<juce::Reverb>()) {
    for (const ParamInfo& p : paramInfo(FxType::Reverb)) params_[static_cast<size_t>(p.id)] = p.defaultValue;
}

ReverbFx::~ReverbFx() = default;

// [main] juce::Reverb cấp phát buffer comb/allpass tại đây.
void ReverbFx::prepare(double sampleRate, int maxBlock) {
    maxBlock_ = std::max(1, maxBlock);
    for (auto& d : dry_) d.assign(static_cast<size_t>(maxBlock_), 0.0f);
    reverb_->setSampleRate(sampleRate > 0.0 ? sampleRate : 48000.0);
    mix_.prepare(sampleRate);
    for (int id = 0; id < 4; ++id) setParam(id, params_[static_cast<size_t>(id)]);
    reset();
}

// Gọi vào juce::Reverb: không cấp phát sau setSampleRate() nhưng JUCE không gắn nonblocking.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfunction-effects"

void ReverbFx::applyParameters() noexcept [[clang::nonblocking]] {
    juce::Reverb::Parameters p;
    p.roomSize = params_[0];
    p.damping = params_[1];
    p.width = params_[2];
    p.wetLevel = 1.0f / 3.0f;   // juce::Reverb nhân wet với 3 → hệ số wet thực = 1
    p.dryLevel = 0.0f;          // tự trộn dry ở ngoài
    p.freezeMode = 0.0f;
    reverb_->setParameters(p);
    dirty_ = false;
}

void ReverbFx::reset() noexcept [[clang::nonblocking]] {
    applyParameters();
    reverb_->reset();
    mix_.snap();
}

// [RT]
void ReverbFx::process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] {
    const int n = std::min(ctx.numFrames, maxBlock_);
    if (n <= 0) return;
    if (dirty_) applyParameters();          // juce::Reverb tự trượt damping/feedback 10 ms
    const bool stereo = numCh >= 2;
    std::copy(io[0], io[0] + n, dry_[0].data());
    if (stereo) {
        std::copy(io[1], io[1] + n, dry_[1].data());
        reverb_->processStereo(io[0], io[1], n);
    } else {
        reverb_->processMono(io[0], n);
    }
    for (int s = 0; s < n; ++s) {
        const float m = mix_.next();
        io[0][s] = dry_[0][static_cast<size_t>(s)] * (1.0f - m) + io[0][s] * m;
        if (stereo) io[1][s] = dry_[1][static_cast<size_t>(s)] * (1.0f - m) + io[1][s] * m;
    }
    guardOutput(io, numCh, n);   // NaN / Inf → 0 + reset (Processor.h)
}

#pragma clang diagnostic pop

void ReverbFx::setParam(int id, float value) noexcept [[clang::nonblocking]] {
    if (id < 0 || id > 3) return;
    const float v = clampParam(FxType::Reverb, id, value);
    params_[static_cast<size_t>(id)] = v;
    if (id == 3) mix_.setTarget(v);
    else dirty_ = true;                      // áp vào juce::Reverb ở đầu block kế tiếp
}

float ReverbFx::getParam(int id) const noexcept [[clang::nonblocking]] {
    return (id >= 0 && id <= 3) ? params_[static_cast<size_t>(id)] : 0.0f;
}

} // namespace le::dsp
