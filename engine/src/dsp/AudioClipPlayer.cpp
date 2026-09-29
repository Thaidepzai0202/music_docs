#include "dsp/AudioClipPlayer.h"

#include "dsp/Interpolators.h"
#include "dsp/Sanitize.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace le::dsp {

namespace {
constexpr double kHalfPi = 1.57079632679489661923;

enum class LoopJoin { None, Crossfade, Dip };

// Đọc tại vị trí nguồn pos. direct = tỉ lệ 1:1 → lấy sample nguyên gần nhất (không nội suy, đúng từng bit).
inline float readAt(const float* ch, int64_t numFrames, double pos, bool direct) noexcept [[clang::nonblocking]] {
    if (direct) {
        const int64_t i = floorToInt(pos + 0.5);
        return (i >= 0 && i < numFrames) ? ch[i] : 0.0f;
    }
    if (pos < -1.0 || pos >= static_cast<double>(numFrames) + 1.0) return 0.0f;   // ngoài vùng đệm → im lặng
    return readHermite(ch, pos);
}

// Hai đầu vòng "khớp" = bước nhảy qua điểm loop không lớn hơn độ dốc bình thường của tín hiệu ở đó.
inline bool endsMatch(const AudioData& d, int64_t loopFrames) noexcept [[clang::nonblocking]] {
    if (loopFrames < 3 || loopFrames > d.numFrames()) return true;
    for (int c = 0; c < d.numChannels(); ++c) {
        const float* x = d.channel(c);
        const float jump = std::fabs(x[loopFrames - 1] - x[0]);
        const float slope = std::max(std::fabs(x[loopFrames - 1] - x[loopFrames - 2]), std::fabs(x[1] - x[0]));
        if (jump > 2.0f * slope + 1.0e-3f) return false;
    }
    return true;
}

// Một nguồn đọc (Re-Pitch hoặc Stretched) cho 1 segment: vị trí = beatInClip · srcSpb.
struct Reader {
    const float* c0 = nullptr;
    const float* c1 = nullptr;
    bool     stereo = false, direct = false;
    int64_t  nf = 0;
    double   srcSpb = 0.0, srcLen = 0.0;
    LoopJoin join = LoopJoin::None;

    void init(const AudioData& d, double sourceSamplesPerBeat, double lenBeats, double spb, double xfBeats) noexcept
        [[clang::nonblocking]] {
        c0 = d.channel(0);
        c1 = d.channelOrMono(1);
        stereo = d.isStereo();
        nf = d.numFrames();
        srcSpb = sourceSamplesPerBeat;
        srcLen = lenBeats * srcSpb;
        direct = std::fabs(srcSpb / spb - 1.0) < 1e-9;
        join = static_cast<double>(nf) >= srcLen + xfBeats * srcSpb + 3.0 ? LoopJoin::Crossfade
             : endsMatch(d, floorToInt(srcLen + 1e-6))                  ? LoopJoin::None
                                                                          : LoopJoin::Dip;
    }

    // Sample tại beatInClip (đã xử lý nối điểm loop). afterFirstLoop: đã qua ít nhất 1 vòng (mới có "đuôi").
    void read(double bic, double len, bool afterFirstLoop, double xfBeats, double dipBeats, float& a, float& b) const
        noexcept [[clang::nonblocking]] {
        const double pos = bic * srcSpb;
        a = readAt(c0, nf, pos, direct);
        b = stereo ? readAt(c1, nf, pos, direct) : a;
        if (join == LoopJoin::Crossfade && afterFirstLoop && bic < xfBeats) {
            // Đầu vòng: trộn dần từ "đuôi" (phần nối tiếp sau điểm loop) sang đầu clip
            const auto t = static_cast<float>(bic / xfBeats);
            const float ta = readAt(c0, nf, pos + srcLen, direct);
            const float tb = stereo ? readAt(c1, nf, pos + srcLen, direct) : ta;
            a = a * t + ta * (1.0f - t);
            b = b * t + tb * (1.0f - t);
        } else if (join == LoopJoin::Dip) {
            float g = 1.0f;
            if (afterFirstLoop && bic < dipBeats) g = static_cast<float>(bic / dipBeats);
            if (len - bic < dipBeats) g = std::min(g, static_cast<float>((len - bic) / dipBeats));
            a *= g;
            b *= g;
        }
    }
};

// Giống Transport::sampleAtBeat: sample đầu tiên có beat ≥ `beat`, tính từ đầu segment.
inline int64_t offsetOf(double beat, double segStartBeat, double spb) noexcept [[clang::nonblocking]] {
    return static_cast<int64_t>(std::ceil((beat - segStartBeat) * spb - 1e-6));
}

int32_t samplesFor(float seconds, double sr) noexcept [[clang::nonblocking]] {
    return std::max<int32_t>(1, static_cast<int32_t>(static_cast<double>(seconds) * sr + 0.5));
}
} // namespace

