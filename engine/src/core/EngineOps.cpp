// Handler của le_call (05 §3). [main] Mọi handler chạy trên main thread, trả Reply.
// Op nào chưa đăng ký ở đây thì CommandProcessor tự trả NOT_IMPLEMENTED.
#include <algorithm>
#include <cmath>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/AudioFileIO.h"
#include "io/AudioSession.h"
#include "io/JuceDeviceIO.h"
#include "io/SfzLoader.h"
#include "spike/measure/StretchBench.h"

namespace le::core {

namespace {
template <typename Container>
juce::var toVarArray(const Container& c) {
    juce::Array<juce::var> a;
    for (const auto& x : c) a.add(juce::var(x));
    return juce::var(a);
}
} // namespace

void Engine::registerOps() {
    auto bind = [this](Reply (Engine::*fn)(const juce::var&)) {
        return [this, fn](const juce::var& req) { return (this->*fn)(req); };
    };
    ops_.add("engine.info", bind(&Engine::opEngineInfo));
    ops_.add("audio.setInputEnabled", bind(&Engine::opSetInputEnabled));
    ops_.add("job.result", bind(&Engine::opJobResult));
    ops_.add("job.cancel", bind(&Engine::opJobCancel));
    // Cấu trúc (P1): sửa model → publishSnapshot
    ops_.add("project.open", bind(&Engine::opProjectOpen));
    ops_.add("project.close", bind(&Engine::opProjectClose));
    ops_.add("transport.setTimeSignature", bind(&Engine::opSetTimeSignature));
    ops_.add("transport.setTempoMode", bind(&Engine::opTransportSetTempoMode));   // P1-39 pedal mode
    ops_.add("track.configure", bind(&Engine::opTrackConfigure));
    ops_.add("clip.setMidi", bind(&Engine::opClipSetMidi));
    ops_.add("clip.getMidi", bind(&Engine::opClipGetMidi));
    ops_.add("clip.clear", bind(&Engine::opClipClear));
    ops_.add("clip.info", bind(&Engine::opClipInfo));
    ops_.add("launchLog.read", bind(&Engine::opLaunchLogRead));
    ops_.add("clip.setAudio", bind(&Engine::opClipSetAudio));
    ops_.add("track.setInstrument", bind(&Engine::opTrackSetInstrument));
    ops_.add("midi.setRecordQuantize", bind(&Engine::opSetRecordQuantize));
    ops_.add("clip.setParams", bind(&Engine::opClipSetParams));
    ops_.add("clip.undoOverdub", bind(&Engine::opClipUndoOverdub));
    ops_.add("midiClip.quantize", bind(&Engine::opMidiClipQuantize));
    ops_.add("capture.start", bind(&Engine::opCaptureStart));   // P3-02 (EngineCapture.cpp)
    ops_.add("capture.stop", bind(&Engine::opCaptureStop));
    ops_.add("capture.analyze", bind(&Engine::opCaptureAnalyze));
    ops_.add("instrument.createFromRecording", bind(&Engine::opInstrumentCreateFromRecording));   // P3-05
    ops_.add("instrument.setMode", bind(&Engine::opInstrumentSetMode));
    ops_.add("instrument.setEnvelope", bind(&Engine::opInstrumentSetEnvelope));
    ops_.add("memory.pressure", bind(&Engine::opMemoryPressure));    // P4-17 (EngineMemory.cpp)
    ops_.add("midi.listDevices", bind(&Engine::opMidiListDevices));   // P4 (EngineMidi.cpp)
    ops_.add("midi.enableDevice", bind(&Engine::opMidiEnableDevice));
    ops_.add("midi.learnStart", bind(&Engine::opMidiLearnStart));
    ops_.add("midi.learnCancel", bind(&Engine::opMidiLearnCancel));
    ops_.add("midi.learnResult", bind(&Engine::opMidiLearnResult));
    ops_.add("midi.setMappings", bind(&Engine::opMidiSetMappings));
    ops_.add("latency.calibrate", bind(&Engine::opLatencyCalibrate));   // P1-35 / P4-12 (EngineLatency.cpp)
    ops_.add("latency.setOffset", bind(&Engine::opLatencySetOffset));
    ops_.add("export.scene", bind(&Engine::opExportScene));         // P3-18/19 (EngineExport.cpp)
    ops_.add("export.jamStart", bind(&Engine::opExportJamStart));   // P3-17
    ops_.add("export.jamStop", bind(&Engine::opExportJamStop));
    ops_.add("fx.set", bind(&Engine::opFxSet));        // P3-12 (EngineFxOps.cpp)
    ops_.add("fx.remove", bind(&Engine::opFxRemove));
    ops_.add("preview.play", bind(&Engine::opPreviewPlay));   // Browser (EnginePreview.cpp)
    ops_.add("preview.stop", bind(&Engine::opPreviewStop));
    // Spike P0 (xoá ở P1-37)
    ops_.add("spike.setBufferSize", bind(&Engine::opSetBufferSize));
    ops_.add("spike.setSessionMode", bind(&Engine::opSetSessionMode));
    ops_.add("spike.sessionInfo", bind(&Engine::opSessionInfo));
    ops_.add("spike.latencyLoopback", bind(&Engine::opLatencyLoopback));
    ops_.add("spike.stretchBench", bind(&Engine::opStretchBench));
    registerSimOps();   // sim.* (chỉ Mac)
}

Reply Engine::opEngineInfo(const juce::var&) {
    auto* r = new juce::DynamicObject();
    const bool running = device_ != nullptr && device_->isRunning();
    const auto lat = running ? device_->latencies() : io::DeviceLatencies{};
    r->setProperty("apiVersion", LE_API_VERSION);
    r->setProperty("nonFiniteSamples", (juce::int64) rt_.mixer().nonFiniteSamples());   // R2: NaN/Inf đã chặn ở master
    r->setProperty("latencyOffsetSamples", latencyOffset_);
    r->setProperty("memoryMB", memoryFootprintMB());   // phys_footprint (P4-17)
    auto* tempo = new juce::DynamicObject();   // P1-39
    tempo->setProperty("mode", model_.tempoFirstLoop ? "firstLoop" : "fixed");
    tempo->setProperty("hasTempo", hasTempo());
    tempo->setProperty("firstLoopBeats", firstLoopBeats_);   // độ dài vòng đầu (beat), 0 = chưa biết
    r->setProperty("tempoState", juce::var(tempo));
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
    r->setProperty("queueFull", (int) queueFull_);
    r->setProperty("ignoredCommands", (int) rt_.ignoredCommands());
    r->setProperty("droppedEvents", (int) rt_.droppedEvents());
    r->setProperty("sessionMode",
                   device_ != nullptr && device_->sessionMode() == io::session::Mode::Measurement ? "measurement" : "default");
    return Reply::ok(juce::var(r));
}

namespace {
constexpr std::int64_t kMaxJobId = 9007199254740992LL;   // 2^53
}

// L2 (le-fuzz-api 80): kiểm jobId là số nguyên hữu hạn TRƯỚC khi ép kiểu (-1e308 → UB trong var::operator int64).
Reply Engine::opJobResult(const juce::var& req) {
    std::int64_t id = 0;
    if (!args::getInt64(req, "jobId", id, -kMaxJobId, kMaxJobId)) return Reply::fail(LE_ERR_INVALID_ARG, "\"jobId\" phải là số nguyên");
    return jobs_->result(id);
}

Reply Engine::opJobCancel(const juce::var& req) {
    std::int64_t id = 0;
    if (!args::getInt64(req, "jobId", id, -kMaxJobId, kMaxJobId)) return Reply::fail(LE_ERR_INVALID_ARG, "\"jobId\" phải là số nguyên");
    if (!jobs_->cancel(id)) return Reply::fail(LE_ERR_JOB_NOT_FOUND, "no job " + std::to_string(id));
    return Reply::ok();
}

// ─────────────────────────── cấu trúc: project / transport / track / clip ───────────────────────────

namespace {
bool getCell(const juce::var& req, int& track, int& slot, Reply& err) {
    if (!args::getInt(req, "track", track, 0, LE_MAX_TRACKS - 1)) {
        err = Reply::fail(LE_ERR_INVALID_ARG, "track phải là số nguyên 0..7");
        return false;
    }
    if (!args::getInt(req, "slot", slot, 0, LE_MAX_SCENES - 1)) {
        err = Reply::fail(LE_ERR_INVALID_ARG, "slot phải là số nguyên 0..7");
        return false;
    }
    return true;
}

const char* kindName(ClipKind k) { return k == ClipKind::Audio ? "audio" : (k == ClipKind::Midi ? "midi" : "empty"); }
} // namespace

Reply Engine::opProjectOpen(const juce::var& req) {
    std::string dir;
    if (!args::getString(req, "dir", dir) || dir.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"dir\"");
    if (!juce::File::isAbsolutePath(juce::String::fromUTF8(dir.c_str())))
        return Reply::fail(LE_ERR_INVALID_ARG, "dir phải là đường dẫn tuyệt đối");
    model_.reset();   // P1-32: khôi phục project = chuỗi lệnh cấu trúc theo 06 §6
    model_.projectDir = dir;
    // P4-19: file dở của lần chạy trước bị kill (CAF / export / cache ghi .tmp rồi mới đổi tên) → xoá.
    for (const auto& f : juce::File(juce::String::fromUTF8(dir.c_str())).findChildFiles(juce::File::findFiles, true, "*.tmp"))
        f.deleteFile();
    peaks_.clear();
    for (auto& row : undoLayer_) for (auto& u : row) u = nullptr;
    resetProjectRtState();
    publishSnapshot();
    return Reply::ok();
}

Reply Engine::opProjectClose(const juce::var&) {
    model_.reset();
    peaks_.clear();
    for (auto& row : undoLayer_) for (auto& u : row) u = nullptr;
    resetProjectRtState();
    publishSnapshot();
    return Reply::ok();
}

Reply Engine::opSetTimeSignature(const juce::var& req) {
    int num = 0, den = 0;
    if (!args::getInt(req, "num", num, 1, 32)) return Reply::fail(LE_ERR_INVALID_ARG, "num phải trong 1..32");
    if (!args::getInt(req, "den", den, 2, 16) || (den != 2 && den != 4 && den != 8 && den != 16))
        return Reply::fail(LE_ERR_INVALID_ARG, "den phải là 2, 4, 8 hoặc 16");
    model_.beatsPerBar = num;
    model_.beatUnit = den;
    publishSnapshot();
    return Reply::ok();
}

Reply Engine::opTrackConfigure(const juce::var& req) {
    int track = 0;
    if (!args::getInt(req, "track", track, 0, LE_MAX_TRACKS - 1)) return Reply::fail(LE_ERR_INVALID_ARG, "track phải là 0..7");
    std::string kind;
    if (!args::getString(req, "kind", kind) || (kind != "audio" && kind != "instrument"))
        return Reply::fail(LE_ERR_INVALID_ARG, "kind phải là \"audio\" hoặc \"instrument\"");
    TrackModel& t = model_.tracks[track];
    t.kind = kind == "audio" ? TrackKind::Audio : TrackKind::Instrument;
    if (t.kind == TrackKind::Audio) t.instrument.reset();
    std::string name;
    if (args::getString(req, "name", name)) t.name = name;
    if (req.hasProperty("color")) {   // "#RRGGBB" → Launchpad chọn màu palette gần nhất (P4-05)
        std::string c;
        const bool ok = args::getString(req, "color", c) && c.size() == 7 && c[0] == '#' &&
                        juce::String(c.substr(1)).containsOnly("0123456789abcdefABCDEF");
        if (!ok) return Reply::fail(LE_ERR_INVALID_ARG, "color phải là \"#RRGGBB\"");
        t.color = (std::uint32_t) juce::String(c.substr(1)).getHexValue32();
        t.hasColor = true;
    }
    publishSnapshot();
    return Reply::ok();
}

Reply Engine::opClipSetMidi(const juce::var& req) {
    int track = 0, slot = 0;
    Reply err;
    if (!getCell(req, track, slot, err)) return err;
    std::string clipId;
    if (!args::getString(req, "clipId", clipId) || clipId.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"clipId\"");
    double lengthBeats = 0;
    if (!args::getDouble(req, "lengthBeats", lengthBeats, 1e-6, 4096.0))
        return Reply::fail(LE_ERR_INVALID_ARG, "lengthBeats phải > 0");
    const juce::var notesVar = req.getProperty("notes", juce::var(juce::Array<juce::var>()));
    if (!notesVar.isArray()) return Reply::fail(LE_ERR_INVALID_ARG, "notes phải là danh sách");

    auto clip = std::make_shared<dsp::MidiClip>();
    clip->lengthBeats = lengthBeats;
    auto* notes = &clip->notes;
    notes->reserve((size_t) notesVar.size());
    for (const auto& n : *notesVar.getArray()) {
        int p = 0, v = 0;
        double st = 0, d = 0;
        if (!args::getInt(n, "p", p, 0, 127) || !args::getInt(n, "v", v, 1, 127) ||
            !args::getDouble(n, "s", st, 0.0, lengthBeats) || st >= lengthBeats || !args::getDouble(n, "d", d, 1e-6, 4096.0))
            return Reply::fail(LE_ERR_INVALID_ARG, "nốt không hợp lệ: " + juce::JSON::toString(n, true).toStdString() +
                                                       " (p 0..127, v 1..127, 0 ≤ s < lengthBeats, d > 0)");
        dsp::MidiNote note;
        note.startBeat = st;
        note.lengthBeats = d;
        note.pitch = (std::uint8_t) p;
        note.velocity = (std::uint8_t) v;
        notes->push_back(note);
    }
    std::stable_sort(notes->begin(), notes->end(),
                     [](const dsp::MidiNote& a, const dsp::MidiNote& b) { return a.startBeat < b.startBeat; });

    ClipModel c;
    c.clipId = clipId;
    c.kind = ClipKind::Midi;
    c.lengthBeats = lengthBeats;
    c.midi = std::move(clip);
    model_.clips[track][slot] = std::move(c);
    model_.cellRequest[track][slot] = 0;   // huỷ clip.setAudio đang chờ cho ô này
    model_.touchCell(track, slot);
    publishSnapshot();
    return Reply::ok();
}

Reply Engine::opClipGetMidi(const juce::var& req) {
    int track = 0, slot = 0;
    Reply err;
    if (!getCell(req, track, slot, err)) return err;
    const auto& c = model_.clips[track][slot];
    if (!c || c->kind != ClipKind::Midi) return Reply::fail(LE_ERR_INVALID_ARG, "ô này không có clip MIDI");
    juce::Array<juce::var> arr;
    for (const auto& n : c->midi->notes) {
        auto* o = new juce::DynamicObject();
        o->setProperty("p", (int) n.pitch);
        o->setProperty("v", (int) n.velocity);
        o->setProperty("s", n.startBeat);
        o->setProperty("d", n.lengthBeats);
        arr.add(juce::var(o));
    }
    auto* r = new juce::DynamicObject();
    r->setProperty("notes", arr);
    return Reply::ok(juce::var(r));
}

Reply Engine::opClipClear(const juce::var& req) {
    int track = 0, slot = 0;
    Reply err;
    if (!getCell(req, track, slot, err)) return err;
    model_.clips[track][slot].reset();
    model_.cellRequest[track][slot] = 0;
    model_.touchCell(track, slot);
    undoLayer_[track][slot] = nullptr;
    publishSnapshot();
    return Reply::ok();
}

Reply Engine::opClipInfo(const juce::var& req) {
    int track = 0, slot = 0;
    Reply err;
    if (!getCell(req, track, slot, err)) return err;
    auto* r = new juce::DynamicObject();
    const auto& c = model_.clips[track][slot];
    r->setProperty("kind", kindName(c ? c->kind : ClipKind::None));
    if (c) {
        r->setProperty("clipId", juce::String::fromUTF8(c->clipId.c_str()));
        r->setProperty("lengthBeats", c->lengthBeats);
        r->setProperty("gainDb", c->gainDb);
        r->setProperty("hasUndo", undoLayer_[track][slot] != nullptr);   // nút "Hoàn tác overdub" (chỉ clip audio có)
        if (c->kind == ClipKind::Audio) {
            r->setProperty("file", juce::String::fromUTF8(c->file.c_str()));
            r->setProperty("originalBpm", c->originalBpm);
            r->setProperty("warp", c->warp == WarpMode::Stretch ? "stretch" : "repitch");
            // P3-08: BPM của bản stretched đang dùng được (0 = chưa có → Re-Pitch)
            r->setProperty("stretchedBpm", c->warp == WarpMode::Stretch && c->stretchedValid() ? c->stretchedBpm : 0.0);
        } else {
            r->setProperty("noteCount", (int) c->midi->notes.size());
        }
    }
    return Reply::ok(juce::var(r));
}

std::string Engine::resolvePath(const std::string& p, bool preferLibrary) const {
    const juce::String s = juce::String::fromUTF8(p.c_str());
    if (juce::File::isAbsolutePath(s)) return p;
    const std::string& base = preferLibrary ? (libraryDir_.empty() ? model_.projectDir : libraryDir_)
                                            : (model_.projectDir.empty() ? libraryDir_ : model_.projectDir);
    if (base.empty()) return {};
    return juce::File(juce::String::fromUTF8(base.c_str())).getChildFile(s).getFullPathName().toStdString();
}

// P1-11: {track, slot, clipId, file, lengthBeats, originalBpm, warp, gainDb} → jobId.
// [worker] decode → [main] gắn AudioData vào model + snapshot mới, rồi mới JOB_DONE.
Reply Engine::opClipSetAudio(const juce::var& req) {
    int track = 0, slot = 0;
    Reply err;
    if (!getCell(req, track, slot, err)) return err;
    std::string clipId, file, warp = "repitch";
    if (!args::getString(req, "clipId", clipId) || clipId.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"clipId\"");
    if (!args::getString(req, "file", file) || file.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"file\"");
    double lengthBeats = 0, originalBpm = 0, gainDb = 0;
    if (!args::getDouble(req, "lengthBeats", lengthBeats, 1e-6, 4096.0)) return Reply::fail(LE_ERR_INVALID_ARG, "lengthBeats phải > 0");
    if (!args::getDouble(req, "originalBpm", originalBpm, 20.0, 300.0)) return Reply::fail(LE_ERR_INVALID_ARG, "originalBpm phải trong 20..300");
    if (req.hasProperty("warp") && (!args::getString(req, "warp", warp) || (warp != "stretch" && warp != "repitch")))
        return Reply::fail(LE_ERR_INVALID_ARG, "warp phải là \"stretch\" hoặc \"repitch\"");
    if (req.hasProperty("gainDb") && !args::getDouble(req, "gainDb", gainDb, -120.0, 24.0))
        return Reply::fail(LE_ERR_INVALID_ARG, "gainDb phải trong -120..24");
    const std::string path = resolvePath(file, false);
    if (path.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "file tương đối nhưng chưa project.open");

    struct Shared {   // worker ghi data trước khi xong; main đọc trong onDone (sau acquire của status)
        dsp::AudioDataPtr data;
        std::shared_ptr<const render::Peaks> peaks;
        std::int64_t jobId = 0;
    };
    auto sh = std::make_shared<Shared>();
    ClipModel c;
    c.clipId = clipId;
    c.kind = ClipKind::Audio;
    c.lengthBeats = lengthBeats;
    c.originalBpm = originalBpm;
    c.gainDb = (float) gainDb;
    c.warp = warp == "stretch" ? WarpMode::Stretch : WarpMode::Repitch;
    c.file = file;

    const std::string cache = model_.projectDir.empty() ? std::string()
        : juce::File(juce::String::fromUTF8(model_.projectDir.c_str())).getChildFile(juce::String::fromUTF8(("cache/" + clipId + ".peaks").c_str()))
              .getFullPathName().toStdString();
    const auto id = jobs_->submit(
        "decode",
        [path, sh, cache](JobSystem::Context& ctx) {   // [worker] decode → peaks (le_get_peaks sẵn sàng khi JOB_DONE)
            if (ctx.cancelled()) return JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
            io::DecodeResult d = io::decodeAudioFile(path);
            if (d.error != LE_OK) return JobOutcome::fail(d.error, d.message);
            sh->peaks = peaksFor(*d.data, cache, &ctx.cancel);
            auto* o = new juce::DynamicObject();
            o->setProperty("frames", (juce::int64) d.data->numFrames());
            o->setProperty("channels", d.data->numChannels());
            o->setProperty("sampleRate", d.data->sampleRate());
            o->setProperty("durationSeconds", d.data->durationSeconds());
            sh->data = std::move(d.data);
            return JobOutcome::ok(juce::var(o));
        },
        [this, track, slot, sh, c](JobOutcome& out) mutable {   // [main]
            if (out.error != LE_OK) return;
            if (model_.cellRequest[track][slot] != sh->jobId) {
                out = JobOutcome::fail(LE_ERR_JOB_CANCELLED, "ô đã được gán lệnh khác trong lúc decode");
                return;
            }
            if (sh->peaks) peaks_[c.clipId] = sh->peaks;
            c.audio = std::move(sh->data);
            model_.clips[track][slot] = std::move(c);
            model_.cellRequest[track][slot] = 0;
            model_.touchCell(track, slot);
            publishSnapshot();
            refreshWarp(track, slot);   // P3-11: loop thư viện khác BPM → render bản stretched ngay (không debounce)
        });
    sh->jobId = id;
    model_.cellRequest[track][slot] = id;
    return Reply::job(id);
}

// job.result của track.setInstrument sfz — một chỗ dựng cho cả nạp mới lẫn dùng lại cache preview (kết quả y hệt).
static juce::var sfzResult(const std::string& name, int regions, int samplesLoaded, const std::vector<std::string>& warnings) {
    auto* res = new juce::DynamicObject();
    res->setProperty("name", juce::String::fromUTF8(name.c_str()));
    res->setProperty("regions", regions);
    res->setProperty("samplesLoaded", samplesLoaded);
    juce::Array<juce::var> w;
    for (const auto& x : warnings) w.add(juce::String::fromUTF8(x.c_str()));
    res->setProperty("warnings", w);
    return juce::var(res);
}

// P1-27: {track, instrument:{kind:"sfz", path}} → jobId. SfzLoader (80) + decodeAudioFile trên worker.
// File đang trong cache preview (Browser vừa nghe thử) → dùng CHUNG Instrument đó: không nạp lại, không gấp đôi RAM.
// Vẫn đi qua job (jobId, JOB_DONE sau khi model đổi) để hợp đồng không đổi.
Reply Engine::opTrackSetInstrument(const juce::var& req) {
    int track = 0;
    if (!args::getInt(req, "track", track, 0, LE_MAX_TRACKS - 1)) return Reply::fail(LE_ERR_INVALID_ARG, "track phải là 0..7");
    const juce::var inst = req.getProperty("instrument", {});
    std::string kind, path;
    if (!inst.isObject() || !args::getString(inst, "kind", kind)) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"instrument\":{kind,…}");
    if (kind == "user") {   // P3-05: nhạc cụ tự thu (EngineCapture.cpp)
        std::string uid;
        if (!args::getString(inst, "id", uid) || uid.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu instrument.id");
        return setUserInstrument(track, uid);
    }
    if (kind != "sfz") return Reply::fail(LE_ERR_INVALID_ARG, "instrument.kind phải là \"sfz\" hoặc \"user\"");
    if (!args::getString(inst, "path", path) || path.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu instrument.path");
    const std::string sfz = resolvePath(path, true);
    if (sfz.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "path tương đối nhưng không có libraryDir");

    struct Shared {
        dsp::InstrumentPtr instrument;
        std::int64_t jobId = 0;
    };
    auto sh = std::make_shared<Shared>();
    std::optional<PreviewEntry> cached;   // bản copy (shared_ptr + kết quả SfzLoader), không phụ thuộc cache bị đẩy ra sau đó
    if (const PreviewEntry* e = previewCachedSfz(sfz)) cached = *e;
    const auto id = jobs_->submit(
        "sfz",
        [sfz, sh, cached](JobSystem::Context& ctx) {   // [worker]
            if (cached) {
                sh->instrument = cached->instrument;
                return JobOutcome::ok(sfzResult(cached->name, cached->regions, cached->samplesLoaded, cached->warnings));
            }
            io::SfzLoadOptions o;
            o.loadSample = &io::decodeAudioFile;
            o.cancel = &ctx.cancel;
            io::SfzLoadResult r = io::loadSfzFile(sfz, o);
            if (!r.ok) return JobOutcome::fail(r.error != LE_OK ? r.error : LE_ERR_FILE_FORMAT, r.message);
            juce::var res = sfzResult(r.instrument->name, r.regions, r.samplesLoaded, r.warnings);
            sh->instrument = std::move(r.instrument);
            return JobOutcome::ok(res);
        },
        [this, track, sh](JobOutcome& out) {   // [main]
            if (out.error != LE_OK) return;
            if (model_.trackRequest[track] != sh->jobId) {
                out = JobOutcome::fail(LE_ERR_JOB_CANCELLED, "track đã được gán nhạc cụ khác");
                return;
            }
            model_.tracks[track].kind = TrackKind::Instrument;
            model_.tracks[track].instrument = std::move(sh->instrument);
            model_.tracks[track].userInstrumentId.clear();
            model_.trackRequest[track] = 0;
            publishSnapshot();
        });
    sh->jobId = id;
    model_.trackRequest[track] = id;
    return Reply::job(id);
}

// 05 §3: {track, slot, gainDb?, warp?} → chỉ đổi tham số, KHÔNG decode lại. Snapshot mới giữ CÙNG AudioData →
// AudioClipPlayer (cùng data) cập nhật gain và trượt 20 ms (không click). warp đổi → P3-08 xếp job WarpRenderer.
Reply Engine::opClipSetParams(const juce::var& req) {
    int track = 0, slot = 0;
    Reply err;
    if (!getCell(req, track, slot, err)) return err;
    auto& cm = model_.clips[track][slot];
    if (!cm) return Reply::fail(LE_ERR_INVALID_ARG, "ô trống");
    double gainDb = cm->gainDb;
    std::string warp;
    if (req.hasProperty("gainDb") && !args::getDouble(req, "gainDb", gainDb, -120.0, 24.0))
        return Reply::fail(LE_ERR_INVALID_ARG, "gainDb phải trong -120..24");
    if (req.hasProperty("warp")) {
        if (!args::getString(req, "warp", warp) || (warp != "stretch" && warp != "repitch"))
            return Reply::fail(LE_ERR_INVALID_ARG, "warp phải là \"stretch\" hoặc \"repitch\"");
        if (cm->kind != ClipKind::Audio) return Reply::fail(LE_ERR_INVALID_ARG, "warp chỉ áp dụng cho clip audio");
    }
    if (!req.hasProperty("gainDb") && warp.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "cần gainDb hoặc warp");
    cm->gainDb = (float) gainDb;
    if (!warp.empty()) cm->warp = warp == "stretch" ? WarpMode::Stretch : WarpMode::Repitch;
    publishSnapshot();
    if (!warp.empty()) refreshWarp(track, slot);   // stretch → xếp job; repitch → huỷ job (snapshot bỏ bản stretched)
    return Reply::ok();
}

// 05 §3: {track, slot} → trả clip về bản TRƯỚC lần overdub gần nhất (1 lớp). Giống bản cũ từng bit (cùng object).
Reply Engine::opClipUndoOverdub(const juce::var& req) {
    int track = 0, slot = 0;
    Reply err;
    if (!getCell(req, track, slot, err)) return err;
    return undoOverdub(track, slot);
}

Reply Engine::undoOverdub(int track, int slot) {
    for (const auto& r : overdubRounds_[track])
        if (r.ticket->slot == slot) return Reply::fail(LE_ERR_INVALID_ARG, "đang overdub: tắt overdub trước");
    auto& cm = model_.clips[track][slot];
    if (!cm || undoLayer_[track][slot] == nullptr) return Reply::fail(LE_ERR_INVALID_ARG, "không có lớp overdub để hoàn tác");
    cm->audio = std::move(undoLayer_[track][slot]);
    undoLayer_[track][slot] = nullptr;
    publishSnapshot();
    refreshWarp(track, slot);   // bản stretched (nếu có) render từ audio trước undo → không còn hợp lệ
    persistClipAudio(track, slot, cm->audio, cm->audio->numFrames());   // file + peaks về bản cũ
    return Reply::ok();
}

// 05 §3: {grid: 0 | 0.25 | 0.5} (beat). Áp dụng khi ghép nốt lúc take MIDI xong (P1-30).
Reply Engine::opSetRecordQuantize(const juce::var& req) {
    double grid = -1;
    if (!args::getDouble(req, "grid", grid, 0.0, 1.0) || (grid != 0.0 && grid != 0.25 && grid != 0.5))
        return Reply::fail(LE_ERR_INVALID_ARG, "grid phải là 0 (tắt), 0.25 hoặc 0.5");
    recordQuantize_ = grid;
    return Reply::ok();
}

// 05 §3: {track, slot, grid} → điểm bắt đầu về lưới gần nhất, GIỮ độ dài (P2, 77 cần sớm).
Reply Engine::opMidiClipQuantize(const juce::var& req) {
    int track = 0, slot = 0;
    Reply err;
    if (!getCell(req, track, slot, err)) return err;
    auto& cm = model_.clips[track][slot];
    if (!cm || cm->kind != ClipKind::Midi || cm->midi == nullptr) return Reply::fail(LE_ERR_INVALID_ARG, "ô này không có clip MIDI");
    double grid = 0;
    if (!args::getDouble(req, "grid", grid, 1e-6, cm->lengthBeats)) return Reply::fail(LE_ERR_INVALID_ARG, "grid phải trong (0, lengthBeats]");
    auto clip = std::make_shared<dsp::MidiClip>(*cm->midi);
    for (auto& n : clip->notes) {
        n.startBeat = std::round(n.startBeat / grid) * grid;
        if (n.startBeat >= clip->lengthBeats) n.startBeat -= clip->lengthBeats;   // tròn lên tới cuối → về đầu vòng
    }
    std::stable_sort(clip->notes.begin(), clip->notes.end(),
                     [](const dsp::MidiNote& a, const dsp::MidiNote& b) { return a.startBeat < b.startBeat; });
    cm->midi = std::move(clip);
    publishSnapshot();
    return Reply::ok();
}

// 05 §3: {sinceIndex} → {events:[{index, beat, track, slot, kind}], nextIndex}
Reply Engine::opLaunchLogRead(const juce::var& req) {
    int since = 0;
    if (req.hasProperty("sinceIndex") && !args::getInt(req, "sinceIndex", since, 0, 1 << 30))
        return Reply::fail(LE_ERR_INVALID_ARG, "sinceIndex phải là số nguyên ≥ 0");
    static const char* kKinds[] = {"launch", "stop", "record", "scene"};
    juce::Array<juce::var> arr;
    for (size_t i = (size_t) since; i < launchLogEntries_.size(); ++i) {
        const LaunchEvent& e = launchLogEntries_[i];
        auto* o = new juce::DynamicObject();
        o->setProperty("index", (int) i);
        o->setProperty("beat", e.beat);
        o->setProperty("track", (int) e.track);
        o->setProperty("slot", (int) e.slot);
        o->setProperty("kind", kKinds[(int) e.kind]);
        arr.add(juce::var(o));
    }
    auto* r = new juce::DynamicObject();
    r->setProperty("events", arr);
    r->setProperty("nextIndex", (int) launchLogEntries_.size());
    return Reply::ok(juce::var(r));
}

// ─────────────────────────── spike (P0) ───────────────────────────

Reply Engine::opSetBufferSize(const juce::var& req) {
    int frames = 0;
    if (!args::getInt(req, "frames", frames, 64, 1024) ||
        (frames != 64 && frames != 128 && frames != 256 && frames != 512 && frames != 1024))
        return Reply::fail(LE_ERR_INVALID_ARG, "frames must be 64/128/256/512/1024");
    if (jobs_->anyRunning("latency")) return Reply::fail(LE_ERR_INVALID_ARG, "đang đo latency, đợi job xong");
    preferredBuffer_ = frames;
    int actual = frames;
    if (device_ != nullptr && device_->isRunning()) {
        io::DeviceConfig dc;
        dc.sampleRate = device_->sampleRate();
        dc.bufferSize = frames;
        dc.numInputs = numInputs_;
        const std::int32_t err = device_->restart(dc);   // P0: đồng bộ (vài chục ms), chấp nhận cho spike
        if (err != LE_OK) return Reply::fail(err, device_->lastError());
        actual = device_->bufferSize();
        refreshDeviceInfo();
        fitRecordBuffersToRate();
    }
    auto* r = new juce::DynamicObject();
    r->setProperty("bufferSize", actual);
    return Reply::ok(juce::var(r));
}

// 05 §3, 07 §4.0: {enabled} → {inputChannels}. Người dùng từ chối quyền mic → chạy CHỈ PHÁT (category Playback,
// không có input); được quyền lại → bật input (PlayAndRecord). Thiết lập toàn cục: project.open không đổi nó.
// Device đang chạy thì khởi động lại (vài chục ms im lặng); chưa chạy thì chỉ ghi lại cho lần audioStart sau.
Reply Engine::opSetInputEnabled(const juce::var& req) {
    bool enabled = false;
    if (!args::getBool(req, "enabled", enabled)) return Reply::fail(LE_ERR_INVALID_ARG, "enabled phải là true/false");
    if (jobs_->anyRunning("latency")) return Reply::fail(LE_ERR_INVALID_ARG, "đang đo latency, đợi job xong");
    const int before = numInputs_;
    numInputs_ = enabled ? configuredInputs_ : 0;
    if (device_ != nullptr && device_->isRunning() && device_->numInputs() != numInputs_) {
        io::DeviceConfig dc;
        dc.sampleRate = preferredRate_;
        dc.bufferSize = preferredBuffer_;
        dc.numInputs = numInputs_;
        dc.numOutputs = 2;
        device_->stop();
        const std::int32_t err = device_->start(dc, &rt_);
        if (err != LE_OK) {   // mở lại như cũ để app vẫn phát được
            const std::string msg = device_->lastError();
            numInputs_ = before;
            dc.numInputs = before;
            if (device_->start(dc, &rt_) != LE_OK) emitEvent(LE_EVT_ERROR, err, 0, 0, 0.0);
            refreshDeviceInfo();
            return Reply::fail(err, msg);
        }
        refreshDeviceInfo();
        fitRecordBuffersToRate();
        startGraceTicks_ = 15;   // macOS: AudioDeviceManager broadcast sau khi mở device, không phải route đổi
    }
    auto* r = new juce::DynamicObject();
    r->setProperty("inputChannels", device_ != nullptr && device_->isRunning() ? device_->numInputs() : numInputs_);
    return Reply::ok(juce::var(r));
}

Reply Engine::opSetSessionMode(const juce::var& req) {
    const juce::String mode = req.getProperty("mode", {}).toString();
    if (mode != "default" && mode != "measurement")
        return Reply::fail(LE_ERR_INVALID_ARG, "mode must be \"default\" or \"measurement\"");
    const auto m = mode == "measurement" ? io::session::Mode::Measurement : io::session::Mode::Default;
    if (device_ == nullptr) device_ = std::make_unique<io::JuceDeviceIO>();
    if (!device_->setSessionMode(m)) return Reply::fail(LE_ERR_AUDIO_DEVICE, device_->lastError());
    auto* r = new juce::DynamicObject();
    r->setProperty("mode", mode);
    r->setProperty("applied", io::session::isSupported());
    return Reply::ok(juce::var(r));
}

Reply Engine::opSessionInfo(const juce::var&) {
    juce::var session;
    juce::JSON::parse(juce::String::fromUTF8(io::session::describeJson().c_str()), session);
    auto* r = new juce::DynamicObject();
    r->setProperty("supported", io::session::isSupported());
    r->setProperty("session", session);
    return Reply::ok(juce::var(r));
}

// P0-07: phát 5 chirp qua loa, thu qua mic, cross-correlation (LatencyProbe của agent 80).
Reply Engine::opLatencyLoopback(const juce::var&) {
    if (device_ == nullptr || !device_->isRunning())
        return Reply::fail(LE_ERR_AUDIO_DEVICE, "audio chưa chạy (gọi le_audio_start trước)");
    if (device_->numInputs() <= 0) return Reply::fail(LE_ERR_AUDIO_DEVICE, "không có input mic (numInputChannels = 0)");
    if (jobs_->anyRunning("latency")) return Reply::fail(LE_ERR_INVALID_ARG, "đang đo latency");

    const int reported = device_->latencies().roundTrip();
    spike::LatencyProbe& probe = rt_.latencyProbe();
    probe.start();   // [main] chỉ bật cờ atomic; RT bắt đầu ở block kế tiếp

    return Reply::job(jobs_->submit("latency", [&probe, reported](JobSystem::Context& ctx) {   // [worker]
        const double sr = probe.sampleRate();
        const double expectedMs = (double) probe.totalSamples() * 1000.0 / sr;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        while (!probe.isDone()) {
            if (ctx.cancelled()) return JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
            const double elapsed = juce::Time::getMillisecondCounterHiRes() - t0;
            if (elapsed > expectedMs + 3000.0) return JobOutcome::fail(LE_ERR_AUDIO_DEVICE, "timeout: audio dừng trong lúc đo?");
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
    }));
}

// P0-09: render các zone pitch-shift từ bản thu SPIKE_RECORD (StretchBench của agent 80).
Reply Engine::opStretchBench(const juce::var& req) {
    const juce::var semis = req.getProperty("semitones", {});
    if (!semis.isArray() || semis.size() == 0)
        return Reply::fail(LE_ERR_INVALID_ARG, "semitones phải là danh sách số nguyên, ví dụ [-12,0,12]");
    spike::StretchBench::Config cfg;
    for (const auto& v : *semis.getArray()) {
        const double d = args::isNumber(v) ? (double) v : 1e9;
        if (std::fabs(d) > 36.0 || std::fabs(d - std::round(d)) > 1e-9)
            return Reply::fail(LE_ERR_INVALID_ARG, "semitones: mỗi phần tử là số nguyên trong [-36, 36]");
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
        return Reply::fail(LE_ERR_INVALID_ARG, "saveDir phải là đường dẫn tuyệt đối");
    cfg.outDir = saveDir.toStdString();

    // [main] Copy bản thu ngay (≤ 10 giây, ~1ms). RT chỉ ghi buffer khi đang thu; nếu SPIKE_RECORD mới bắt đầu
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
    return Reply::job(jobs_->submit("stretch", [samples = std::move(audio), sr, cfg, precheck = err](JobSystem::Context& ctx) mutable {
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
    }));
}

} // namespace le::core
