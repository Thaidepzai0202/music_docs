// le-fuzz-api — fuzz C API của engine (chuẩn bị P4-16, agent 80). [main] (CLI)
// Chuỗi le_call / le_send / le_get_peaks / le_audio_start|stop ngẫu nhiên, có seed (tái lập được), chỉ qua C API như
// app Dart: op hợp lệ lẫn không hợp lệ, tham số biên và sai kiểu, JSON hỏng, xen kẽ render (sim.advance) hoặc
// chạy audio thật. Bất biến (vi phạm → mã thoát 1, in cách tái hiện):
//   • không crash (ASan/UBSan/TSan báo → death callback in seed + bước)
//   • le_call luôn trả envelope đúng 05 §3
//   • LeState hợp lý (clipState trong dải, mọi số thực hữu hạn, peak ≥ 0, master ≤ 0 dBFS, bpm 20..300 …)
//   • event hợp lệ; mọi job đã trả jobId đều KẾT THÚC (done/failed) trong thời gian giới hạn và tra được bằng job.result
//   • bản debug: KHÔNG có "JUCE Assertion failure" nào trên stderr (StderrWatch, gán vào bước / seed)
//
//   le-fuzz-api [--seed N | --seeds A-B] [--ops 300] [--mode sim|device] [--verbose] [--selftest]
//     --mode sim     (mặc định) sim.offline + sim.advance: deterministic, chạy dưới ASan+UBSan (preset mac-asan)
//     --mode device  audio thật (CoreAudio, không mic) + Timer 30 Hz: dùng cho TSan (preset mac-tsan)
// Chuỗi LỆNH chỉ phụ thuộc seed (không phụ thuộc response), nên tái hiện được; thời điểm job trên worker xong thì có
// thể lệch vài bước giữa các lần chạy. Cách chạy chi tiết: tools/docs/fuzz-soak.md.
// So sánh float bằng != với giá trị canh (sentinel) là CÓ CHỦ ĐÍCH.
#pragma clang diagnostic ignored "-Wfloat-equal"
#include "api_client.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unistd.h>
#include <vector>

#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(undefined_behavior_sanitizer)
#include <sanitizer/common_interface_defs.h>
#define LE_HAVE_SANITIZER_API 1
#endif
#endif
#if !defined(LE_HAVE_SANITIZER_API) && defined(__SANITIZE_ADDRESS__)
#include <sanitizer/common_interface_defs.h>
#define LE_HAVE_SANITIZER_API 1
#endif

using namespace le::tools;

