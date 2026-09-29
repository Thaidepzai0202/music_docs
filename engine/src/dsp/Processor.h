// Processor — interface chung của FX (04 §9, P3-12). HỢP ĐỒNG NỘI BỘ 80 ↔ 68: 68 làm FxChain (3 slot/track)
// + master chain + snapshot, 80 làm các Processor (Filter, Delay, Reverb, EQ3, Compressor).
//
// Vòng đời (04 §9, 03 §4):
//   [main]  p = createProcessor(type) → p->prepare(sr, maxBlock) → p->setParam(id, v) cho mọi tham số đã lưu
//           → p->reset()  (xoá state + NHẢY tham số tới đích: mở project không nghe tiếng "quét" từ mặc định)
//           → đưa vào snapshot (std::unique_ptr/shared_ptr do snapshot giữ; huỷ ở main qua ReleasePool)
//   [RT]    p->process(io, numCh, ctx) mỗi block · p->setParam(id, v) khi có LE_CMD_FX_PARAM (chỉ đặt ĐÍCH,
//           giá trị trượt tới trong 20 ms → không "zipper") · p->reset() khi transport stop (xoá đuôi reverb/delay)
//   Snapshot mới giữ lại CÙNG instance processor (không tạo lại) → đuôi reverb/delay không bị cắt.
//   Tại một thời điểm, mỗi instance chỉ được đúng 1 audio thread gọi (không dùng chung giữa 2 track).
//
// An toàn RT: mọi buffer (delay line, reverb comb…) cấp phát trong prepare(). process/setParam/reset chỉ
// toán số trên bộ nhớ có sẵn. Bypass (không click) do FxChain của 68 crossfade dry/wet.
//
// Chặn NaN / Inf (R2, rt-review 29/09): cuối mỗi process() các FX gọi guardOutput(). Block ra có mẫu không hữu
// hạn → mẫu đó thành 0 và reset() (xoá state IIR / delay line đã nhiễm) → NaN không bao giờ ra khỏi FX, và
// block sau FX chạy lại từ im lặng. setParam kẹp NaN về giá trị mặc định (clampParam).
#pragma once

#include "dsp/Sanitize.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace le::dsp {

struct ProcessContext {
    int    numFrames = 0;
    double sampleRate = 48000.0;
    double bpm = 120.0;       // delay tempo-sync đọc giá trị này mỗi block
    double beat = 0.0;        // beat đầu block (cho LFO đồng bộ sau này)
    bool   playing = false;
};

// Giá trị int khớp chuỗi "type" trong JSON project (06 §2) qua fxTypeName/fxTypeFromName.
enum class FxType : int32_t { Filter = 0, Delay = 1, Reverb = 2, EQ3 = 3, Compressor = 4 };
inline constexpr int kNumFxTypes = 5;

struct ParamInfo {
    int32_t     id;
    const char* name;         // tiếng Anh ngắn, cho log/debug
    float       min, max, defaultValue;
    bool        logScale;     // UI knob theo thang log (cutoff, thời gian)
    bool        integer;      // mode, chọn nhịp delay, ping-pong 0/1
};

class Processor {
public:
    virtual ~Processor() = default;

    virtual FxType type() const noexcept = 0;

    // [main] Cấp phát mọi thứ process() cần. Có thể gọi lại khi sample rate / maxBlock đổi.
    virtual void prepare(double sampleRate, int maxBlock) = 0;

    // [RT hoặc main] Xoá state âm thanh (đuôi reverb/delay, bộ nhớ filter) và nhảy mọi tham số tới đích.
    virtual void reset() noexcept [[clang::nonblocking]] = 0;

    // [RT] Xử lý tại chỗ io[0..numCh)[0..ctx.numFrames). numCh = 1 hoặc 2. ctx.numFrames ≤ maxBlock.
    virtual void process(float* const* io, int numCh, const ProcessContext& ctx) noexcept [[clang::nonblocking]] = 0;

    // [main trước khi publish] / [RT] Đặt ĐÍCH cho tham số (tự kẹp vào [min, max] của ParamInfo). id lạ → bỏ qua.
    virtual void setParam(int id, float value) noexcept [[clang::nonblocking]] = 0;
    // [main trước khi publish] / [RT] Giá trị đích hiện tại. KHÔNG gọi từ main khi processor đã nằm trong
    // snapshot (RT có thể đang setParam → data race). Nguồn sự thật để lưu project là EngineModel của 68.
    virtual float getParam(int id) const noexcept [[clang::nonblocking]] = 0;

    virtual int latencySamples() const noexcept [[clang::nonblocking]] { return 0; }

    // [RT / test] Số lần processor tự reset vì output có NaN / Inf.
    uint32_t nonFiniteResets() const noexcept [[clang::nonblocking]] { return nonFiniteResets_; }

protected:
    // [RT] Gọi ở CUỐI process(). Trả true nếu đã phải sửa (mẫu hỏng → 0, rồi reset()).
    bool guardOutput(float* const* io, int numCh, int n) noexcept [[clang::nonblocking]] {
        bool bad = false;
        for (int c = 0; c < numCh; ++c) bad = !allFinite(io[c], n) || bad;
        if (!bad) return false;
        for (int c = 0; c < numCh; ++c) sanitize(io[c], n);
        reset();
        ++nonFiniteResets_;
        return true;
    }

private:
    uint32_t nonFiniteResets_ = 0;
};

// ── Metadata + factory (định nghĩa ở dsp/Fx.cpp) ──
std::span<const ParamInfo> paramInfo(FxType type) noexcept [[clang::nonblocking]];   // [any] bảng tĩnh
const char* fxTypeName(FxType type) noexcept [[clang::nonblocking]];                 // "filter" "delay" "reverb" "eq3" "comp"
bool fxTypeFromName(std::string_view name, FxType& out) noexcept;                    // nhận thêm bí danh "compressor"
std::unique_ptr<Processor> createProcessor(FxType type);                             // [main] chưa prepare

// [any] Kẹp v vào [min, max] của tham số id (id lạ → trả v). Dùng trong setParam của các FX.
float clampParam(FxType type, int id, float v) noexcept [[clang::nonblocking]];

} // namespace le::dsp
