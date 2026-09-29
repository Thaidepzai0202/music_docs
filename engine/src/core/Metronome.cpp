#include "core/Metronome.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace le::core {

namespace {
constexpr double kTwoPi = 6.283185307179586;

// Click = sine × envelope (attack 1 ms tuyến tính, decay mũ τ = 8 ms, fade-out 2 ms cuối để không cụt).
// Pha bắt đầu từ sample 1 (sin(2πf/sr) ≠ 0) → sample đầu tiên của click khác 0.
void synth(std::vector<float>& out, double sr, double hz, double gain) {
    const int len = std::max(1, (int) std::lround(0.030 * sr));
    const double attack = 0.001 * sr, tau = 0.008 * sr, fade = 0.002 * sr;
    out.assign((size_t) len, 0.0f);
    for (int i = 0; i < len; ++i) {
        const double env = std::min(1.0, (i + 1) / attack) * std::exp(-i / tau) * std::min(1.0, (len - i) / fade);
        out[(size_t) i] = (float) (gain * env * std::sin(kTwoPi * hz * (i + 1) / sr));
    }
}
} // namespace

void Metronome::prepare(double sampleRate) {
    synth(accent_, sampleRate, 1500.0, 1.0);
    synth(normal_, sampleRate, 1000.0, 0.6);
    clickPos_ = -1;
    click_ = nullptr;
}

void Metronome::setMode(int mode, float volume) noexcept [[clang::nonblocking]] {
    mode_ = (mode >= Off && mode <= RecordOnly) ? mode : Off;
    volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void Metronome::render(const Transport& tr, const Segment& seg, float* L, float* R) noexcept [[clang::nonblocking]] {
    const int len = (int) accent_.size();
    if (len == 0) return;

    // Click tiếp theo trong segment: beat nguyên nhỏ nhất có sample ≥ đầu segment.
    std::int64_t nextClick = std::numeric_limits<std::int64_t>::max();
    std::int64_t k = 0;
    if (tr.playing() && audible()) {
        k = (std::int64_t) std::floor(seg.startBeat);
        while (tr.sampleAtBeat((double) k) < seg.startSample) ++k;
        nextClick = tr.sampleAtBeat((double) k);
    }

    const float g = volume_;
    for (int i = 0; i < seg.numFrames; ++i) {
        const std::int64_t s = seg.startSample + i;
        if (s == nextClick) {
            click_ = (k % tr.beatsPerBar() == 0) ? accent_.data() : normal_.data();
            clickPos_ = 0;
            ++k;
            nextClick = tr.sampleAtBeat((double) k);
        }
        if (clickPos_ >= 0) {
            const float v = g * click_[clickPos_];
            L[seg.startFrame + i] += v;
            R[seg.startFrame + i] += v;
            if (++clickPos_ >= len) clickPos_ = -1;
        }
    }
}

} // namespace le::core
