// le-harness: chạy engine trên Mac không cần UI.
//   le-harness spike [tuỳ chọn]              real-time qua C API: sine, tải giả lập, job spike      (P0-03)
//   le-harness render <scenario.json> -o x   offline, deterministic, kiểm kỳ vọng                 (P1-02/03)
//   le-harness play [scenario.json]          real-time qua C API, điều khiển bằng bàn phím         (P1-02)
// spike/play chỉ dùng C API (giống app Flutter). render dùng ScenarioRunner (OfflineDeviceIO).
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <juce_core/juce_core.h>

#include "le/engine_api.h"
#include "sim/ScenarioRunner.h"

namespace {

volatile std::sig_atomic_t g_stop = 0;
void onSigint(int) { g_stop = 1; }

// [main] Cho message loop của JUCE (Timer, event) chạy — giống main run loop của iOS trong app thật.
void pumpMainRunLoop(double seconds) {
#if defined(__APPLE__)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
#else
    (void) seconds;
#endif
}

const char* eventName(int32_t t) {
    switch (t) {
        case LE_EVT_RECORDING_FINISHED: return "RECORDING_FINISHED";
        case LE_EVT_JOB_PROGRESS: return "JOB_PROGRESS";
        case LE_EVT_JOB_DONE: return "JOB_DONE";
        case LE_EVT_JOB_FAILED: return "JOB_FAILED";
        case LE_EVT_XRUN: return "XRUN";
        case LE_EVT_AUDIO_INTERRUPTED: return "AUDIO_INTERRUPTED";
        case LE_EVT_ROUTE_CHANGED: return "ROUTE_CHANGED";
        case LE_EVT_ERROR: return "ERROR";
        default: return "EVENT";
    }
}

bool g_quietEvents = false;
// [main] Event callback chỉ ghi lại; vòng lặp chính mới gọi le_call (không gọi C API ngay trong callback).
bool g_recordingFinished = false;
std::vector<int64_t> g_finishedJobs;

void onEvent(int32_t type, int32_t a, int32_t b, int64_t jobId, double value) {
    if (type == LE_EVT_RECORDING_FINISHED) g_recordingFinished = true;
    if (type == LE_EVT_JOB_DONE || type == LE_EVT_JOB_FAILED) g_finishedJobs.push_back(jobId);
    if (type == LE_EVT_JOB_PROGRESS) return;
    if (g_quietEvents && type == LE_EVT_XRUN) return;
    std::printf("  [event] %s a=%d b=%d job=%lld value=%g\n", eventName(type), a, b, (long long) jobId, value);
    std::fflush(stdout);
}

bool send(uint16_t type, int32_t i0 = 0, float f0 = 0, float f1 = 0) {
    LeCommand c{};
    c.type = type;
    c.track = c.slot = -1;
    c.i0 = i0;
    c.f0 = f0;
    c.f1 = f1;
    return le_send(&c);
}

std::string call(const std::string& json) {
    char* r = le_call(json.c_str());
    std::string s = r != nullptr ? r : "";
    le_free_string(r);
    return s;
}

double toDb(float x) { return x > 1e-9f ? 20.0 * std::log10((double) x) : -120.0; }

void usage() {
    std::printf(
        "le-harness — chạy LoopCore engine trên Mac không cần UI\n\n"
        "Cách dùng:\n"
        "  le-harness spike [tuỳ chọn]                 Spike P0: sine, tải giả lập, job; in CPU/xrun mỗi giây\n"
        "  le-harness render <scenario.json> [tuỳ chọn] Render offline (deterministic) + kiểm kỳ vọng\n"
        "  le-harness play [scenario.json] [tuỳ chọn]   Chạy real-time, điều khiển bằng bàn phím\n"
        "  le-harness --help\n\n"
        "Tuỳ chọn cho render:\n"
        "  -o, --out FILE   ghi WAV float32 stereo\n"
        "  --block N        ép kích thước block (mặc định theo scenario, 128)\n"
        "  --no-check       không kiểm kỳ vọng (chỉ render)\n"
        "  --no-golden      kiểm kỳ vọng nhưng bỏ qua so golden (golden_update.sh dùng)\n"
        "  Mã thoát: 0 = đạt, 1 = kỳ vọng sai, 2 = lỗi scenario/tham số\n\n"
        "Tuỳ chọn cho play:\n"
        "  --buffer N  --rate HZ  --inputs N  --seconds N (0 = tới khi Ctrl-C)\n"
        "  Phím: 1–8 launch ô (track, scene đang chọn) · q w e r t y u i chọn scene 1–8 · Space play/stop\n"
        "        [ ] chọn track · Shift+R thu ở track đang chọn · o overdub · z x c v b n m nốt C4–B4\n"
        "        s bật/tắt sine thử (spike) · Ctrl-C thoát\n\n"
        "Tuỳ chọn cho spike:\n"
        "  --seconds N      chạy N giây rồi dừng (mặc định 0 = tới khi Ctrl-C)\n"
        "  --buffer N       buffer size mong muốn (mặc định 128)\n"
        "  --rate HZ        sample rate mong muốn (mặc định 48000)\n"
        "  --inputs N       số kênh mic 0..2 (mặc định 0; 1 = mic, macOS sẽ hỏi quyền)\n"
        "  --freq HZ        tần số sine (mặc định 440)\n"
        "  --gain G         gain sine 0..1 (mặc định 0.1; 0 = tắt)\n"
        "  --voices N       số voice tải giả lập 0..128 (mặc định 0)\n"
        "  --passthrough    mic → loa (DÙNG TAI NGHE)\n"
        "  --record-ms N    thu N ms từ mic ngay khi start (SPIKE_RECORD). --inputs 0 → thu im lặng\n"
        "  --stretch LIST   thu xong thì chạy spike.stretchBench, LIST dạng \"-12,0,12\"\n"
        "  --formant        stretchBench giữ formant\n"
        "  --out DIR        thư mục WAV cho stretchBench (đường dẫn tuyệt đối)\n"
        "  --latency        sau 1 giây chạy spike.latencyLoopback (cần --inputs 1, loa → mic)\n"
        "  --quiet-xrun     không in từng event XRUN\n");
}

int runSpike(int argc, char** argv) {
    int seconds = 0, buffer = 128, inputs = 0, voices = 0;
    double rate = 48000.0, freq = 440.0, gain = 0.1;
    bool passthrough = false, formant = false, latency = false;
    int recordMs = 0;
    std::string stretch, outDir;

    for (int i = 0; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "thiếu giá trị cho %s\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--seconds") seconds = std::atoi(next("--seconds"));
        else if (a == "--buffer") buffer = std::atoi(next("--buffer"));
        else if (a == "--rate") rate = std::atof(next("--rate"));
        else if (a == "--inputs") inputs = std::atoi(next("--inputs"));
        else if (a == "--freq") freq = std::atof(next("--freq"));
        else if (a == "--gain") gain = std::atof(next("--gain"));
        else if (a == "--voices") voices = std::atoi(next("--voices"));
        else if (a == "--passthrough") passthrough = true;
        else if (a == "--quiet-xrun") g_quietEvents = true;
        else if (a == "--record-ms") recordMs = std::atoi(next("--record-ms"));
        else if (a == "--stretch") stretch = next("--stretch");
        else if (a == "--formant") formant = true;
        else if (a == "--out") outDir = next("--out");
        else if (a == "--latency") latency = true;
        else if (a == "--help" || a == "-h") {
            usage();
            return 0;
        } else {
            std::fprintf(stderr, "tuỳ chọn không rõ: %s\n\n", a.c_str());
            usage();
            return 2;
        }
    }

