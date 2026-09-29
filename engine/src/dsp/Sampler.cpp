#include "dsp/Sampler.h"

#include "dsp/Interpolators.h"
#include "dsp/Sanitize.h"

#include <algorithm>
#include <cmath>

namespace le::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;

// State của voice dùng được: vị trí / bước hữu hạn, bước > 0 (không bao giờ đọc lùi ra ngoài vùng đệm), gain hữu hạn.
// NaN ở pos sẽ thành chỉ số mảng tuỳ ý trong readHermite → phải chặn TRƯỚC khi đọc.
bool finiteState(double pos, double inc, float gainL, float gainR) noexcept [[clang::nonblocking]] {
    return std::isfinite(pos) && std::isfinite(inc) && inc > 0.0 && std::isfinite(gainL) && std::isfinite(gainR);
}

// inst sở hữu (qua Instrument::samples) đúng AudioData này không. Chỉ so con trỏ, không cấp phát.
bool ownsData(const Instrument* inst, const AudioData* data) noexcept [[clang::nonblocking]] {
    if (inst == nullptr || data == nullptr) return false;
    for (const AudioDataPtr& s : inst->samples)
        if (s.get() == data) return true;
    return false;
}
} // namespace

// [main]
void Sampler::prepare(double sampleRate, int /*maxBlock*/) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    voices_.fill(Voice{});
    ageCounter_ = 0;
}

// [RT]
void Sampler::setInstrument(const Instrument* inst, uint32_t generation) noexcept [[clang::nonblocking]] {
    if (inst == inst_) {
        // Instrument vẫn do snapshot mới giữ → voice đang đọc nó không còn giữ chân snapshot cũ.
        for (Voice& v : voices_)
            if (v.active() && v.inst == inst) v.generation = generation;
    } else {
        for (Voice& v : voices_) {
            if (!v.active() || v.inst != inst_) continue;
            if (ownsData(inst, v.data)) {   // instrument phái sinh (chung samples) → đổi mềm, không cắt nốt đang ngân
                v.inst = inst;
                v.generation = generation;
            } else {
                v.env.fastRelease(kSwapFadeSec, sampleRate_);
            }
        }
        inst_ = inst;
    }
    gen_ = generation;
}

// [RT] Slot chính còn trống, hoặc cướp một voice (chép sang slot dự phòng để fade).
Sampler::Voice* Sampler::allocateVoice() noexcept [[clang::nonblocking]] {
    for (int i = 0; i < kMaxVoices; ++i)
        if (!voices_[static_cast<size_t>(i)].active()) return &voices_[static_cast<size_t>(i)];

    // Nạn nhân: voice đang release có mức thấp nhất; không có thì voice được bấm lâu nhất.
    Voice* victim = nullptr;
    for (int i = 0; i < kMaxVoices; ++i) {
        Voice& v = voices_[static_cast<size_t>(i)];
        if (v.env.isReleasing() && (victim == nullptr || v.env.level() < victim->env.level())) victim = &v;
    }
    if (victim == nullptr) {
        victim = &voices_[0];
        for (int i = 1; i < kMaxVoices; ++i) {
            Voice& v = voices_[static_cast<size_t>(i)];
            if (v.age - victim->age > 0x80000000u) victim = &v;   // so sánh "cũ hơn" chịu được tràn số
        }
    }

    // Slot dự phòng: trống, hoặc (hiếm: > 8 lần cướp trong 3 ms) slot đang nhỏ tiếng nhất.
    Voice* spare = nullptr;
    for (int i = kMaxVoices; i < kMaxVoices + kSpareVoices; ++i) {
        Voice& v = voices_[static_cast<size_t>(i)];
        if (!v.active()) { spare = &v; break; }
        if (spare == nullptr || v.env.level() < spare->env.level()) spare = &v;
    }
    *spare = *victim;                       // chép toàn bộ trạng thái (vị trí, envelope…) → phát tiếp liền mạch
    spare->held = false;
    spare->env.fastRelease(kStealFadeSec, sampleRate_);
    victim->env.reset();
    return victim;
}

