// le-soak — phiên chơi dài ngẫu nhiên có seed, chạy offline (chuẩn bị P1-36, agent 80). [main] (CLI)
// Engine thật + OfflineDeviceIO: tool tự render từng block (như audio thread) và pump ở nhịp ~30 Hz như app
// (KHÔNG pump sau mỗi block như ScenarioRunner / sim.advance — nhịp thật mới lộ được race kiểu R1 của rt-review).
// Lệnh đi qua đúng Engine::send / Engine::call (= le_send / le_call). Mặc định 30 phút thời gian nhạc.
//
// Hành động ngẫu nhiên: launch / stop / scene / stop all, thu audio (có độ dài và tự do), thu MIDI, overdub,
// nốt nhạc cụ, đổi BPM / quantize / metronome, mixer, fx.set / fx.remove / FX_PARAM, clip.clear, undo overdub,
// transport stop / play.
// Kiểm (vi phạm → mã thoát 1):
//   • mọi mẫu output hữu hạn và ≤ 0 dBFS (sau limiter) — meter peak KHÔNG thấy NaN nên phải kiểm output trực tiếp
//   • LeState hợp lý (api_client.h), envelope le_call đúng
//   • bộ nhớ phẳng: heap đang cấp (malloc_zone_statistics) sau project.close ở CUỐI phiên − cùng số đo đó ở trạng thái
//     "project đã đóng" TRƯỚC phiên (setup → close → đo → setup lại) ≤ --max-leak-mb (mặc định 8 MB).
//     (Trong phiên footprint TĂNG hợp lệ vì take / bản copy overdub / lớp undo lấp dần các ô — có giới hạn; phần còn
//      lại sau khi đóng project mới là thứ bị giữ mà không ai giải phóng.) In thêm đường tăng mỗi 5 phút để tham khảo.
//   • mọi take audio thu xong (RECORDING_FINISHED) đều có file audio/<clipId>.caf trên đĩa khi event tới
//   • build mac-rtsan: vi phạm RT → RTSan dừng tiến trình, death callback in seed + thời điểm
//   • bản debug: KHÔNG có "JUCE Assertion failure" nào trên stderr (StderrWatch, kiểm mỗi lần pump)
//
//   le-soak [--seed N] [--minutes 30] [--block 128] [--warmup 5] [--max-leak-mb 8] [--hostile] [--verbose]
//     --hostile: thêm bật/tắt overdub dồn dập (< 50 ms) — đường race R1 (rt-review 29/09)
// Cách chạy: tools/docs/fuzz-soak.md.
#include "api_client.h"
#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <malloc/malloc.h>
#endif
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(realtime_sanitizer)
#include <sanitizer/common_interface_defs.h>
#define LE_HAVE_SANITIZER_API 1
#endif
#endif

using namespace le::tools;

namespace {

char g_where[256] = "";
void writeWhere() {
    StderrWatch::instance().emergencyRestore();   // fd 2 về stderr thật, đổ nốt pipe
    const char head[] = "\n💥 le-soak chết — ";
    (void) !write(2, head, sizeof(head) - 1);
    (void) !write(2, g_where, std::strlen(g_where));
    (void) !write(2, "\n", 1);
}
void onSignal(int sig) {
    writeWhere();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

struct Mem {
    double residentMb = 0, footprintMb = 0;
    double heapInUseMb = 0;   // byte malloc đang cấp THẬT (không tính trang allocator giữ lại để dùng lại)
};
Mem memoryNow() {
    Mem m;
#if defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        m.residentMb = static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
    task_vm_info_data_t vm{};
    mach_msg_type_number_t vcount = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&vm), &vcount) == KERN_SUCCESS)
        m.footprintMb = static_cast<double>(vm.phys_footprint) / (1024.0 * 1024.0);
    malloc_statistics_t ms{};
    malloc_zone_statistics(nullptr, &ms);
    m.heapInUseMb = static_cast<double>(ms.size_in_use) / (1024.0 * 1024.0);
#endif
    return m;
}