namespace {

// ── In cách tái hiện khi chết (signal / sanitizer) ──
char g_repro[512] = "";
void writeRepro() {
    StderrWatch::instance().emergencyRestore();   // fd 2 về stderr thật, đổ nốt pipe (báo cáo sanitizer không bị nuốt)
    const char head[] = "\n💥 le-fuzz-api chết — tái hiện: ";
    (void) !write(2, head, sizeof(head) - 1);
    (void) !write(2, g_repro, std::strlen(g_repro));
    (void) !write(2, "\n", 1);
}
void onSignal(int sig) {
    writeRepro();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

struct Options {
    uint64_t seedFrom = 1, seedTo = 1;
    int ops = 300;
    bool device = false;
    bool verbose = false;
    bool selftest = false;   // tự gây 1 JUCE assertion, kiểm StderrWatch bắt được (bản debug)
};

struct Stats {
    int calls = 0, sends = 0, sendRejected = 0, advances = 0, violations = 0, internalErrors = 0, restarts = 0;
    std::map<std::string, int> opCount;
    std::map<std::string, double> opMaxMs;   // op chậm nhất trên main (không tính sim.advance)
};

const std::string kFixtures = std::string(LE_ENGINE_DIR) + "/tests/fixtures/";

class Fuzzer {
public:
    Fuzzer(uint64_t seed, const Options& o, Stats& st) : rng_(seed), seed_(seed), opt_(o), st_(st) {}

    // Trả số vi phạm của seed này; −1 = bỏ qua (không mở được thiết bị).
    int run() {
        tmp_ = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("le-fuzz-" + juce::String(getpid()) + "-" + juce::String(static_cast<juce::int64>(seed_)));
        tmp_.deleteRecursively();
        tmp_.createDirectory();
        if (!create()) {
            le_destroy();
            tmp_.deleteRecursively();
            return violations_;
        }
        if (skipped_) return -1;
        for (step_ = 0; step_ < opt_.ops; ++step_) {
            std::snprintf(g_repro, sizeof(g_repro), "le-fuzz-api --mode %s --seed %llu --ops %d --verbose",
                          opt_.device ? "device" : "sim", static_cast<unsigned long long>(seed_), step_ + 1);
            doStep();
            checkAfterStep();
        }
        std::snprintf(g_repro, sizeof(g_repro), "le-fuzz-api --mode %s --seed %llu --ops %d --verbose  (lúc chờ job)",
                      opt_.device ? "device" : "sim", static_cast<unsigned long long>(seed_), opt_.ops);
        drainJobs();
        le_destroy();
        LeState after{};
        le_read_state(&after);   // [any] vẫn đọc được sau le_destroy
        checkAssertions();       // assertion lúc huỷ engine
        tmp_.deleteRecursively();
        return violations_;
    }

private:
    // ─────────────── engine ───────────────
    bool create() {
        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.preferredBufferSize = rng_.pick(std::vector<int>{64, 128, 256});
        cfg.preferredSampleRate = rng_.pick(std::vector<double>{44100.0, 48000.0});
        cfg.numInputChannels = opt_.device ? 0 : rng_.range(0, 2);   // device: không xin quyền mic
        dataDir_ = tmp_.getChildFile("data").getFullPathName().toStdString();
        libDir_ = std::string(LE_ENGINE_DIR) + "/tests";
        cfg.dataDir = dataDir_.c_str();
        cfg.libraryDir = libDir_.c_str();
        le_set_event_callback(&onEvent);
        if (const int32_t e = le_create(&cfg); e != LE_OK) return violation("le_create", "trả " + std::to_string(e));
        if (opt_.device) {
            if (le_audio_start() != LE_OK) {
                std::printf("  seed %llu: không mở được thiết bị audio → bỏ qua (mode device cần loa)\n",
                            static_cast<unsigned long long>(seed_));
                skipped_ = true;
                le_destroy();
            }
            return true;
        }
        juce::var q = request("sim.offline");
        q.getDynamicObject()->setProperty("sampleRate", rng_.pick(std::vector<double>{44100.0, 48000.0, 96000.0}));
        q.getDynamicObject()->setProperty("blockSize", rng_.pick(std::vector<int>{64, 128, 256, 512, 1024}));
        const CallResult r = call(toJson(q));
        if (!r.envelopeOk || !r.ok) return violation("sim.offline", r.raw);
        return true;
    }

    // ─────────────── sinh giá trị ───────────────
    juce::var edgeNumber() {
        switch (rng_.below(16)) {
            case 0: return 0;
            case 1: return -1;
            case 2: return 1;
            case 3: return 0.5;
            case 4: return 1e-9;
            case 5: return 1e9;
            case 6: return 1e308;
            case 7: return -1e308;
            case 8: return 4096;
            case 9: return 4097;
            case 10: return static_cast<juce::int64>(2147483647);
            case 11: return -2147483648.0;
            case 12: return 9.0e18;
            case 13: return "12";              // sai kiểu
            case 14: return juce::var();       // null
            default: return rng_.uniform(-1000.0, 1000.0);
        }
    }
    juce::var weird() {   // giá trị sai kiểu bất kỳ
        switch (rng_.below(6)) {
            case 0: return juce::var();
            case 1: return true;
            case 2: return "x";
            case 3: { juce::Array<juce::var> a; a.add(1); a.add("2"); return a; }
            case 4: return juce::var(new juce::DynamicObject());
            default: return edgeNumber();
        }
    }
    juce::var track() { return rng_.chance(0.9) ? juce::var(rng_.range(0, 7)) : juce::var(rng_.pick(std::vector<int>{-1, 8, 127, -128, 1000})); }
    juce::var slot() { return rng_.chance(0.9) ? juce::var(rng_.range(0, 7)) : juce::var(rng_.pick(std::vector<int>{-1, 8, 99})); }
    // clipId thành tên file audio/<clipId>.caf và key peaks → thỉnh thoảng có dấu / emoji (UTF-8 hợp lệ)
    std::string clipId() {
        const int k = rng_.range(0, 12);
        return rng_.chance(0.25) ? "Đàn \xf0\x9f\x8e\xb8 " + std::to_string(k) : "c" + std::to_string(k);
    }
    // Chuỗi UTF-8 HỢP LỆ (dựng bằng fromUTF8 — juce::String(const char*) là ASCII và làm hỏng chuỗi trước khi tới engine)
    static const std::vector<std::string>& validUtf8() {
        static const std::vector<std::string> v = {
            "Trống cơm – Nguyễn Thị Hương",                        // tiếng Việt dựng sẵn (NFC)
            "Tro\xcc\x82\xcc\x81ng co\xcc\x9b\xcc\x80m",          // cùng chữ nhưng tổ hợp dấu (NFD)
            "\xf0\x9f\xa5\x81\xf0\x9f\x8e\xb9\xf0\x9f\x8e\xb8",     // emoji 4 byte 🥁🎹🎸
            "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x8e\xa4 ZWJ",    // 👩‍🎤 (chuỗi ZWJ)
            "音楽 مرحبا עברית",                                      // CJK + RTL
            "\xef\xbb\xbf" "BOM đầu chuỗi",                         // BOM
            "\xf4\x8f\xbf\xbf max",                                 // U+10FFFF (hợp lệ, lớn nhất)
            "tab\tvà xuống\ndòng \"ngoặc\" \\ /",
        };
        return v;
    }
    juce::var randomString() {
        switch (rng_.below(6)) {
            case 0: return juce::String();
            case 1: case 2: return juce::String::fromUTF8(rng_.pick(validUtf8()).c_str());
            case 3: return juce::String::repeatedString("x", rng_.range(100, 5000));
            case 4: return juce::String("../../etc/passwd");
            default: return "track " + juce::String(rng_.range(0, 99));
        }
    }
    std::string audioFile() {
        switch (rng_.below(8)) {
            case 0: return kFixtures + "clip_click_4beats_100.wav";
            case 1: return kFixtures + "clip_click_4beats_120.wav";
            case 2: return kFixtures + "clip_sine_4beats_120.wav";
            case 3: return kFixtures + "clip_silence_4beats_120.wav";
            case 4: return kFixtures + "missing.wav";
            case 5: return kFixtures;                                   // thư mục
            case 6: return kFixtures + "inst_synth/inst_synth.sfz";    // sai định dạng
            default: return "fixtures/clip_sine_4beats_120.wav";       // tương đối (cần project.open)
        }
    }

    // Sửa request hợp lệ thành biên / sai kiểu với xác suất nhỏ
    void mutate(juce::var& q) {
        auto* o = q.getDynamicObject();
        auto& props = o->getProperties();
        if (props.size() <= 1) return;
        if (rng_.chance(0.25)) {
            const int i = rng_.range(1, props.size() - 1);   // giữ "op"
            o->setProperty(props.getName(i), rng_.chance(0.5) ? edgeNumber() : weird());
        }
        if (rng_.chance(0.08)) o->removeProperty(props.getName(rng_.range(1, props.size() - 1)));
        if (rng_.chance(0.05)) o->setProperty("junk", weird());
    }

    // ─────────────── request hợp lệ-ish cho từng op ───────────────
    juce::var validRequestFor(const std::string& op) {
        std::string name = op;
        return validRequest(name, op.c_str());
    }
    juce::var validRequest(std::string& opName, const char* forcedOp = nullptr) {
        static const std::vector<const char*> ops = {
            "engine.info", "audio.setInputEnabled", "job.result", "job.cancel", "project.open", "project.close",
            "transport.setTimeSignature", "track.configure", "track.setInstrument", "clip.setAudio", "clip.setMidi",
            "clip.getMidi", "clip.setParams", "clip.info", "clip.clear", "clip.undoOverdub", "launchLog.read",
            "midi.setRecordQuantize", "midiClip.quantize", "fx.set", "fx.remove", "spike.sessionInfo",
            "spike.setSessionMode", "spike.setBufferSize", "spike.latencyLoopback", "spike.stretchBench", "sim.offline",
            "clip.setAudio", "clip.setMidi", "fx.set", "track.setInstrument", "clip.info"};   // op hay dùng: nặng ký hơn
        const char* op = forcedOp != nullptr ? forcedOp : rng_.pick(ops);
        if (forcedOp == nullptr && std::strcmp(op, "spike.stretchBench") == 0 && !rng_.chance(0.1)) op = "engine.info";   // nặng → hiếm
        if (forcedOp == nullptr && std::strcmp(op, "sim.offline") == 0 && !rng_.chance(0.2)) op = "clip.info";
        opName = op;
        juce::var q = request(op);
        auto* o = q.getDynamicObject();
        const std::string s = op;
        if (s == "audio.setInputEnabled") o->setProperty("enabled", rng_.chance(0.5));
        else if (s == "job.result" || s == "job.cancel") o->setProperty("jobId", jobIdArg());
        else if (s == "project.open") {
            const int k = rng_.below(4);
            o->setProperty("dir", k == 3 ? juce::String("/nonexistent-le-fuzz/p")
                                         : tmp_.getChildFile(juce::String::fromUTF8(k == 2 ? "Dự án \xf0\x9f\x8e\xb5" : "proj")
                                                             + juce::String(k)).getFullPathName());
        } else if (s == "transport.setTimeSignature") {
            o->setProperty("num", rng_.range(0, 34));
            o->setProperty("den", rng_.pick(std::vector<int>{1, 2, 3, 4, 8, 16, 32}));
        } else if (s == "track.configure") {
            o->setProperty("track", track());
            o->setProperty("kind", rng_.pick(std::vector<juce::String>{"audio", "instrument", "midi", ""}));
            o->setProperty("name", randomString());
        } else if (s == "track.setInstrument") {
            o->setProperty("track", track());
            auto* inst = new juce::DynamicObject();
            inst->setProperty("kind", rng_.chance(0.9) ? "sfz" : (rng_.chance(0.5) ? "user" : "vst"));
            inst->setProperty("path", rng_.pick(std::vector<juce::String>{
                                          "fixtures/inst_synth/inst_synth.sfz", "fixtures/kit_synth/kit_synth.sfz",
                                          juce::String(kFixtures + "inst_synth/inst_synth.sfz"), "fixtures/none.sfz",
                                          juce::String(kFixtures + "clip_sine_4beats_120.wav"), ""}));
            o->setProperty("instrument", juce::var(inst));
        } else if (s == "clip.setAudio") {
            o->setProperty("track", track());
            o->setProperty("slot", slot());
            o->setProperty("clipId", juce::String::fromUTF8(clipId().c_str()));
            o->setProperty("file", juce::String::fromUTF8(audioFile().c_str()));
            o->setProperty("lengthBeats", rng_.pick(std::vector<double>{4.0, 4.0, 2.0, 8.0, 0.25, 1000.0}));
            o->setProperty("originalBpm", rng_.pick(std::vector<double>{100.0, 120.0, 120.0, 90.0, 20.0, 300.0}));
            if (rng_.chance(0.5)) o->setProperty("warp", rng_.pick(std::vector<juce::String>{"repitch", "stretch", "x"}));
            if (rng_.chance(0.5)) o->setProperty("gainDb", rng_.uniform(-30.0, 6.0));
        } else if (s == "clip.setMidi") {
            o->setProperty("track", track());
            o->setProperty("slot", slot());
            o->setProperty("clipId", juce::String::fromUTF8(clipId().c_str()));
            const double len = rng_.pick(std::vector<double>{4.0, 1.0, 16.0, 0.5});
            o->setProperty("lengthBeats", len);
            juce::Array<juce::var> notes;
            const int n = rng_.chance(0.03) ? 3000 : rng_.range(0, 24);
            for (int i = 0; i < n; ++i) {
                auto* nt = new juce::DynamicObject();
                nt->setProperty("p", rng_.chance(0.95) ? juce::var(rng_.range(24, 96)) : edgeNumber());
                nt->setProperty("v", rng_.chance(0.95) ? juce::var(rng_.range(1, 127)) : edgeNumber());
                nt->setProperty("s", rng_.chance(0.95) ? juce::var(rng_.uniform(0.0, len)) : edgeNumber());
                nt->setProperty("d", rng_.chance(0.95) ? juce::var(rng_.uniform(0.01, len)) : edgeNumber());
                notes.add(juce::var(nt));
            }
            o->setProperty("notes", notes);
        } else if (s == "clip.getMidi" || s == "clip.info" || s == "clip.clear" || s == "clip.undoOverdub") {
            o->setProperty("track", track());
            o->setProperty("slot", slot());
        } else if (s == "clip.setParams") {
            o->setProperty("track", track());
            o->setProperty("slot", slot());
            if (rng_.chance(0.7)) o->setProperty("gainDb", rng_.uniform(-130.0, 30.0));
            if (rng_.chance(0.5)) o->setProperty("warp", rng_.pick(std::vector<juce::String>{"repitch", "stretch"}));
        } else if (s == "launchLog.read") {
            o->setProperty("sinceIndex", rng_.pick(std::vector<int>{0, 0, 5, 100000, -1}));
        } else if (s == "midi.setRecordQuantize") {
            o->setProperty("grid", rng_.pick(std::vector<double>{0.0, 0.25, 0.5, 0.3, 1.0}));
        } else if (s == "midiClip.quantize") {
            o->setProperty("track", track());
            o->setProperty("slot", slot());
            o->setProperty("grid", rng_.pick(std::vector<double>{0.25, 0.5, 1.0, 0.0, 64.0}));
        } else if (s == "fx.set") {
            o->setProperty("track", rng_.chance(0.95) ? track() : juce::var(-1));
            o->setProperty("index", rng_.chance(0.9) ? juce::var(rng_.range(0, 2)) : juce::var(rng_.pick(std::vector<int>{-1, 3, 9})));
            o->setProperty("type", rng_.pick(std::vector<juce::String>{"filter", "delay", "reverb", "eq3", "comp", "compressor", "chorus"}));
            auto* params = new juce::DynamicObject();
            const int np = rng_.range(0, 5);
            for (int i = 0; i < np; ++i)
                params->setProperty(juce::String(rng_.chance(0.9) ? rng_.range(0, 4) : rng_.range(-3, 40)),
                                    rng_.chance(0.8) ? juce::var(rng_.uniform(-100.0, 20000.0)) : edgeNumber());
            o->setProperty("params", juce::var(params));
            o->setProperty("bypass", rng_.chance(0.2));
        } else if (s == "fx.remove") {
            o->setProperty("track", track());
            o->setProperty("index", rng_.range(-1, 3));
        } else if (s == "spike.setSessionMode") {
            o->setProperty("mode", rng_.pick(std::vector<juce::String>{"default", "measurement", "x"}));
        } else if (s == "spike.setBufferSize") {
            o->setProperty("frames", rng_.pick(std::vector<int>{64, 128, 256, 512, 1024, 100, 2048}));
        } else if (s == "spike.stretchBench") {
            juce::Array<juce::var> semis;
            semis.add(rng_.range(-12, 12));
            o->setProperty("semitones", semis);
            o->setProperty("saveDir", tmp_.getChildFile("bench").getFullPathName());
        } else if (s == "sim.offline") {
            o->setProperty("enabled", true);
            o->setProperty("sampleRate", rng_.pick(std::vector<double>{44100.0, 48000.0, 96000.0, 7000.0}));
            o->setProperty("blockSize", rng_.pick(std::vector<int>{64, 128, 256, 1024, 5000}));
        }
        if (forcedOp == nullptr) mutate(q);
        return q;
    }

    juce::var jobIdArg() {
        if (!knownJobs_.empty() && rng_.chance(0.8)) return static_cast<juce::int64>(rng_.pick(knownJobs_));
        return rng_.chance(0.5) ? juce::var(static_cast<juce::int64>(rng_.range(0, 100000))) : edgeNumber();
    }

    // JSON hỏng / không phải object / thiếu op / op lạ
    std::string garbageRequest() {
        static const std::vector<std::string> fixed = {
            "", " ", "{", "}", "[]", "null", "42", "\"op\"", "{\"op\":", "{\"op\":null}", "{\"op\":123}", "{\"op\":\"\"}",
            "{\"op\":\"engine.info\"", "{\"op\":\"engine.info\",}", "{\"op\":\"ENGINE.INFO\"}", "{\"op\":\"engine.info \"}",
            "{\"op\":\"foo.bar\"}", "{\"op\":[\"engine.info\"]}", "{\"op\":\"clip.info\",\"track\":1e999,\"slot\":0}",
            "{\"op\":\"clip.info\",\"track\":NaN,\"slot\":0}", "{\"op\":\"clip.info\",\"track\":-0,\"slot\":-0}",
            "{'op':'engine.info'}", "{\"op\":\"engine.info\"}{\"op\":\"engine.info\"}", "\xff\xfe\x00", "{\"op\":\"\xc3\x28\"}",
            "{\"op\":\"engine.info\",\"\":1}",                                           // key rỗng: JSON HỢP LỆ (RFC 8259)
            "{\"op\":\"fx.set\",\"track\":0,\"index\":0,\"type\":\"eq3\",\"params\":{\"\":3}}",
        };
        if (rng_.chance(0.3)) return invalidUtf8Request();
        if (rng_.chance(0.5)) return rng_.pick(fixed);
        std::string opName;
        std::string s = toJson(validRequest(opName));
        switch (rng_.below(4)) {
            case 0: s.resize(static_cast<size_t>(rng_.below(static_cast<int>(s.size()) + 1))); break;   // cắt cụt
            case 1: s.insert(static_cast<size_t>(rng_.below(static_cast<int>(s.size()) + 1)), 1, "{}[],:\"\\x0"[rng_.below(11)]); break;
            case 2: { const int d = rng_.range(10, 400); s = std::string(static_cast<size_t>(d), '[') + s + std::string(static_cast<size_t>(d), ']'); break; }
            default: for (char& c : s) if (rng_.chance(0.02)) c = static_cast<char>(rng_.range(32, 126)); break;
        }
        return s;
    }

    // Request đúng cú pháp JSON nhưng chuỗi bên trong là UTF-8 KHÔNG hợp lệ (byte thô chèn thẳng vào JSON, vì
    // juce::var / JSON::toString không mang được byte hỏng) hoặc escape surrogate lẻ.
    std::string invalidUtf8Request() {
        static const std::vector<std::string> bad = {
            "\x80", "abc\xbf", "\xc3", "\xe2\x82", "\xc0\xaf", "\xe0\x80\xaf", "\xf0\x80\x80\xaf",   // lẻ / cụt / overlong
            "\xed\xa0\x80", "\xed\xbf\xbf",                                                   // surrogate mã hoá UTF-8
            "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xfe\xff", "Tr\xe1\xbb",                  // > U+10FFFF, FE/FF, cụt giữa chữ
            "\\ud800", "\\udc00x", "\\ud83d", "a\\u0000b",                                       // escape JSON: surrogate lẻ, NUL
            "Tr\\u1ed1ng \\ud83e\\udd41",                                                       // escape HỢP LỆ: "Trống 🥁"
        };
        static const std::vector<std::pair<const char*, const char*>> sites = {   // (op, tên tham số chuỗi)
            {"track.configure", "name"}, {"clip.setMidi", "clipId"}, {"clip.setAudio", "clipId"}, {"clip.setAudio", "file"},
            {"fx.set", "type"}, {"project.open", "dir"}, {"spike.setSessionMode", "mode"}, {"track.setInstrument", "kind"}};
        const auto& site = rng_.pick(sites);
        std::string op = site.first;
        juce::var q = validRequestFor(op);
        auto* o = q.getDynamicObject();
        const std::string ph = "@@BAD@@";
        if (std::string(site.second) == "kind" && q["instrument"].isObject()) q["instrument"].getDynamicObject()->setProperty("kind", juce::String(ph));
        else o->setProperty(site.second, juce::String(ph));
        if (rng_.chance(0.15)) o->setProperty("op", juce::String(ph));   // cả tên op cũng hỏng
        std::string json = toJson(q);
        const std::string b = rng_.pick(bad);
        for (size_t at; (at = json.find(ph)) != std::string::npos;) json.replace(at, ph.size(), b);
        return json;
    }

    // ─────────────── le_send ───────────────
    LeCommand randomCommand() {
        static const std::vector<int> types = {1, 2, 3, 4, 5, 6, 10, 10, 10, 11, 12, 13, 14, 14, 15, 16, 16, 20, 21, 22, 23, 24,
                                               25, 26, 30, 30, 31, 32, 40, 40, 41, 42, 900, 901, 903, 904};
        LeCommand c{};
        const bool sane = rng_.chance(0.75);
        c.type = static_cast<uint16_t>(sane || rng_.chance(0.5) ? rng_.pick(types) : rng_.range(0, 65535));
        c.track = static_cast<int8_t>(sane ? rng_.range(-1, 7) : rng_.range(-128, 127));
        c.slot = static_cast<int8_t>(sane ? rng_.range(-1, 7) : rng_.range(-128, 127));
        c.i0 = sane ? rng_.range(-1, 130) : static_cast<int32_t>(rng_.next());
        auto f = [&](float lo, float hi) {
            if (sane) return static_cast<float>(rng_.uniform(lo, hi));
            switch (rng_.below(5)) {
                case 0: return std::numeric_limits<float>::quiet_NaN();
                case 1: return std::numeric_limits<float>::infinity();
                case 2: return -std::numeric_limits<float>::infinity();
                case 3: return 3.0e38f;
                default: return static_cast<float>(rng_.uniform(-1e6, 1e6));
            }
        };
        c.f0 = f(-1.0f, 1.0f);
        c.f1 = f(0.0f, 1.0f);
        c.d0 = sane ? rng_.uniform(20.0, 300.0) : static_cast<double>(f(-1e6f, 1e6f));
        if (sane && c.type == LE_CMD_NOTE_ON) c.f0 = static_cast<float>(rng_.uniform(0.01, 1.0));
        if (sane && c.type == LE_CMD_TRACK_GAIN) c.f0 = static_cast<float>(rng_.uniform(-60.0, 6.0));
        return c;
    }

    // ─────────────── một bước ───────────────
    void doStep() {
        const int k = rng_.below(100);
        if (k < 24) {
            std::string op;
            const juce::var q = validRequest(op);
            doCall(op, toJson(q));
        } else if (k < 32) {
            doCall("<garbage>", garbageRequest());
        } else if (k < 64) {
            const LeCommand c = randomCommand();
            const bool accepted = le_send(&c);
            ++st_.sends;
            if (!accepted) ++st_.sendRejected;
            if (opt_.verbose)
                std::printf("  #%d le_send type %u track %d slot %d i0 %d f0 %g f1 %g d0 %g → %s\n", step_, c.type, c.track, c.slot,
                            c.i0, c.f0, c.f1, c.d0, accepted ? "nhận" : "từ chối");
        } else if (k < 82) {
            advance();
        } else if (k < 88) {
            peaks();
        } else if (k < 93) {
            juce::var q = request(rng_.chance(0.7) ? "job.result" : "job.cancel");
            q.getDynamicObject()->setProperty("jobId", jobIdArg());
            doCall(q["op"].toString().toStdString(), toJson(q));
        } else if (k < 96) {
            const bool stop = rng_.chance(0.5);
            if (stop) le_audio_stop();
            else (void) le_audio_start();   // sim: start lại OfflineDeviceIO; device: CoreAudio
            if (opt_.verbose) std::printf("  #%d le_audio_%s\n", step_, stop ? "stop" : "start");
        } else if (k < 97) {
            restart();
        } else {
            doCall("engine.info", toJson(request("engine.info")));
        }
    }

    void doCall(const std::string& op, const std::string& json) {
        const CallResult r = call(json);
        ++st_.calls;
        ++st_.opCount[op];
        if (op != "sim.advance") st_.opMaxMs[op] = std::max(st_.opMaxMs[op], r.ms);
        if (opt_.verbose)
            std::printf("  #%d le_call %.200s\n       → %.300s  (%.2f ms)\n", step_, json.c_str(), r.raw.c_str(), r.ms);
        if (!r.envelopeOk) {
            violation("envelope", op + ": " + r.envelopeError + "\n    request: " + json.substr(0, 400) + "\n    response: " + r.raw.substr(0, 400));
            return;
        }
        if (!r.ok && r.code == "INTERNAL") {
            ++st_.internalErrors;   // không phải vi phạm, nhưng là mã lỗi mơ hồ: báo trong tổng kết
            if (internalSamples_.size() < 5) internalSamples_.push_back(op + ": " + r.message);
        }
        if (r.ok && r.result.hasProperty("jobId")) {
            jobs_.noteResponse(r);
            knownJobs_.push_back(static_cast<int64_t>(r.result["jobId"]));
        }
    }

    void advance() {
        ++st_.advances;
        if (opt_.device) {
            runLoop(rng_.uniform(0.002, 0.04));
            return;
        }
        juce::var q = request("sim.advance");
        if (rng_.chance(0.8)) q.getDynamicObject()->setProperty("frames", rng_.pick(std::vector<int>{1, 63, 128, 1000, 4096, 24000, 96000}));
        else q.getDynamicObject()->setProperty("beats", rng_.pick(std::vector<double>{0.25, 1.0, 4.0, 16.0}));
        doCall("sim.advance", toJson(q));
    }

    void peaks() {
        const std::string id = rng_.chance(0.8) ? clipId() : std::string("rec_x");
        const int level = rng_.range(-1, 12);
        const int maxPairs = rng_.pick(std::vector<int>{-1, 0, 1, 64, 4096, 200000});
        std::vector<float> buf(static_cast<size_t>(std::max(0, maxPairs)) * 2 + 1, 12345.0f);
        const bool nullBuf = rng_.chance(0.05);
        const int32_t n = le_get_peaks(rng_.chance(0.05) ? nullptr : id.c_str(), level, nullBuf ? nullptr : buf.data(), maxPairs);
        if (n > std::max(0, maxPairs)) violation("le_get_peaks", "trả " + std::to_string(n) + " cặp > maxPairs " + std::to_string(maxPairs));
        if (n > 0 && buf[static_cast<size_t>(2 * n)] != 12345.0f) violation("le_get_peaks", "ghi quá maxPairs (tràn buffer)");
        for (int i = 0; i < 2 * std::max(0, n); ++i)
            if (!std::isfinite(buf[static_cast<size_t>(i)])) {
                violation("le_get_peaks", "peak không hữu hạn");
                break;
            }
    }

    void restart() {   // như app bị huỷ rồi tạo lại (le_destroy khi job còn chạy, audio còn chạy)
        ++st_.restarts;
        if (opt_.verbose) std::printf("  #%d le_destroy + le_create\n", step_);
        le_destroy();
        jobs_.pending.clear();
        knownJobs_.clear();
        create();
    }

    void checkAssertions() {
        for (const std::string& a : StderrWatch::instance().sync()) violation("JUCE assertion", a);
    }

    void checkAfterStep() {
        checkAssertions();
        LeState s{};
        le_read_state(&s);
        if (const std::string e = checkState(s); !e.empty()) violation("LeState", e);
        auto& ev = events();
        for (; seenEvents_ < ev.size(); ++seenEvents_)
            if (const std::string e = checkEvent(ev[seenEvents_]); !e.empty()) violation("event", e);
    }

    void drainJobs() {
        const auto t0 = std::chrono::steady_clock::now();
        const double limitSec = 60.0;
        while (!jobs_.pending.empty()) {
            runLoop(0.005);   // Timer 30 Hz của JUCE → Engine::pump (như app); sim.advance bị từ chối khi audio đã stop
            if (opt_.device) runLoop(0.015);
            else {
                juce::var q = request("sim.advance");
                q.getDynamicObject()->setProperty("frames", 4800);
                (void) call(toJson(q));   // có thể lỗi nếu audio đã bị stop: job phải tự kết thúc (timeout của nó)
                juce::Thread::sleep(2);
            }
            if (const std::string e = jobs_.poll(); !e.empty()) violation("job", e);
            if (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() > limitSec) {
                std::string ids;
                for (int64_t id : jobs_.pending) ids += " " + std::to_string(id);
                violation("job", "job không kết thúc sau " + std::to_string(static_cast<int>(limitSec)) + " s:" + ids);
                break;
            }
        }
        checkAfterStep();
    }

    bool violation(const std::string& what, const std::string& detail) {
        ++violations_;
        ++st_.violations;
        std::printf("\n❌ VI PHẠM [%s] seed %llu bước %d: %s\n   tái hiện: %s\n", what.c_str(),
                    static_cast<unsigned long long>(seed_), step_, detail.c_str(), g_repro);
        return false;
    }

    Rng rng_;
    uint64_t seed_;
    const Options& opt_;
    Stats& st_;
    juce::File tmp_;
    std::string dataDir_, libDir_;
    JobTracker jobs_;
    std::vector<int64_t> knownJobs_;
    size_t seenEvents_ = 0;
    int step_ = -1;
    int violations_ = 0;
    bool skipped_ = false;

public:
    std::vector<std::string> internalSamples_;
};

bool parseSeeds(const char* s, Options& o) {
    const char* dash = std::strchr(s, '-');
    o.seedFrom = std::strtoull(s, nullptr, 10);
    o.seedTo = dash != nullptr ? std::strtoull(dash + 1, nullptr, 10) : o.seedFrom;
    return o.seedTo >= o.seedFrom;
}

} // namespace

int main(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const bool next = i + 1 < argc;
        if ((std::strcmp(argv[i], "--seed") == 0 || std::strcmp(argv[i], "--seeds") == 0) && next) {
            if (!parseSeeds(argv[++i], o)) return 2;
        } else if (std::strcmp(argv[i], "--ops") == 0 && next) o.ops = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--mode") == 0 && next) o.device = std::strcmp(argv[++i], "device") == 0;
        else if (std::strcmp(argv[i], "--verbose") == 0) o.verbose = true;
        else if (std::strcmp(argv[i], "--selftest") == 0) o.selftest = true;
        else {
            std::fprintf(stderr, "usage: le-fuzz-api [--seed N | --seeds A-B] [--ops 300] [--mode sim|device] [--verbose] [--selftest]\n");
            return 2;
        }
    }
    StderrWatch::instance().start();   // "JUCE Assertion failure" trên stderr → vi phạm (api_client.h)
    std::signal(SIGSEGV, onSignal);
    std::signal(SIGBUS, onSignal);
    std::signal(SIGABRT, onSignal);
    std::signal(SIGFPE, onSignal);
    std::signal(SIGILL, onSignal);
#if defined(LE_HAVE_SANITIZER_API)
    __sanitizer_set_death_callback(&writeRepro);
#endif
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    if (o.selftest) {
#if JUCE_DEBUG || JUCE_LOG_ASSERTIONS
        juce::logAssertion("selftest.cpp", 42);   // giống hệt đường jassert() thật
        const auto a = StderrWatch::instance().sync();
        const bool ok = a.size() == 1 && a[0].find("selftest.cpp:42") != std::string::npos;
        std::printf("selftest StderrWatch: %s (bắt được %zu assertion)\n", ok ? "OK" : "HỎNG", a.size());
        return ok ? 0 : 1;
#else
        std::printf("selftest: bản release không có jassert → bỏ qua\n");
        return 0;
#endif
    }

