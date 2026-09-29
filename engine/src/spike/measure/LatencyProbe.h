// LatencyProbe — đo round-trip latency bằng loopback (P0-07, docs/phases/P0-spike.md).
//
// Cách hoạt động:
//   [RT]     phát 5 "chirp" ngắn (quét tần số 500 Hz → 8 kHz, ~20 ms), cách nhau 500 ms,
//            đồng thời ghi input (mic) vào buffer cấp phát sẵn.
//   [worker] analyze(): với mỗi lần phát, tìm độ trễ bằng cross-correlation giữa
//            chirp đã phát và đoạn input thu được → 5 giá trị, lấy median.
//
// Luồng dùng (68 nối vào RtEngine + le_call "spike.latencyLoopback"):
//   probe.prepare(sr, maxBlock);   // [main] cấp phát
//   probe.start();                 // [main] chỉ bật cờ atomic, RT sẽ bắt đầu ở block kế tiếp
//   ... audio thread gọi processRt(...) mỗi block ...
//   while (!probe.isDone()) sleep; // [worker] hoặc poll bằng Timer
//   auto r = probe.analyze();      // [worker]
//
// An toàn thread: mỗi lượt đo có một "số thứ tự" (generation).
//   - main: start() tăng requestGen_ (atomic).
//   - RT:   thấy requestGen_ khác lượt đang chạy → bắt đầu lượt mới (pos_ = 0). Thu xong thì
//           store doneGen_ = số của lượt đó (release).
//   - worker: isDone() = (doneGen_ == requestGen_), load bằng acquire → khi thấy true thì chắc chắn
//           thấy đủ dữ liệu RT đã ghi vào buffer trước lúc store (cặp release/acquire).
// Nhờ so số thứ tự, start() gọi lúc lượt cũ vừa xong cũng không làm isDone() báo nhầm.
// Không gọi start() trong lúc worker đang analyze() (RT sẽ ghi đè buffer đang đọc).
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace le::spike {

struct LatencyResult {
    static constexpr int kRuns = 5;

    bool    ok = false;              // true nếu ≥ 3/5 lần đo tìm được tín hiệu
    int32_t measuredSamples = -1;    // median các lần hợp lệ (sample)
    double  measuredMs = 0.0;
    std::array<int32_t, kRuns> runs{};    // -1 = lần đó không tìm thấy tín hiệu
    std::array<float, kRuns>   score{};   // độ giống nhau (normalized correlation) 0..1
    int32_t validRuns = 0;
    int32_t spreadSamples = 0;       // max - min của các lần hợp lệ (DoD: ≤ 1 ms)
    float   inputPeak = 0.0f;        // biên độ lớn nhất thu được, để biết mic có nghe thấy không
};

class LatencyProbe {
public:
    struct Config {
        double intervalMs  = 500.0;  // khoảng cách giữa 2 chirp = độ trễ tối đa đo được
        double preRollMs   = 200.0;  // im lặng trước chirp đầu (device ổn định)
        double chirpMs     = 20.0;
        double chirpLoHz   = 500.0;  // loa iPad yếu ở tần số thấp
        double chirpHiHz   = 8000.0;
        float  gain        = 0.5f;   // -6 dBFS
        float  minScore    = 0.2f;   // dưới ngưỡng này coi như không thấy tín hiệu
    };

    LatencyProbe() = default;

    // [main] Cấp phát buffer thu + bảng chirp. Không gọi khi audio thread đang dùng probe.
    void prepare(double sampleRate, int maxBlock, const Config& cfg);
    void prepare(double sampleRate, int maxBlock) { prepare(sampleRate, maxBlock, Config{}); }

    // [main] Yêu cầu bắt đầu một lượt đo mới. RT nhận ở block kế tiếp.
    void start() noexcept;

    // [RT] Khi đang đo: GHI ĐÈ out[0..numCh) (chirp hoặc 0) và ghi `in` vào buffer.
    //      Khi không đo: không đụng vào out (caller tự xử lý), trả về ngay.
    //      `in` có thể là nullptr (không có mic) → ghi 0.
    void processRt(const float* in, float* const* out, int numCh, int n) noexcept [[clang::nonblocking]];

    // [any] true khi lượt đo được yêu cầu gần nhất đã thu xong.
    bool isDone() const noexcept {
        const uint32_t req = requestGen_.load(std::memory_order_acquire);
        return req != 0 && doneGen_.load(std::memory_order_acquire) == req;
    }
    // [any] true từ lúc start() tới khi thu xong.
    bool isRunning() const noexcept {
        return requestGen_.load(std::memory_order_acquire) != doneGen_.load(std::memory_order_acquire);
    }

    // [worker] Chỉ gọi sau khi isDone() == true. Trả ok=false nếu chưa xong hoặc không thấy tín hiệu.
    LatencyResult analyze() const;

    // Thông tin cho test/harness
    double  sampleRate() const noexcept { return sampleRate_; }
    int64_t totalSamples() const noexcept { return static_cast<int64_t>(recording_.size()); }
    int64_t emitStart(int run) const noexcept { return preRoll_ + static_cast<int64_t>(run) * interval_; }
    const std::vector<float>& chirp() const noexcept { return chirp_; }

    // [worker] Hàm lõi, tách ra để unit test trực tiếp: tìm lag ∈ [0, maxLag] sao cho
    // |Σ ref[k]·rec[start+lag+k]| lớn nhất. Trả lag, ghi score (normalized correlation).
    static int32_t findLag(const float* ref, int refLen, const float* rec, int64_t recLen,
                           int64_t start, int32_t maxLag, float* outScore);

private:
    Config  cfg_{};
    double  sampleRate_ = 0.0;
    int64_t preRoll_ = 0, interval_ = 0;
    int32_t maxLag_ = 0;

    std::vector<float> chirp_;      // [main] tạo trong prepare, RT chỉ đọc
    std::vector<float> recording_;  // [RT] ghi trong lúc đo, [worker] đọc sau khi done

    // [RT] chỉ audio thread đọc/ghi
    int64_t  pos_ = 0;
    uint32_t activeGen_ = 0;
    bool     active_ = false;

    std::atomic<uint32_t> requestGen_{0};  // [main] ghi
    std::atomic<uint32_t> doneGen_{0};     // [RT] ghi
};

} // namespace le::spike
