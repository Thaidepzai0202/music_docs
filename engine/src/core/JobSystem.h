#pragma once
// JobSystem (04 §15), bản tối thiểu cho P0 (spike.latencyLoopback, spike.stretchBench). P1-05 hoàn thiện.
// - submit() [main]: trả jobId ngay, việc chạy trên juce::ThreadPool(2).
// - Job [worker]: chỉ đọc/ghi Job::Context (progress, cancel) và trả về JobOutcome. Không chạm RtState.
// - pump() [main, Timer 30Hz]: phát JOB_PROGRESS (≤ 10 lần/giây), JOB_DONE / JOB_FAILED.
// - resultJson() [main]: job.result theo định dạng đã chốt ở 05 §3.
//
// An toàn thread: kết quả (outcome) do worker ghi TRƯỚC khi store status = Done/Failed (release);
// main load status bằng acquire rồi mới đọc outcome → luôn thấy kết quả đầy đủ. Sau đó worker không ghi gì nữa.
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include <juce_core/juce_core.h>

#include "core/CommandProcessor.h"

namespace le::core {

struct JobOutcome {
    std::int32_t error = 0;    // LeError, 0 = thành công
    std::string message;
    juce::var result;          // khi thành công

    static JobOutcome ok(juce::var r) { return {0, {}, std::move(r)}; }
    static JobOutcome fail(std::int32_t err, std::string msg) { return {err, std::move(msg), {}}; }
};

class JobSystem {
public:
    struct Context {
        std::atomic<float> progress{0.0f};
        std::atomic<bool> cancel{false};
        bool cancelled() const noexcept { return cancel.load(std::memory_order_relaxed); }
    };
    using Fn = std::function<JobOutcome(Context&)>;   // chạy trên worker (NRT: std::function được phép)
    // [main] Chạy trong pump() khi job xong, TRƯỚC khi phát JOB_DONE/JOB_FAILED. Dùng để áp kết quả vào model
    // (tạo snapshot mới) để khi Dart nhận JOB_DONE thì engine đã dùng kết quả đó. Có thể đổi outcome.
    using MainDone = std::function<void(JobOutcome&)>;

    explicit JobSystem(int numThreads = 2);
    ~JobSystem();   // huỷ mọi job còn chạy và chờ worker dừng

    // emitEvents = false: job nội bộ của engine (VD ghi file take) → không phát JOB_* cho Dart.
    std::int64_t submit(const char* name, Fn fn, MainDone onDone = {}, bool emitEvents = true);   // [main]
    bool cancel(std::int64_t id);                   // [main] false nếu không có job này
    bool exists(std::int64_t id) const;             // [main]
    bool anyRunning(const char* name) const;        // [main] có job tên `name` đang chạy không
    // [main] Chờ worker chạy xong job (KHÔNG chạy onDone — việc đó vẫn ở pump()). Chỉ dùng khi render offline
    // (test / sim) để kết quả tất định. false nếu hết giờ hoặc không có job.
    bool waitWorker(std::int64_t id, int timeoutMs) const;
    Reply result(std::int64_t id) const;            // [main] job.result (05 §3); jobId lạ → JOB_NOT_FOUND
    void pump();                                    // [main]

    static constexpr juce::uint32 kKeepReportedMs = 60000;   // job.result còn trả được 60 s sau JOB_DONE
    static constexpr int kMaxReported = 256;

private:
    struct Job;
    class Runner;

    std::unique_ptr<juce::ThreadPool> pool_;
    std::map<std::int64_t, std::shared_ptr<Job>> jobs_;   // [main]
    std::int64_t nextId_ = 1;
};

} // namespace le::core