struct Options {
    uint64_t seed = 1;
    double minutes = 30.0, warmup = 5.0, maxLeakMb = 8.0;
    int block = 128;
    int holdSec = 0;   // --hold N: ngủ N giây ngay sau project.close (engine còn sống) để soi `heap` / `malloc_history`
    bool hostile = false, verbose = false;
};

const std::string kFix = std::string(LE_ENGINE_DIR) + "/tests/fixtures/";

LeCommand cmd(int type, int track = -1, int slot = -1, int i0 = 0, float f0 = 0.0f, double d0 = 0.0) {
    LeCommand c{};
    c.type = static_cast<uint16_t>(type);
    c.track = static_cast<int8_t>(track);
    c.slot = static_cast<int8_t>(slot);
    c.i0 = i0;
    c.f0 = f0;
    c.d0 = d0;
    return c;
}

class Soak {
public:
    Soak(const Options& o) : opt_(o), rng_(o.seed) {}

    int run() {
        tmp_ = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("le-soak-" + juce::String(getpid()) + "-" + juce::String(static_cast<juce::int64>(opt_.seed)));
        tmp_.deleteRecursively();
        tmp_.createDirectory();
        proj_ = tmp_.getChildFile("proj");
        setup();
        loop();
        finish();
        tmp_.deleteRecursively();
        return violations_;
    }

private:
    // ─────────────── engine + dự án ban đầu ───────────────
    void setup() {
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.preferredBufferSize = opt_.block;
        cfg.preferredSampleRate = 48000.0;
        cfg.numInputChannels = 1;
        dataDir_ = tmp_.getChildFile("data").getFullPathName().toStdString();
        libDir_ = std::string(LE_ENGINE_DIR) + "/tests";
        cfg.dataDir = dataDir_.c_str();
        cfg.libraryDir = libDir_.c_str();
        le::core::setEventCallback(&onEvent);
        memPre_ = memoryNow();   // trước khi có engine
        auto dev = std::make_unique<le::io::OfflineDeviceIO>(std::max(opt_.block, 1024));
        dev_ = dev.get();
        dev_->setLatencies(96, 160);   // L = 256 sample (≈ iPad loa trong)
        eng_ = std::make_unique<le::core::Engine>(cfg, std::move(dev));
        if (eng_->audioStart() != LE_OK) violation("audioStart", "OfflineDeviceIO không start");
        sr_ = dev_->sampleRate();

        openProject();
        // Mức nền đo ở trạng thái ĐÃ ĐÓNG project — giống hệt lúc đo cuối phiên, nên chênh lệch = thứ phiên chơi để lại
        callAny(request("project.close"));
        settle(400);
        memBase_ = memoryNow();
        openProject();
        send(cmd(LE_CMD_TRANSPORT_PLAY));
        for (int t = 0; t < 8; ++t) send(cmd(LE_CMD_CLIP_LAUNCH, t, 0));
    }