// [main]
void AudioClipPlayer::prepare(double sampleRate, int /*maxBlock*/) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    cur_ = Head{};
    old_ = Head{};
}

void AudioClipPlayer::rampTo(Head& h, float target, float seconds) noexcept [[clang::nonblocking]] {
    // Đếm đúng N sample (làm tròn) thay vì cộng dồn float tới khi "chạm" đích → độ dài fade chính xác.
    h.target = target;
    h.rampLeft = samplesFor(seconds, sampleRate_);
    h.step = (target - h.level) / static_cast<float>(h.rampLeft);
}

// Số sample NGUỒN cho 1 beat: Re-Pitch theo originalBpm, hoặc đúng tốc độ gốc nếu không biết BPM.
double AudioClipPlayer::srcSamplesPerBeat(const Head& h, double spb, double engineSr) noexcept [[clang::nonblocking]] {
    const double fileSr = h.data->sampleRate();
    return h.originalBpm > 0.0 ? 60.0 * fileSr / h.originalBpm : spb * fileSr / engineSr;
}

double AudioClipPlayer::effectiveLength(const Head& h, double spb, double engineSr) noexcept [[clang::nonblocking]] {
    if (h.lengthBeats > 0.0) return h.lengthBeats;
    const double sspb = srcSamplesPerBeat(h, spb, engineSr);
    return sspb > 0.0 ? static_cast<double>(h.data->numFrames()) / sspb : 0.0;
}

// [RT]
void AudioClipPlayer::setClip(const AudioData* data, double lengthBeats, double originalBpm, float gain,
                              uint32_t generation) noexcept [[clang::nonblocking]] {
    if (!std::isfinite(gain)) gain = 0.0f;                     // NaN không được vào ramp gain
    if (!std::isfinite(lengthBeats)) lengthBeats = 0.0;        // 0 = tính từ số frame
    if (!std::isfinite(originalBpm)) originalBpm = 0.0;        // 0 = tốc độ gốc
    if (data == cur_.data) {   // cùng buffer (snapshot mới giữ lại clip cũ) → chỉ cập nhật
        cur_.lengthBeats = lengthBeats;
        cur_.originalBpm = originalBpm;
        cur_.generation = generation;
        if (std::fabs(gain - cur_.gainTarget) > 1e-7f) {   // clip.setParams đổi gain → trượt 20 ms
            cur_.gainTarget = gain;
            cur_.gainLeft = samplesFor(kGainRampSec, sampleRate_);
            cur_.gainStep = (gain - cur_.gain) / static_cast<float>(cur_.gainLeft);
        }
        return;
    }
    const bool wasPlaying = cur_.running && !cur_.stopping;
    if (cur_.running) {        // bản cũ fade-out song song (nếu old_ còn đang fade thì bị thay — hiếm)
        old_ = cur_;
        old_.stopping = true;
        old_.freeRun = true;
        rampTo(old_, 0.0f, kSwapFadeSec);
    }
    const double launch = cur_.launchBeat;
    cur_ = Head{};
    cur_.data = data;
    cur_.lengthBeats = lengthBeats;
    cur_.originalBpm = originalBpm;
    cur_.gain = cur_.gainTarget = gain;   // head mới đã có fade-in / crossfade → không cần trượt gain
    cur_.generation = generation;
    cur_.launchBeat = launch;
    if (wasPlaying && data != nullptr) {   // phát tiếp CÙNG PHA, fade-in 5 ms
        cur_.running = true;
        rampTo(cur_, 1.0f, kSwapFadeSec);
    }
}

// [RT]
void AudioClipPlayer::setStretched(const AudioData* stretched, double stretchedBpm, uint32_t generation) noexcept
    [[clang::nonblocking]] {
    cur_.stretched = stretched;
    cur_.stretchedBpm = stretchedBpm;
    cur_.stretchedGen = generation;
    if (stretched != nullptr && stretched == cur_.activeSt) cur_.activeStGen = generation;   // snapshot mới giữ lại bản cũ
}

void AudioClipPlayer::setBeatsPerBar(int beatsPerBar) noexcept [[clang::nonblocking]] {
    beatsPerBar_ = std::clamp(beatsPerBar, 1, 32);
}

