#include "core/JobSystem.h"

#include <cmath>
#include <vector>

#include "core/Engine.h"
#include "core/Json.h"
#include "le/engine_api.h"

namespace le::core {

namespace {
enum Status : int { Running = 0, Done = 1, Failed = 2 };
}

struct JobSystem::Job {
    std::int64_t id = 0;
    std::string name;
    Fn fn;
    MainDone onDone;
    bool emitEvents = true;
    Context ctx;
    std::atomic<int> status{Running};
    JobOutcome outcome;           // [worker] ghi trước khi store status (release)
    // [main] trạng thái đã báo cho Dart
    bool reported = false;
    juce::uint32 reportedMs = 0;   // lúc pump() báo xong (dọn sau kKeepReportedMs)
    float lastProgress = -1.0f;
    juce::uint32 lastProgressMs = 0;
};

class JobSystem::Runner final : public juce::ThreadPoolJob {
public:
    explicit Runner(std::shared_ptr<Job> job) : juce::ThreadPoolJob(job->name), job_(std::move(job)) {}

    JobStatus runJob() override {   // [worker]
        JobOutcome out = job_->ctx.cancelled() ? JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled") : job_->fn(job_->ctx);
        if (out.error == 0 && job_->ctx.cancelled()) out = JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
        // L1 (le-soak 80): thả lambda NGAY — nó capture shared_ptr (AudioData của take, peaks…). Giữ lại tới khi
        // engine huỷ = rò toàn bộ audio của mọi take. Chỉ worker chạm fn sau submit → không race.
        job_->fn = nullptr;
        job_->outcome = std::move(out);
        job_->ctx.progress.store(1.0f, std::memory_order_relaxed);
        job_->status.store(job_->outcome.error == 0 ? Done : Failed, std::memory_order_release);
        return jobHasFinished;
    }

private:
    std::shared_ptr<Job> job_;   // shared_ptr ở NRT là được (không phải audio thread)
};

JobSystem::JobSystem(int numThreads)
    : pool_(std::make_unique<juce::ThreadPool>(juce::ThreadPoolOptions{}.withThreadName("le-worker").withNumberOfThreads(numThreads))) {}

JobSystem::~JobSystem() {
    for (auto& [id, job] : jobs_) job->ctx.cancel.store(true, std::memory_order_relaxed);
    pool_->removeAllJobs(true, 30000);   // chờ worker xong (job tự kiểm cancel)
    pool_.reset();
}

std::int64_t JobSystem::submit(const char* name, Fn fn, MainDone onDone, bool emitEvents) {
    auto job = std::make_shared<Job>();
    job->id = nextId_++;
    job->name = name;
    job->fn = std::move(fn);
    job->onDone = std::move(onDone);
    job->emitEvents = emitEvents;
    jobs_[job->id] = job;
    pool_->addJob(new Runner(job), true);   // pool sở hữu Runner
    return job->id;
}

bool JobSystem::cancel(std::int64_t id) {
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) return false;
    it->second->ctx.cancel.store(true, std::memory_order_relaxed);
    return true;
}

bool JobSystem::exists(std::int64_t id) const { return jobs_.count(id) != 0; }

bool JobSystem::waitWorker(std::int64_t id, int timeoutMs) const {
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) return false;
    const auto until = juce::Time::getMillisecondCounterHiRes() + timeoutMs;
    while (it->second->status.load(std::memory_order_acquire) == Running) {
        if (juce::Time::getMillisecondCounterHiRes() > until) return false;
        juce::Thread::sleep(1);
    }
    return true;
}

bool JobSystem::anyRunning(const char* name) const {
    for (const auto& [id, job] : jobs_)
        if (job->name == name && job->status.load(std::memory_order_acquire) == Running) return true;
    return false;
}

Reply JobSystem::result(std::int64_t id) const {
    const auto it = jobs_.find(id);
    if (it == jobs_.end()) return Reply::fail(LE_ERR_JOB_NOT_FOUND, "no job " + std::to_string(id));
    const Job& job = *it->second;
    auto* r = new juce::DynamicObject();
    int st = job.status.load(std::memory_order_acquire);
    if (st != Running && !job.reported) st = Running;   // chỉ "xong" sau khi pump() đã áp kết quả trên main
    switch (st) {
        case Running:
            r->setProperty("status", "running");
            r->setProperty("progress", (double) job.ctx.progress.load(std::memory_order_relaxed));
            break;
        case Done:
            r->setProperty("status", "done");
            r->setProperty("result", job.outcome.result.isVoid() ? juce::var(new juce::DynamicObject()) : job.outcome.result);
            break;
        default: {
            auto* e = new juce::DynamicObject();
            e->setProperty("code", json::errorCodeName(job.outcome.error));
            e->setProperty("message", juce::String::fromUTF8(job.outcome.message.c_str()));
            r->setProperty("status", "failed");
            r->setProperty("error", juce::var(e));
        }
    }
    return Reply::ok(juce::var(r));   // job thất bại vẫn là ok:true (05 §3)
}

void JobSystem::pump() {
    // Gom event trước, phát sau: callback có thể gọi le_call (tạo job mới) → không được sửa jobs_ khi đang duyệt.
    struct Pending { std::int32_t type, a; std::int64_t id; double value; };
    std::vector<Pending> out;
    const auto now = juce::Time::getMillisecondCounter();
    for (auto& [id, job] : jobs_) {
        if (job->reported) continue;
        const int st = job->status.load(std::memory_order_acquire);
        if (st == Running) {
            const float p = job->ctx.progress.load(std::memory_order_relaxed);
            if (job->emitEvents && std::abs(p - job->lastProgress) > 1e-4f && now - job->lastProgressMs >= 100) {   // ≤ 10 lần/giây
                job->lastProgress = p;
                job->lastProgressMs = now;
                out.push_back({LE_EVT_JOB_PROGRESS, 0, id, (double) p});
            }
            continue;
        }
        job->reported = true;
        job->reportedMs = now;
        if (job->onDone) {   // [main] áp kết quả vào model trước khi báo Dart; có thể biến done thành failed
            job->onDone(job->outcome);
            job->onDone = nullptr;
        }
        const bool ok = job->outcome.error == 0;
        if (!ok && st == Done) job->status.store(Failed, std::memory_order_relaxed);
        if (!job->emitEvents) continue;
        if (ok) out.push_back({LE_EVT_JOB_DONE, 0, id, 1.0});
        else out.push_back({LE_EVT_JOB_FAILED, job->outcome.error, id, 0.0});
    }
    for (const auto& e : out) emitEvent(e.type, e.a, 0, e.id, e.value);

    // R9: job đã báo xong được giữ kKeepReportedMs cho job.result, rồi xoá (outcome + result JSON không tích mãi).
    // Luôn giữ tối đa kMaxReported job gần nhất dù chưa hết giờ.
    int reportedCount = 0;
    for (const auto& [id, job] : jobs_) reportedCount += job->reported ? 1 : 0;
    for (auto it = jobs_.begin(); it != jobs_.end();) {   // map theo id tăng dần = cũ trước
        const Job& j = *it->second;
        const bool expired = j.reported && (now - j.reportedMs > kKeepReportedMs || reportedCount > kMaxReported);
        if (expired) {
            --reportedCount;
            it = jobs_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace le::core
