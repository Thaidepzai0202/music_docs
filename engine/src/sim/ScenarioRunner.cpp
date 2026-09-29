#include "sim/ScenarioRunner.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>

#include <juce_audio_formats/juce_audio_formats.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"
#include "le/engine_api.h"

namespace le::sim {

namespace {

struct Named {
    const char* name;
    int value;
};

constexpr Named kCommands[] = {
    {"TRANSPORT_PLAY", LE_CMD_TRANSPORT_PLAY}, {"TRANSPORT_STOP", LE_CMD_TRANSPORT_STOP},
    {"SET_BPM", LE_CMD_SET_BPM}, {"SET_QUANTIZE", LE_CMD_SET_QUANTIZE}, {"METRONOME", LE_CMD_METRONOME},
    {"SET_COUNT_IN", LE_CMD_SET_COUNT_IN}, {"CLIP_LAUNCH", LE_CMD_CLIP_LAUNCH}, {"CLIP_STOP", LE_CMD_CLIP_STOP},
    {"SCENE_LAUNCH", LE_CMD_SCENE_LAUNCH}, {"STOP_ALL", LE_CMD_STOP_ALL}, {"CLIP_RECORD", LE_CMD_CLIP_RECORD},
    {"RECORD_STOP", LE_CMD_RECORD_STOP}, {"OVERDUB_TOGGLE", LE_CMD_OVERDUB_TOGGLE},
    {"TRACK_GAIN", LE_CMD_TRACK_GAIN}, {"TRACK_PAN", LE_CMD_TRACK_PAN}, {"TRACK_MUTE", LE_CMD_TRACK_MUTE},
    {"TRACK_SOLO", LE_CMD_TRACK_SOLO}, {"TRACK_ARM", LE_CMD_TRACK_ARM}, {"TRACK_MONITOR", LE_CMD_TRACK_MONITOR},
    {"SELECT_TRACK", LE_CMD_SELECT_TRACK}, {"NOTE_ON", LE_CMD_NOTE_ON}, {"NOTE_OFF", LE_CMD_NOTE_OFF},
    {"ALL_NOTES_OFF", LE_CMD_ALL_NOTES_OFF}, {"FX_PARAM", LE_CMD_FX_PARAM}, {"FX_BYPASS", LE_CMD_FX_BYPASS},
    {"MASTER_GAIN", LE_CMD_MASTER_GAIN}, {"SPIKE_SINE", LE_CMD_SPIKE_SINE},
    {"SPIKE_LOAD_VOICES", LE_CMD_SPIKE_LOAD_VOICES}, {"SPIKE_RECORD", LE_CMD_SPIKE_RECORD},
    {"SPIKE_PLAY_RECORD", LE_CMD_SPIKE_PLAY_RECORD}, {"SPIKE_PASSTHROUGH", LE_CMD_SPIKE_PASSTHROUGH},
};

constexpr Named kEnums[] = {
    {"LE_Q_NONE", LE_Q_NONE}, {"LE_Q_1_16", LE_Q_1_16}, {"LE_Q_1_8", LE_Q_1_8}, {"LE_Q_1_4", LE_Q_1_4},
    {"LE_Q_1_2", LE_Q_1_2}, {"LE_Q_1_BAR", LE_Q_1_BAR}, {"LE_Q_2_BAR", LE_Q_2_BAR}, {"LE_Q_4_BAR", LE_Q_4_BAR},
};

double toDb(double amp) { return amp > 1e-12 ? 20.0 * std::log10(amp) : -240.0; }
double fromDb(double db) { return std::pow(10.0, db / 20.0); }

bool isNumber(const juce::var& v) { return v.isInt() || v.isInt64() || v.isDouble(); }

bool intFrom(const juce::var& v, int& out) {
    if (isNumber(v)) {
        out = (int) v;
        return true;
    }
    if (v.isString()) {
        const auto s = v.toString().toStdString();
        for (const auto& e : kEnums)
            if (s == e.name) {
                out = e.value;
                return true;
            }
    }
    return false;
}

struct Event {
    std::int64_t sample = 0;
    int order = 0;
    bool isCall = false;
    juce::var payload;
};

std::string describe(const juce::var& v) { return juce::JSON::toString(v, true).toStdString(); }

class Runner {
public:
    Runner(const juce::var& sc, const RunOptions& opt, std::string baseDir)
        : sc_(sc), opt_(opt), baseDir_(std::move(baseDir)) {}