// [RT]
void AudioClipPlayer::start(double launchBeat) noexcept [[clang::nonblocking]] {
    if (cur_.data == nullptr || !std::isfinite(launchBeat)) return;
    if (cur_.running) {        // đang kêu mà launch lại → bản đang kêu fade-out, bản mới từ đầu clip
        old_ = cur_;
        old_.stopping = true;
        old_.freeRun = true;
        rampTo(old_, 0.0f, kFadeOutSec);
    }
    cur_.launchBeat = launchBeat;
    cur_.running = true;
    cur_.stopping = false;
    cur_.freeRun = false;
    cur_.nextRel = 0.0;
    cur_.level = 0.0f;
    rampTo(cur_, 1.0f, kFadeInSec);
}

// [RT]
void AudioClipPlayer::stop() noexcept [[clang::nonblocking]] {
    if (!cur_.running || cur_.stopping) return;
    cur_.stopping = true;
    cur_.freeRun = true;       // từ giờ đi tiếp theo sample, bỏ qua beat của transport
    rampTo(cur_, 0.0f, kFadeOutSec);
}

// [RT]
void AudioClipPlayer::renderHead(Head& h, float* L, float* R, int n, double segStartBeat, double spb) noexcept
    [[clang::nonblocking]] {
    if (!h.running || h.data == nullptr || !(spb > 0.0)) return;
    if (!std::isfinite(segStartBeat) || !std::isfinite(spb) || !std::isfinite(h.launchBeat) || !std::isfinite(h.nextRel) ||
        !std::isfinite(h.gain) || !std::isfinite(h.level)) {   // state hỏng → im lặng hẳn (không đọc vị trí NaN)
        h.running = false;
        ++nonFinite_;
        return;
    }
    const double srcSpb = srcSamplesPerBeat(h, spb, sampleRate_);
    const double len = effectiveLength(h, spb, sampleRate_);
    if (len <= 0.0 || srcSpb <= 0.0) return;

    const double dBeat = 1.0 / spb;
    const double xfBeats = static_cast<double>(kLoopXfadeSec) * sampleRate_ / spb;
    const double dipBeats = static_cast<double>(kLoopDipSec) * sampleRate_ / spb;
    Reader rp;
    rp.init(*h.data, srcSpb, len, spb, xfBeats);

    // ── Warp hybrid: quyết định nguồn ở đầu segment (BPM không đổi trong 1 segment) ──
    const double bpmNow = 60.0 * sampleRate_ / spb;
    const bool offeredOk = h.stretched != nullptr && h.stretchedBpm > 0.0 && std::fabs(bpmNow - h.stretchedBpm) <= kBpmMatch;
    int64_t switchAt = std::numeric_limits<int64_t>::max();
    if (!h.freeRun) {
        if (h.src == Source::RePitch) {
            if (offeredOk) {
                if (h.pendingBeat < 0.0) {   // ranh giới bar kế tiếp của transport (đang đứng đúng ranh giới thì dùng luôn)
                    const double bpb = static_cast<double>(beatsPerBar_);
                    h.pendingBeat = std::ceil((segStartBeat - 1e-9) / bpb) * bpb;
                }
                switchAt = std::max<int64_t>(0, offsetOf(h.pendingBeat, segStartBeat, spb));
            } else {
                h.pendingBeat = -1.0;        // BPM đổi trước khi tới ranh giới → huỷ
            }
        } else if (h.src == Source::Stretched && !(offeredOk && h.stretched == h.activeSt)) {
            h.src = Source::ToRePitch;       // BPM lại đổi / bản stretched bị thay → về Re-Pitch NGAY
            h.srcLen = h.srcLeft = samplesFor(kToRePitchSec, sampleRate_);
        }
    }
    Reader st;
    if (h.activeSt != nullptr && h.activeStBpm > 0.0)
        st.init(*h.activeSt, 60.0 * h.activeSt->sampleRate() / h.activeStBpm, len, spb, xfBeats);

    // beat kể từ lúc launch: theo transport khi đang phát, theo vị trí đã phát khi đang fade-out
    double rel = h.freeRun ? h.nextRel : segStartBeat - h.launchBeat;
    double bic = rel > 0.0 ? std::fmod(rel, len) : 0.0;        // beat trong clip (tính lại mỗi segment)

    for (int s = 0; s < n; ++s) {
        if (s == switchAt && h.src == Source::RePitch) {         // tới ranh giới bar: bắt đầu crossfade sang Stretched
            h.activeSt = h.stretched;
            h.activeStBpm = h.stretchedBpm;
            h.activeStGen = h.stretchedGen;
            st.init(*h.activeSt, 60.0 * h.activeSt->sampleRate() / h.activeStBpm, len, spb, xfBeats);
            h.src = Source::ToStretched;
            h.srcLen = h.srcLeft = samplesFor(kToStretchedSec, sampleRate_);
            h.pendingBeat = -1.0;
        }

        float a = 0.0f, b = 0.0f;
        if (rel >= -1e-9) {
            const bool afterFirstLoop = rel >= len - 1e-9;
            if (h.src == Source::RePitch) {
                rp.read(bic, len, afterFirstLoop, xfBeats, dipBeats, a, b);
            } else if (h.src == Source::Stretched) {
                st.read(bic, len, afterFirstLoop, xfBeats, dipBeats, a, b);
            } else {                                              // crossfade equal-power giữa 2 nguồn
                float ra, rb, sa, sb;
                rp.read(bic, len, afterFirstLoop, xfBeats, dipBeats, ra, rb);
                st.read(bic, len, afterFirstLoop, xfBeats, dipBeats, sa, sb);
                const double t = 1.0 - static_cast<double>(h.srcLeft - 1) / static_cast<double>(h.srcLen);
                const double tSt = h.src == Source::ToStretched ? t : 1.0 - t;   // phần của Stretched
                const auto gSt = static_cast<float>(std::sin(kHalfPi * tSt));
                const auto gRp = static_cast<float>(std::cos(kHalfPi * tSt));
                a = ra * gRp + sa * gSt;
                b = rb * gRp + sb * gSt;
            }
        }
        if (h.src == Source::ToStretched || h.src == Source::ToRePitch) {
            if (--h.srcLeft <= 0) {
                if (h.src == Source::ToStretched) {
                    h.src = Source::Stretched;
                } else {
                    h.src = Source::RePitch;
                    h.activeSt = nullptr;                          // không còn đọc bản stretched cũ
                }
            }
        }

        if (h.rampLeft > 0) {                                    // ramp fade-in / fade-out
            h.level += h.step;
            if (--h.rampLeft == 0) h.level = h.target;
        }
        if (h.gainLeft > 0) {                                    // gain clip trượt 20 ms
            h.gain += h.gainStep;
            if (--h.gainLeft == 0) h.gain = h.gainTarget;
        }
        const float g = h.gain * h.level;
        if (R != nullptr) {
            L[s] += a * g;
            R[s] += b * g;
        } else {
            L[s] += 0.5f * (a + b) * g;
        }
        if (h.stopping && h.rampLeft == 0 && h.level <= 0.0f) {  // fade-out xong
            h.running = false;
            return;
        }

        rel += dBeat;
        bic += dBeat;
        if (bic >= len) bic -= len;
    }
    h.nextRel = rel;
}