    // project.open + 8 clip audio + 4 nhạc cụ + clip MIDI, chờ decode / sfz xong
    void openProject() {
        juce::var q = request("project.open");
        q.getDynamicObject()->setProperty("dir", proj_.getFullPathName());
        callOk(q);
        const char* files[] = {"clip_click_4beats_100.wav", "clip_sine_4beats_120.wav", "clip_click_4beats_120.wav",
                               "clip_silence_4beats_120.wav"};
        for (int t = 0; t < 4; ++t) {   // track 0..3: audio, slot 0..1 có clip
            for (int s = 0; s < 2; ++s) {
                juce::var c = request("clip.setAudio");
                auto* o = c.getDynamicObject();
                o->setProperty("track", t);
                o->setProperty("slot", s);
                o->setProperty("clipId", "a" + juce::String(t) + juce::String(s));
                o->setProperty("file", juce::String(kFix + files[(t + s) % 4]));
                o->setProperty("lengthBeats", 4.0);
                o->setProperty("originalBpm", (t + s) % 4 == 0 ? 100.0 : 120.0);
                o->setProperty("gainDb", -12.0);
                callOk(c);
            }
        }
        for (int t = 4; t < 8; ++t) {   // track 4..7: nhạc cụ + clip MIDI slot 0..1
            juce::var c = request("track.configure");
            c.getDynamicObject()->setProperty("track", t);
            c.getDynamicObject()->setProperty("kind", "instrument");
            c.getDynamicObject()->setProperty("name", "inst " + juce::String(t));
            callOk(c);
            juce::var i = request("track.setInstrument");
            auto* inst = new juce::DynamicObject();
            inst->setProperty("kind", "sfz");
            inst->setProperty("path", t % 2 == 0 ? "fixtures/inst_synth/inst_synth.sfz" : "fixtures/kit_synth/kit_synth.sfz");
            i.getDynamicObject()->setProperty("track", t);
            i.getDynamicObject()->setProperty("instrument", juce::var(inst));
            callOk(i);
            for (int s = 0; s < 2; ++s) setMidi(t, s);
            send(cmd(LE_CMD_TRACK_GAIN, t, -1, 0, -18.0f));
        }
        send(cmd(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_1_BAR));
        // Buffer thu (66 s / track) được cấp phát lần đầu arm → cấp hết ngay từ đầu để "RSS phẳng" chỉ đo rò rỉ thật
        for (int t = 0; t < 8; ++t) {
            send(cmd(LE_CMD_TRACK_ARM, t, -1, 1));
            send(cmd(LE_CMD_TRACK_ARM, t, -1, 0));
        }
        // chờ decode / sfz xong (worker) trước khi chơi
        for (int i = 0; i < 2000 && !jobs_.pending.empty(); ++i) {
            eng_->pump();
            checkEvents();
            if (const std::string e = jobs_.poll(callFn()); !e.empty()) violation("job", e);
            juce::Thread::sleep(2);
        }
        if (!jobs_.pending.empty()) violation("setup", "decode / sfz không xong sau 4 s");
        settle(200);
    }

    void setMidi(int t, int s) {
        juce::var c = request("clip.setMidi");
        auto* o = c.getDynamicObject();
        o->setProperty("track", t);
        o->setProperty("slot", s);
        o->setProperty("clipId", "m" + juce::String(t) + juce::String(s) + "_" + juce::String(++midiCounter_));
        const double len = rng_.pick(std::vector<double>{4.0, 8.0, 2.0});
        o->setProperty("lengthBeats", len);
        juce::Array<juce::var> notes;
        const int n = rng_.range(4, 24);
        for (int k = 0; k < n; ++k) {
            auto* nt = new juce::DynamicObject();
            nt->setProperty("p", rng_.range(36, 84));
            nt->setProperty("v", rng_.range(40, 127));
            nt->setProperty("s", std::floor(rng_.uniform(0.0, len) * 4.0) / 4.0);
            nt->setProperty("d", rng_.pick(std::vector<double>{0.25, 0.5, 1.0, 2.0}));
            notes.add(juce::var(nt));
        }
        o->setProperty("notes", notes);
        callOk(c);
    }