    ScenarioResult run() {
        R_.name = sc_["name"].toString().toStdString();
        R_.sampleRate = isNumber(sc_["sampleRate"]) ? (double) sc_["sampleRate"] : 48000.0;
        R_.blockSize = opt_.blockSize > 0 ? opt_.blockSize : (isNumber(sc_["blockSize"]) ? (int) sc_["blockSize"] : 128);
        if (R_.blockSize < 1 || R_.blockSize > 4096) return fail("blockSize phải trong [1, 4096]");
        if (R_.sampleRate < 8000.0 || R_.sampleRate > 192000.0) return fail("sampleRate không hợp lệ");
        bpm_ = isNumber(sc_["bpm"]) ? (double) sc_["bpm"] : 120.0;

        if (!loadInput()) return R_;

        LeConfig cfg{};
        cfg.apiVersion = LE_API_VERSION;
        cfg.preferredBufferSize = R_.blockSize;
        cfg.preferredSampleRate = R_.sampleRate;
        cfg.numInputChannels = isNumber(sc_["inputChannels"]) ? (int) sc_["inputChannels"] : (input_.empty() ? 0 : 1);
        const std::string tmp = tmpDir_.getFullPathName().toStdString();
        cfg.dataDir = tmp.c_str();
        if (const auto err = core::Engine::validate(&cfg); err != LE_OK) return fail("LeConfig không hợp lệ");

        const int maxChunk = std::max(R_.blockSize, 1024);
        auto dev = std::make_unique<io::OfflineDeviceIO>(maxChunk);
        device_ = dev.get();
        engine_ = std::make_unique<core::Engine>(cfg, std::move(dev));
        if (engine_->audioStart() != LE_OK) return fail("audioStart (offline) lỗi");

        // setup
        if (const juce::var* steps = sc_["setup"].getArray() ? &sc_["setup"] : nullptr) {
            for (const auto& step : *steps->getArray())
                if (!runStep(step)) return R_;
        }
        engine_->pump();

        if (!buildTimeline()) return R_;
        if (!render(maxChunk)) return R_;
        engine_->audioStop();
        engine_.reset();

        if (opt_.checkExpectations) checkExpectations();
        R_.ok = R_.error.empty() && R_.failures.empty();
        return R_;
    }

    juce::File tmpDir_;

private:
    ScenarioResult& fail(const std::string& msg) {
        R_.error = msg;
        R_.ok = false;
        return R_;
    }

    juce::File resolve(const juce::String& path) const {
        if (juce::File::isAbsolutePath(path)) return juce::File(path);
        return juce::File(juce::String::fromUTF8(baseDir_.c_str())).getChildFile(path);
    }

    bool loadInput() {
        const juce::var in = sc_["input"];
        if (!in.isString()) return true;
        std::vector<std::vector<float>> ch;
        double sr = 0;
        std::string err;
        if (!readWav(resolve(in.toString()).getFullPathName().toStdString(), ch, sr, &err)) {
            fail("không đọc được input: " + err);
            return false;
        }
        if (std::fabs(sr - R_.sampleRate) > 0.5) {
            fail("input có sample rate khác scenario");
            return false;
        }
        input_ = std::move(ch[0]);
        return true;
    }

    bool runStep(const juce::var& step) {
        if (step.hasProperty("send")) return doSend(step["send"]);
        if (step.hasProperty("call")) return doCall(step["call"]);
        fail("bước không có \"send\" hoặc \"call\": " + describe(step));
        return false;
    }

