#include "core/FxChain.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace le::core {

namespace {

int samplesFor(double seconds, double sr) noexcept { return std::max(1, (int) std::lround(seconds * sr)); }

// ctx cho khúc bắt đầu ở frame `offset` của khối.
dsp::ProcessContext pieceContext(const dsp::ProcessContext& ctx, int offset, int n) noexcept [[clang::nonblocking]] {
    dsp::ProcessContext c = ctx;
    c.numFrames = n;
    if (ctx.sampleRate > 0.0) c.beat = ctx.beat + (double) offset * ctx.bpm / (60.0 * ctx.sampleRate);
    return c;
}

bool settledAt(const LinearRamp& r, bool one) noexcept [[clang::nonblocking]] {
    return r.remaining == 0 && (one ? r.current >= 1.0f : r.current <= 0.0f);
}

} // namespace

// ─────────────────────────── FxChain ───────────────────────────

void FxChain::prepare(double sampleRate) noexcept {
    swapLen_ = samplesFor(kSwapSeconds, sampleRate);
    bypassLen_ = samplesFor(kBypassSeconds, sampleRate);
    fastLen_ = samplesFor(kFastSeconds, sampleRate);
    for (Slot& s : slots_) {
        s.old = Inst{};   // audio đã dừng: không còn gì để fade
        s.fading = false;
        s.xf.snap(1.0f);
        s.cur.wet.snap(s.cur.bypass ? 0.0f : 1.0f);
    }
}

void FxChain::remap(const FxSlotSnapshot* slots, std::uint32_t generation) noexcept [[clang::nonblocking]] {
    for (int k = 0; k < kFxSlots; ++k) {
        const FxSlotSnapshot& f = slots[k];
        Slot& s = slots_[k];
        const std::uint32_t id = f.proc != nullptr ? f.instanceId : 0;
        if (id != s.wantId) {
            s.wantP = f.proc.get();
            s.wantId = id;
            s.wantBypass = f.bypass;   // giá trị ĐẦU; cùng instance thì RT giữ trạng thái của nó (theo lệnh)
            if (id != 0 && s.stashId == id) {   // lệnh đã tới trước snapshot → áp ngay (RT là thread duy nhất dùng nó)
                for (int p = 0; p < kMaxFxParams; ++p)
                    if ((s.stashMask & (1u << p)) != 0) s.wantP->setParam(p, s.stash[p]);
                if (s.stashBypass >= 0) s.wantBypass = s.stashBypass != 0;
            }
            if (s.stashId <= id) {   // stash cho instance này hoặc cho instance đã bị thay trước khi tới RT
                s.stashId = 0;
                s.stashMask = 0;
                s.stashBypass = -1;
            }
        }
        s.wantGen = generation;
        if (s.cur.p != nullptr && s.cur.id == id) s.cur.gen = generation;   // snapshot mới vẫn giữ instance đang chạy
    }
}

FxChain::Slot* FxChain::stashFor(int slot, std::uint32_t instanceId) noexcept [[clang::nonblocking]] {
    Slot& s = slots_[slot];
    // Instance cũ hơn snapshot RT (đang fade out hoặc đã bị thay) → bỏ lệnh.
    if (instanceId == 0 || instanceId < s.wantId || instanceId == s.cur.id || instanceId == s.old.id) return nullptr;
    if (s.stashId != instanceId) {
        s.stashId = instanceId;
        s.stashMask = 0;
        s.stashBypass = -1;
    }
    return &s;
}

void FxChain::setParam(int slot, std::uint32_t instanceId, int id, float value) noexcept [[clang::nonblocking]] {
    if (slot < 0 || slot >= kFxSlots || id < 0 || id >= kMaxFxParams) return;
    Slot& s = slots_[slot];
    if (instanceId != 0 && instanceId == s.wantId) {   // instance của snapshot RT (đang chạy hoặc chờ fade xong)
        s.wantP->setParam(id, value);
        return;
    }
    if (Slot* st = stashFor(slot, instanceId)) {
        st->stash[id] = value;
        st->stashMask |= 1u << id;
    }
}