// [RT] Tính mọi thứ của nốt một lần (exp2, cos, sin là toán số, chỉ chạy lúc bấm nốt).
void Sampler::startVoice(Voice& v, const Zone& z, int note, int velocity) noexcept [[clang::nonblocking]] {
    const AudioData& d = *z.data;
    v.inst = inst_;
    v.offBy = z.offBy;
    v.data = &d;
    v.generation = gen_;
    v.age = ++ageCounter_;
    v.note = static_cast<int16_t>(note);
    v.held = true;
    v.oneShot = z.loopMode == LoopMode::OneShot;

    // inc = 2^((note − root + cents/100) / 12) · (zoneSR / engineSR)
    const double semis = (note - z.rootKey) + static_cast<double>(z.tuneCents) / 100.0;
    v.inc = std::exp2(semis / 12.0) * (d.sampleRate() / sampleRate_);
    v.pos = 0.0;

    const int64_t n = d.numFrames();
    v.loopStart = std::clamp<int64_t>(z.loopStart, 0, n);
    v.loopEnd = z.loopEnd < 0 ? n : std::min(z.loopEnd, n);
    v.looping = z.loopMode == LoopMode::LoopContinuous && v.loopEnd - v.loopStart >= 4;
    v.wrapped = false;
    v.endPos = static_cast<double>(n) + 1.0;   // qua mốc này thì cả 4 điểm Hermite đều nằm trong vùng đệm 0

    // Gain: volume (dB) · velocity² (đường cong mặc định của SFZ) · pan
    const float vel = static_cast<float>(velocity) / 127.0f;
    const float gain = std::pow(10.0f, z.gainDb / 20.0f) * vel * vel;
    const float pan = std::clamp(z.pan, -1.0f, 1.0f);
    if (d.isStereo()) {   // stereo: cân bằng trái/phải, giữa = 0 dB
        v.gainL = gain * std::min(1.0f, 1.0f - pan);
        v.gainR = gain * std::min(1.0f, 1.0f + pan);
    } else {              // mono: luật pan −3 dB (04 §11)
        const double theta = (static_cast<double>(pan) + 1.0) * kPi / 4.0;
        v.gainL = gain * static_cast<float>(std::cos(theta));
        v.gainR = gain * static_cast<float>(std::sin(theta));
    }

    v.env.reset();
    if (!finiteState(v.pos, v.inc, v.gainL, v.gainR)) {   // zone / sample rate lạ → không phát (voice vẫn rảnh)
        ++nonFinite_;
        return;
    }
    v.env.start(z.env, sampleRate_);
}

// [RT]
void Sampler::noteOn(int note, float velocity) noexcept [[clang::nonblocking]] {
    if (inst_ == nullptr || note < 0 || note > 127) return;
    const int vel = std::clamp(static_cast<int>(velocity * 127.0f + 0.5f), 1, 127);
    const Zone* z = inst_->findZone(note, vel);
    if (z == nullptr || z->data == nullptr) return;

    if (z->group != 0) {   // choke: tắt nhanh các voice bị nhóm này "off_by"
        for (Voice& v : voices_)
            if (v.active() && v.offBy == z->group) {
                v.held = false;
                v.env.fastRelease(kChokeFadeSec, sampleRate_);
            }
    }
    Voice* v = allocateVoice();
    startVoice(*v, *z, note, vel);
}

// [RT]
void Sampler::noteOff(int note) noexcept [[clang::nonblocking]] {
    for (Voice& v : voices_) {
        if (!v.active() || !v.held || v.note != note) continue;
        v.held = false;
        if (!v.oneShot) v.env.noteOff();
    }
}

// [RT]
void Sampler::allNotesOff(bool fast) noexcept [[clang::nonblocking]] {
    for (Voice& v : voices_) {
        if (!v.active()) continue;
        v.held = false;
        if (fast) v.env.fastRelease(kChokeFadeSec, sampleRate_);
        else if (!v.oneShot) v.env.noteOff();
    }
}