    bool parseCommand(const juce::var& v, LeCommand& c) {
        c = LeCommand{};
        c.track = -1;
        c.slot = -1;
        const juce::var t = v["type"];
        int type = -1;
        if (isNumber(t)) type = (int) t;
        else if (t.isString()) type = commandTypeFromName(t.toString().toStdString());
        if (type <= 0) return false;
        c.type = (std::uint16_t) type;
        int x = 0;
        if (v.hasProperty("track") && intFrom(v["track"], x)) c.track = (std::int8_t) x;
        if (v.hasProperty("slot") && intFrom(v["slot"], x)) c.slot = (std::int8_t) x;
        if (v.hasProperty("i0")) {
            if (!intFrom(v["i0"], x)) return false;
            c.i0 = x;
        }
        if (isNumber(v["f0"])) c.f0 = (float) (double) v["f0"];
        if (isNumber(v["f1"])) c.f1 = (float) (double) v["f1"];
        if (isNumber(v["d0"])) c.d0 = (double) v["d0"];
        return true;
    }

    bool doSend(const juce::var& v) {
        LeCommand c;
        if (!parseCommand(v, c)) {
            fail("lệnh không hợp lệ: " + describe(v));
            return false;
        }
        if (!engine_->send(c)) {
            fail("le_send từ chối lệnh (chưa hỗ trợ hoặc sai tham số): " + describe(v));
            return false;
        }
        return true;
    }

    bool doCall(const juce::var& v) {
        juce::var res;
        juce::JSON::parse(juce::String::fromUTF8(engine_->call(describe(v).c_str()).c_str()), res);
        if (!(bool) res["ok"]) {
            fail("le_call lỗi: " + describe(v) + " → " + describe(res));
            return false;
        }
        const juce::var jobId = res["result"]["jobId"];
        if (!isNumber(jobId)) return true;
        const std::string q = "{\"op\":\"job.result\",\"jobId\":" + jobId.toString().toStdString() + "}";
        for (int i = 0; i < 6000; ++i) {   // ≤ 30 giây
            engine_->pump();
            juce::var jr;
            juce::JSON::parse(juce::String::fromUTF8(engine_->call(q.c_str()).c_str()), jr);
            const auto status = jr["result"]["status"].toString();
            if (status == "done") return true;
            if (status == "failed") {
                fail("job thất bại: " + describe(v) + " → " + describe(jr["result"]["error"]));
                return false;
            }
            juce::Thread::sleep(5);
        }
        fail("job quá 30 giây: " + describe(v));
        return false;
    }

    // Đổi beat → sample theo bpm từng đoạn (SET_BPM trong timeline đổi bpm từ beat đó trở đi).
    std::int64_t beatToSample(double beat) const {
        return anchorSample_ + (std::int64_t) std::llround((beat - anchorBeat_) * 60.0 / bpm_ * R_.sampleRate);
    }

    bool buildTimeline() {
        const juce::var tl = sc_["timeline"];
        struct Raw { double beat; std::int64_t sample; bool byBeat; int order; juce::var v; };
        std::vector<Raw> raw;
        if (tl.isArray()) {
            int order = 0;
            for (const auto& e : *tl.getArray()) {
                Raw r{0.0, 0, false, order++, e};
                if (isNumber(e["atSample"])) r.sample = (std::int64_t) (juce::int64) e["atSample"];
                else if (isNumber(e["atBeat"])) { r.beat = (double) e["atBeat"]; r.byBeat = true; }
                else { fail("sự kiện timeline thiếu atBeat/atSample: " + describe(e)); return false; }
                raw.push_back(r);
            }
        }
        // Beat events theo thứ tự beat để áp dụng SET_BPM đúng đoạn.
        std::stable_sort(raw.begin(), raw.end(), [](const Raw& a, const Raw& b) {
            if (a.byBeat && b.byBeat) return a.beat < b.beat;
            return false;
        });
        for (auto& r : raw) {
            Event ev;
            ev.order = r.order;
            ev.sample = r.byBeat ? beatToSample(r.beat) : r.sample;
            if (r.v.hasProperty("send")) {
                ev.payload = r.v["send"];
                const juce::var t = ev.payload["type"];
                const bool isBpm = (t.isString() && commandTypeFromName(t.toString().toStdString()) == LE_CMD_SET_BPM) ||
                                   (isNumber(t) && (int) t == LE_CMD_SET_BPM);
                if (isBpm && r.byBeat && isNumber(ev.payload["d0"])) {
                    anchorSample_ = ev.sample;
                    anchorBeat_ = r.beat;
                    bpm_ = (double) ev.payload["d0"];
                }
            } else if (r.v.hasProperty("call")) {
                ev.isCall = true;
                ev.payload = r.v["call"];
            } else {
                fail("sự kiện timeline không có send/call: " + describe(r.v));
                return false;
            }
            events_.push_back(ev);
        }
        std::stable_sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) {
            return a.sample < b.sample || (a.sample == b.sample && a.order < b.order);
        });