    std::signal(SIGINT, onSigint);
    le_set_event_callback(onEvent);

    LeConfig cfg{};
    cfg.apiVersion = LE_API_VERSION;
    cfg.preferredBufferSize = buffer;
    cfg.preferredSampleRate = rate;
    cfg.numInputChannels = inputs;
    if (const int32_t err = le_create(&cfg); err != LE_OK) {
        std::fprintf(stderr, "le_create lỗi %d\n", err);
        return 1;
    }
    if (const int32_t err = le_audio_start(); err != LE_OK) {
        std::fprintf(stderr, "le_audio_start lỗi %d\n", err);
        le_destroy();
        return 1;
    }
    std::printf("engine.info: %s\n", call(R"({"op":"engine.info"})").c_str());

    send(LE_CMD_SPIKE_SINE, 0, (float) freq, (float) gain);
    send(LE_CMD_SPIKE_LOAD_VOICES, voices);
    if (passthrough) send(LE_CMD_SPIKE_PASSTHROUGH, 1);
    if (recordMs > 0) send(LE_CMD_SPIKE_RECORD, recordMs);
    std::vector<int64_t> jobs;
    auto submit = [&](const std::string& req) {
        const std::string res = call(req);
        std::printf("le_call %s\n  → %s\n", req.c_str(), res.c_str());
        const auto pos = res.find("\"jobId\":");
        if (pos != std::string::npos) jobs.push_back(std::atoll(res.c_str() + pos + 8));
    };
    std::printf("sine %.1f Hz gain %.2f, %d voice tải, Ctrl-C để dừng\n", freq, gain, voices);
    std::fflush(stdout);

