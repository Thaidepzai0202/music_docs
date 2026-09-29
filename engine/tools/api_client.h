// ApiClient — phần chung của le-fuzz-api và le-soak (agent 80): gọi engine CHỈ qua C API (le_call / le_send /
// le_read_state …) như app Dart, kiểm envelope 05 §3, kiểm bất biến của LeState, ghi event, theo dõi job.
// [main] Mọi hàm chạy trên thread gọi le_create (= message thread của JUCE). Event callback cũng tới trên thread đó
// (Engine::pump chạy trong sim.advance hoặc Timer 30 Hz), nên không cần lock.
#pragma once

#include "le/engine_api.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace le::tools {

// ── RNG tái lập được (splitmix64): cùng seed → cùng chuỗi lệnh trên mọi máy / thư viện chuẩn ──
// Trạng thái đầu = mix(seed), KHÔNG phải seed·γ: splitmix tiến mỗi lần rút đúng γ, nên seed·γ làm luồng của seed N+1
// chính là luồng của seed N lệch 1 lần rút (các seed gần như trùng nhau). mix() rải các seed ra khắp không gian 2^64.
struct Rng {
    uint64_t s;
    static uint64_t mix(uint64_t z) {
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    explicit Rng(uint64_t seed) : s(mix(mix(seed ^ 0x2545F4914F6CDD1Dull) + 0x9E3779B97F4A7C15ull)) {}
    uint64_t next() { return mix(s += 0x9E3779B97F4A7C15ull); }
    int below(int n) { return n <= 0 ? 0 : static_cast<int>(next() % static_cast<uint64_t>(n)); }
    int range(int lo, int hi) { return lo + below(hi - lo + 1); }   // [lo, hi]
    double uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
    double uniform(double lo, double hi) { return lo + (hi - lo) * uniform(); }
    bool chance(double p) { return uniform() < p; }
    template <typename T> const T& pick(const std::vector<T>& v) { return v[static_cast<size_t>(below(static_cast<int>(v.size())))]; }
};

// ── Event từ engine ──
struct Event {
    int32_t type, a, b;
    int64_t jobId;
    double value;
};
inline std::vector<Event>& events() {
    static std::vector<Event> e;
    return e;
}
inline void onEvent(int32_t type, int32_t a, int32_t b, int64_t jobId, double value) { events().push_back({type, a, b, jobId, value}); }

// ── le_call + kiểm envelope ──
struct CallResult {
    std::string raw;
    bool envelopeOk = false;   // đúng dạng {"ok":true,"result":{…}} / {"ok":false,"error":{code,message}}
    std::string envelopeError;
    bool ok = false;
    juce::var result;
    std::string code, message;
    double ms = 0.0;
};

inline std::string checkEnvelope(const juce::var& v, CallResult& r) {
    auto* o = v.getDynamicObject();
    if (o == nullptr) return "response không phải object JSON";
    if (!o->hasProperty("ok") || !o->getProperty("ok").isBool()) return "thiếu \"ok\" kiểu bool";
    r.ok = static_cast<bool>(o->getProperty("ok"));
    if (r.ok) {
        if (!o->hasProperty("result")) return "ok:true nhưng thiếu \"result\"";
        r.result = o->getProperty("result");
        if (!r.result.isObject()) return "\"result\" không phải object";
        return {};
    }
    const juce::var e = o->getProperty("error");
    if (!e.isObject()) return "ok:false nhưng thiếu \"error\" object";
    if (!e["code"].isString() || e["code"].toString().isEmpty()) return "error.code rỗng / không phải chuỗi";
    if (!e["message"].isString()) return "error.message không phải chuỗi";
    r.code = e["code"].toString().toStdString();
    r.message = e["message"].toString().toStdString();
    for (const char ch : r.code)
        if (!((ch >= 'A' && ch <= 'Z') || ch == '_')) return "error.code không phải UPPER_SNAKE: " + r.code;
    return {};
}

inline CallResult call(const std::string& json) {
    CallResult r;
    const auto t0 = std::chrono::steady_clock::now();
    char* s = le_call(json.c_str());
    r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (s == nullptr) {
        r.envelopeError = "le_call trả nullptr";
        return r;
    }
    r.raw = s;
    le_free_string(s);
    juce::var v;
    const juce::Result pr = juce::JSON::parse(juce::String::fromUTF8(r.raw.c_str()), v);
    if (pr.failed()) {
        r.envelopeError = "response không phải JSON hợp lệ: " + pr.getErrorMessage().toStdString();
        return r;
    }
    r.envelopeError = checkEnvelope(v, r);
    r.envelopeOk = r.envelopeError.empty();
    return r;
}

inline std::string toJson(const juce::var& v) { return juce::JSON::toString(v, true).toStdString(); }

// {"op": op, ...props}
inline juce::var request(const char* op) {
    auto* o = new juce::DynamicObject();
    o->setProperty("op", op);
    return juce::var(o);
}

// ── Bất biến của LeState (05 §2). Trả "" nếu hợp lý. ──
inline std::string checkState(const LeState& s) {
    char buf[256];
    auto bad = [&](const char* what, double v) {
        std::snprintf(buf, sizeof(buf), "LeState.%s = %g", what, v);
        return std::string(buf);
    };
    const double fin[] = {s.beat, s.bpm, s.sampleRate, s.cpuLoad, s.cpuPeak, s.inputPeak, s.masterPeak[0], s.masterPeak[1]};
    const char* names[] = {"beat", "bpm", "sampleRate", "cpuLoad", "cpuPeak", "inputPeak", "masterPeak[0]", "masterPeak[1]"};
    for (size_t i = 0; i < sizeof(fin) / sizeof(fin[0]); ++i)
        if (!std::isfinite(fin[i])) return bad(names[i], fin[i]);
    if (s.playing > 1) return bad("playing", s.playing);
    if (s.masterPeak[0] < 0 || s.masterPeak[1] < 0 || s.masterPeak[0] > 1.0001f || s.masterPeak[1] > 1.0001f)
        return bad("masterPeak (sau limiter phải ≤ 0 dBFS)", std::max(s.masterPeak[0], s.masterPeak[1]));
    if (s.inputPeak < 0) return bad("inputPeak", s.inputPeak);
    if (s.activeVoices < 0 || s.activeVoices > 8 * 72 + 128) return bad("activeVoices", s.activeVoices);
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        for (int c = 0; c < 2; ++c)
            if (!std::isfinite(s.trackPeak[t][c]) || s.trackPeak[t][c] < 0) return bad("trackPeak", s.trackPeak[t][c]);
        if (!std::isfinite(s.trackClipProgress[t]) || s.trackClipProgress[t] < 0 || s.trackClipProgress[t] > 1.0f)
            return bad("trackClipProgress", s.trackClipProgress[t]);
        if (s.trackPlayingSlot[t] < -1 || s.trackPlayingSlot[t] >= LE_MAX_SCENES) return bad("trackPlayingSlot", s.trackPlayingSlot[t]);
        for (int k = 0; k < LE_MAX_SCENES; ++k)
            if (s.clipState[t][k] > LE_CLIP_OVERDUBBING) return bad("clipState", s.clipState[t][k]);
    }
    if (s.publishCounter > 0) {   // trước block đầu tiên LeState còn toàn 0
        if (s.bpm < 20.0 || s.bpm > 300.0) return bad("bpm", s.bpm);
        if (!(s.sampleRate > 0.0)) return bad("sampleRate", s.sampleRate);
        if (s.quantize < LE_Q_NONE || s.quantize > LE_Q_4_BAR) return bad("quantize", s.quantize);
        if (s.beatsPerBar < 1 || s.beatsPerBar > 32) return bad("beatsPerBar", s.beatsPerBar);
        if (s.bufferSize < 1) return bad("bufferSize", s.bufferSize);
    }
    return {};
}