std::uint32_t FxChain::setParamLatest(int slot, int id, float value) noexcept [[clang::nonblocking]] {
    if (slot < 0 || slot >= kFxSlots || id < 0 || id >= kMaxFxParams || slots_[slot].wantP == nullptr) return 0;
    slots_[slot].wantP->setParam(id, value);
    return slots_[slot].wantId;
}

void FxChain::setBypass(int slot, std::uint32_t instanceId, bool on) noexcept [[clang::nonblocking]] {
    if (slot < 0 || slot >= kFxSlots) return;
    Slot& s = slots_[slot];
    if (instanceId != 0 && instanceId == s.wantId) {
        s.wantBypass = on;
        if (s.cur.id == instanceId) {
            s.cur.bypass = on;
            s.cur.wet.set(on ? 0.0f : 1.0f, bypassLen_);
        }
        return;
    }
    if (Slot* st = stashFor(slot, instanceId)) st->stashBypass = on ? 1 : 0;
}

// cur → old (fade out), instance của snapshot → cur (fade in).
void FxChain::beginSwap(Slot& s) noexcept [[clang::nonblocking]] {
    s.old = s.cur;
    s.cur = Inst{};
    s.cur.p = s.wantP;
    s.cur.id = s.wantId;
    s.cur.gen = s.wantGen;
    s.cur.bypass = s.wantBypass;
    s.cur.wet.snap(s.wantBypass ? 0.0f : 1.0f);
    s.xf.snap(0.0f);
    s.xf.set(1.0f, s.urgent ? fastLen_ : swapLen_);
    s.urgent = false;
    s.fading = true;
}

// Y = dry + wet · (P(dry) − dry), tại chỗ trên L/R.
void FxChain::renderInst(Inst& in, float* L, float* R, int n, const dsp::ProcessContext& ctx) noexcept
    [[clang::nonblocking]] {
    if (in.p == nullptr) return;   // slot trống = dry
    if (settledAt(in.wet, true)) {
        float* io[2] = {L, R};
        in.p->process(io, 2, ctx);
        return;
    }
    if (settledAt(in.wet, false)) {   // bypass hẳn: processor nhận im lặng (đuôi tắt dần), output = dry
        std::memset(w_[0], 0, sizeof(float) * (std::size_t) n);
        std::memset(w_[1], 0, sizeof(float) * (std::size_t) n);
        float* io[2] = {w_[0], w_[1]};
        in.p->process(io, 2, ctx);
        return;
    }
    std::memcpy(w_[0], L, sizeof(float) * (std::size_t) n);
    std::memcpy(w_[1], R, sizeof(float) * (std::size_t) n);
    float* io[2] = {w_[0], w_[1]};
    in.p->process(io, 2, ctx);
    for (int i = 0; i < n; ++i) {   // (1−g)·dry + g·wet: đúng từng bit ở g = 0 và g = 1
        const float g = in.wet.next();
        L[i] = (1.0f - g) * L[i] + g * w_[0][i];
        R[i] = (1.0f - g) * R[i] + g * w_[1][i];
    }
}

void FxChain::processSlot(Slot& s, float* L, float* R, int n, const dsp::ProcessContext& ctx) noexcept
    [[clang::nonblocking]] {
    if (!s.fading && s.wantId != s.cur.id) beginSwap(s);
    if (!s.fading) {
        renderInst(s.cur, L, R, n, ctx);
        return;
    }
    // Crossfade old → cur: cả hai nhận cùng input.
    const std::size_t bytes = sizeof(float) * (std::size_t) n;
    std::memcpy(a_[0], L, bytes);
    std::memcpy(a_[1], R, bytes);
    std::memcpy(b_[0], L, bytes);
    std::memcpy(b_[1], R, bytes);
    renderInst(s.cur, a_[0], a_[1], n, ctx);
    renderInst(s.old, b_[0], b_[1], n, ctx);
    for (int i = 0; i < n; ++i) {
        const float x = s.xf.next();
        L[i] = (1.0f - x) * b_[0][i] + x * a_[0][i];
        R[i] = (1.0f - x) * b_[1][i] + x * a_[1][i];
    }
    if (s.xf.remaining == 0) {   // fade xong: nhả instance cũ → snapshot của nó được thu hồi ở block sau
        s.old = Inst{};
        s.fading = false;
    }
}