    int elapsed = 0;
    while (g_stop == 0 && (seconds <= 0 || elapsed < seconds)) {
        for (int k = 0; k < 20 && g_stop == 0; ++k) {   // 1 giây
            pumpMainRunLoop(0.05);
            if (g_recordingFinished) {
                g_recordingFinished = false;
                if (!stretch.empty()) {
                    std::string req = "{\"op\":\"spike.stretchBench\",\"semitones\":[" + stretch + "],\"formant\":" +
                                      (formant ? "true" : "false");
                    if (!outDir.empty()) req += ",\"saveDir\":\"" + outDir + "\"";
                    submit(req + "}");
                }
            }
            for (int64_t id : g_finishedJobs)
                std::printf("job.result %lld → %s\n", (long long) id,
                            call("{\"op\":\"job.result\",\"jobId\":" + std::to_string(id) + "}").c_str());
            g_finishedJobs.clear();
        }
        ++elapsed;
        if (latency && elapsed == 1) submit(R"({"op":"spike.latencyLoopback"})");
        LeState s{};
        le_read_state(&s);
        std::printf("t=%3ds  sr=%.0f buf=%d  cpu=%5.2f%% peak=%5.2f%%  xrun=%u  voices=%d  in=%6.1fdB  out=%6.1fdB  rtl=%d  n=%u\n",
                    elapsed, s.sampleRate, s.bufferSize, s.cpuLoad * 100.0, s.cpuPeak * 100.0, s.xrunCount,
                    s.activeVoices, toDb(s.inputPeak), toDb(s.masterPeak[0]), s.latencyRoundTripSamples,
                    s.publishCounter);
        std::fflush(stdout);
    }

    le_audio_stop();
    le_destroy();
    std::printf("đã dừng sạch\n");
    return 0;
}

// ─────────────────────────── render (P1-02/03) ───────────────────────────

