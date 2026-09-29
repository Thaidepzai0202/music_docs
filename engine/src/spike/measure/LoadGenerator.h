// LoadGenerator — N "voice giả" để đo CPU audio thread trên iPad (P0-08, docs/phases/P0-spike.md).
//
// Mỗi voice làm ĐÚNG những việc một voice sampler thật sẽ làm mỗi sample (docs/04-engine-design.md §6):
//   - đọc buffer mono 1 giây với bước phân số (pos += inc, inc = 2^(semitone/12))
//   - nội suy Hermite 4 điểm
//   - envelope ADSR (attack tuyến tính, decay/release hàm mũ, hệ số tính sẵn)
//   - nhân gain + pan rồi cộng dồn vào 2 kênh
// Hết nốt thì voice tự "bấm nốt mới" (cao độ, vị trí, pan ngẫu nhiên) để envelope luôn đi qua đủ các pha.
// Giảm số voice: voice thừa fade 3 ms (giống voice bị cướp) rồi tắt, không click.
//
// Luồng dùng (68 nối vào RtEngine, lệnh LE_CMD_SPIKE_LOAD_VOICES):
//   gen.prepare(sr, maxBlock);      // [main] cấp phát
//   gen.setVoices(64);              // [main] hoặc [RT] khi xử lý lệnh: chỉ ghi 1 atomic
//   gen.processRt(out, 2, n);       // [RT] CỘNG DỒN vào out (caller tự xoá buffer trước)
//   state.activeVoices = gen.activeVoices();
//
// An toàn RT: mọi bộ nhớ (buffer nguồn, mảng voice, bảng hệ số) cấp phát trong prepare().
// processRt chỉ đọc/ghi mảng có sẵn + toán số; số ngẫu nhiên dùng xorshift (không phải std::random).
// Giao tiếp main → RT chỉ qua std::atomic<int> targetVoices_.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace le::spike {

class LoadGenerator {
public:
    static constexpr int kMaxVoices = 128;   // khớp LE_CMD_SPIKE_LOAD_VOICES (0..128)

    // [main] Cấp phát và tạo buffer nguồn. seed cố định → output lặp lại được (test).
    void prepare(double sampleRate, int maxBlock, uint32_t seed = 0x1234567u);

    // [any] Đặt số voice mong muốn, tự kẹp vào [0, kMaxVoices]. RT áp dụng ở block kế tiếp.
    //       Gọi được từ audio thread (RtEngine::drainCommands): chỉ 1 atomic store.
    void setVoices(int n) noexcept [[clang::nonblocking]];
    int  targetVoices() const noexcept [[clang::nonblocking]] { return targetVoices_.load(std::memory_order_relaxed); }

    // [any] Số voice đang kêu (kể cả voice đang fade), cập nhật cuối mỗi block.
    int  activeVoices() const noexcept [[clang::nonblocking]] { return activeVoices_.load(std::memory_order_relaxed); }

    // [RT] Cộng dồn tiếng của các voice vào out. numCh = 1 → chỉ kênh trái; ≥ 2 → trái/phải vào out[0], out[1].
    void processRt(float* const* out, int numCh, int n) noexcept [[clang::nonblocking]];

private:
    enum class Stage : uint8_t { Off, Attack, Decay, Sustain, Release };

    struct Voice {
        double  pos = 0.0, inc = 1.0;
        float   level = 0.0f;         // mức envelope hiện tại
        float   gainL = 0.0f, gainR = 0.0f;
        int32_t samplesToRelease = 0; // còn bao nhiêu sample thì nhả phím
        Stage   stage = Stage::Off;
        bool    fastRelease = false;  // bị tắt bớt → fade 3 ms rồi dừng hẳn
    };

    void  noteOn(Voice& v) noexcept [[clang::nonblocking]];
    float nextRandom01() noexcept [[clang::nonblocking]];

    double sampleRate_ = 48000.0;
    int    length_ = 0;                // số sample của buffer nguồn (1 giây)
    std::vector<float> source_;        // length_ + 3: 1 sample đệm đầu, 2 sample đệm cuối cho Hermite

    std::array<Voice, kMaxVoices> voices_{};

    // Hệ số tính sẵn trong prepare()
    float attackStep_ = 0.0f;          // attack tuyến tính 5 ms: level += attackStep_
    float decayCoef_ = 0.0f;           // decay 200 ms về sustain
    float releaseCoef_ = 0.0f;         // release 300 ms
    float fastReleaseCoef_ = 0.0f;     // 3 ms
    float sustain_ = 0.7f;
    float voiceGain_ = 0.0f;
    std::array<double, 25> pitchRatio_{};   // -12..+12 nửa cung
    std::array<float, 16>  panL_{}, panR_{};

    uint32_t rng_ = 1;                 // [RT] trạng thái xorshift32

    std::atomic<int> targetVoices_{0};
    std::atomic<int> activeVoices_{0};
};

} // namespace le::spike
