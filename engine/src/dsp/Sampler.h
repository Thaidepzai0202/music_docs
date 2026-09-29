// Sampler — voice pool phát một Instrument (04 §6, P1-25/P1-26). Mỗi track instrument có 1 Sampler
// (68 giữ trong RtState). Mọi hàm trừ prepare() chạy trên audio thread.
//
// Pool: 64 voice chính + 8 voice DỰ PHÒNG. Khi đủ 64 voice mà có nốt mới:
//   chọn nạn nhân (voice đang release có mức thấp nhất, không có thì voice lâu nhất) → CHÉP nó sang
//   một slot dự phòng và fade 3 ms ở đó (không click) → nốt mới dùng ngay slot chính vừa trống.
// Choke (SFZ group/off_by): nốt mới của zone có group = g tắt nhanh 5 ms mọi voice có offBy = g.
// Đổi instrument: voice của instrument cũ fade 5 ms, vẫn đọc dữ liệu cũ trong lúc fade. Mỗi voice ghi
// `generation` của snapshot nó đang đọc → 68 chỉ retire snapshot khi countVoicesUsing(gen) == 0 (03 §4.2).
// Ngoại lệ "đổi mềm" (instrument phái sinh: instrument.setEnvelope / setMode tạo Instrument mới dùng CHUNG samples):
// voice mà instrument mới cũng sở hữu dữ liệu nó đang đọc thì KHÔNG fade — ngân tiếp với envelope / điểm loop / gain
// đã chốt lúc bấm (05 §3: "voice đang kêu giữ envelope cũ, nốt mới dùng envelope mới") và chuyển luôn sang generation
// mới (không giữ chân snapshot cũ). An toàn vì sau khi start, voice chỉ còn đọc AudioData (không đọc Zone/Instrument).
//
// An toàn RT: mảng voice cố định (std::array), cấp phát xong ở compile time. Mọi hàm [RT] chỉ đọc
// Instrument bất biến qua con trỏ thô + toán số; không cấp phát, không lock, không huỷ gì.
// NaN / Inf (rt-review 29/09 R2): voice có state không hữu hạn không được phát; voice đọc ra dữ liệu hỏng bị tắt;
// vùng output của mỗi render() được kiểm 1 lượt và mẫu hỏng thành 0 → sampler không bao giờ đẩy NaN ra bus.
// Sự kiện nốt áp dụng NGAY tại vị trí render hiện tại: caller chia block thành segment tại mốc nốt
// (BlockSplitter) rồi gọi render() cho từng segment.
#pragma once

#include "dsp/Adsr.h"
#include "dsp/Instrument.h"

#include <array>
#include <cstdint>

namespace le::dsp {

class Sampler {
public:
    static constexpr int   kMaxVoices = 64;
    static constexpr int   kSpareVoices = 8;
    static constexpr float kStealFadeSec = 0.003f;
    static constexpr float kChokeFadeSec = 0.005f;
    static constexpr float kSwapFadeSec = 0.005f;
    static constexpr float kRetireFadeSec = 0.003f;

    // [main] Khi audio chưa chạy (hoặc track chưa được RT dùng). Tắt mọi voice.
    void prepare(double sampleRate, int maxBlock);

    // [RT] Gọi trong remap mỗi lần swap snapshot. Cùng con trỏ → chỉ cập nhật generation của voice.
    // Khác con trỏ: voice mà inst mới VẪN sở hữu AudioData của nó (Instrument::samples) → ngân tiếp, sang generation
    // mới; voice còn lại → fade kSwapFadeSec (giữ generation cũ). Nốt mới dùng instrument mới.
    void setInstrument(const Instrument* inst, uint32_t generation) noexcept [[clang::nonblocking]];

    // [RT] velocity 0..1 (LE_CMD_NOTE_ON f0) → MIDI 1..127. Không có zone khớp → bỏ qua.
    void noteOn(int note, float velocity) noexcept [[clang::nonblocking]];
    // [RT] Nhả mọi voice đang giữ nốt này (voice one-shot bỏ qua note-off).
    void noteOff(int note) noexcept [[clang::nonblocking]];
    // [RT] fast = false: nhả mọi nốt (one-shot vẫn kêu hết). fast = true: tắt tất cả trong 5 ms.
    void allNotesOff(bool fast) noexcept [[clang::nonblocking]];

    // [RT] CỘNG DỒN vào out[c][start .. start+n). numCh = 1 → trộn về mono; ≥ 2 → kênh 0 và 1.
    void render(float* const* out, int numCh, int start, int n) noexcept [[clang::nonblocking]];

    // [RT] Cho cơ chế retire theo generation của 68 (tính cả voice dự phòng đang fade).
    int  countVoicesUsing(uint32_t generation) const noexcept [[clang::nonblocking]];
    void fastReleaseGeneration(uint32_t generation) noexcept [[clang::nonblocking]];

    int activeVoices() const noexcept [[clang::nonblocking]];   // slot chính đang dùng (≤ 64)
    int fadingVoices() const noexcept [[clang::nonblocking]];   // slot dự phòng đang fade (≤ 8)
    const Instrument* instrument() const noexcept [[clang::nonblocking]] { return inst_; }
    uint32_t generation() const noexcept [[clang::nonblocking]] { return gen_; }
    // [RT / test] Số lần phải chặn NaN / Inf (voice bị tắt vì state/dữ liệu hỏng, hoặc block output phải sửa).
    uint32_t nonFiniteEvents() const noexcept [[clang::nonblocking]] { return nonFinite_; }

private:
    struct Voice {
        const Instrument* inst = nullptr;
        const AudioData*  data = nullptr;
        int32_t offBy = 0;                    // chép từ Zone lúc start (choke) → không đọc Zone sau đó
        double  pos = 0.0, inc = 1.0;
        int64_t loopStart = 0, loopEnd = 0;   // đã kiểm tra hợp lệ
        double  endPos = 0.0;                 // không loop: vượt mốc này là hết dữ liệu
        float   gainL = 0.0f, gainR = 0.0f;
        Adsr    env;
        uint32_t age = 0, generation = 0;
        int16_t note = -1;
        bool    looping = false, wrapped = false, oneShot = false, held = false;
        bool    active() const noexcept [[clang::nonblocking]] { return env.isActive(); }
    };

    Voice* allocateVoice() noexcept [[clang::nonblocking]];
    void   startVoice(Voice& v, const Zone& z, int note, int velocity) noexcept [[clang::nonblocking]];
    void   renderVoice(Voice& v, float* L, float* R, int n) noexcept [[clang::nonblocking]];

    std::array<Voice, kMaxVoices + kSpareVoices> voices_{};
    const Instrument* inst_ = nullptr;
    uint32_t gen_ = 0;
    uint32_t ageCounter_ = 0;
    uint32_t nonFinite_ = 0;
    double   sampleRate_ = 48000.0;
};

} // namespace le::dsp