// [RT]
void AudioClipPlayer::render(float* const* out, int numCh, int start, int n, double segStartBeat,
                             double samplesPerBeat) noexcept [[clang::nonblocking]] {
    if (out == nullptr || numCh <= 0 || n <= 0) return;
    float* L = out[0] + start;
    float* R = numCh >= 2 ? out[1] + start : nullptr;
    const bool any = old_.running || cur_.running;
    renderHead(old_, L, R, n, segStartBeat, samplesPerBeat);
    renderHead(cur_, L, R, n, segStartBeat, samplesPerBeat);
    if (any && sanitizeIfNeeded(L, R, n)) ++nonFinite_;   // dữ liệu clip có NaN / Inf (file hỏng, overdub mic rác) → 0
}

bool AudioClipPlayer::headUses(const Head& h, uint32_t generation) noexcept [[clang::nonblocking]] {
    return (h.data != nullptr && h.generation == generation) ||
           (h.stretched != nullptr && h.stretchedGen == generation) ||
           (h.activeSt != nullptr && h.activeStGen == generation);
}

// [RT] Head hiện tại giữ con trỏ cả khi im lặng (start() sau đó sẽ đọc); head cũ chỉ khi còn đang fade.
bool AudioClipPlayer::usesGeneration(uint32_t generation) const noexcept [[clang::nonblocking]] {
    return headUses(cur_, generation) || (old_.running && headUses(old_, generation));
}

// [RT]
double AudioClipPlayer::nextLoopBeat(double fromBeat) const noexcept [[clang::nonblocking]] {
    const double len = cur_.lengthBeats;
    if (!cur_.running || len <= 0.0) return std::numeric_limits<double>::infinity();
    if (fromBeat < cur_.launchBeat) return cur_.launchBeat;
    const double k = std::floor((fromBeat - cur_.launchBeat) / len + 1e-9) + 1.0;
    return cur_.launchBeat + k * len;
}

// [RT]
float AudioClipPlayer::progress(double beat) const noexcept [[clang::nonblocking]] {
    const double len = cur_.lengthBeats;
    if (!isPlaying() || len <= 0.0) return 0.0f;
    const double rel = beat - cur_.launchBeat;
    return rel <= 0.0 ? 0.0f : static_cast<float>(std::fmod(rel, len) / len);
}

} // namespace le::dsp