    // ─────────────── vòng chính ───────────────
    void loop() {
        const int64_t total = static_cast<int64_t>(opt_.minutes * 60.0 * sr_);
        const int64_t warm = static_cast<int64_t>(std::min(opt_.warmup, opt_.minutes * 0.5) * 60.0 * sr_);
        const int pumpEvery = std::max(1, static_cast<int>(std::lround(sr_ / 30.0 / opt_.block)));   // ~30 Hz
        std::vector<float> in(static_cast<size_t>(opt_.block)), L(in.size()), R(in.size());
        const float* ins[1] = {in.data()};
        float* outs[2] = {L.data(), R.data()};
        int64_t nextAction = 0, blocks = 0;
        double phase = 0.0;
        int lastMinute = -1;
        for (pos_ = 0; pos_ < total; pos_ += opt_.block) {
            std::snprintf(g_where, sizeof(g_where), "tái hiện: le-soak --seed %llu --minutes %g --block %d%s  (chết ở %.1f s nhạc)",
                          static_cast<unsigned long long>(opt_.seed), opt_.minutes, opt_.block, opt_.hostile ? " --hostile" : "",
                          static_cast<double>(pos_) / sr_);
            if (pos_ >= nextAction) {
                act();
                nextAction = pos_ + static_cast<int64_t>(rng_.uniform(0.05, 1.5) * sr_);   // 50 ms … 1.5 s giữa 2 hành động
            }
            for (int i = 0; i < opt_.block; ++i) {   // input: sine 220 Hz −12 dB + nhiễu nhỏ (tiếng người chơi)
                in[static_cast<size_t>(i)] = static_cast<float>(0.25 * std::sin(phase) + 0.01 * (rng_.uniform() - 0.5));
                phase += 2.0 * 3.14159265358979 * 220.0 / sr_;
            }
            if (phase > 1e6) phase = std::fmod(phase, 2.0 * 3.14159265358979);
            dev_->render(ins, 1, outs, 2, opt_.block);
            checkOutput(L, R);
            if (++blocks % pumpEvery == 0) {
                eng_->pump();
                checkEvents();
                LeState st{};
                le_read_state(&st);
                if (const std::string e = checkState(st); !e.empty()) violation("LeState", e);
                if (blocks % (pumpEvery * 30) == 0)
                    if (const std::string e = jobs_.poll(callFn()); !e.empty()) violation("job", e);
            }
            if (pos_ >= warm && memWarm_.footprintMb == 0.0) memWarm_ = memoryNow();
            if (blocks % static_cast<int64_t>(std::max(1.0, 10.0 * sr_ / opt_.block)) == 0) memMax_ = std::max(memMax_, memoryNow().footprintMb);
            const int minute = static_cast<int>(static_cast<double>(pos_) / sr_ / 60.0);
            if (minute != lastMinute) {
                lastMinute = minute;
                const Mem m = memoryNow();
                memMax_ = std::max(memMax_, m.footprintMb);
                if (opt_.verbose || minute % 5 == 0)
                    std::printf("  %3d phút · footprint %.1f MB · RSS %.1f MB · take xong %d · hành động %d · vi phạm %d\n", minute,
                                m.footprintMb, m.residentMb, takesDone_, actions_, violations_);
            }
        }
    }

