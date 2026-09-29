// P1-05: JobSystem — job giả hoàn thành và phát đúng thứ tự event (JOB_PROGRESS… → JOB_DONE/FAILED).
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <juce_core/juce_core.h>

#include "core/CommandProcessor.h"
#include "core/Engine.h"
#include "core/JobSystem.h"

using namespace le::core;

namespace {
struct Ev {
    int32_t type, a;
    int64_t job;
    double value;
};
std::vector<Ev>& events() {
    static std::vector<Ev> v;
    return v;
}
void onEvent(int32_t type, int32_t a, int32_t, int64_t job, double value) { events().push_back({type, a, job, value}); }

// Pump tới khi job được báo xong (hoặc hết giờ). [main]
void pumpUntilReported(JobSystem& js, int64_t id, int maxMs = 5000) {
    for (int t = 0; t < maxMs; t += 10) {
        js.pump();
        for (const auto& e : events())
            if (e.job == id && (e.type == LE_EVT_JOB_DONE || e.type == LE_EVT_JOB_FAILED)) return;
        juce::Thread::sleep(10);
    }
}

struct CallbackScope {
    CallbackScope() {
        events().clear();
        setEventCallback(&onEvent);
    }
    ~CallbackScope() { setEventCallback(nullptr); }
};
} // namespace

TEST_CASE("JobSystem: job giả báo tiến độ tăng dần rồi đúng 1 JOB_DONE", "[core][job]") {
    CallbackScope cb;
    JobSystem js(2);
    const int64_t id = js.submit("fake", [](JobSystem::Context& ctx) {
        for (int i = 1; i <= 4; ++i) {
            juce::Thread::sleep(120);
            ctx.progress.store(0.25f * (float) i, std::memory_order_relaxed);
        }
        auto* o = new juce::DynamicObject();
        o->setProperty("x", 42);
        return JobOutcome::ok(juce::var(o));
    });
    REQUIRE(id > 0);
    REQUIRE(js.anyRunning("fake"));
    REQUIRE(js.result(id).result["status"].toString() == "running");

    pumpUntilReported(js, id);
    for (int i = 0; i < 10; ++i) js.pump();   // pump thêm: không được phát thêm gì

    int progress = 0, done = 0;
    double last = -1.0;
    for (size_t i = 0; i < events().size(); ++i) {
        const auto& e = events()[i];
        REQUIRE(e.job == id);
        if (e.type == LE_EVT_JOB_PROGRESS) {
            REQUIRE(done == 0);            // không có PROGRESS sau DONE
            REQUIRE(e.value >= last);      // tăng dần
            last = e.value;
            ++progress;
        } else {
            REQUIRE(e.type == LE_EVT_JOB_DONE);
            REQUIRE(i == events().size() - 1);   // DONE là event cuối
            ++done;
        }
    }
    REQUIRE(progress >= 1);
    REQUIRE(done == 1);

    const Reply r = js.result(id);
    REQUIRE(r.error == LE_OK);
    REQUIRE(r.result["status"].toString() == "done");
    REQUIRE((int) r.result["result"]["x"] == 42);
    REQUIRE_FALSE(js.anyRunning("fake"));
}

TEST_CASE("JobSystem: job lỗi → JOB_FAILED(a = mã lỗi), job.result failed vẫn ok", "[core][job]") {
    CallbackScope cb;
    JobSystem js(2);
    const int64_t id = js.submit("fail", [](JobSystem::Context&) { return JobOutcome::fail(LE_ERR_FILE_NOT_FOUND, "no such file"); });
    pumpUntilReported(js, id);
    REQUIRE(events().size() >= 1);
    REQUIRE(events().back().type == LE_EVT_JOB_FAILED);
    REQUIRE(events().back().a == LE_ERR_FILE_NOT_FOUND);

    const Reply r = js.result(id);
    REQUIRE(r.error == LE_OK);
    REQUIRE(r.result["status"].toString() == "failed");
    REQUIRE(r.result["error"]["code"].toString() == "FILE_NOT_FOUND");
    REQUIRE(r.result["error"]["message"].toString() == "no such file");
}

TEST_CASE("JobSystem: cancel → JOB_FAILED JOB_CANCELLED; jobId lạ → JOB_NOT_FOUND", "[core][job]") {
    CallbackScope cb;
    JobSystem js(2);
    const int64_t id = js.submit("loop", [](JobSystem::Context& ctx) {
        while (!ctx.cancelled()) juce::Thread::sleep(5);
        return JobOutcome::ok({});   // runner tự đổi thành JOB_CANCELLED
    });
    REQUIRE(js.cancel(id));
    pumpUntilReported(js, id);
    REQUIRE(events().back().type == LE_EVT_JOB_FAILED);
    REQUIRE(events().back().a == LE_ERR_JOB_CANCELLED);
    REQUIRE(js.result(id).result["error"]["code"].toString() == "JOB_CANCELLED");

    REQUIRE_FALSE(js.cancel(999));
    REQUIRE(js.result(999).error == LE_ERR_JOB_NOT_FOUND);
}

