// AudioClipPlayer — lõi thuật toán phát một clip audio lặp theo beat (04 §4, P1-12; warp hybrid P3-10). [RT] trừ prepare().
// 68 nối vào Track/ClipScheduler: mỗi track audio có 1 player, Transport/BlockSplitter cấp beat từng segment.
//
// Vị trí phát TÍNH TỪ BEAT, không cộng dồn qua các block:
//     beatInClip = (beat − launchBeat) mod lengthBeats
//     Re-Pitch:  srcPos = beatInClip · (60 · fileSR / originalBpm)      // BPM khác → tốc độ và cao độ đổi theo
//     Stretched: srcPos = beatInClip · (60 · stretchSR / stretchedBpm)   // buffer đã co giãn sẵn (WarpRenderer)
// Đầu mỗi segment tính lại từ segStartBeat (fmod), trong segment chỉ bước từng sample → sai số không tích luỹ.
// Tỉ lệ nguồn/đích đúng bằng 1 (cùng BPM, cùng sample rate) → đọc thẳng sample nguyên, không nội suy
// (null test ra đúng từng bit). Ngược lại → Hermite 4 điểm.
//
// Warp hybrid (04 §10): setStretched(buffer, bpm) đưa bản đã co giãn vào. Khi BPM hiện tại khớp (±0.005):
//   chờ tới RANH GIỚI BAR KẾ TIẾP của transport (beat chia hết cho beatsPerBar) → crossfade equal-power 10 ms
//   Re-Pitch → Stretched, CÙNG PHA (cả hai tính vị trí từ cùng beatInClip).
//   BPM lại đổi (không còn khớp) hoặc bản stretched bị gỡ/thay → quay về Re-Pitch NGAY, crossfade 5 ms.
//
// Chống click:
//   start()  fade-in 2 ms (sample đầu = data[0] · 1/96, khác 0) · stop() fade-out 5 ms
//   Đang fade-out thì đầu đọc chạy tiếp theo số sample (không tính lại từ beat) → TRANSPORT_STOP reset
//   beat về 0 cũng không làm nhảy về đầu clip.
//   Đổi data khi đang phát (setClip với data khác): bản cũ fade-out 5 ms SONG SONG với bản mới fade-in 5 ms,
//   cùng pha (cùng launchBeat) → không hụt tiếng. Bản cũ giữ generation cũ tới khi fade xong (03 §4.2).
//   setClip CÙNG data mà gain đổi (clip.setParams) → gain trượt tuyến tính 20 ms.
//   Điểm loop (áp dụng riêng cho từng nguồn Re-Pitch / Stretched):
//     - data còn "đuôi" sau điểm loop (take thu có lề): crossfade 2 ms đuôi → đầu clip (liền mạch thật)
//     - không có đuôi, hai đầu khớp nhau (loop thư viện cắt chuẩn): nối thẳng, không làm gì
//     - không có đuôi, hai đầu lệch: "trũng" 1 ms quanh điểm loop (fade-out cuối + fade-in đầu)
//
// An toàn RT: không cấp phát (2 "đầu đọc" cố định), chỉ đọc AudioData bất biến qua con trỏ thô + toán số.
#pragma once

#include "dsp/AudioData.h"

#include <cstdint>

namespace le::dsp {

class AudioClipPlayer {
public:
    static constexpr float  kFadeInSec = 0.002f;
    static constexpr float  kFadeOutSec = 0.005f;
    static constexpr float  kSwapFadeSec = 0.005f;
    static constexpr float  kGainRampSec = 0.020f;
    static constexpr float  kLoopXfadeSec = 0.002f;
    static constexpr float  kLoopDipSec = 0.001f;
    static constexpr float  kToStretchedSec = 0.010f;
    static constexpr float  kToRePitchSec = 0.005f;
    static constexpr double kBpmMatch = 0.005;

    // [main]
    void prepare(double sampleRate, int maxBlock);

    // [RT] Gán clip (gọi khi remap snapshot hoặc trước start()). Cùng data → cập nhật tham số và
    // generation (gain đổi thì trượt 20 ms). Khác data mà đang phát → crossfade 5 ms cùng pha, và bỏ bản
    // stretched cũ (gọi setStretched SAU setClip). data = nullptr → tắt (fade-out).
    // lengthBeats ≤ 0 → lấy theo độ dài data; originalBpm ≤ 0 → phát đúng tốc độ gốc của file.
    void setClip(const AudioData* data, double lengthBeats, double originalBpm, float gain,
                 uint32_t generation) noexcept [[clang::nonblocking]];

    // [RT] Bản đã co giãn tới stretchedBpm (P3-10). nullptr = không có. Gọi mỗi lần remap, sau setClip.
    void setStretched(const AudioData* stretched, double stretchedBpm, uint32_t generation) noexcept
        [[clang::nonblocking]];
    // [RT] Số beat mỗi bar (ranh giới chuyển sang Stretched). Mặc định 4.
    void setBeatsPerBar(int beatsPerBar) noexcept [[clang::nonblocking]];