inline std::string checkEvent(const Event& e) {
    char buf[160];
    if (e.type < LE_EVT_RECORDING_FINISHED || e.type > LE_EVT_MEMORY_WARNING || !std::isfinite(e.value)) {
        std::snprintf(buf, sizeof(buf), "event lạ: type %d a %d b %d job %lld value %g", e.type, e.a, e.b,
                      static_cast<long long>(e.jobId), e.value);
        return buf;
    }
    if ((e.type == LE_EVT_JOB_DONE || e.type == LE_EVT_JOB_FAILED || e.type == LE_EVT_JOB_PROGRESS) && e.jobId <= 0) {
        std::snprintf(buf, sizeof(buf), "event job %d có jobId %lld", e.type, static_cast<long long>(e.jobId));
        return buf;
    }
    return {};
}

// ── Bắt "JUCE Assertion failure" (bản debug) thành vi phạm ──
// JUCE in assertion qua DBG → Logger::outputDebugString → fputs(stderr) (juce_Logger.cpp: logAssertion), KHÔNG qua
// juce::Logger hiện hành → cách chắc chắn duy nhất là chặn fd 2: fd 2 → pipe, một thread đọc chuyển nguyên văn ra stderr
// thật (tee) và quét từng dòng. sync(): main ghi 1 dòng đánh dấu vào fd 2 rồi chờ thread đọc tới nó → mọi thứ main đã ghi
// TRƯỚC đó chắc chắn đã được quét (gán đúng bước / seed). Assertion từ thread khác (worker) được gán cho lần sync kế tiếp.
// Khi tiến trình sắp chết (ASan / signal): emergencyRestore() (async-signal-safe) trả fd 2 về stderr thật và đổ nốt pipe
// → báo cáo sanitizer không bị nuốt. Tắt bằng biến môi trường LE_NO_STDERR_WATCH=1.
class StderrWatch {
public:
    static StderrWatch& instance() {
        static StderrWatch* w = new StderrWatch();   // không bao giờ huỷ: thread đọc còn chạy tới lúc tiến trình thoát
        return *w;
    }
    bool start() {
        if (active_.load() || std::getenv("LE_NO_STDERR_WATCH") != nullptr) return false;
        std::fflush(stderr);
        orig_ = ::dup(2);
        int p[2];
        if (orig_ < 0 || ::pipe(p) != 0) return false;
        rd_ = p[0];
        if (::dup2(p[1], 2) < 0) return false;
        ::close(p[1]);   // fd 2 giữ đầu ghi
        active_.store(true);
        reader_ = std::thread([this] { loop(); });
        reader_.detach();   // sống tới hết tiến trình (không join lúc thoát: fd 2 không bao giờ đóng)
        return true;
    }
    bool active() const noexcept { return active_.load(); }

