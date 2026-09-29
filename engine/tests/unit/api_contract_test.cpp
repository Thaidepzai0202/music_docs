// Test hợp đồng FFI (05 §5): layout struct khớp với bindings Dart, envelope JSON, mã lỗi.
// Không mở thiết bị audio: chỉ le_create / le_call / le_send / le_destroy.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <juce_core/juce_core.h>

#include "le/engine_api.h"

// ── Layout: khoá đúng các giá trị mà test Dart (sizeOf / offset) của engine_ffi cũng kiểm ──
static_assert(sizeof(LeCommand) == 32);
static_assert(offsetof(LeCommand, type) == 0);
static_assert(offsetof(LeCommand, track) == 2);
static_assert(offsetof(LeCommand, slot) == 3);
static_assert(offsetof(LeCommand, i0) == 4);
static_assert(offsetof(LeCommand, f0) == 8);
static_assert(offsetof(LeCommand, f1) == 12);
static_assert(offsetof(LeCommand, d0) == 16);
static_assert(offsetof(LeCommand, hostTimeNs) == 24);

static_assert(sizeof(LeState) == 248);
static_assert(offsetof(LeState, beat) == 8);
static_assert(offsetof(LeState, bpm) == 16);
static_assert(offsetof(LeState, sampleRate) == 24);
static_assert(offsetof(LeState, bufferSize) == 32);
static_assert(offsetof(LeState, quantize) == 40);
static_assert(offsetof(LeState, cpuLoad) == 48);
static_assert(offsetof(LeState, xrunCount) == 56);
static_assert(offsetof(LeState, inputPeak) == 64);
static_assert(offsetof(LeState, masterPeak) == 68);
static_assert(offsetof(LeState, trackPeak) == 76);
static_assert(offsetof(LeState, clipState) == 140);
static_assert(offsetof(LeState, trackPlayingSlot) == 204);
static_assert(offsetof(LeState, trackClipProgress) == 212);

static_assert(sizeof(LeConfig) == 40);

namespace {

LeConfig defaultConfig() {
    LeConfig c{};
    c.apiVersion = LE_API_VERSION;
    c.preferredBufferSize = 128;
    c.preferredSampleRate = 48000.0;
    c.numInputChannels = 1;
    return c;
}

// Gọi le_call, parse kết quả, giải phóng chuỗi.
juce::var call(const std::string& json) {
    char* res = le_call(json.c_str());
    REQUIRE(res != nullptr);
    juce::var v;
    const auto r = juce::JSON::parse(juce::String::fromUTF8(res), v);
    le_free_string(res);
    REQUIRE(r.wasOk());
    return v;
}

std::string errorCode(const juce::var& v) { return v["error"]["code"].toString().toStdString(); }

LeCommand cmd(uint16_t type, int32_t i0 = 0, float f0 = 0, float f1 = 0) {
    LeCommand c{};
    c.type = type;
    c.track = -1;
    c.slot = -1;
    c.i0 = i0;
    c.f0 = f0;
    c.f1 = f1;
    return c;
}

// RAII: mỗi test tự tạo và huỷ engine.
struct EngineScope {
    EngineScope() { REQUIRE(le_create(&cfg) == LE_OK); }
    ~EngineScope() { le_destroy(); }
    LeConfig cfg = defaultConfig();
};

} // namespace

TEST_CASE("le_api_version trả LE_API_VERSION", "[api]") {
    REQUIRE(le_api_version() == 1);
    REQUIRE(le_api_version() == LE_API_VERSION);
}

TEST_CASE("le_create kiểm tra tham số", "[api]") {
    REQUIRE(le_create(nullptr) == LE_ERR_INVALID_ARG);

    auto bad = defaultConfig();
    bad.apiVersion = LE_API_VERSION + 1;
    REQUIRE(le_create(&bad) == LE_ERR_API_VERSION);

    bad = defaultConfig();
    bad.numInputChannels = 3;
    REQUIRE(le_create(&bad) == LE_ERR_INVALID_ARG);

    auto ok = defaultConfig();
    REQUIRE(le_create(&ok) == LE_OK);
    REQUIRE(le_create(&ok) == LE_ERR_ALREADY_CREATED);
    le_destroy();
    le_destroy();   // gọi lần 2 không sao
    REQUIRE(le_create(&ok) == LE_OK);   // tạo lại được sau khi huỷ
    le_destroy();
}

