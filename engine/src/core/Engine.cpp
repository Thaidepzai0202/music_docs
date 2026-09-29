#include "core/Engine.h"

#include <algorithm>
#include <atomic>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include "core/Json.h"
#include "io/AudioSession.h"
#include "io/JuceDeviceIO.h"
#include "spike/measure/StretchBench.h"

namespace le::core {

// ─────────────────────────── toàn cục ───────────────────────────

StatePublisher& globalStatePublisher() {
    static StatePublisher p;
    return p;
}

namespace {
std::atomic<LeEventCallback> g_eventCallback{nullptr};

constexpr int kRecordSeconds = 10;
constexpr int kRecordMaxRate = 96000;   // cấp phát đủ cho 10 giây ở 96 kHz (3.8 MB)

bool isSpikeCommand(std::uint16_t t) {
    return t == LE_CMD_SPIKE_SINE || t == LE_CMD_SPIKE_LOAD_VOICES || t == LE_CMD_SPIKE_RECORD
        || t == LE_CMD_SPIKE_PLAY_RECORD || t == LE_CMD_SPIKE_PASSTHROUGH;
}
} // namespace

void setEventCallback(LeEventCallback cb) { g_eventCallback.store(cb, std::memory_order_release); }

void emitEvent(std::int32_t type, std::int32_t a, std::int32_t b, std::int64_t jobId, double value) {
    if (auto cb = g_eventCallback.load(std::memory_order_acquire)) cb(type, a, b, jobId, value);
}

// ─────────────────────────── EventPump ───────────────────────────

// [main] juce::Timer chạy trên message thread (= main thread của iOS / harness).
class EventPump final : public juce::Timer {
public:
    explicit EventPump(Engine& e) : engine_(e) { startTimerHz(30); }
    ~EventPump() override { stopTimer(); }
    void timerCallback() override { engine_.pump(); }

private:
    Engine& engine_;
};

// ─────────────────────────── Engine ───────────────────────────

std::int32_t Engine::validate(const LeConfig* cfg) {
    if (cfg == nullptr) return LE_ERR_INVALID_ARG;
    if (cfg->apiVersion != LE_API_VERSION) return LE_ERR_API_VERSION;
    if (cfg->numInputChannels < 0 || cfg->numInputChannels > 2) return LE_ERR_INVALID_ARG;
    if (cfg->preferredBufferSize < 0 || cfg->preferredBufferSize > 4096) return LE_ERR_INVALID_ARG;
    if (cfg->preferredSampleRate < 0.0 || cfg->preferredSampleRate > 192000.0) return LE_ERR_INVALID_ARG;
    return LE_OK;
}

Engine::Engine(const LeConfig& cfg, std::unique_ptr<io::DeviceIO> device)
    : juce_(std::make_unique<juce::ScopedJuceInitialiser_GUI>()),   // thread gọi le_create thành message thread
      cfg_(cfg),
      dataDir_(cfg.dataDir != nullptr ? cfg.dataDir : ""),
      libraryDir_(cfg.libraryDir != nullptr ? cfg.libraryDir : ""),
      preferredBuffer_(cfg.preferredBufferSize > 0 ? cfg.preferredBufferSize : 128),
      preferredRate_(cfg.preferredSampleRate > 0.0 ? cfg.preferredSampleRate : 48000.0),
      numInputs_(cfg.numInputChannels),
      rt_(commands_, toNrt_, globalStatePublisher()),
      jobs_(std::make_unique<JobSystem>(2)),
      device_(std::move(device)) {
    cfg_.dataDir = nullptr;   // không giữ con trỏ của Dart
    cfg_.libraryDir = nullptr;
    io::session::installObservers();
    const auto& c = io::session::counters();
    seenInterruptBegan_ = c.interruptionBegan.load();
    seenInterruptEnded_ = c.interruptionEnded.load();
    seenRoute_ = c.routeChanged.load();
    pump_ = std::make_unique<EventPump>(*this);
}

Engine::~Engine() {
    pump_.reset();
    jobs_.reset();   // huỷ job đang chạy và chờ worker dừng, trước khi rt_ / device bị huỷ
    audioStop();
    device_.reset();
    io::session::removeObservers();
    // Xả event còn lại trong queue để RtMessage không bị "mất" im lặng (không có gì cần giải phóng ở P0).
    while (toNrt_.front() != nullptr) toNrt_.pop();
}

std::int32_t Engine::audioStart() {
    if (device_ == nullptr) device_ = std::make_unique<io::JuceDeviceIO>();
    if (device_->isRunning()) return LE_OK;

    if (recordBuf_ == nullptr) {   // cấp phát 1 lần trên main, audio thread chỉ ghi vào
        recordCapacity_ = kRecordSeconds * kRecordMaxRate;
        recordBuf_ = std::make_unique<float[]>((std::size_t) recordCapacity_);
    }
    rt_.setRecordBuffer(recordBuf_.get(), recordCapacity_);

    io::DeviceConfig dc;
    dc.sampleRate = preferredRate_;
    dc.bufferSize = preferredBuffer_;
    dc.numInputs = numInputs_;
    dc.numOutputs = 2;
    const std::int32_t err = device_->start(dc, &rt_);
    if (err != LE_OK) {
        emitEvent(LE_EVT_ERROR, err, 0, 0, 0.0);
        return err;
    }
    refreshDeviceInfo();
    startGraceTicks_ = 15;   // 0.5 giây
    return LE_OK;
}

void Engine::audioStop() {
    if (device_ != nullptr) device_->stop();
}

bool Engine::send(const LeCommand& cmd) {
    // P0: chỉ nhận lệnh spike. P1-04 mở rộng: validate track/slot + side effect NRT rồi mới push.
    const bool trackOk = cmd.track >= -1 && cmd.track < LE_MAX_TRACKS;
    const bool slotOk = cmd.slot >= -1 && cmd.slot < LE_MAX_SCENES;
    if (!isSpikeCommand(cmd.type) || !trackOk || !slotOk) {
        ++rejectedCommands_;
        return false;
    }
    if (!commands_.try_push(cmd)) {   // queue đầy: báo false, không chờ
        ++rejectedCommands_;
        return false;
    }
    return true;
}

void Engine::refreshDeviceInfo() {
    if (device_ == nullptr || !device_->isRunning()) return;
    rt_.setLatencyRoundTrip(device_->latencies().roundTrip());
    rt_.setDeviceXruns(device_->xrunCount());
}

void Engine::pump() {
    jobs_->pump();

    // 1) Event từ audio thread
    while (const RtMessage* m = toNrt_.front()) {
        if (m->kind == RtMessage::Event) emitEvent(m->type, m->a, m->b, 0, m->value);
        toNrt_.pop();
    }

    // 2) Xrun: so với lần trước, tối đa 30 event/giây
    if (device_ != nullptr && device_->isRunning()) rt_.setDeviceXruns(device_->xrunCount());
    LeState s;
    globalStatePublisher().read(s);
    if (s.xrunCount > lastXruns_) {
        lastXruns_ = s.xrunCount;
        emitEvent(LE_EVT_XRUN, (std::int32_t) s.xrunCount, 0, 0, 0.0);
    }

    // 3) Interruption & route (bộ đếm do observer AVAudioSession tăng)
    auto& c = io::session::counters();
    if (const auto v = c.interruptionBegan.load(); v != seenInterruptBegan_) {
        seenInterruptBegan_ = v;
        emitEvent(LE_EVT_AUDIO_INTERRUPTED, 1, 0, 0, 0.0);
    }
    if (const auto v = c.interruptionEnded.load(); v != seenInterruptEnded_) {
        seenInterruptEnded_ = v;
        emitEvent(LE_EVT_AUDIO_INTERRUPTED, 0, 0, 0, 0.0);
    }
    bool routeChanged = false;
    if (const auto v = c.routeChanged.load(); v != seenRoute_) {
        seenRoute_ = v;
        routeChanged = true;
    }
    if (!io::session::isSupported() && device_ != nullptr) {   // macOS: dựa vào AudioDeviceManager
        if (const auto v = device_->deviceChangeCount(); v != seenDeviceChanges_) {
            seenDeviceChanges_ = v;
            // AudioDeviceManager broadcast (bất đồng bộ) ngay sau khi mở device → không phải route đổi.
            routeChanged = startGraceTicks_ == 0;
        }
    }
    if (startGraceTicks_ > 0) --startGraceTicks_;
    if (routeChanged) {
        const auto r = io::session::currentRoute();
        refreshDeviceInfo();   // latency đổi theo route (08 §2, 03 §5)
        emitEvent(LE_EVT_ROUTE_CHANGED, r.wired ? 1 : 0, r.bluetooth ? 1 : 0, 0, 0.0);
    }

    // 4) Latency đọc lại mỗi giây (getter rẻ)
    if (++pumpTicks_ >= 30) {
        pumpTicks_ = 0;
        refreshDeviceInfo();
    }
}

// ─────────────────────────── le_call ───────────────────────────

std::string Engine::call(const char* requestJson) {
    if (requestJson == nullptr) return json::error(LE_ERR_INVALID_ARG, "request is null");
    juce::var req;
    const auto parsed = juce::JSON::parse(juce::String::fromUTF8(requestJson), req);
    if (parsed.failed() || !req.isObject()) return json::error(LE_ERR_INVALID_ARG, "invalid JSON request");
    const juce::var op = req.getProperty("op", {});
    if (!op.isString() || op.toString().isEmpty()) return json::error(LE_ERR_INVALID_ARG, "missing \"op\"");
    return handleCall(op.toString().toStdString(), &req);
}

std::string Engine::handleCall(const std::string& op, const void* request) {
    const juce::var& req = *static_cast<const juce::var*>(request);

    if (op == "engine.info") {
        auto* r = new juce::DynamicObject();
        const bool running = device_ != nullptr && device_->isRunning();
        const auto lat = running ? device_->latencies() : io::DeviceLatencies{};
        r->setProperty("apiVersion", LE_API_VERSION);
        r->setProperty("running", running);
        r->setProperty("sampleRate", running ? device_->sampleRate() : preferredRate_);
        r->setProperty("bufferSize", running ? device_->bufferSize() : preferredBuffer_);
        r->setProperty("inputChannels", running ? device_->numInputs() : numInputs_);
        r->setProperty("latencyRoundTripSamples", lat.roundTrip());
        r->setProperty("inputLatencySamples", lat.inputSamples);
        r->setProperty("outputLatencySamples", lat.outputSamples);
        r->setProperty("device", juce::String(running ? device_->deviceName() : std::string()));
        r->setProperty("deviceXruns", running ? device_->xrunCount() : -1);
        r->setProperty("stateSize", (int) sizeof(LeState));
        r->setProperty("commandSize", (int) sizeof(LeCommand));
        r->setProperty("configSize", (int) sizeof(LeConfig));
        r->setProperty("rejectedCommands", (int) rejectedCommands_);
        r->setProperty("droppedEvents", (int) rt_.droppedEvents());
        r->setProperty("sessionMode",
                       device_ != nullptr && device_->sessionMode() == io::session::Mode::Measurement ? "measurement"
                                                                                                        : "default");
        return json::ok(juce::var(r));
    }

    if (op == "spike.setBufferSize") {
        const juce::var f = req.getProperty("frames", {});
        const int frames = f.isInt() || f.isInt64() || f.isDouble() ? (int) f : -1;
        if (frames != 64 && frames != 128 && frames != 256 && frames != 512 && frames != 1024)
            return json::error(LE_ERR_INVALID_ARG, "frames must be 64/128/256/512/1024");
        if (jobs_->anyRunning("latency")) return json::error(LE_ERR_INVALID_ARG, "đang đo latency, đợi job xong");
        preferredBuffer_ = frames;
        int actual = frames;
        if (device_ != nullptr && device_->isRunning()) {
            io::DeviceConfig dc;
            dc.sampleRate = device_->sampleRate();
            dc.bufferSize = frames;
            dc.numInputs = numInputs_;
            const std::int32_t err = device_->restart(dc);   // P0: đồng bộ (vài chục ms), chấp nhận cho spike
            if (err != LE_OK) return json::error(err, device_->lastError());
            actual = device_->bufferSize();
            refreshDeviceInfo();
        }
        auto* r = new juce::DynamicObject();
        r->setProperty("bufferSize", actual);
        return json::ok(juce::var(r));
    }

    if (op == "spike.setSessionMode") {
        const juce::String mode = req.getProperty("mode", {}).toString();
        if (mode != "default" && mode != "measurement")
            return json::error(LE_ERR_INVALID_ARG, "mode must be \"default\" or \"measurement\"");
        const auto m = mode == "measurement" ? io::session::Mode::Measurement : io::session::Mode::Default;
        if (device_ == nullptr) device_ = std::make_unique<io::JuceDeviceIO>();
        if (!device_->setSessionMode(m)) return json::error(LE_ERR_AUDIO_DEVICE, device_->lastError());
        auto* r = new juce::DynamicObject();
        r->setProperty("mode", mode);
        r->setProperty("applied", io::session::isSupported());
        return json::ok(juce::var(r));
    }

    if (op == "spike.sessionInfo") {
        juce::var session;
        juce::JSON::parse(juce::String::fromUTF8(io::session::describeJson().c_str()), session);
        auto* r = new juce::DynamicObject();
        r->setProperty("supported", io::session::isSupported());
        r->setProperty("session", session);
        return json::ok(juce::var(r));
    }

    if (op == "spike.latencyLoopback") return startLatencyJob();
    if (op == "spike.stretchBench") return startStretchJob(request);

    if (op == "job.result" || op == "job.cancel") {
        const juce::var jid = req.getProperty("jobId", {});
        if (!(jid.isInt() || jid.isInt64() || jid.isDouble())) return json::error(LE_ERR_INVALID_ARG, "missing \"jobId\"");
        const auto id = (std::int64_t) (juce::int64) jid;
        if (op == "job.result") return jobs_->resultJson(id);
        if (!jobs_->cancel(id)) return json::error(LE_ERR_JOB_NOT_FOUND, "no job " + std::to_string(id));
        return json::ok({});
    }

    return json::error(LE_ERR_NOT_IMPLEMENTED, "op not implemented: " + op);
}

namespace {
std::string jobCreated(std::int64_t id) {
    auto* r = new juce::DynamicObject();
    r->setProperty("jobId", (juce::int64) id);
    return json::ok(juce::var(r));
}

template <typename Container>
juce::var toVarArray(const Container& c) {
    juce::Array<juce::var> a;
    for (const auto& x : c) a.add(juce::var(x));
    return juce::var(a);
}
} // namespace

// P0-07: phát 5 chirp qua loa, thu qua mic, cross-correlation (LatencyProbe của agent 80).
std::string Engine::startLatencyJob() {
    if (device_ == nullptr || !device_->isRunning())
        return json::error(LE_ERR_AUDIO_DEVICE, "audio chưa chạy (gọi le_audio_start trước)");
    if (device_->numInputs() <= 0)
        return json::error(LE_ERR_AUDIO_DEVICE, "không có input mic (numInputChannels = 0)");
    if (jobs_->anyRunning("latency")) return json::error(LE_ERR_INVALID_ARG, "đang đo latency");

    const int reported = device_->latencies().roundTrip();
    spike::LatencyProbe& probe = rt_.latencyProbe();
    probe.start();   // [main] chỉ bật cờ atomic; RT bắt đầu ở block kế tiếp

    const auto id = jobs_->submit("latency", [&probe, reported](JobSystem::Context& ctx) {   // [worker]
        const double sr = probe.sampleRate();
        const double expectedMs = (double) probe.totalSamples() * 1000.0 / sr;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        while (!probe.isDone()) {
            if (ctx.cancelled()) return JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
            const double elapsed = juce::Time::getMillisecondCounterHiRes() - t0;
            if (elapsed > expectedMs + 3000.0)
                return JobOutcome::fail(LE_ERR_AUDIO_DEVICE, "timeout: audio dừng trong lúc đo?");
            ctx.progress.store((float) std::min(0.95, elapsed / expectedMs), std::memory_order_relaxed);
            juce::Thread::sleep(20);
        }
        const spike::LatencyResult r = probe.analyze();
        auto* o = new juce::DynamicObject();
        o->setProperty("ok", r.ok);
        o->setProperty("measuredSamples", r.measuredSamples);
        o->setProperty("measuredMs", r.measuredMs);
        o->setProperty("reportedSamples", reported);
        o->setProperty("reportedMs", (double) reported * 1000.0 / sr);
        o->setProperty("runs", toVarArray(r.runs));
        o->setProperty("score", toVarArray(r.score));
        o->setProperty("validRuns", r.validRuns);
        o->setProperty("spreadSamples", r.spreadSamples);
        o->setProperty("spreadMs", (double) r.spreadSamples * 1000.0 / sr);
        o->setProperty("inputPeak", r.inputPeak);
        o->setProperty("sampleRate", sr);
        return JobOutcome::ok(juce::var(o));
    });
    return jobCreated(id);
}

// P0-09: render các zone pitch-shift từ bản thu SPIKE_RECORD (StretchBench của agent 80).
std::string Engine::startStretchJob(const void* request) {
    const juce::var& req = *static_cast<const juce::var*>(request);

    const juce::var semis = req.getProperty("semitones", {});
    if (!semis.isArray() || semis.size() == 0)
        return json::error(LE_ERR_INVALID_ARG, "semitones phải là danh sách số nguyên, ví dụ [-12,0,12]");
    spike::StretchBench::Config cfg;
    for (const auto& v : *semis.getArray()) {
        const double d = v.isInt() || v.isInt64() || v.isDouble() ? (double) v : 1e9;
        if (std::fabs(d) > 36.0 || std::fabs(d - std::round(d)) > 1e-9)
            return json::error(LE_ERR_INVALID_ARG, "semitones: mỗi phần tử là số nguyên trong [-36, 36]");
        cfg.semitones.push_back((int) std::lround(d));
    }
    cfg.formant = (bool) req.getProperty("formant", false);
    cfg.formantBaseHz = (double) req.getProperty("baseHz", 0.0);
    cfg.blockMs = (double) req.getProperty("blockMs", 0.0);
    cfg.intervalMs = (double) req.getProperty("intervalMs", 0.0);
    cfg.tonalityLimitHz = (double) req.getProperty("tonalityLimitHz", 0.0);
    cfg.cheaper = (bool) req.getProperty("cheaper", false);

    juce::String saveDir = req.getProperty("saveDir", {}).toString();
    if (saveDir.isEmpty() && !dataDir_.empty()) saveDir = juce::String::fromUTF8(dataDir_.c_str()) + "/spike";
    if (saveDir.isNotEmpty() && !juce::File::isAbsolutePath(saveDir))
        return json::error(LE_ERR_INVALID_ARG, "saveDir phải là đường dẫn tuyệt đối");
    cfg.outDir = saveDir.toStdString();

    // [main] Copy bản thu ngay (≤ 10 giây, ~1ms). RT chỉ ghi buffer khi đang thu; nếu một SPIKE_RECORD mới bắt đầu
    // giữa chừng thì recordedFrames() về 0 → phát hiện và báo lỗi thay vì dùng dữ liệu lẫn lộn.
    const auto& sp = rt_.spikeProcessor();
    std::vector<float> audio;
    std::string err;
    const int n = sp.isRecording() ? 0 : sp.recordedFrames();
    if (n > 0 && sp.recordBuffer() != nullptr) {
        audio.assign(sp.recordBuffer(), sp.recordBuffer() + n);
        if (sp.isRecording() || sp.recordedFrames() != n) {
            audio.clear();
            err = "bản thu bị ghi đè trong lúc copy (có SPIKE_RECORD mới)";
        }
    } else {
        err = sp.isRecording() ? "đang thu, đợi RECORDING_FINISHED" : "chưa có bản thu";
    }
    const double sr = sp.sampleRate();

    // Job thất bại vẫn tạo jobId (05 §3): Dart nhận JOB_FAILED + job.result có error.
    const auto id = jobs_->submit("stretch", [samples = std::move(audio), sr, cfg, precheck = err](JobSystem::Context& ctx) mutable {
        if (!precheck.empty()) return JobOutcome::fail(LE_ERR_INVALID_ARG, precheck);
        if (!cfg.outDir.empty() && !juce::File(juce::String::fromUTF8(cfg.outDir.c_str())).createDirectory())
            return JobOutcome::fail(LE_ERR_DISK_FULL, "không tạo được thư mục " + cfg.outDir);
        cfg.cancel = &ctx.cancel;
        cfg.progress = &ctx.progress;
        const spike::StretchBench::Result r = spike::StretchBench::run(samples.data(), (std::int64_t) samples.size(), sr, cfg);
        if (r.cancelled) return JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
        if (!r.ok) return JobOutcome::fail(LE_ERR_INTERNAL, r.error);
        auto* o = new juce::DynamicObject();
        o->setProperty("msTotal", r.msTotal);
        o->setProperty("msPerZone", toVarArray(r.msPerZone));
        o->setProperty("semitones", toVarArray(r.semitones));
        juce::Array<juce::var> files;
        for (const auto& f : r.files) files.add(juce::String::fromUTF8(f.c_str()));
        o->setProperty("files", files);
        o->setProperty("peakPerZone", toVarArray(r.peakPerZone));
        o->setProperty("rmsDbPerZone", toVarArray(r.rmsDbPerZone));
        o->setProperty("inputRmsDb", r.inputRmsDb);
        o->setProperty("msSetup", r.msSetup);
        o->setProperty("msWrite", r.msWrite);
        o->setProperty("numSamples", (juce::int64) r.numSamples);
        o->setProperty("sampleRate", sr);
        o->setProperty("formant", cfg.formant);
        o->setProperty("blockSamples", r.blockSamples);
        o->setProperty("intervalSamples", r.intervalSamples);
        return JobOutcome::ok(juce::var(o));
    });
    return jobCreated(id);
}

} // namespace le::core