    // [main] Các dòng assertion xuất hiện từ lần sync trước tới giờ.
    std::vector<std::string> sync() {
        if (!active_.load()) return {};
        const uint64_t id = ++seq_;
        char m[48];
        const int len = std::snprintf(m, sizeof(m), "\x01LESYNC %llu\n", static_cast<unsigned long long>(id));
        (void) !::write(2, m, static_cast<size_t>(len));   // < PIPE_BUF → nguyên khối
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait_for(lk, std::chrono::seconds(5), [&] { return synced_ >= id; });
        std::vector<std::string> out;
        out.swap(assertions_);
        return out;
    }

    // [signal / death callback] async-signal-safe
    void emergencyRestore() noexcept {
        if (!active_.exchange(false)) return;
        (void) ::dup2(orig_, 2);
        const int fl = ::fcntl(rd_, F_GETFL);
        (void) ::fcntl(rd_, F_SETFL, fl | O_NONBLOCK);
        char buf[4096];
        ssize_t n;
        while ((n = ::read(rd_, buf, sizeof(buf))) > 0) (void) !::write(orig_, buf, static_cast<size_t>(n));
    }

private:
    void loop() {
        char buf[4096];
        std::string carry, line;
        for (;;) {
            const ssize_t n = ::read(rd_, buf, sizeof(buf));
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;
            std::string chunk = carry;
            chunk.append(buf, static_cast<size_t>(n));
            carry.clear();
            // Tách dòng đánh dấu (\x01LESYNC n\n) ra khỏi luồng; phần còn lại chuyển nguyên văn + quét theo dòng
            std::string pass;
            size_t i = 0;
            while (i < chunk.size()) {
                const size_t mk = chunk.find('\x01', i);
                if (mk == std::string::npos) {
                    pass.append(chunk, i, std::string::npos);
                    break;
                }
                pass.append(chunk, i, mk - i);
                const size_t nl = chunk.find('\n', mk);
                if (nl == std::string::npos) {   // đánh dấu bị cắt giữa 2 lần read → giữ lại
                    carry.assign(chunk, mk, std::string::npos);
                    break;
                }
                const uint64_t id = std::strtoull(chunk.c_str() + mk + 8, nullptr, 10);   // "\x01LESYNC " = 8 byte
                scan(line, pass);   // quét phần trước đánh dấu trước khi báo đã tới
                pass.clear();
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    synced_ = std::max(synced_, id);
                }
                cv_.notify_all();
                i = nl + 1;
            }
            scan(line, pass);
        }
    }
    void scan(std::string& line, const std::string& bytes) {
        if (bytes.empty()) return;
        (void) !::write(orig_, bytes.data(), bytes.size());
        for (const char c : bytes) {
            if (c == '\n') {
                if (line.find("JUCE Assertion failure") != std::string::npos) {
                    std::lock_guard<std::mutex> lk(mu_);
                    assertions_.push_back(line);
                }
                line.clear();
            } else if (line.size() < 4096) {
                line.push_back(c);
            }
        }
    }

    std::atomic<bool> active_{false};
    int orig_ = -1, rd_ = -1;
    std::thread reader_;
    std::atomic<uint64_t> seq_{0};
    std::mutex mu_;
    std::condition_variable cv_;
    uint64_t synced_ = 0;
    std::vector<std::string> assertions_;
};