    void act() {
        ++actions_;
        const int k = rng_.below(100);
        const int t = rng_.range(0, 7);
        const int s = rng_.range(0, 7);
        if (k < 18) send(cmd(LE_CMD_CLIP_LAUNCH, t, rng_.range(0, 2)));
        else if (k < 22) send(cmd(LE_CMD_SCENE_LAUNCH, -1, rng_.range(0, 2)));
        else if (k < 26) send(cmd(LE_CMD_CLIP_STOP, t));
        else if (k < 27) send(cmd(LE_CMD_STOP_ALL));
        else if (k < 35) {   // thu vào ô trống (audio 0..3 → take audio, 4..7 → take MIDI)
            LeState st{};
            le_read_state(&st);
            for (int tries = 0; tries < 8; ++tries) {
                const int slot = rng_.range(2, 7);
                if (st.clipState[t][slot] == LE_CLIP_EMPTY) {
                    send(cmd(LE_CMD_CLIP_RECORD, t, slot, rng_.pick(std::vector<int>{0, 1, 1, 2, 4})));
                    break;
                }
            }
        } else if (k < 39) send(cmd(LE_CMD_RECORD_STOP, t));
        else if (k < 45) {
            const int tr = rng_.range(0, 3);
            send(cmd(LE_CMD_OVERDUB_TOGGLE, tr));
            if (opt_.hostile && rng_.chance(0.5)) {   // tắt / bật lại dồn dập trong < 50 ms (đường race R1)
                renderQuiet(rng_.range(1, 8));
                send(cmd(LE_CMD_OVERDUB_TOGGLE, tr));
                renderQuiet(rng_.range(1, 4));
                send(cmd(LE_CMD_OVERDUB_TOGGLE, tr));
            }
        } else if (k < 60) {
            const int tr = rng_.range(4, 7);
            const int note = rng_.range(40, 80);
            send(cmd(LE_CMD_NOTE_ON, tr, -1, note, static_cast<float>(rng_.uniform(0.3, 1.0))));
            held_.push_back({tr, note});
            if (held_.size() > 6 || rng_.chance(0.5)) {
                const auto h = held_.front();
                held_.erase(held_.begin());
                send(cmd(LE_CMD_NOTE_OFF, h.first, -1, h.second));
            }
        } else if (k < 63) send(cmd(LE_CMD_SET_BPM, -1, -1, 0, 0.0f, rng_.pick(std::vector<double>{120.0, 120.0, 100.0, 90.0, 140.0, 75.0})));
        else if (k < 65) send(cmd(LE_CMD_SET_QUANTIZE, -1, -1, rng_.pick(std::vector<int>{LE_Q_NONE, LE_Q_1_4, LE_Q_1_BAR, LE_Q_2_BAR})));
        else if (k < 66) send(cmd(LE_CMD_METRONOME, -1, -1, rng_.range(0, 2), 0.5f));
        else if (k < 71) {
            const int m = rng_.below(4);
            if (m == 0) send(cmd(LE_CMD_TRACK_GAIN, t, -1, 0, static_cast<float>(rng_.uniform(-40.0, 0.0))));
            else if (m == 1) send(cmd(LE_CMD_TRACK_PAN, t, -1, 0, static_cast<float>(rng_.uniform(-1.0, 1.0))));
            else if (m == 2) send(cmd(LE_CMD_TRACK_MUTE, t, -1, rng_.range(0, 1)));
            else send(cmd(LE_CMD_TRACK_SOLO, t, -1, rng_.chance(0.2) ? 1 : 0));
        } else if (k < 78) {
            juce::var q = request("fx.set");
            auto* o = q.getDynamicObject();
            o->setProperty("track", t);
            o->setProperty("index", rng_.range(0, 2));
            o->setProperty("type", rng_.pick(std::vector<juce::String>{"filter", "delay", "reverb", "eq3", "comp"}));
            auto* p = new juce::DynamicObject();
            p->setProperty("0", rng_.uniform(0.0, 1.0));
            p->setProperty("1", rng_.uniform(0.0, 1.0));
            o->setProperty("params", juce::var(p));
            o->setProperty("bypass", rng_.chance(0.1));
            callAny(q);
        } else if (k < 81) {
            juce::var q = request("fx.remove");
            q.getDynamicObject()->setProperty("track", t);
            q.getDynamicObject()->setProperty("index", rng_.range(0, 2));
            callAny(q);
        } else if (k < 88) {
            send(cmd(LE_CMD_FX_PARAM, t, rng_.range(0, 2), rng_.range(0, 3), static_cast<float>(rng_.uniform(0.0, 1.0))));
        } else if (k < 90) {
            juce::var q = request("clip.clear");
            q.getDynamicObject()->setProperty("track", t);
            q.getDynamicObject()->setProperty("slot", rng_.range(2, 7));   // giữ slot 0..1 để luôn có gì phát
            callAny(q);
        } else if (k < 92) {
            juce::var q = request("clip.undoOverdub");
            q.getDynamicObject()->setProperty("track", rng_.range(0, 3));
            q.getDynamicObject()->setProperty("slot", rng_.range(0, 7));
            callAny(q);
        } else if (k < 94) {
            setMidi(rng_.range(4, 7), rng_.range(0, 1));
        } else if (k < 95) {
            send(cmd(LE_CMD_TRANSPORT_STOP));
            send(cmd(LE_CMD_TRANSPORT_PLAY));
            for (int tr = 0; tr < 8; ++tr) send(cmd(LE_CMD_CLIP_LAUNCH, tr, 0));
        } else if (k < 97) {
            send(cmd(LE_CMD_MASTER_GAIN, -1, -1, 0, static_cast<float>(rng_.uniform(-12.0, 3.0))));
        } else {
            juce::var q = request("launchLog.read");
            q.getDynamicObject()->setProperty("sinceIndex", 0);
            callAny(q);
        }
        (void) s;
    }