TEST_CASE("Gọi API khi chưa le_create", "[api]") {
    REQUIRE(le_audio_start() == LE_ERR_NOT_CREATED);
    le_audio_stop();
    auto c = cmd(LE_CMD_SPIKE_SINE, 0, 440.0f, 0.1f);
    REQUIRE_FALSE(le_send(&c));
    const auto v = call(R"({"op":"engine.info"})");
    REQUIRE_FALSE((bool) v["ok"]);
    REQUIRE(errorCode(v) == "NOT_CREATED");
    LeState s{};
    le_read_state(&s);   // không crash
    le_read_state(nullptr);
}

TEST_CASE("le_call: envelope lỗi", "[api]") {
    EngineScope e;

    SECTION("JSON hỏng → INVALID_ARG") {
        const auto v = call("{not json");
        REQUIRE_FALSE((bool) v["ok"]);
        REQUIRE(errorCode(v) == "INVALID_ARG");
        REQUIRE(v["error"]["message"].toString().isNotEmpty());
    }
    SECTION("thiếu op → INVALID_ARG") {
        REQUIRE(errorCode(call(R"({"x":1})")) == "INVALID_ARG");
        REQUIRE(errorCode(call(R"([1,2])")) == "INVALID_ARG");
    }
    SECTION("request null → INVALID_ARG") {
        char* res = le_call(nullptr);
        juce::var v;
        REQUIRE(juce::JSON::parse(juce::String::fromUTF8(res), v).wasOk());
        le_free_string(res);
        REQUIRE(errorCode(v) == "INVALID_ARG");
    }
    SECTION("job.* với jobId sai") {
        REQUIRE(errorCode(call(R"({"op":"job.result"})")) == "INVALID_ARG");
        REQUIRE(errorCode(call(R"({"op":"job.result","jobId":424242})")) == "JOB_NOT_FOUND");
        REQUIRE(errorCode(call(R"({"op":"job.cancel","jobId":424242})")) == "JOB_NOT_FOUND");
    }
    SECTION("op chưa làm → NOT_IMPLEMENTED") {
        for (const char* op : {"project.open", "clip.setAudio", "track.setInstrument", "fx.set", "no.such.op"}) {
            const auto v = call(std::string(R"({"op":")") + op + R"("})");
            REQUIRE_FALSE((bool) v["ok"]);
            REQUIRE(errorCode(v) == "NOT_IMPLEMENTED");
        }
    }
}

TEST_CASE("le_call engine.info trả kích thước struct", "[api]") {
    EngineScope e;
    const auto v = call(R"({"op":"engine.info"})");
    REQUIRE((bool) v["ok"]);
    const auto& r = v["result"];
    REQUIRE((int) r["apiVersion"] == LE_API_VERSION);
    REQUIRE((int) r["stateSize"] == (int) sizeof(LeState));
    REQUIRE((int) r["commandSize"] == (int) sizeof(LeCommand));
    REQUIRE((int) r["configSize"] == (int) sizeof(LeConfig));
    REQUIRE((int) r["bufferSize"] == 128);
    REQUIRE((double) r["sampleRate"] == 48000.0);
    REQUIRE((int) r["inputChannels"] == 1);
    REQUIRE_FALSE((bool) r["running"]);
}

TEST_CASE("le_call spike.setBufferSize", "[api][spike]") {
    EngineScope e;
    REQUIRE(errorCode(call(R"({"op":"spike.setBufferSize","frames":100})")) == "INVALID_ARG");
    REQUIRE(errorCode(call(R"({"op":"spike.setBufferSize"})")) == "INVALID_ARG");

    const auto v = call(R"({"op":"spike.setBufferSize","frames":256})");   // audio chưa chạy: chỉ lưu
    REQUIRE((bool) v["ok"]);
    REQUIRE((int) v["result"]["bufferSize"] == 256);
    REQUIRE((int) call(R"({"op":"engine.info"})")["result"]["bufferSize"] == 256);
}

TEST_CASE("le_call spike.setSessionMode", "[api][spike]") {
    EngineScope e;
    REQUIRE(errorCode(call(R"({"op":"spike.setSessionMode","mode":"loud"})")) == "INVALID_ARG");
    const auto v = call(R"({"op":"spike.setSessionMode","mode":"measurement"})");
    REQUIRE((bool) v["ok"]);
    REQUIRE(v["result"]["mode"].toString() == "measurement");
    REQUIRE(call(R"({"op":"engine.info"})")["result"]["sessionMode"].toString() == "measurement");
    REQUIRE((bool) call(R"({"op":"spike.sessionInfo"})")["ok"]);
}

TEST_CASE("le_send: validate và queue đầy", "[api]") {
    EngineScope e;
    REQUIRE_FALSE(le_send(nullptr));

    auto unknown = cmd(777);
    REQUIRE_FALSE(le_send(&unknown));

    auto badTrack = cmd(LE_CMD_SPIKE_SINE, 0, 440.0f, 0.1f);
    badTrack.track = 8;
    REQUIRE_FALSE(le_send(&badTrack));

    auto notYet = cmd(LE_CMD_TRANSPORT_PLAY);   // P1-04
    REQUIRE_FALSE(le_send(&notYet));

    // Audio chưa chạy → không ai pop: đúng 1024 lệnh vào được, lệnh thứ 1025 bị từ chối.
    auto sine = cmd(LE_CMD_SPIKE_SINE, 0, 440.0f, 0.1f);
    int accepted = 0;
    for (int i = 0; i < 1100; ++i) accepted += le_send(&sine) ? 1 : 0;
    REQUIRE(accepted == 1024);
    REQUIRE_FALSE(le_send(&sine));
    REQUIRE((int) call(R"({"op":"engine.info"})")["result"]["rejectedCommands"] > 0);
}

TEST_CASE("le_get_peaks chưa làm", "[api]") {
    float buf[4];
    REQUIRE(le_get_peaks("c_1", 0, buf, 2) == LE_ERR_NOT_IMPLEMENTED);
}

namespace {
// Cho message loop của JUCE chạy (Timer 30Hz phát event) — giống main run loop của app.
void pumpMainLoop(double seconds) {
#if defined(__APPLE__)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
#else
    (void) seconds;
#endif
}

struct EventLog {
    struct E { int32_t type, a, b; int64_t job; double value; };
    static std::vector<E>& events() { static std::vector<E> v; return v; }
    static void cb(int32_t type, int32_t a, int32_t b, int64_t job, double value) { events().push_back({type, a, b, job, value}); }
};
} // namespace

TEST_CASE("spike.latencyLoopback cần audio đang chạy", "[api][spike]") {
    EngineScope e;
    const auto v = call(R"({"op":"spike.latencyLoopback"})");
    REQUIRE_FALSE((bool) v["ok"]);
    REQUIRE(errorCode(v) == "AUDIO_DEVICE");
}

TEST_CASE("spike.stretchBench: kiểm tham số", "[api][spike]") {
    EngineScope e;
    REQUIRE(errorCode(call(R"({"op":"spike.stretchBench"})")) == "INVALID_ARG");
    REQUIRE(errorCode(call(R"({"op":"spike.stretchBench","semitones":[]})")) == "INVALID_ARG");
    REQUIRE(errorCode(call(R"({"op":"spike.stretchBench","semitones":[-18,0.5]})")) == "INVALID_ARG");
    REQUIRE(errorCode(call(R"({"op":"spike.stretchBench","semitones":[48]})")) == "INVALID_ARG");
    REQUIRE(errorCode(call(R"({"op":"spike.stretchBench","semitones":[0],"saveDir":"relative/dir"})")) == "INVALID_ARG");
}

TEST_CASE("spike.stretchBench chưa có bản thu → job failed INVALID_ARG + JOB_FAILED", "[api][spike][job]") {
    EventLog::events().clear();
    le_set_event_callback(&EventLog::cb);
    {
        EngineScope e;
        const auto v = call(R"({"op":"spike.stretchBench","semitones":[-12,0,12],"formant":true})");
        REQUIRE((bool) v["ok"]);   // job được tạo, thất bại báo qua job.result
        const auto jobId = (int64_t) (juce::int64) v["result"]["jobId"];
        REQUIRE(jobId > 0);

        juce::var res;
        for (int i = 0; i < 100; ++i) {   // ≤ 5 giây
            res = call(R"({"op":"job.result","jobId":)" + std::to_string(jobId) + "}");
            if (res["result"]["status"].toString() != "running") break;
            pumpMainLoop(0.05);
        }
        REQUIRE((bool) res["ok"]);
        REQUIRE(res["result"]["status"].toString() == "failed");
        REQUIRE(res["result"]["error"]["code"].toString() == "INVALID_ARG");
        REQUIRE(res["result"]["error"]["message"].toString() == juce::String::fromUTF8("chưa có bản thu"));

        bool gotFailed = false;
        for (int i = 0; i < 40 && !gotFailed; ++i) {
            pumpMainLoop(0.05);
            for (const auto& ev : EventLog::events())
                if (ev.type == LE_EVT_JOB_FAILED && ev.job == jobId && ev.a == LE_ERR_INVALID_ARG) gotFailed = true;
        }
        REQUIRE(gotFailed);
    }
    le_set_event_callback(nullptr);
}