// ── Chạy message loop của JUCE (Timer 30 Hz → Engine::pump) khi không dùng sim ──
inline void runLoop(double seconds) {
#if defined(__APPLE__)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
#else
    juce::Thread::sleep(static_cast<int>(seconds * 1000.0));
#endif
}

// ── Theo dõi job: status từ job.result ──
struct JobTracker {
    std::set<int64_t> pending;
    std::map<std::string, int> finished;   // "done" / "failed" → số lượng
    // Ghi jobId nếu response là {"jobId":…}
    void noteResponse(const CallResult& r) {
        if (r.ok && r.result.isObject() && r.result.hasProperty("jobId")) pending.insert(static_cast<int64_t>(r.result["jobId"]));
    }
    // Hỏi job.result cho mọi job đang chờ; trả mô tả vi phạm (nếu có). fn = cách gọi le_call (mặc định: C API).
    std::string poll(const std::function<CallResult(const std::string&)>& fn = call) {
        std::string err;
        for (auto it = pending.begin(); it != pending.end();) {
            juce::var q = request("job.result");
            q.getDynamicObject()->setProperty("jobId", static_cast<juce::int64>(*it));
            const CallResult r = fn(toJson(q));
            if (!r.envelopeOk) return "job.result: " + r.envelopeError + " — " + r.raw;
            if (!r.ok) {   // job biến mất (JOB_NOT_FOUND) là lỗi: jobId đã được trả cho client thì phải tra được
                err = "job.result " + std::to_string(*it) + " → " + r.code;
                it = pending.erase(it);
                continue;
            }
            const std::string st = r.result["status"].toString().toStdString();
            if (st == "done" || st == "failed") {
                ++finished[st];
                it = pending.erase(it);
            } else if (st != "running") {
                return "job.result status lạ: " + r.raw;
            } else {
                ++it;
            }
        }
        return err;
    }
};

} // namespace le::tools