    // Render + pump ~n block (snapshot cũ retire, job xong) — dùng quanh các mốc đo bộ nhớ
    void settle(int blocks) {
        std::vector<float> in(static_cast<size_t>(opt_.block)), L(in.size()), R(in.size());
        const float* ins[1] = {in.data()};
        float* outs[2] = {L.data(), R.data()};
        for (int i = 0; i < blocks; ++i) {
            dev_->render(ins, 1, outs, 2, opt_.block);
            eng_->pump();
            checkEvents();
            if (i % 10 == 0) juce::Thread::sleep(1);
        }
        if (const std::string e = jobs_.poll(callFn()); !e.empty()) violation("job", e);
    }

    // Render thêm vài block KHÔNG pump (giữa các lệnh dồn dập)
    void renderQuiet(int blocks) {
        std::vector<float> in(static_cast<size_t>(opt_.block), 0.1f), L(in.size()), R(in.size());
        const float* ins[1] = {in.data()};
        float* outs[2] = {L.data(), R.data()};
        for (int i = 0; i < blocks; ++i) {
            dev_->render(ins, 1, outs, 2, opt_.block);
            checkOutput(L, R);
            pos_ += opt_.block;
        }
    }

    // ─────────────── kiểm ───────────────
    void checkOutput(const std::vector<float>& L, const std::vector<float>& R) {
        for (size_t i = 0; i < L.size(); ++i) {
            const float a = L[i], b = R[i];
            if (!std::isfinite(a) || !std::isfinite(b) || std::fabs(a) > 1.0001f || std::fabs(b) > 1.0001f) {
                if (outputViolations_++ < 3) {
                    char buf[160];
                    std::snprintf(buf, sizeof(buf), "output mẫu %zu = (%g, %g) tại %.3f s nhạc", i, a, b, static_cast<double>(pos_) / sr_);
                    violation("output", buf);
                }
                return;
            }
        }
    }

    void checkEvents() {
        for (const std::string& a : StderrWatch::instance().sync()) violation("JUCE assertion", a);
        auto& ev = events();
        for (; seen_ < ev.size(); ++seen_) {
            const Event& e = ev[seen_];
            if (const std::string err = checkEvent(e); !err.empty()) violation("event", err);
            if (e.type == LE_EVT_RECORDING_FINISHED && e.a >= 0 && e.a < 4 && e.b >= 0) checkTakeFile(e.a, e.b);
            if (e.type == LE_EVT_RECORDING_FINISHED && e.a >= 4) ++midiTakes_;
            if (e.type == LE_EVT_ERROR) ++errorEvents_[e.a];
        }
    }

    // RECORDING_FINISHED của track audio → file đã đóng (04 §5.6) → phải có trên đĩa
    void checkTakeFile(int t, int s) {
        ++takesDone_;
        juce::var q = request("clip.info");
        q.getDynamicObject()->setProperty("track", t);
        q.getDynamicObject()->setProperty("slot", s);
        const CallResult r = call(q);
        if (!r.envelopeOk || !r.ok) return violation("clip.info", r.raw);
        const juce::String kind = r.result["kind"].toString();
        if (kind != "audio") return;   // đã bị xoá / thay trước khi event tới
        const juce::String file = r.result["file"].toString();
        const juce::File f = juce::File::isAbsolutePath(file) ? juce::File(file) : proj_.getChildFile(file);
        if (file.isEmpty() || !f.existsAsFile() || f.getSize() < 64)
            violation("take", "RECORDING_FINISHED(" + std::to_string(t) + "," + std::to_string(s) + ") nhưng file \"" +
                                  file.toStdString() + "\" không có / rỗng");
        else
            ++takeFilesOk_;
    }

