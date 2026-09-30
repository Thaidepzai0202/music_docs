#include "core/PreviewPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace le::core {

void PreviewPlayer::prepare(double sampleRate, int maxBlock) {
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock_ = std::max(1, maxBlock);
    sampler_.prepare(sampleRate_, maxBlock_);   // tắt mọi voice → vé sampler đang retire được nhả ở block đầu
    samplerBusy_ = false;
    scratch_ = std::make_unique<float[]>((std::size_t) maxBlock_ * 2);
    fadeIn_ = std::max(1, (int) std::lround(kFadeInSec * sampleRate_));
    fadeOut_ = std::max(1, (int) std::lround(kFadeOutSec * sampleRate_));
    // cur_ / retiring_ GIỮ NGUYÊN: audio chạy lại (đổi route) thì lượt đang nghe tiếp tục, vé nào cũng được báo nhả.
}

std::int64_t PreviewPlayer::frameOf(double sec) const noexcept [[clang::nonblocking]] {
    return (std::int64_t) std::llround(std::max(0.0, sec) * sampleRate_);
}

// [RT] Nội suy tuyến tính, gain ramp theo mẫu. Hết dữ liệu → gain về 0 ngay (đuôi 0 của AudioData không click).
void PreviewPlayer::readHead(Head& h, float* L, float* R, int n) noexcept [[clang::nonblocking]] {
    if (h.data == nullptr) return;
    const dsp::AudioData& d = *h.data;
    const float* c0 = d.channel(0);
    const float* c1 = d.channelOrMono(1);
    const double end = (double) d.numFrames();
    for (int i = 0; i < n; ++i) {
        if (h.pos >= end) {
            h.gain.snap(0.0f);
            return;
        }
        const auto i0 = (std::int64_t) h.pos;
        const float fr = (float) (h.pos - (double) i0);
        const float g = h.gain.next();
        L[i] += g * (c0[i0] + fr * (c0[i0 + 1] - c0[i0]));   // i0 + 1 ≤ numFrames: nằm trong vùng đệm 0
        R[i] += g * (c1[i0] + fr * (c1[i0 + 1] - c1[i0]));
        h.pos += h.step;
    }
}

bool PreviewPlayer::retireCurrent(bool fast) noexcept [[clang::nonblocking]] {
    if (cur_.id == 0) return true;
    if (numRetiring_ >= kMaxRetiring) return false;   // đầy: lượt hiện tại chạy tiếp, block sau thử lại
    Retiring& r = retiring_[numRetiring_++];
    r = Retiring{};
    r.id = cur_.id;
    r.sampler = cur_.sampler;
    if (cur_.sampler) {
        if (fast) sampler_.fastReleaseGeneration(cur_.id);   // lượt mới / stop: tắt 5 ms; hết lịch: đuôi release kêu tiếp
    } else {
        r.head = cur_.head;
        r.head.gain.set(0.0f, fadeOut_);
    }
    cur_ = Live{};
    return true;
}

// Caller bảo đảm còn ≥ 2 chỗ retire (lượt cũ + vé rỗng).
void PreviewPlayer::start(const Ticket& t) noexcept [[clang::nonblocking]] {
    (void) retireCurrent(true);
    cur_ = Live{};
    cur_.id = t.id;
    cur_.length = frameOf(t.lengthSec);
    if (t.instrument != nullptr) {
        cur_.sampler = true;
        cur_.events = t.events;
        cur_.numEvents = std::clamp(t.numEvents, 0, kMaxEvents);
        // Cùng instrument với lượt trước (cache LRU): voice cũ sang generation mới (vẫn đang fade) — dữ liệu vẫn do vé
        // mới giữ. Khác instrument: voice cũ fade 5 ms và giữ generation cũ tới khi tắt hẳn.
        sampler_.setInstrument(t.instrument, t.id);
        samplerBusy_ = true;
    } else if (t.audio != nullptr && t.audio->numFrames() > 0) {
        cur_.head.data = t.audio;
        cur_.head.step = t.audio->sampleRate() / sampleRate_;
        cur_.head.gain.snap(0.0f);
        cur_.head.gain.set(1.0f, fadeIn_);
        cur_.length = std::min(cur_.length, (std::int64_t) std::ceil((double) t.audio->numFrames() / cur_.head.step));
    }
    // Vé rỗng: retire ngay (được báo nhả ở cuối block)
    if (!cur_.sampler && cur_.head.data == nullptr) (void) retireCurrent(false);
}