    // [RT] Phát từ ĐẦU clip, gốc pha tại launchBeat (thường = beat đầu segment hiện tại). Fade-in 2 ms.
    void start(double launchBeat) noexcept [[clang::nonblocking]];
    // [RT] Fade-out 5 ms rồi im lặng.
    void stop() noexcept [[clang::nonblocking]];

    // [RT] CỘNG DỒN vào out[c][start .. start+n). segStartBeat = beat của sample đầu segment,
    // samplesPerBeat = của Transport. numCh = 1 → mono; ≥ 2 → kênh 0, 1 (data mono thì ra cả 2 kênh).
    void render(float* const* out, int numCh, int start, int n, double segStartBeat,
                double samplesPerBeat) noexcept [[clang::nonblocking]];

    // [RT] Cho retire theo generation: true nếu còn giữ con trỏ data HOẶC stretched của generation này.
    bool usesGeneration(uint32_t generation) const noexcept [[clang::nonblocking]];
    bool isPlaying() const noexcept [[clang::nonblocking]] { return cur_.running && !cur_.stopping; }
    bool isSilent() const noexcept [[clang::nonblocking]] { return !cur_.running && !old_.running; }
    // [RT] true khi đang phát hẳn bản stretched (không tính lúc đang crossfade).
    bool isStretched() const noexcept [[clang::nonblocking]] { return cur_.src == Source::Stretched; }
    double launchBeat() const noexcept [[clang::nonblocking]] { return cur_.launchBeat; }
    double lengthBeats() const noexcept [[clang::nonblocking]] { return cur_.lengthBeats; }

    // [RT] Điểm bắt đầu vòng kế tiếp (> fromBeat) — để BlockSplitter thêm ranh giới nếu cần.
    double nextLoopBeat(double fromBeat) const noexcept [[clang::nonblocking]];
    // [RT] Vị trí trong clip 0..1 (LeState.trackClipProgress). Không phát → 0.
    float progress(double beat) const noexcept [[clang::nonblocking]];
    // [RT / test] Số lần phải chặn NaN / Inf (head có state hỏng bị dừng, hoặc block output có mẫu hỏng → 0).
    uint32_t nonFiniteEvents() const noexcept [[clang::nonblocking]] { return nonFinite_; }

private:
    enum class Source : uint8_t { RePitch, ToStretched, Stretched, ToRePitch };

    struct Head {                       // một "đầu đọc": clip hiện tại, hoặc clip cũ đang fade-out
        const AudioData* data = nullptr;
        double   lengthBeats = 0.0, originalBpm = 0.0;
        uint32_t generation = 0;
        double   launchBeat = 0.0;
        bool     running = false, stopping = false;
        // Đầu đọc đang fade-out KHÔNG theo beat của transport nữa (transport stop reset beat về 0), mà
        // chạy tiếp từ vị trí đã phát: nextRel = beat-kể-từ-launch của sample kế tiếp.
        bool     freeRun = false;
        double   nextRel = 0.0;
        float    level = 0.0f, target = 0.0f, step = 0.0f;   // ramp fade-in / fade-out
        int32_t  rampLeft = 0;          // còn bao nhiêu sample; về 0 thì level = target CHÍNH XÁC
        float    gain = 1.0f, gainTarget = 1.0f, gainStep = 0.0f;   // gain clip, trượt 20 ms khi đổi
        int32_t  gainLeft = 0;
        // Warp hybrid
        const AudioData* stretched = nullptr;   // bản mới nhất được đưa vào (setStretched)
        double   stretchedBpm = 0.0;
        uint32_t stretchedGen = 0;
        const AudioData* activeSt = nullptr;    // bản đang phát / đang crossfade
        double   activeStBpm = 0.0;
        uint32_t activeStGen = 0;
        Source   src = Source::RePitch;
        int32_t  srcLeft = 0, srcLen = 1;       // crossfade giữa 2 nguồn
        double   pendingBeat = -1.0;            // ranh giới bar sẽ chuyển sang Stretched (< 0: không có)
    };

    void rampTo(Head& h, float target, float seconds) noexcept [[clang::nonblocking]];
    void renderHead(Head& h, float* L, float* R, int n, double segStartBeat, double spb) noexcept
        [[clang::nonblocking]];
    static double srcSamplesPerBeat(const Head& h, double spb, double engineSr) noexcept [[clang::nonblocking]];
    static double effectiveLength(const Head& h, double spb, double engineSr) noexcept [[clang::nonblocking]];
    static bool   headUses(const Head& h, uint32_t generation) noexcept [[clang::nonblocking]];

    Head cur_, old_;
    uint32_t nonFinite_ = 0;
    double sampleRate_ = 48000.0;
    int beatsPerBar_ = 4;
};

} // namespace le::dsp