void FxChain::process(float* L, float* R, int n, const dsp::ProcessContext& ctx) noexcept [[clang::nonblocking]] {
    bool any = false;
    for (const Slot& s : slots_) any |= s.cur.p != nullptr || s.fading || s.wantId != s.cur.id;
    if (!any) return;   // chuỗi trống: bus giữ nguyên từng bit
    for (int off = 0; off < n; off += kMaxFxBlock) {
        const int m = std::min(kMaxFxBlock, n - off);
        const dsp::ProcessContext c = pieceContext(ctx, off, m);
        for (Slot& s : slots_) processSlot(s, L + off, R + off, m, c);
    }
}

bool FxChain::usesGeneration(std::uint32_t g) const noexcept [[clang::nonblocking]] {
    for (const Slot& s : slots_) {
        if (s.cur.p != nullptr && s.cur.gen == g) return true;
        if (s.fading && s.old.p != nullptr && s.old.gen == g) return true;
    }
    return false;
}

void FxChain::fastReleaseGeneration(std::uint32_t g) noexcept [[clang::nonblocking]] {
    for (Slot& s : slots_) {
        if (s.fading && s.old.p != nullptr && s.old.gen == g && s.xf.remaining > fastLen_) s.xf.set(1.0f, fastLen_);
        if (s.cur.p != nullptr && s.cur.gen == g && s.cur.id != s.wantId) s.urgent = true;
    }
}

// ─────────────────────────── MasterEq ───────────────────────────

MasterEq::MasterEq() : eq_(dsp::createProcessor(dsp::FxType::EQ3)) {}

void MasterEq::prepare(double sampleRate) {
    fadeLen_ = samplesFor(FxChain::kBypassSeconds, sampleRate);
    eq_->prepare(sampleRate, FxChain::kMaxFxBlock);   // giữ tham số, xoá state
    running_ = false;                                  // process() tự bật lại nếu cần
    wet_.snap(0.0f);
}

void MasterEq::setGainDb(int band, float db) noexcept [[clang::nonblocking]] {
    if (band < 0 || band > 2) return;
    gains_[band] = dsp::clampParam(dsp::FxType::EQ3, band, db);
    eq_->setParam(band, gains_[band]);   // đang không chạy: chỉ đặt đích, reset() lúc bật sẽ nhảy tới
}

void MasterEq::setBypass(bool on) noexcept [[clang::nonblocking]] { bypass_ = on; }

void MasterEq::process(float* L, float* R, int n, const dsp::ProcessContext& ctx) noexcept [[clang::nonblocking]] {
    bool flat = true;
    for (const float g : gains_) flat = flat && std::fabs(g) < 1e-6f;
    const bool want = !bypass_ && !flat;
    if (want && !running_) {
        eq_->reset();
        running_ = true;
        wet_.snap(0.0f);
        wet_.set(1.0f, fadeLen_);
    } else if (running_ && want != (wet_.target > 0.5f)) {
        wet_.set(want ? 1.0f : 0.0f, fadeLen_);
    }
    if (!running_) return;

    for (int off = 0; off < n; off += FxChain::kMaxFxBlock) {
        const int m = std::min(FxChain::kMaxFxBlock, n - off);
        const dsp::ProcessContext c = pieceContext(ctx, off, m);
        float* l = L + off;
        float* r = R + off;
        if (settledAt(wet_, true)) {
            float* io[2] = {l, r};
            eq_->process(io, 2, c);
            continue;
        }
        std::memcpy(w_[0], l, sizeof(float) * (std::size_t) m);
        std::memcpy(w_[1], r, sizeof(float) * (std::size_t) m);
        float* io[2] = {w_[0], w_[1]};
        eq_->process(io, 2, c);
        for (int i = 0; i < m; ++i) {
            const float g = wet_.next();
            l[i] = (1.0f - g) * l[i] + g * w_[0][i];
            r[i] = (1.0f - g) * r[i] + g * w_[1][i];
        }
    }
    if (settledAt(wet_, false)) running_ = false;   // đã về dry hẳn → thôi xử lý
}

} // namespace le::core