    void finish() {
        send(cmd(LE_CMD_STOP_ALL));
        std::vector<float> in(static_cast<size_t>(opt_.block)), L(in.size()), R(in.size());
        const float* ins[1] = {in.data()};
        float* outs[2] = {L.data(), R.data()};
        for (int i = 0; i < 3000 && !(jobs_.pending.empty() && i > 400); ++i) {   // ≥ ~1 s cho take / file cuối
            dev_->render(ins, 1, outs, 2, opt_.block);
            eng_->pump();
            checkEvents();
            if (i % 20 == 0)
                if (const std::string e = jobs_.poll(callFn()); !e.empty()) violation("job", e);
            juce::Thread::sleep(1);
        }
        const Mem end = memoryNow();
        const double growth = end.footprintMb - memWarm_.footprintMb;
        // Đóng project → model / snapshot / clip / undo được thả; render + pump cho snapshot cũ retire về main
        juce::var q = request("project.close");
        callAny(q);
        settle(400);
        const Mem closed = memoryNow();
        if (opt_.holdSec > 0) {   // soi heap khi engine CÒN SỐNG, ngay sau project.close
            std::printf("sau project.close — giữ %d s (pid %d) để soi heap…\n", opt_.holdSec, static_cast<int>(getpid()));
            juce::Thread::sleep(opt_.holdSec * 1000);
        }
        const double leak = closed.heapInUseMb - memBase_.heapInUseMb;   // byte còn cấp thật so với nền
        std::printf("\n══ le-soak seed %llu · %.1f phút nhạc · block %d%s ══\n", static_cast<unsigned long long>(opt_.seed),
                    static_cast<double>(pos_) / sr_ / 60.0, opt_.block, opt_.hostile ? " · hostile" : "");
        std::printf("hành động %d · lệnh gửi %d (bị từ chối %d) · le_call %d · take audio xong %d (có file %d) · take MIDI %d\n",
                    actions_, sends_, rejected_, calls_, takesDone_, takeFilesOk_, midiTakes_);
        std::printf("footprint: nền %.1f MB · sau %g phút %.1f MB → cuối phiên %.1f MB (tăng trong phiên %.1f MB, max %.1f MB)\n",
                    memBase_.footprintMb, std::min(opt_.warmup, opt_.minutes * 0.5), memWarm_.footprintMb, end.footprintMb,
                    growth, memMax_);
        std::printf("sau project.close: heap đang cấp %.1f MB (nền, project đã đóng trước phiên: %.1f) → phiên để lại %+.1f MB (ngưỡng %.1f) · footprint %.1f MB (nền %.1f) · RSS %.1f MB\n",
                    closed.heapInUseMb, memBase_.heapInUseMb, leak, opt_.maxLeakMb, closed.footprintMb, memBase_.footprintMb,
                    closed.residentMb);
        for (const auto& [code, n] : errorEvents_) std::printf("LE_EVT_ERROR %d × %d\n", code, n);
        if (leak > opt_.maxLeakMb)
            violation("memory", "sau project.close còn giữ " + std::to_string(leak) + " MB so với nền > " + std::to_string(opt_.maxLeakMb) + " MB");
        if (!jobs_.pending.empty()) violation("job", std::to_string(jobs_.pending.size()) + " job chưa kết thúc");
        eng_.reset();
        for (const std::string& a : StderrWatch::instance().sync()) violation("JUCE assertion", a);   // lúc huỷ engine
        const Mem gone = memoryNow();
        std::printf("sau khi huỷ engine: heap đang cấp %.1f MB (trước khi tạo engine %.1f MB) · footprint %.1f MB (trước %.1f MB)\n",
                    gone.heapInUseMb, memPre_.heapInUseMb, gone.footprintMb, memPre_.footprintMb);
        std::printf("vi phạm: %d\n", violations_);

    }