        if (isNumber(sc_["renderFrames"])) total_ = (std::int64_t) (juce::int64) sc_["renderFrames"];
        else if (isNumber(sc_["renderSeconds"])) total_ = (std::int64_t) std::llround((double) sc_["renderSeconds"] * R_.sampleRate);
        else if (isNumber(sc_["renderBeats"])) total_ = beatToSample((double) sc_["renderBeats"]);
        else { fail("thiếu renderBeats / renderSeconds / renderFrames"); return false; }
        if (total_ <= 0 || total_ > (std::int64_t) (R_.sampleRate * 600)) { fail("độ dài render không hợp lệ (tối đa 10 phút)"); return false; }
        return true;
    }

    bool render(int maxChunk) {
        R_.left.assign((size_t) total_, 0.0f);
        R_.right.assign((size_t) total_, 0.0f);
        std::vector<float> inBuf((size_t) maxChunk, 0.0f);
        const int numIn = device_->numInputs();
        size_t next = 0;
        std::int64_t t = 0;
        while (t < total_) {
            while (next < events_.size() && events_[next].sample <= t) {   // sự kiện đúng sample t: áp dụng TRƯỚC khi render
                const auto& ev = events_[next++];
                if (!(ev.isCall ? doCall(ev.payload) : doSend(ev.payload))) return false;
            }
            std::int64_t end = std::min(total_, t + R_.blockSize);
            if (next < events_.size() && events_[next].sample > t) end = std::min(end, events_[next].sample);
            const int n = (int) (end - t);

            for (int i = 0; i < n; ++i) {
                const auto src = (size_t) (t + i);
                inBuf[(size_t) i] = src < input_.size() ? input_[src] : 0.0f;
            }
            const float* ins[1] = {inBuf.data()};
            float* outs[2] = {R_.left.data() + t, R_.right.data() + t};
            device_->render(numIn > 0 ? ins : nullptr, numIn > 0 ? 1 : 0, outs, 2, n);
            engine_->pump();
            t = end;
        }
        return true;
    }

