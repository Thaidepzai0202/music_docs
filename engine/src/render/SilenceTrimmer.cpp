#include "render/SilenceTrimmer.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace le::render {

namespace {
bool loud(const dsp::AudioData& d, int64_t i, float thr) noexcept {
    for (int c = 0; c < d.numChannels(); ++c)
        if (std::fabs(d.channel(c)[i]) >= thr) return true;
    return false;
}
int64_t msToFrames(double ms, double sr) noexcept {
    return std::max<int64_t>(0, static_cast<int64_t>(std::llround(ms * sr / 1000.0)));
}
} // namespace

TrimRange findTrimRange(const dsp::AudioData& d, const TrimConfig& cfg) noexcept {
    TrimRange r;
    const int64_t n = d.numFrames();
    const float thr = std::pow(10.0f, cfg.thresholdDb / 20.0f);
    int64_t first = -1, last = -1;
    for (int64_t i = 0; i < n; ++i)
        if (loud(d, i, thr)) { first = i; break; }
    if (first < 0) return r;                 // im lặng hoàn toàn
    for (int64_t i = n - 1; i >= first; --i)
        if (loud(d, i, thr)) { last = i; break; }

    r.silent = false;
    r.start = cfg.trimStart ? std::max<int64_t>(0, first - msToFrames(cfg.preRollMs, d.sampleRate())) : 0;
    r.end = cfg.trimEnd ? std::min<int64_t>(n, last + 1 + msToFrames(cfg.postRollMs, d.sampleRate())) : n;
    return r;
}

// [worker]
dsp::AudioDataPtr trimSilence(const dsp::AudioData& d, const TrimConfig& cfg, TrimRange* rangeOut) {
    const TrimRange r = findTrimRange(d, cfg);
    if (rangeOut != nullptr) *rangeOut = r;
    if (r.silent) return nullptr;

    const int64_t len = r.end - r.start;
    auto out = std::make_shared<dsp::AudioData>(d.numChannels(), len, d.sampleRate());
    const float thr = std::pow(10.0f, cfg.thresholdDb / 20.0f);
    // Độ dài fade = đúng phần pre-roll / post-roll thực có (dưới ngưỡng), không lấn vào phần có tiếng
    int64_t fadeIn = 0, fadeOut = 0;
    if (cfg.fades) {
        while (fadeIn < len && !loud(d, r.start + fadeIn, thr)) ++fadeIn;
        while (fadeOut < len - fadeIn && !loud(d, r.end - 1 - fadeOut, thr)) ++fadeOut;
        if (!cfg.trimStart) fadeIn = 0;      // không cắt đầu → giữ nguyên đầu
        if (!cfg.trimEnd) fadeOut = 0;
    }
    for (int c = 0; c < d.numChannels(); ++c) {
        const float* src = d.channel(c) + r.start;
        float* dst = out->writePointer(c);
        std::copy(src, src + len, dst);
        for (int64_t i = 0; i < fadeIn; ++i) dst[i] *= static_cast<float>(i) / static_cast<float>(fadeIn);
        for (int64_t i = 0; i < fadeOut; ++i) dst[len - 1 - i] *= static_cast<float>(i) / static_cast<float>(fadeOut);
    }
    return out;
}

} // namespace le::render
