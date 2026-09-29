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

    explicit JobSystem(int numThreads = 2);
    ~JobSystem();   // huỷ mọi job còn chạy và chờ worker dừng

    std::int64_t submit(const char* name, Fn fn);   // [main]
    bool cancel(std::int64_t id);                   // [main] false nếu không có job này
    bool exists(std::int64_t id) const;             // [main]
    bool anyRunning(const char* name) const;        // [main] có job tên `name` đang chạy không
    std::string resultJson(std::int64_t id) const;  // [main] envelope JSON của job.result
    void pump();                                    // [main]

private:
    struct Job;
    class Runner;

    std::unique_ptr<juce::ThreadPool> pool_;
    std::map<std::int64_t, std::shared_ptr<Job>> jobs_;   // [main]
    std::int64_t nextId_ = 1;
};

} // namespace le::core