    void checkExpectations() {
        const juce::var ex = sc_["expect"];
        if (!ex.isObject()) return;
        const auto& L = R_.left;
        const auto& Rr = R_.right;
        const size_t n = L.size();

        double peak = 0.0;
        for (size_t i = 0; i < n; ++i) peak = std::max(peak, (double) std::max(std::fabs(L[i]), std::fabs(Rr[i])));
        R_.notes.push_back("peak = " + std::to_string(toDb(peak)) + " dBFS");
        if (isNumber(ex["peakMaxDb"]) && toDb(peak) > (double) ex["peakMaxDb"] + 1e-9)
            R_.failures.push_back("peak " + std::to_string(toDb(peak)) + " dBFS > peakMaxDb " + ex["peakMaxDb"].toString().toStdString());
        if (isNumber(ex["peakMinDb"]) && toDb(peak) < (double) ex["peakMinDb"])
            R_.failures.push_back("peak " + std::to_string(toDb(peak)) + " dBFS < peakMinDb " + ex["peakMinDb"].toString().toStdString());

        if (ex.hasProperty("firstNonSilentSample")) {
            const double thr = fromDb(isNumber(ex["silenceThresholdDb"]) ? (double) ex["silenceThresholdDb"] : -90.0);
            std::int64_t first = -1;
            for (size_t i = 0; i < n; ++i)
                if (std::fabs(L[i]) > thr || std::fabs(Rr[i]) > thr) { first = (std::int64_t) i; break; }
            const auto expected = (std::int64_t) (juce::int64) ex["firstNonSilentSample"];
            const auto tol = isNumber(ex["firstNonSilentTolerance"]) ? (std::int64_t) (juce::int64) ex["firstNonSilentTolerance"] : 0;
            R_.notes.push_back("firstNonSilentSample = " + std::to_string(first));
            if (first < 0 || std::llabs(first - expected) > tol)
                R_.failures.push_back("firstNonSilentSample = " + std::to_string(first) + ", kỳ vọng " + std::to_string(expected));
        }

        if (isNumber(ex["maxSampleJumpDb"])) {
            double maxJump = 0.0;
            size_t at = 0;
            for (size_t i = 1; i < n; ++i) {
                const double j = std::max(std::fabs(L[i] - L[i - 1]), std::fabs(Rr[i] - Rr[i - 1]));
                if (j > maxJump) { maxJump = j; at = i; }
            }
            R_.notes.push_back("maxSampleJump = " + std::to_string(toDb(maxJump)) + " dB tại sample " + std::to_string(at));
            if (toDb(maxJump) > (double) ex["maxSampleJumpDb"])
                R_.failures.push_back("bước nhảy " + std::to_string(toDb(maxJump)) + " dB tại sample " + std::to_string(at) +
                                      " > maxSampleJumpDb " + ex["maxSampleJumpDb"].toString().toStdString());
        }

        if (ex["golden"].isString() && !opt_.ignoreGolden) checkGolden(ex);
    }

    void checkGolden(const juce::var& ex) {
        const juce::File gf = resolve(ex["golden"].toString());
        if (!gf.existsAsFile()) {
            R_.failures.push_back("chưa có golden " + gf.getFullPathName().toStdString() + " → chạy scripts/golden_update.sh " +
                                  R_.name + " rồi TỰ NGHE file trước khi commit");
            return;
        }
        std::vector<std::vector<float>> g;
        double sr = 0;
        std::string err;
        if (!readWav(gf.getFullPathName().toStdString(), g, sr, &err)) {
            R_.failures.push_back("không đọc được golden: " + err);
            return;
        }
        if (g.size() < 2) g.push_back(g[0]);
        if (std::fabs(sr - R_.sampleRate) > 0.5 || g[0].size() != R_.left.size()) {
            R_.failures.push_back("golden khác định dạng: " + std::to_string(g[0].size()) + " frame @ " + std::to_string(sr) +
                                  " Hz, output " + std::to_string(R_.left.size()) + " frame @ " + std::to_string(R_.sampleRate) + " Hz");
            return;
        }
        const double maxDb = isNumber(ex["nullTestMaxDb"]) ? (double) ex["nullTestMaxDb"] : -90.0;
        const double thr = fromDb(maxDb);
        double sumSq = 0.0;
        std::int64_t firstBad = -1;
        int badCh = 0;
        const std::vector<float>* outs[2] = {&R_.left, &R_.right};
        for (int c = 0; c < 2; ++c)
            for (size_t i = 0; i < g[(size_t) c].size(); ++i) {
                const double d = (double) (*outs[c])[i] - (double) g[(size_t) c][i];
                sumSq += d * d;
                if (std::fabs(d) > thr && (firstBad < 0 || (std::int64_t) i < firstBad)) { firstBad = (std::int64_t) i; badCh = c; }
            }
        const double rmsDb = toDb(std::sqrt(sumSq / (2.0 * (double) R_.left.size())));
        R_.notes.push_back("null test RMS = " + std::to_string(rmsDb) + " dBFS");
        if (rmsDb > maxDb || firstBad >= 0) {
            std::ostringstream os;
            os << "golden lệch: null RMS " << rmsDb << " dBFS (ngưỡng " << maxDb << ")";
            if (firstBad >= 0)
                os << ", sample đầu tiên bị lệch: " << firstBad << " (kênh " << badCh << ", out=" << (*outs[badCh])[(size_t) firstBad]
                   << ", golden=" << g[(size_t) badCh][(size_t) firstBad] << ")";
            R_.failures.push_back(os.str());
        }
    }

