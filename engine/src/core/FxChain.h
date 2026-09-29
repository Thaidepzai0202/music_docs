#pragma once
// FxChain (P3-12, 04 §9) [RT] trừ prepare(): 3 slot FX nối tiếp trên bus stereo của một track.
// MasterEq (P3-15, 04 §11): EQ3 cố định của master (gain → EQ3 → Limiter).
//
// - Thêm / đổi loại / xoá FX là lệnh CẤU TRÚC: main tạo Processor mới (prepare → setParam → reset) rồi đưa vào
//   snapshot. RT thấy slot có instance khác → crossfade 20 ms từ instance cũ (hoặc dry nếu slot đang trống) sang
//   instance mới (hoặc dry nếu xoá). Trong lúc fade cả hai xử lý CÙNG input → không click.
// - Bypass (LE_CMD_FX_BYPASS): crossfade dry/wet 10 ms. Khi đã bypass hẳn, processor vẫn chạy nhưng nhận IM LẶNG:
//   đuôi reverb/delay tắt tự nhiên, bật lại không nghe vọng cũ, và tải CPU tệ nhất không phụ thuộc bypass.
// - Transport stop KHÔNG reset processor: đuôi reverb/delay ngân tự nhiên.
// - Processor luôn được gọi theo khúc ≤ kMaxFxBlock frame → main prepare(sr, kMaxFxBlock), không phụ thuộc buffer
//   của thiết bị.
//
// Ownership & generation (vì sao an toàn):
// - Snapshot (và EngineModel) giữ shared_ptr<Processor>; FxChain chỉ MƯỢN con trỏ thô. Instance đang fade out thuộc
//   về snapshot cũ → FxChain là GenerationUser: báo generation mới nhất còn chứa instance đó cho tới khi fade xong.
//   Sau đó RtEngine trả snapshot về main (Retire) và processor bị huỷ trên main (ReleasePool), không bao giờ trên RT.
// - Sau khi publish, chỉ RT gọi process / setParam của instance. Main không đụng nữa (tham số lưu ở EngineModel).
// - Lệnh FX mang instanceId (main ghi vào d0). Lệnh gửi ngay sau fx.set có thể tới RT TRƯỚC snapshot chứa instance
//   mới → FxChain giữ tạm (stash) rồi áp khi instance đó xuất hiện. Lệnh không bao giờ bị áp nhầm sang instance khác.
#include <cstdint>
#include <memory>

#include "core/GraphSnapshot.h"
#include "core/Mixer.h"
#include "dsp/Processor.h"

namespace le::core {

class FxChain final : public GenerationUser {
public:
    static constexpr int kMaxFxBlock = 256;         // maxBlock truyền cho Processor::prepare
    static constexpr double kSwapSeconds = 0.020;   // thêm / đổi / xoá FX
    static constexpr double kBypassSeconds = 0.010;
    static constexpr double kFastSeconds = 0.003;   // retiring đầy (fastReleaseGeneration)

    // [main, audio chưa chạy] Độ dài fade theo sample rate. Instance đang fade out được bỏ luôn.
    void prepare(double sampleRate) noexcept;

    // [RT] Snapshot mới (RtEngine::remap).
    void remap(const FxSlotSnapshot* slots, std::uint32_t generation) noexcept [[clang::nonblocking]];
    // [RT] LE_CMD_FX_PARAM / LE_CMD_FX_BYPASS (đã validate ở main).
    void setParam(int slot, std::uint32_t instanceId, int id, float value) noexcept [[clang::nonblocking]];
    void setBypass(int slot, std::uint32_t instanceId, bool on) noexcept [[clang::nonblocking]];
    // [RT] Mapping MIDI (P4-04): áp cho instance của snapshot RT hiện tại. Trả instanceId (0 = slot trống).
    std::uint32_t setParamLatest(int slot, int id, float value) noexcept [[clang::nonblocking]];
    // [RT] Xử lý tại chỗ bus stereo (n bất kỳ, tự chia khúc ≤ kMaxFxBlock).
    void process(float* L, float* R, int n, const dsp::ProcessContext& ctx) noexcept [[clang::nonblocking]];

