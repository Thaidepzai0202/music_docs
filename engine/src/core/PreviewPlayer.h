#pragma once
// PreviewPlayer (05 §3 preview.play / preview.stop) [RT] trừ prepare(): kênh nghe thử của Browser, NGOÀI 8 track.
// Một Sampler riêng (kit → groove, nhạc cụ → arpeggio) + một đầu đọc audio (loop), đồng hồ riêng (không dùng
// transport). RtEngine trộn vào master SAU master gain / EQ và TRƯỚC limiter, gain kGain (≈ −6 dB).
//
// Ownership (vì sao an toàn): main tạo VÉ {id, instrument / audio, lịch nốt} và giữ vé cùng dữ liệu (shared_ptr)
// SỐNG cho tới khi RT báo PreviewReleased(id). Trao vé bằng atomic exchange: RT lấy vé mới (bản cũ fade), main thu
// lại vé RT chưa lấy. RT chỉ báo nhả khi không còn voice / đầu đọc nào dùng dữ liệu của vé đó.
#include <atomic>
#include <cstdint>
#include <memory>

#include "core/Mixer.h"
#include "core/RtQueues.h"
#include "dsp/AudioData.h"
#include "dsp/Instrument.h"
#include "dsp/Sampler.h"

namespace le::core {

class PreviewPlayer {
public:
    static constexpr float kGain = 0.5f;          // ≈ −6 dB
    static constexpr int kMaxEvents = 32;
    static constexpr int kMaxRetiring = 8;
    static constexpr double kFadeInSec = 0.002, kFadeOutSec = 0.010;

    struct Event {
        double sec = 0.0;         // tính từ đầu lượt nghe (RT đổi ra frame theo SR lúc phát)
        std::uint8_t note = 60;
        float velocity = 0.8f;
        bool on = true;
    };
    struct Ticket {
        std::uint32_t id = 0;                      // ≥ 1, cũng là generation của Sampler cho lượt này
        const dsp::Instrument* instrument = nullptr;
        const dsp::AudioData* audio = nullptr;
        Event events[kMaxEvents];                  // sắp theo sec; mỗi note-on có note-off tương ứng
        int numEvents = 0;
        double lengthSec = 0.0;                    // audio: fade-out xong tại đây; sampler: hết lịch (đuôi release vẫn kêu)
    };

    // [main, audio chưa chạy]
    void prepare(double sampleRate, int maxBlock);
    // [main] Đưa vé mới; trả vé cũ RT CHƯA lấy (main nhả được ngay).
    Ticket* offer(Ticket* t) noexcept { return offer_.exchange(t, std::memory_order_acq_rel); }
    // [main] preview.stop: dừng lượt đang nghe (fade / nhả nốt).
    void requestStop() noexcept { stopSeq_.fetch_add(1, std::memory_order_acq_rel); }

    // [RT] CỘNG vào L/R (đã nhân kGain). Báo PreviewReleased qua out.
    void process(float* L, float* R, int n, RtToNrtQueue& out) noexcept [[clang::nonblocking]];

    bool active() const noexcept [[clang::nonblocking]] { return cur_.id != 0; }   // [RT / test]

private:
    struct Head {                  // đầu đọc audio
        const dsp::AudioData* data = nullptr;
        double pos = 0.0, step = 1.0;
        LinearRamp gain;
    };
    struct Live {
        std::uint32_t id = 0;
        bool sampler = false;
        Head head;
        const Event* events = nullptr;   // trỏ vào vé (main giữ sống)
        int numEvents = 0, nextEvent = 0;
        std::int64_t pos = 0, length = 0;
    };
    struct Retiring {
        std::uint32_t id = 0;
        bool sampler = false;
        bool done = false;               // hết người dùng, chờ push PreviewReleased (queue đầy → thử lại block sau)
        Head head;                       // audio đang fade out
    };

    void start(const Ticket& t) noexcept [[clang::nonblocking]];
    bool retireCurrent(bool fast) noexcept [[clang::nonblocking]];   // false = danh sách retire đầy
    void renderSampler(float* L, float* R, int n) noexcept [[clang::nonblocking]];
    std::int64_t frameOf(double sec) const noexcept [[clang::nonblocking]];
    static void readHead(Head& h, float* L, float* R, int n) noexcept [[clang::nonblocking]];

    dsp::Sampler sampler_;
    std::atomic<Ticket*> offer_{nullptr};
    std::atomic<std::uint32_t> stopSeq_{0};
    std::uint32_t seenStop_ = 0;
    Live cur_;
    Retiring retiring_[kMaxRetiring];
    int numRetiring_ = 0;
    bool samplerBusy_ = false;           // còn voice (cập nhật sau mỗi lần render) → nhánh rảnh chỉ tốn O(1)
    double sampleRate_ = 48000.0;
    int fadeIn_ = 96, fadeOut_ = 480;
    std::unique_ptr<float[]> scratch_;   // [L | R] maxBlock
    int maxBlock_ = 0;
};

} // namespace le::core