    // ─────────────── gọi engine ───────────────
    void send(const LeCommand& c) {
        ++sends_;
        if (!eng_->send(c)) ++rejected_;
    }
    CallResult call(const juce::var& q) {
        ++calls_;
        CallResult r;
        const auto t0 = std::chrono::steady_clock::now();
        r.raw = eng_->call(toJson(q).c_str());
        r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        juce::var v;
        if (juce::JSON::parse(juce::String::fromUTF8(r.raw.c_str()), v).failed()) r.envelopeError = "không phải JSON";
        else r.envelopeError = checkEnvelope(v, r);
        r.envelopeOk = r.envelopeError.empty();
        if (!r.envelopeOk) violation("envelope", toJson(q) + " → " + r.raw);
        jobs_.noteResponse(r);
        return r;
    }
    std::function<CallResult(const std::string&)> callFn() {
        return [this](const std::string& json) {
            CallResult r;
            r.raw = eng_->call(json.c_str());
            juce::var rv;
            if (juce::JSON::parse(juce::String::fromUTF8(r.raw.c_str()), rv).failed()) r.envelopeError = "không phải JSON";
            else r.envelopeError = checkEnvelope(rv, r);
            r.envelopeOk = r.envelopeError.empty();
            return r;
        };
    }
    void callOk(const juce::var& q) {
        const CallResult r = call(q);
        if (r.envelopeOk && !r.ok) violation("setup", toJson(q) + " → " + r.raw);
    }
    void callAny(const juce::var& q) { (void) call(q); }

    void violation(const std::string& what, const std::string& detail) {
        ++violations_;
        if (violations_ <= 20)
            std::printf("\n❌ VI PHẠM [%s] seed %llu tại %.3f s nhạc: %s\n   %s\n", what.c_str(), static_cast<unsigned long long>(opt_.seed),
                        static_cast<double>(pos_) / sr_, detail.c_str(), g_where);
    }

    const Options& opt_;
    Rng rng_;
    juce::File tmp_, proj_;
    std::string dataDir_, libDir_;
    std::unique_ptr<le::core::Engine> eng_;
    le::io::OfflineDeviceIO* dev_ = nullptr;
    double sr_ = 48000.0;
    int64_t pos_ = 0;
    JobTracker jobs_;
    size_t seen_ = 0;
    std::vector<std::pair<int, int>> held_;
    Mem memWarm_{}, memBase_{}, memPre_{};
    double memMax_ = 0.0;
    int violations_ = 0, outputViolations_ = 0, actions_ = 0, sends_ = 0, rejected_ = 0, calls_ = 0;
    int takesDone_ = 0, takeFilesOk_ = 0, midiTakes_ = 0, midiCounter_ = 0;
    std::map<int, int> errorEvents_;
};

} // namespace

int main(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const bool next = i + 1 < argc;
        if (std::strcmp(argv[i], "--seed") == 0 && next) o.seed = std::strtoull(argv[++i], nullptr, 10);
        else if (std::strcmp(argv[i], "--minutes") == 0 && next) o.minutes = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--warmup") == 0 && next) o.warmup = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--block") == 0 && next) o.block = std::clamp(std::atoi(argv[++i]), 16, 4096);
        else if ((std::strcmp(argv[i], "--max-leak-mb") == 0 || std::strcmp(argv[i], "--max-growth-mb") == 0) && next) o.maxLeakMb = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--hostile") == 0) o.hostile = true;
        else if (std::strcmp(argv[i], "--hold") == 0 && next) o.holdSec = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--verbose") == 0) o.verbose = true;
        else {
            std::fprintf(stderr, "usage: le-soak [--seed N] [--minutes 30] [--block 128] [--warmup 5] [--max-leak-mb 8] [--hostile] [--verbose]\n");
            return 2;
        }
    }
    if (!(o.minutes > 0.0)) return 2;
    StderrWatch::instance().start();   // "JUCE Assertion failure" trên stderr → vi phạm (api_client.h)
    std::signal(SIGSEGV, onSignal);
    std::signal(SIGBUS, onSignal);
    std::signal(SIGABRT, onSignal);
    std::signal(SIGILL, onSignal);
#if defined(LE_HAVE_SANITIZER_API)
    __sanitizer_set_death_callback(&writeWhere);
#endif
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    const auto t0 = std::chrono::steady_clock::now();
    Soak soak(o);
    const int v = soak.run();
    std::printf("thời gian chạy thật: %.1f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    return v > 0 ? 1 : 0;
}