    // [RT / test]
    std::uint32_t currentInstance(int slot) const noexcept [[clang::nonblocking]] { return slots_[slot].cur.id; }
    bool fading(int slot) const noexcept [[clang::nonblocking]] { return slots_[slot].fading; }

    // GenerationUser
    bool usesGeneration(std::uint32_t g) const noexcept [[clang::nonblocking]] override;
    void fastReleaseGeneration(std::uint32_t g) noexcept [[clang::nonblocking]] override;

private:
    struct Inst {
        dsp::Processor* p = nullptr;   // nullptr = dry (slot trống)
        std::uint32_t id = 0;
        std::uint32_t gen = 0;         // generation mới nhất của snapshot còn giữ p
        bool bypass = false;
        LinearRamp wet;                // 1 = qua FX, 0 = dry
    };
    struct Slot {
        Inst cur, old;                 // old: đang fade out (chỉ khi fading)
        bool fading = false;
        LinearRamp xf;                 // 0 → 1: phần của cur trong lúc fade
        // Instance trong snapshot RT mới nhất; thành cur khi không còn fade nào đang chạy.
        dsp::Processor* wantP = nullptr;
        std::uint32_t wantId = 0, wantGen = 0;
        bool wantBypass = false;
        bool urgent = false;           // fastReleaseGeneration: lần chuyển tới dùng fade nhanh
        // Lệnh cho instance chưa có trong snapshot RT (id lớn hơn wantId).
        std::uint32_t stashId = 0;
        float stash[kMaxFxParams] = {};
        std::uint32_t stashMask = 0;
        int stashBypass = -1;          // -1 = không có lệnh bypass
    };

    void beginSwap(Slot& s) noexcept [[clang::nonblocking]];
    void processSlot(Slot& s, float* L, float* R, int n, const dsp::ProcessContext& ctx) noexcept [[clang::nonblocking]];
    void renderInst(Inst& in, float* L, float* R, int n, const dsp::ProcessContext& ctx) noexcept [[clang::nonblocking]];
    Slot* stashFor(int slot, std::uint32_t instanceId) noexcept [[clang::nonblocking]];

    Slot slots_[kFxSlots];
    int swapLen_ = 960, bypassLen_ = 480, fastLen_ = 144;
    // Buffer tạm (cố định, không cấp phát): a/b = input của cur/old khi crossfade, w = wet khi đang trộn dry/wet.
    float a_[2][kMaxFxBlock] = {}, b_[2][kMaxFxBlock] = {}, w_[2][kMaxFxBlock] = {};
};

// Master EQ3 cố định. RtEngine sở hữu instance (không qua snapshot, không bao giờ bị thay).
// Phẳng (3 band = 0 dB) hoặc bypass → KHÔNG xử lý: output giống hệt từng bit như khi chưa có EQ (golden cũ vẫn đúng).
// Bật lại → reset() (xoá state cũ, tham số nhảy tới đích) rồi crossfade dry → EQ 10 ms.
class MasterEq {
public:
    MasterEq();                                   // [main] tạo Eq3Fx
    void prepare(double sampleRate);              // [main, audio chưa chạy]
    void setGainDb(int band, float db) noexcept [[clang::nonblocking]];
    void setBypass(bool on) noexcept [[clang::nonblocking]];
    void process(float* L, float* R, int n, const dsp::ProcessContext& ctx) noexcept [[clang::nonblocking]];

    bool running() const noexcept [[clang::nonblocking]] { return running_; }   // [RT / test]
    float gainDb(int band) const noexcept [[clang::nonblocking]] { return gains_[band]; }

private:
    std::unique_ptr<dsp::Processor> eq_;
    float gains_[3] = {};
    bool bypass_ = false;
    bool running_ = false;
    LinearRamp wet_;
    int fadeLen_ = 480;
    float w_[2][FxChain::kMaxFxBlock] = {};
};

} // namespace le::core