// [RT] Một voice, n sample. L/R đã dịch tới vị trí bắt đầu. R == nullptr → output mono.
void Sampler::renderVoice(Voice& v, float* L, float* R, int n) noexcept [[clang::nonblocking]] {
    const AudioData& d = *v.data;
    const float* c0 = d.channel(0);
    const float* c1 = d.channelOrMono(1);
    const bool stereoData = d.isStereo();
    const int64_t ls = v.loopStart, le = v.loopEnd;
    const double loopLen = static_cast<double>(le - ls);
    const float monoL = R == nullptr ? 0.5f * v.gainL : v.gainL;
    const float monoR = R == nullptr ? 0.5f * v.gainR : v.gainR;
    if (!finiteState(v.pos, v.inc, v.gainL, v.gainR)) {   // state hỏng → tắt voice, không đọc gì
        v.env.reset();
        ++nonFinite_;
        return;
    }

    float acc = 0.0f;   // tổng mẫu đã đọc: NaN / Inf trong dữ liệu sample → tắt voice sau block này
    for (int s = 0; s < n; ++s) {
        const float e = v.env.next();
        if (!v.env.isActive()) break;   // envelope đã tắt → voice rảnh

        float a, b;
        // Trong vòng lặp (sau khi đã vòng lần đầu, hoặc đã đủ xa loopStart) → điểm lân cận lấy vòng.
        if (v.looping && (v.wrapped || v.pos >= static_cast<double>(ls) + 1.0)) {
            a = readHermiteLoop(c0, v.pos, ls, le);
            b = stereoData ? readHermiteLoop(c1, v.pos, ls, le) : a;
        } else {
            a = readHermite(c0, v.pos);
            b = stereoData ? readHermite(c1, v.pos) : a;
        }
        acc += a + b;
        if (R != nullptr) {
            L[s] += a * e * monoL;
            R[s] += b * e * monoR;
        } else {
            L[s] += (a * monoL + b * monoR) * e;
        }

        v.pos += v.inc;
        if (v.looping) {
            if (v.pos >= static_cast<double>(le)) {
                do { v.pos -= loopLen; } while (v.pos >= static_cast<double>(le));
                v.wrapped = true;
            }
        } else if (v.pos >= v.endPos) {
            v.env.reset();                 // hết dữ liệu (NoLoop / OneShot)
            break;
        }
    }
    if (!std::isfinite(acc)) {             // dữ liệu sample có NaN / Inf → tắt hẳn (render() xoá mẫu hỏng đã cộng)
        v.env.reset();
        ++nonFinite_;
    }
}

// [RT]
void Sampler::render(float* const* out, int numCh, int start, int n) noexcept [[clang::nonblocking]] {
    if (out == nullptr || numCh <= 0 || n <= 0) return;
    float* L = out[0] + start;
    float* R = numCh >= 2 ? out[1] + start : nullptr;
    bool any = false;
    for (Voice& v : voices_)
        if (v.active()) {
            renderVoice(v, L, R, n);
            any = true;
        }
    if (any && sanitizeIfNeeded(L, R, n)) ++nonFinite_;   // 1 lượt kiểm rẻ mỗi block (dsp/Sanitize.h)
}

// [RT]
int Sampler::countVoicesUsing(uint32_t generation) const noexcept [[clang::nonblocking]] {
    int count = 0;
    for (const Voice& v : voices_)
        if (v.active() && v.generation == generation) ++count;
    return count;
}

// [RT]
void Sampler::fastReleaseGeneration(uint32_t generation) noexcept [[clang::nonblocking]] {
    for (Voice& v : voices_)
        if (v.active() && v.generation == generation) {
            v.held = false;
            v.env.fastRelease(kRetireFadeSec, sampleRate_);
        }
}

int Sampler::activeVoices() const noexcept [[clang::nonblocking]] {
    int count = 0;
    for (int i = 0; i < kMaxVoices; ++i)
        if (voices_[static_cast<size_t>(i)].active()) ++count;
    return count;
}

int Sampler::fadingVoices() const noexcept [[clang::nonblocking]] {
    int count = 0;
    for (int i = kMaxVoices; i < kMaxVoices + kSpareVoices; ++i)
        if (voices_[static_cast<size_t>(i)].active()) ++count;
    return count;
}

} // namespace le::dsp