    juce::var sc_;
    RunOptions opt_;
    std::string baseDir_;
    ScenarioResult R_;
    std::unique_ptr<core::Engine> engine_;
    io::OfflineDeviceIO* device_ = nullptr;
    std::vector<float> input_;
    std::vector<Event> events_;
    double bpm_ = 120.0;
    std::int64_t anchorSample_ = 0;
    double anchorBeat_ = 0.0;
    std::int64_t total_ = 0;
};

ScenarioResult runParsed(const juce::String& text, const RunOptions& opt, const std::string& defaultBase) {
    juce::File tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-scenario", "", false);
    tmp.createDirectory();

    juce::var sc;
    const auto r = juce::JSON::parse(text.replace("$TMP", tmp.getFullPathName()), sc);
    ScenarioResult res;
    if (r.failed() || !sc.isObject()) {
        res.error = "JSON scenario hỏng: " + r.getErrorMessage().toStdString();
    } else {
        Runner runner(sc, opt, opt.baseDir.empty() ? defaultBase : opt.baseDir);
        runner.tmpDir_ = tmp;
        res = runner.run();
    }
    tmp.deleteRecursively();
    return res;
}

} // namespace

int commandTypeFromName(const std::string& name) {
    const std::string n = name.rfind("LE_CMD_", 0) == 0 ? name.substr(7) : name;
    for (const auto& c : kCommands)
        if (n == c.name) return c.value;
    return -1;
}

ScenarioResult runScenarioFile(const std::string& path, const RunOptions& opt) {
    const juce::File f(juce::String::fromUTF8(path.c_str()));
    if (!f.existsAsFile()) {
        ScenarioResult r;
        r.error = "không có file " + path;
        return r;
    }
    // Mặc định đường dẫn tương đối tính từ engine/tests/ (thư mục cha của scenarios/).
    return runParsed(f.loadFileAsString(), opt, f.getParentDirectory().getParentDirectory().getFullPathName().toStdString());
}

ScenarioResult runScenarioJson(const std::string& json, const RunOptions& opt) {
    return runParsed(juce::String::fromUTF8(json.c_str()), opt,
                     juce::File::getCurrentWorkingDirectory().getFullPathName().toStdString());
}

bool writeWavStereo(const std::string& path, const std::vector<float>& left, const std::vector<float>& right,
                    double sampleRate, std::string* error) {
    const juce::File f(juce::String::fromUTF8(path.c_str()));
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(f);
    if (!static_cast<juce::FileOutputStream*>(stream.get())->openedOk()) {
        if (error != nullptr) *error = "không mở được " + path;
        return false;
    }
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                                  .withSampleRate(sampleRate)
                                                  .withNumChannels(2)
                                                  .withBitsPerSample(32)
                                                  .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if (writer == nullptr) {
        if (error != nullptr) *error = "không tạo được WAV writer";
        return false;
    }
    const float* chans[2] = {left.data(), right.data()};
    return writer->writeFromFloatArrays(chans, 2, (int) std::min(left.size(), right.size()));
}

bool readWav(const std::string& path, std::vector<std::vector<float>>& channels, double& sampleRate, std::string* error) {
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(juce::File(juce::String::fromUTF8(path.c_str()))));
    if (reader == nullptr) {
        if (error != nullptr) *error = "không đọc được " + path;
        return false;
    }
    const int numCh = (int) reader->numChannels;
    const auto len = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buf(numCh, len);
    reader->read(&buf, 0, len, 0, true, true);
    channels.assign((size_t) numCh, {});
    for (int c = 0; c < numCh; ++c) channels[(size_t) c].assign(buf.getReadPointer(c), buf.getReadPointer(c) + len);
    sampleRate = reader->sampleRate;
    return true;
}

} // namespace le::sim