// [RT] Sampler: chia block tại mốc nốt của lượt hiện tại; voice của lượt cũ (đang fade / release) render chung.
void PreviewPlayer::renderSampler(float* L, float* R, int n) noexcept [[clang::nonblocking]] {
    float* const chans[2] = {L, R};
    int done = 0;
    while (cur_.sampler && done < n) {
        const std::int64_t now = cur_.pos + done;
        if (cur_.nextEvent < cur_.numEvents) {
            const Event& e = cur_.events[cur_.nextEvent];
            const std::int64_t at = frameOf(e.sec);
            if (at <= now) {
                if (e.on) sampler_.noteOn(e.note, e.velocity);
                else sampler_.noteOff(e.note);
                ++cur_.nextEvent;
                continue;
            }
            const int upTo = (int) std::min<std::int64_t>(n, at - cur_.pos);
            sampler_.render(chans, 2, done, upTo - done);
            done = upTo;
        } else if (now >= cur_.length && retireCurrent(false)) {
            break;   // hết lịch: đuôi release render tiếp bên dưới
        } else {
            const int upTo = now >= cur_.length ? n : (int) std::min<std::int64_t>(n, cur_.length - cur_.pos);
            sampler_.render(chans, 2, done, upTo - done);
            done = upTo;
        }
    }
    if (cur_.sampler) cur_.pos += n;
    if (done < n && sampler_.activeVoices() + sampler_.fadingVoices() > 0) sampler_.render(chans, 2, done, n - done);
}

void PreviewPlayer::process(float* L, float* R, int n, RtToNrtQueue& out) noexcept [[clang::nonblocking]] {
    // 1) stop / vé mới. Danh sách retire đầy (≥ 8 lượt fade trong vài ms, hiếm) → để lại block sau.
    const std::uint32_t stop = stopSeq_.load(std::memory_order_acquire);
    if (stop != seenStop_ && numRetiring_ < kMaxRetiring) {
        seenStop_ = stop;
        retireCurrent(true);
    }
    if (numRetiring_ <= kMaxRetiring - 2 && offer_.load(std::memory_order_relaxed) != nullptr)
        if (const Ticket* t = offer_.exchange(nullptr, std::memory_order_acq_rel)) start(*t);

    // Im hoàn toàn khi không nghe thử: không chạm L/R (bit-exact với engine không có preview), chỉ đọc 3 biến.
    if (cur_.id == 0 && numRetiring_ == 0 && !samplerBusy_) return;

    // 2) Render vào scratch rồi cộng × kGain vào master.
    float* sL = scratch_.get();
    float* sR = sL + maxBlock_;
    for (int done = 0; done < n;) {
        const int m = std::min(maxBlock_, n - done);
        std::memset(sL, 0, sizeof(float) * (std::size_t) m);
        std::memset(sR, 0, sizeof(float) * (std::size_t) m);
        if (cur_.id != 0 && !cur_.sampler) {
            // Fade-out xong đúng lúc hết độ dài; hết file sớm (gain đã về 0) cũng kết thúc.
            const bool end = cur_.pos + m + fadeOut_ >= cur_.length ||
                             (cur_.head.gain.current <= 0.0f && cur_.head.gain.target <= 0.0f);
            if (!end || !retireCurrent(false)) {
                readHead(cur_.head, sL, sR, m);
                cur_.pos += m;
            }
        }
        for (int i = 0; i < numRetiring_; ++i)
            if (!retiring_[i].sampler) readHead(retiring_[i].head, sL, sR, m);
        if (samplerBusy_) renderSampler(sL, sR, m);
        for (int i = 0; i < m; ++i) {
            L[done + i] += kGain * sL[i];
            R[done + i] += kGain * sR[i];
        }
        done += m;
    }

    samplerBusy_ = cur_.sampler || sampler_.activeVoices() + sampler_.fadingVoices() > 0;

    // 3) Lượt cũ hết người dùng → báo main nhả vé (queue đầy → thử lại block sau).
    for (int i = 0; i < numRetiring_;) {
        Retiring& r = retiring_[i];
        if (!r.done) {
            r.done = r.sampler ? sampler_.countVoicesUsing(r.id) == 0
                               : r.head.data == nullptr || (r.head.gain.current <= 0.0f && r.head.gain.remaining == 0);
            if (r.done) r.head.data = nullptr;
        }
        if (r.done) {
            RtMessage msg;
            msg.kind = RtMessage::PreviewReleased;
            msg.a = (std::int32_t) r.id;
            if (out.try_push(msg)) {
                retiring_[i] = retiring_[--numRetiring_];
                continue;
            }
        }
        ++i;
    }
}

} // namespace le::core