    Stats st;
    int bad = 0, skipped = 0;
    std::vector<std::string> internal;
    const auto t0 = std::chrono::steady_clock::now();
    for (uint64_t seed = o.seedFrom; seed <= o.seedTo; ++seed) {
        Fuzzer f(seed, o, st);
        const int v = f.run();
        if (v < 0) ++skipped;
        else if (v > 0) ++bad;
        for (const auto& s : f.internalSamples_)
            if (internal.size() < 5) internal.push_back(s);
        if (o.seedTo - o.seedFrom < 20 || v != 0) std::printf("seed %llu: %s\n", static_cast<unsigned long long>(seed), v == 0 ? "ok" : (v < 0 ? "bỏ qua" : "VI PHẠM"));
    }
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::printf("\n══ le-fuzz-api (%s) seed %llu..%llu × %d bước — %.1f s ══\n", o.device ? "device" : "sim",
                static_cast<unsigned long long>(o.seedFrom), static_cast<unsigned long long>(o.seedTo), o.ops, sec);
    std::printf("le_call %d · le_send %d (bị từ chối %d) · advance %d · restart %d · vi phạm %d (seed lỗi %d, bỏ qua %d)\n",
                st.calls, st.sends, st.sendRejected, st.advances, st.restarts, st.violations, bad, skipped);
    std::printf("error.code INTERNAL (mã mơ hồ, không tính vi phạm): %d\n", st.internalErrors);
    for (const auto& s : internal) std::printf("   vd: %s\n", s.c_str());
    std::vector<std::pair<double, std::string>> slow;
    for (const auto& [op, ms] : st.opMaxMs) slow.emplace_back(ms, op);
    std::sort(slow.rbegin(), slow.rend());
    std::printf("le_call chậm nhất trên main (luật 05 §1: < 1 ms, trừ sim.*):\n");
    for (size_t i = 0; i < std::min<size_t>(6, slow.size()); ++i)
        std::printf("   %-28s %8.2f ms\n", slow[i].second.c_str(), slow[i].first);
    return st.violations > 0 ? 1 : 0;
}