int runRender(int argc, char** argv) {
    std::string scenario, out;
    le::sim::RunOptions opt;
    for (int i = 0; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "-o" || a == "--out") && i + 1 < argc) out = argv[++i];
        else if (a == "--block" && i + 1 < argc) opt.blockSize = std::atoi(argv[++i]);
        else if (a == "--no-check") opt.checkExpectations = false;
        else if (a == "--no-golden") opt.ignoreGolden = true;
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (!a.empty() && a[0] != '-' && scenario.empty()) scenario = a;
        else { std::fprintf(stderr, "tuỳ chọn không rõ: %s\n", a.c_str()); return 2; }
    }
    if (scenario.empty()) { std::fprintf(stderr, "thiếu <scenario.json>\n"); return 2; }
    if (!out.empty() && !juce::File::isAbsolutePath(out))
        out = juce::File::getCurrentWorkingDirectory().getChildFile(out).getFullPathName().toStdString();

    const auto r = le::sim::runScenarioFile(juce::File::getCurrentWorkingDirectory().getChildFile(scenario).getFullPathName().toStdString(), opt);
    if (!r.error.empty()) { std::fprintf(stderr, "LỖI scenario: %s\n", r.error.c_str()); return 2; }
    std::printf("scenario %s: %zu frame @ %.0f Hz, block %d\n", r.name.c_str(), r.left.size(), r.sampleRate, r.blockSize);
    for (const auto& n : r.notes) std::printf("  · %s\n", n.c_str());
    if (!out.empty()) {
        std::string err;
        if (!le::sim::writeWavStereo(out, r.left, r.right, r.sampleRate, &err)) { std::fprintf(stderr, "ghi WAV lỗi: %s\n", err.c_str()); return 2; }
        std::printf("  → %s\n", out.c_str());
    }
    for (const auto& f : r.failures) std::printf("  ✗ %s\n", f.c_str());
    if (!opt.checkExpectations) return 0;
    std::printf(r.failures.empty() ? "ĐẠT\n" : "KHÔNG ĐẠT\n");
    return r.failures.empty() ? 0 : 1;
}

// ─────────────────────────── play (P1-02) ───────────────────────────

struct RawTerminal {   // raw mode: đọc từng phím, không echo; giữ ISIG để Ctrl-C vẫn là SIGINT
    termios saved{};
    bool active = false;
    RawTerminal() {
        if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &saved) != 0) return;
        termios raw = saved;
        raw.c_lflag &= ~(tcflag_t) (ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        active = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
    }
    ~RawTerminal() { if (active) tcsetattr(STDIN_FILENO, TCSANOW, &saved); }
    int readKey() const {   // -1 nếu không có phím
        if (!active) return -1;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        timeval tv{0, 0};
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0) return -1;
        unsigned char c = 0;
        return read(STDIN_FILENO, &c, 1) == 1 ? c : -1;
    }
};

bool sendCmd(const char* what, uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0, float f0 = 0, float f1 = 0) {
    LeCommand c{};
    c.type = type;
    c.track = (int8_t) track;
    c.slot = (int8_t) slot;
    c.i0 = i0;
    c.f0 = f0;
    c.f1 = f1;
    const bool ok = le_send(&c);
    std::printf("  %s %s\n", ok ? "→" : "✗", what);
    if (!ok) std::printf("    (engine chưa nhận lệnh này ở phase hiện tại)\n");
    std::fflush(stdout);
    return ok;
}

// Chạy phần "setup" của scenario qua C API (timeline bỏ qua ở chế độ play).
bool runSetupViaApi(const std::string& path) {
    juce::var sc;
    if (juce::JSON::parse(juce::File(juce::String::fromUTF8(path.c_str())).loadFileAsString(), sc).failed()) {
        std::fprintf(stderr, "JSON scenario hỏng\n");
        return false;
    }
    if (const auto* steps = sc["setup"].getArray()) {
        for (const auto& st : *steps) {
            if (st.hasProperty("call")) {
                const auto res = call(juce::JSON::toString(st["call"], true).toStdString());
                std::printf("  call %s → %s\n", juce::JSON::toString(st["call"], true).toRawUTF8(), res.c_str());
            } else if (st.hasProperty("send")) {
                const auto& v = st["send"];
                const int type = v["type"].isString() ? le::sim::commandTypeFromName(v["type"].toString().toStdString()) : (int) v["type"];
                if (type <= 0) { std::fprintf(stderr, "lệnh không rõ trong setup\n"); return false; }
                LeCommand c{};
                c.type = (uint16_t) type;
                c.track = (int8_t) (int) v.getProperty("track", -1);
                c.slot = (int8_t) (int) v.getProperty("slot", -1);
                c.i0 = (int) v.getProperty("i0", 0);
                c.f0 = (float) (double) v.getProperty("f0", 0.0);
                c.f1 = (float) (double) v.getProperty("f1", 0.0);
                c.d0 = (double) v.getProperty("d0", 0.0);
                std::printf("  send %s → %s\n", juce::JSON::toString(v, true).toRawUTF8(), le_send(&c) ? "ok" : "bị từ chối");
            }
        }
    }
    if (sc["timeline"].isArray() && sc["timeline"].size() > 0) std::printf("  (timeline bị bỏ qua ở chế độ play)\n");
    return true;
}