TEST_CASE("JobSystem: jobId tăng dần, nhiều job song song đều xong", "[core][job]") {
    CallbackScope cb;
    JobSystem js(2);
    std::vector<int64_t> ids;
    for (int i = 0; i < 6; ++i)
        ids.push_back(js.submit("n", [i](JobSystem::Context&) { juce::Thread::sleep(20 + 5 * i); return JobOutcome::ok(juce::var(i)); }));
    for (size_t i = 1; i < ids.size(); ++i) REQUIRE(ids[i] == ids[i - 1] + 1);
    for (auto id : ids) pumpUntilReported(js, id);
    int done = 0;
    for (const auto& e : events()) done += e.type == LE_EVT_JOB_DONE ? 1 : 0;
    REQUIRE(done == 6);
}

TEST_CASE("JobSystem: huỷ JobSystem khi job còn chạy → dừng sạch", "[core][job]") {
    auto js = std::make_unique<JobSystem>(2);
    js->submit("slow", [](JobSystem::Context& ctx) {
        for (int i = 0; i < 1000 && !ctx.cancelled(); ++i) juce::Thread::sleep(5);
        return JobOutcome::ok({});
    });
    juce::Thread::sleep(20);
    js.reset();   // destructor bật cancel + chờ worker
    SUCCEED();
}

TEST_CASE("CommandProcessor: dispatch, envelope, lỗi parse", "[core][cmd]") {
    CommandProcessor cp;
    cp.add("echo", [](const juce::var& req) { return Reply::ok(req.getProperty("v", {})); });
    cp.add("boom", [](const juce::var&) { return Reply::fail(LE_ERR_FILE_FORMAT, "bad"); });

    auto parse = [](const std::string& s) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(s.c_str()), v);
        return v;
    };
    REQUIRE((int) parse(cp.call(R"({"op":"echo","v":7})"))["result"] == 7);
    const auto e = parse(cp.call(R"({"op":"boom"})"));
    REQUIRE_FALSE((bool) e["ok"]);
    REQUIRE(e["error"]["code"].toString() == "FILE_FORMAT");
    REQUIRE(parse(cp.call(R"({"op":"nope"})"))["error"]["code"].toString() == "NOT_IMPLEMENTED");
    REQUIRE(parse(cp.call("{"))["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(parse(cp.call(R"({"op":5})"))["error"]["code"].toString() == "INVALID_ARG");
    REQUIRE(parse(cp.call(nullptr))["error"]["code"].toString() == "INVALID_ARG");
    // echo không có result → {} chứ không phải null
    REQUIRE(parse(cp.call(R"({"op":"echo"})"))["result"].isObject());

    int i = 0;
    double d = 0;
    REQUIRE(args::getInt(parse(R"({"a":3})"), "a", i, 0, 7));
    REQUIRE_FALSE(args::getInt(parse(R"({"a":3.5})"), "a", i, 0, 7));
    REQUIRE_FALSE(args::getInt(parse(R"({"a":8})"), "a", i, 0, 7));
    REQUIRE_FALSE(args::getInt(parse(R"({"a":"3"})"), "a", i, 0, 7));
    REQUIRE(args::getDouble(parse(R"({"a":1.5})"), "a", d, 0, 2));
}

TEST_CASE("L1: job xong → lambda (và shared_ptr nó capture) được thả, không đợi tới khi huỷ JobSystem", "[core][job]") {
    JobSystem js(1);
    auto data = std::make_shared<std::vector<float>>(1'000'000, 1.0f);   // như AudioData của một take
    std::weak_ptr<std::vector<float>> w = data;
    auto doneData = std::make_shared<int>(7);
    std::weak_ptr<int> wd = doneData;
    const auto id = js.submit("persist", [data](JobSystem::Context&) { return JobOutcome::ok(juce::var((int) data->size())); },
                              [doneData](JobOutcome&) { (void) doneData; }, false);
    data.reset();
    doneData.reset();
    for (int i = 0; i < 400 && !w.expired(); ++i) juce::Thread::sleep(5);
    REQUIRE(w.expired());   // worker đã thả fn ngay khi chạy xong
    for (int i = 0; i < 400 && !wd.expired(); ++i) {
        js.pump();
        juce::Thread::sleep(5);
    }
    REQUIRE(wd.expired());   // onDone chạy xong ở pump rồi bị thả
    REQUIRE(js.exists(id));  // job.result vẫn trả được trong kKeepReportedMs
}

TEST_CASE("R9: giữ tối đa kMaxReported job đã báo xong", "[core][job]") {
    JobSystem js(2);
    std::int64_t first = 0, last = 0;
    for (int i = 0; i < JobSystem::kMaxReported + 50; ++i) {
        last = js.submit("x", [](JobSystem::Context&) { return JobOutcome::ok({}); }, {}, false);
        if (i == 0) first = last;
    }
    for (int i = 0; i < 400; ++i) {
        js.pump();
        juce::Thread::sleep(2);
        if (!js.anyRunning("x")) break;
    }
    js.pump();
    REQUIRE_FALSE(js.exists(first));   // cũ nhất bị dọn
    REQUIRE(js.exists(last));
}