char clipChar(uint8_t s) {
    switch (s) {
        case LE_CLIP_EMPTY: return '.';
        case LE_CLIP_STOPPED: return 's';
        case LE_CLIP_QUEUED_PLAY: return '>';
        case LE_CLIP_PLAYING: return 'P';
        case LE_CLIP_QUEUED_STOP: return 'x';
        case LE_CLIP_QUEUED_RECORD: return 'r';
        case LE_CLIP_RECORDING: return 'R';
        case LE_CLIP_OVERDUBBING: return 'O';
        default: return '?';
    }
}

int runPlay(int argc, char** argv) {
    int seconds = 0, buffer = 128, inputs = 0;
    double rate = 48000.0;
    std::string scenario;
    for (int i = 0; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--seconds" && i + 1 < argc) seconds = std::atoi(argv[++i]);
        else if (a == "--buffer" && i + 1 < argc) buffer = std::atoi(argv[++i]);
        else if (a == "--rate" && i + 1 < argc) rate = std::atof(argv[++i]);
        else if (a == "--inputs" && i + 1 < argc) inputs = std::atoi(argv[++i]);
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (!a.empty() && a[0] != '-' && scenario.empty()) scenario = a;
        else { std::fprintf(stderr, "tuỳ chọn không rõ: %s\n", a.c_str()); return 2; }
    }

    std::signal(SIGINT, onSigint);
    le_set_event_callback(onEvent);
    LeConfig cfg{};
    cfg.apiVersion = LE_API_VERSION;
    cfg.preferredBufferSize = buffer;
    cfg.preferredSampleRate = rate;
    cfg.numInputChannels = inputs;
    if (const int32_t err = le_create(&cfg); err != LE_OK) { std::fprintf(stderr, "le_create lỗi %d\n", err); return 1; }
    if (!scenario.empty() && !runSetupViaApi(scenario)) { le_destroy(); return 2; }
    if (const int32_t err = le_audio_start(); err != LE_OK) { std::fprintf(stderr, "le_audio_start lỗi %d\n", err); le_destroy(); return 1; }

    std::printf("play: Space play/stop · 1–8 launch · q–i scene · [ ] track · R thu · o overdub · z–m nốt · s sine · Ctrl-C thoát\n");
    std::fflush(stdout);

    RawTerminal term;
    int scene = 0, track = 0;
    bool playing = false, sine = false;
    int pendingNoteOff = -1;
    double noteOffAt = 0;
    const char* sceneKeys = "qwertyui";
    const char* noteKeys = "zxcvbnm";
    const int notes[7] = {60, 62, 64, 65, 67, 69, 71};

    const double t0 = juce::Time::getMillisecondCounterHiRes();
    double nextStatus = 1000.0;
    char buf[96];
    while (g_stop == 0) {
        pumpMainRunLoop(0.01);
        const double now = juce::Time::getMillisecondCounterHiRes() - t0;
        if (seconds > 0 && now >= seconds * 1000.0) break;

        if (pendingNoteOff >= 0 && now >= noteOffAt) {   // terminal không báo nhả phím → note off sau 300ms
            sendCmd("NOTE_OFF", LE_CMD_NOTE_OFF, track, -1, pendingNoteOff);
            pendingNoteOff = -1;
        }

        for (int k = term.readKey(); k >= 0; k = term.readKey()) {
            if (k >= '1' && k <= '8') {
                std::snprintf(buf, sizeof(buf), "CLIP_LAUNCH track %d scene %d", k - '1' + 1, scene + 1);
                sendCmd(buf, LE_CMD_CLIP_LAUNCH, k - '1', scene);
            } else if (const char* p = std::strchr(sceneKeys, k); p != nullptr && k != 0) {
                scene = (int) (p - sceneKeys);
                std::printf("  scene %d\n", scene + 1);
            } else if (k == ' ') {
                playing = !playing;
                sendCmd(playing ? "TRANSPORT_PLAY" : "TRANSPORT_STOP", playing ? LE_CMD_TRANSPORT_PLAY : LE_CMD_TRANSPORT_STOP);
            } else if (k == '[' || k == ']') {
                track = (track + (k == ']' ? 1 : LE_MAX_TRACKS - 1)) % LE_MAX_TRACKS;
                std::printf("  track %d\n", track + 1);
            } else if (k == 'R') {
                std::snprintf(buf, sizeof(buf), "CLIP_RECORD track %d scene %d", track + 1, scene + 1);
                sendCmd(buf, LE_CMD_CLIP_RECORD, track, scene, 0);
            } else if (k == 'o') {
                sendCmd("OVERDUB_TOGGLE", LE_CMD_OVERDUB_TOGGLE, track);
            } else if (const char* q = std::strchr(noteKeys, k); q != nullptr && k != 0) {
                const int note = notes[q - noteKeys];
                if (pendingNoteOff >= 0) sendCmd("NOTE_OFF", LE_CMD_NOTE_OFF, track, -1, pendingNoteOff);
                std::snprintf(buf, sizeof(buf), "NOTE_ON %d track %d", note, track + 1);
                sendCmd(buf, LE_CMD_NOTE_ON, track, -1, note, 0.8f);
                pendingNoteOff = note;
                noteOffAt = now + 300.0;
            } else if (k == 's') {
                sine = !sine;
                sendCmd(sine ? "SPIKE_SINE 440 Hz" : "SPIKE_SINE off", LE_CMD_SPIKE_SINE, -1, -1, 0, 440.0f, sine ? 0.1f : 0.0f);
            }
        }

        if (now >= nextStatus) {
            nextStatus += 1000.0;
            LeState s{};
            le_read_state(&s);
            char clips[LE_MAX_TRACKS + 1] = {};
            for (int t = 0; t < LE_MAX_TRACKS; ++t) clips[t] = clipChar(s.clipState[t][scene]);
            std::printf("beat=%7.2f bpm=%5.1f %s  cpu=%5.2f%% peak=%5.2f%%  xrun=%u  scene %d [%s]  track %d  out=%6.1fdB\n",
                        s.beat, s.bpm, s.playing ? "▶" : "■", s.cpuLoad * 100.0, s.cpuPeak * 100.0, s.xrunCount,
                        scene + 1, clips, track + 1, toDb(s.masterPeak[0]));
            std::fflush(stdout);
        }
    }

    le_audio_stop();
    le_destroy();
    std::printf("đã dừng sạch\n");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
        usage();
        return argc < 2 ? 2 : 0;
    }
    const std::string cmd = argv[1];
    if (cmd == "spike") return runSpike(argc - 2, argv + 2);
    if (cmd == "render") return runRender(argc - 2, argv + 2);
    if (cmd == "play") return runPlay(argc - 2, argv + 2);
    std::fprintf(stderr, "lệnh không rõ: %s (xem le-harness --help)\n", cmd.c_str());
    return 2;
}
