// P3-02 capture.* (thu một MẪU cho sampler) · P3-05 instrument.createFromRecording (+ cache zone) ·
// P3-06/07 instrument.setMode / setEnvelope · track.setInstrument {kind:"user"}. [main] Mọi hàm chạy trên main.
#include <algorithm>
#include <cmath>
#include <cstring>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/AudioFileIO.h"
#include "io/CafWriter.h"
#include "render/PitchRenderer.h"
#include "render/SilenceTrimmer.h"
#include "render/Yin.h"

namespace le::core {

namespace {

constexpr int kAnalyzePeakPairs = 512;
constexpr int kZoneCacheVersion = 1;

juce::File fileOf(const std::string& abs) { return juce::File(juce::String::fromUTF8(abs.c_str())); }

// Trộn mọi kênh thành mono (PitchRenderer / Yin nhận mono).
std::vector<float> mixMono(const dsp::AudioData& d) {
    std::vector<float> m((size_t) d.numFrames(), 0.0f);
    const int ch = std::max(1, d.numChannels());
    for (int c = 0; c < d.numChannels(); ++c) {
        const float* x = d.channel(c);
        for (std::int64_t i = 0; i < d.numFrames(); ++i) m[(size_t) i] += x[i] / (float) ch;
    }
    return m;
}

// FNV-1a 64 trên dữ liệu đã trim + tham số render → khoá cache zone.
std::uint64_t renderKey(const std::vector<float>& mono, std::int64_t start, std::int64_t end, double sr, int rootNote) {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&h](const void* p, size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) {
            h ^= b[i];
            h *= 1099511628211ull;
        }
    };
    mix(mono.data() + start, sizeof(float) * (size_t) (end - start));
    mix(&sr, sizeof sr);
    mix(&rootNote, sizeof rootNote);
    const int v = kZoneCacheVersion;
    mix(&v, sizeof v);
    return h;
}

// Dữ liệu zone ra JSON (meta.json của cache) và ngược lại.
juce::var zoneToVar(const dsp::Zone& z, int sampleIndex) {
    auto* o = new juce::DynamicObject();
    o->setProperty("loKey", z.loKey);
    o->setProperty("hiKey", z.hiKey);
    o->setProperty("loVel", z.loVel);
    o->setProperty("hiVel", z.hiVel);
    o->setProperty("rootKey", z.rootKey);
    o->setProperty("tuneCents", z.tuneCents);
    o->setProperty("gainDb", z.gainDb);
    o->setProperty("sample", sampleIndex);
    return juce::var(o);
}

struct RenderedInstrument {
    dsp::InstrumentPtr instrument;
    int rootNote = -1;
    float cents = 0.0f, confidence = 0.0f;
    bool fromCache = false;
};

// [worker] Đọc cache zone nếu khoá khớp và đủ file. nullptr instrument = không dùng được.
RenderedInstrument readZoneCache(const juce::File& dir, std::uint64_t key) {
    RenderedInstrument out;
    const juce::var meta = juce::JSON::parse(dir.getChildFile("meta.json"));
    if (!meta.isObject() || (int) meta["version"] != kZoneCacheVersion ||
        meta["key"].toString() != juce::String::toHexString((juce::int64) key))
        return out;
    auto inst = std::make_shared<dsp::Instrument>();
    inst->name = meta["name"].toString().toStdString();
    inst->classicZone = (int) meta["classicZone"];
    const auto* samples = meta["samples"].getArray();
    const auto* zones = meta["zones"].getArray();
    if (samples == nullptr || zones == nullptr) return out;
    for (const auto& s : *samples) {
        io::DecodeResult d = io::decodeAudioFile(dir.getChildFile(s.toString()).getFullPathName().toStdString());
        if (d.error != LE_OK) return out;   // thiếu / hỏng → render lại
        inst->samples.push_back(std::move(d.data));
    }
    for (const auto& z : *zones) {
        const int si = (int) z["sample"];
        if (si < 0 || si >= (int) inst->samples.size()) return out;
        dsp::Zone zone;
        zone.loKey = (std::int16_t) (int) z["loKey"];
        zone.hiKey = (std::int16_t) (int) z["hiKey"];
        zone.loVel = (std::int16_t) (int) z["loVel"];
        zone.hiVel = (std::int16_t) (int) z["hiVel"];
        zone.rootKey = (std::int16_t) (int) z["rootKey"];
        zone.tuneCents = (float) (double) z["tuneCents"];
        zone.gainDb = (float) (double) z["gainDb"];
        zone.data = inst->samples[(size_t) si].get();
        inst->zones.push_back(zone);
    }
    out.rootNote = (int) meta["rootNote"];
    out.cents = (float) (double) meta["cents"];
    out.confidence = (float) (double) meta["confidence"];
    out.fromCache = true;
    out.instrument = std::move(inst);
    return out;
}

// [worker] Ghi cache zone (lỗi ghi: vẫn dùng bản trong RAM).
void writeZoneCache(const juce::File& dir, std::uint64_t key, const dsp::Instrument& inst, const RenderedInstrument& r) {
    dir.deleteRecursively();
    if (!dir.createDirectory()) return;
    juce::Array<juce::var> samples, zones;
    for (size_t i = 0; i < inst.samples.size(); ++i) {
        const std::string name = "s" + std::to_string(i) + ".caf";
        if (!io::writeCafFloat32(dir.getChildFile(name).getFullPathName().toStdString(), *inst.samples[i], nullptr)) return;
        samples.add(juce::String::fromUTF8(name.c_str()));
    }
    for (const dsp::Zone& z : inst.zones) {
        int si = -1;
        for (size_t i = 0; i < inst.samples.size(); ++i)
            if (inst.samples[i].get() == z.data) si = (int) i;
        zones.add(zoneToVar(z, si));
    }
    auto* o = new juce::DynamicObject();
    o->setProperty("version", kZoneCacheVersion);
    o->setProperty("key", juce::String::toHexString((juce::int64) key));
    o->setProperty("name", juce::String::fromUTF8(inst.name.c_str()));
    o->setProperty("classicZone", inst.classicZone);
    o->setProperty("rootNote", r.rootNote);
    o->setProperty("cents", r.cents);
    o->setProperty("confidence", r.confidence);
    o->setProperty("samples", samples);
    o->setProperty("zones", zones);
    dir.getChildFile("meta.json").replaceWithText(juce::JSON::toString(juce::var(o)));   // ghi CUỐI: có meta = đủ file
}

bool parseMode(const std::string& s, dsp::Instrument::Mode& out) {
    if (s == "natural") out = dsp::Instrument::Mode::Natural;
    else if (s == "classic") out = dsp::Instrument::Mode::Classic;
    else return false;
    return true;
}

} // namespace

// ─────────────────────────── capture.* (P3-02) ───────────────────────────

// {path, maxSeconds} → {}. Buffer maxSeconds cấp phát ở đây (main); RT ghi input từ đầu, tự dừng khi đầy.
Reply Engine::opCaptureStart(const juce::var& req) {
    std::string path;
    double maxSeconds = 0.0;
    if (!args::getString(req, "path", path) || path.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"path\"");
    if (!args::getDouble(req, "maxSeconds", maxSeconds, 0.1, 120.0)) return Reply::fail(LE_ERR_INVALID_ARG, "maxSeconds phải trong 0.1..120");
    if (device_ == nullptr || !device_->isRunning()) return Reply::fail(LE_ERR_AUDIO_DEVICE, "audio chưa chạy (le_audio_start)");
    if (numInputs_ == 0 || device_->numInputs() == 0)
        return Reply::fail(LE_ERR_MIC_PERMISSION, "input đang tắt (audio.setInputEnabled false / chưa có quyền mic)");
    const std::string abs = resolvePath(path, false);
    if (abs.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "path tương đối nhưng chưa project.open");

    if (captureId_ != 0 && !captureDone_) rt_.recorder().stopCapture(captureId_);   // lượt cũ chưa stop: bỏ
    const double sr = device_->sampleRate();
    const int ch = std::clamp(device_->numInputs(), 1, 2);
    CaptureState st;
    st.buf = std::make_shared<dsp::AudioData>(ch, (std::int64_t) std::ceil(maxSeconds * sr), sr);
    st.ticket = std::make_unique<Recorder::CaptureTicket>();
    st.ticket->id = (++captureId_) & Recorder::kCaptureIdMask;
    if (st.ticket->id == 0) st.ticket->id = captureId_ = 1;
    captureId_ = st.ticket->id;
    st.ticket->buf = st.buf.get();
    Recorder::CaptureTicket* prev = rt_.recorder().offerCapture(st.ticket.get());
    captures_.push_back(std::move(st));
    if (prev != nullptr)   // vé cũ RT chưa kịp lấy → nhả ngay
        captures_.erase(std::remove_if(captures_.begin(), captures_.end(),
                                       [prev](const CaptureState& c) { return c.ticket.get() == prev; }),
                        captures_.end());
    captureDone_ = false;
    captureFile_ = path;
    captureAbs_ = abs;
    captureSeconds_ = 0.0;
    return Reply::ok();
}

// Ghi [0, frames) của lượt hiện tại ra file (RT không bao giờ ghi lại vào vùng đó).
bool Engine::finalizeCapture(std::int64_t frames) {
    const auto it = std::find_if(captures_.begin(), captures_.end(),
                                 [this](const CaptureState& c) { return c.ticket->id == captureId_; });
    const dsp::AudioData* buf = it != captures_.end() ? it->buf.get() : nullptr;
    const int ch = buf != nullptr ? buf->numChannels() : 1;
    const double sr = buf != nullptr ? buf->sampleRate() : (device_ != nullptr ? device_->sampleRate() : preferredRate_);
    frames = buf != nullptr ? std::clamp<std::int64_t>(frames, 0, buf->numFrames()) : 0;
    dsp::AudioData out(ch, frames, sr);
    for (int c = 0; buf != nullptr && c < ch; ++c) std::copy(buf->channel(c), buf->channel(c) + frames, out.writePointer(c));
    fileOf(captureAbs_).getParentDirectory().createDirectory();
    std::string err;
    if (!io::writeCafFloat32(captureAbs_, out, &err)) return false;
    captureSeconds_ = (double) frames / sr;
    captureDone_ = true;
    return true;
}

// – → {file, seconds}. Idempotent: gọi lại (hoặc sau khi tự dừng ở maxSeconds) trả đúng kết quả cũ.
Reply Engine::opCaptureStop(const juce::var&) {
    if (captureId_ == 0) return Reply::fail(LE_ERR_INVALID_ARG, "chưa capture.start");
    if (!captureDone_) {
        const std::int64_t frames = rt_.recorder().captureFrames(captureId_);   // đã ghi XONG (acquire)
        rt_.recorder().stopCapture(captureId_);
        if (Recorder::CaptureTicket* back = rt_.recorder().offerCapture(nullptr))   // RT chưa bắt đầu lượt này
            captures_.erase(std::remove_if(captures_.begin(), captures_.end(),
                                           [back](const CaptureState& c) { return c.ticket.get() == back; }),
                            captures_.end());
        if (!finalizeCapture(frames)) return Reply::fail(LE_ERR_DISK_FULL, "không ghi được " + captureFile_);
    }
    auto* r = new juce::DynamicObject();
    r->setProperty("file", juce::String::fromUTF8(captureFile_.c_str()));
    r->setProperty("seconds", captureSeconds_);
    return Reply::ok(juce::var(r));
}

// RT trả buffer (tự dừng vì đầy, hoặc đã nhận lệnh stop).
void Engine::handleCaptureFinished(const RtMessage& m) {
    const auto id = (std::uint32_t) m.a;
    if (id == captureId_ && !captureDone_ && m.i1 != 0 && finalizeCapture(m.i0))   // tới maxSeconds
        emitEvent(LE_EVT_RECORDING_FINISHED, -2, -2, 0, (double) m.i0);
    captures_.erase(std::remove_if(captures_.begin(), captures_.end(), [id](const CaptureState& c) { return c.ticket->id == id; }),
                    captures_.end());
}

// {file} → jobId → {trimStartSample, trimEndSample, rootNote, cents, confidence, peaks:[min,max,…] (512 cặp)}.
Reply Engine::opCaptureAnalyze(const juce::var& req) {
    std::string file;
    if (!args::getString(req, "file", file) || file.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"file\"");
    const std::string abs = resolvePath(file, false);
    if (abs.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "file tương đối nhưng chưa project.open");
    const auto id = jobs_->submit("analyze", [abs](JobSystem::Context& ctx) {   // [worker]
        io::DecodeResult d = io::decodeAudioFile(abs);
        if (d.error != LE_OK) return JobOutcome::fail(d.error, d.message);
        if (ctx.cancelled()) return JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
        const std::vector<float> mono = mixMono(*d.data);
        const render::TrimRange tr = render::findTrimRange(*d.data);
        le::PitchEstimate est;
        if (!tr.silent) {
            le::Yin yin;
            est = yin.analyze(mono.data() + tr.start, tr.end - tr.start, d.data->sampleRate());
        }
        ctx.progress.store(0.8f, std::memory_order_relaxed);
        juce::Array<juce::var> peaks;   // 512 cặp (min, max) trên toàn file, để vẽ waveform + handle trim
        const auto n = (std::int64_t) mono.size();
        for (int p = 0; p < kAnalyzePeakPairs; ++p) {
            const std::int64_t a = n * p / kAnalyzePeakPairs, b = std::max(a + 1, n * (p + 1) / kAnalyzePeakPairs);
            float lo = 0.0f, hi = 0.0f;
            for (std::int64_t i = a; i < b && i < n; ++i) {
                lo = std::min(lo, mono[(size_t) i]);
                hi = std::max(hi, mono[(size_t) i]);
            }
            peaks.add(lo);
            peaks.add(hi);
        }
        auto* r = new juce::DynamicObject();
        r->setProperty("trimStartSample", (juce::int64) (tr.silent ? 0 : tr.start));
        r->setProperty("trimEndSample", (juce::int64) (tr.silent ? 0 : tr.end));
        r->setProperty("silent", tr.silent);
        r->setProperty("rootNote", est.ok ? est.rootNote : -1);
        r->setProperty("cents", est.ok ? est.cents : 0.0f);
        r->setProperty("confidence", est.confidence);
        r->setProperty("frames", (juce::int64) n);
        r->setProperty("sampleRate", d.data->sampleRate());
        r->setProperty("peaks", peaks);
        return JobOutcome::ok(juce::var(r));
    });
    return Reply::job(id);
}

// ─────────────────────────── Nhạc cụ tự thu (P3-05..07) ───────────────────────────

// Bản track dùng = bản render + mode + envelope (copy Instrument: zones copy, samples dùng chung).
void Engine::applyUserInstrument(const std::string& id) {
    const auto it = model_.userInstruments.find(id);
    if (it == model_.userInstruments.end()) return;
    UserInstrumentModel& u = it->second;
    if (u.rendered != nullptr) {
        auto inst = std::make_shared<dsp::Instrument>(*u.rendered);
        inst->mode = u.mode;
        for (dsp::Zone& z : inst->zones) z.env = u.env;
        u.current = std::move(inst);
    }
    bool changed = false;
    for (auto& t : model_.tracks) {
        if (t.userInstrumentId != id) continue;
        t.kind = TrackKind::Instrument;
        t.instrument = u.current;
        changed = true;
    }
    if (changed) publishSnapshot();
}

// {instrumentId, file, trimStartSample?, trimEndSample?, rootNote?, mode} → jobId → {rootNote, cents, confidence, zones}.
// Đăng ký instrumentId NGAY; setMode / setEnvelope / track.setInstrument {kind:"user"} tới trước khi job xong vẫn hợp lệ.
Reply Engine::opInstrumentCreateFromRecording(const juce::var& req) {
    std::string id, file, modeName = "natural";
    if (!args::getString(req, "instrumentId", id) || id.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"instrumentId\"");
    if (!args::getString(req, "file", file) || file.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"file\"");
    dsp::Instrument::Mode mode = dsp::Instrument::Mode::Natural;
    if (req.hasProperty("mode") && (!args::getString(req, "mode", modeName) || !parseMode(modeName, mode)))
        return Reply::fail(LE_ERR_INVALID_ARG, "mode phải là \"natural\" hoặc \"classic\"");
    int rootNote = -1;
    if (req.hasProperty("rootNote") && !args::getInt(req, "rootNote", rootNote, 0, 127))
        return Reply::fail(LE_ERR_INVALID_ARG, "rootNote phải là 0..127");
    double trimStart = -1.0, trimEnd = -1.0;
    if (req.hasProperty("trimStartSample") && !args::getDouble(req, "trimStartSample", trimStart, 0.0, 1e12))
        return Reply::fail(LE_ERR_INVALID_ARG, "trimStartSample phải ≥ 0");
    if (req.hasProperty("trimEndSample") && !args::getDouble(req, "trimEndSample", trimEnd, 0.0, 1e12))
        return Reply::fail(LE_ERR_INVALID_ARG, "trimEndSample phải ≥ 0");
    if (trimStart >= 0.0 && trimEnd >= 0.0 && trimEnd <= trimStart)
        return Reply::fail(LE_ERR_INVALID_ARG, "trimEndSample phải > trimStartSample");
    const std::string abs = resolvePath(file, false);
    if (abs.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "file tương đối nhưng chưa project.open");
    UserInstrumentModel& u = model_.userInstruments[id];   // đăng ký ngay (giữ env nếu đã setEnvelope trước)
    u.mode = mode;
    u.source = {abs, rootNote, trimStart, trimEnd};
    return Reply::job(submitUserInstrumentRender(id));
}

// Job render (hoặc đọc cache zone) theo u.source. createFromRecording và setUserInstrument (tạo lại sau P4-17) dùng chung.
std::int64_t Engine::submitUserInstrumentRender(const std::string& id) {
    UserInstrumentModel& u = model_.userInstruments[id];
    const std::string abs = u.source.file;
    const int rootNote = u.source.rootNote;
    const double trimStart = u.source.trimStart, trimEnd = u.source.trimEnd;
    const juce::File cacheDir = model_.projectDir.empty()
                                    ? juce::File()
                                    : fileOf(model_.projectDir).getChildFile("instruments").getChildFile(juce::String::fromUTF8(id.c_str())).getChildFile("zones");

    struct Shared {
        RenderedInstrument out;
        std::int64_t jobId = 0;
    };
    auto sh = std::make_shared<Shared>();
    const auto job = jobs_->submit(
        "instrument",
        [abs, rootNote, trimStart, trimEnd, cacheDir, sh](JobSystem::Context& ctx) {   // [worker]
            io::DecodeResult d = io::decodeAudioFile(abs);
            if (d.error != LE_OK) return JobOutcome::fail(d.error, d.message);
            std::vector<float> mono = mixMono(*d.data);
            const auto n = (std::int64_t) mono.size();
            std::int64_t start = 0, end = n;
            if (trimStart >= 0.0 || trimEnd >= 0.0) {   // trim do người dùng chọn (UI) + fade 5 ms ở hai đầu
                start = std::clamp<std::int64_t>(trimStart >= 0.0 ? (std::int64_t) trimStart : 0, 0, n);
                end = std::clamp<std::int64_t>(trimEnd >= 0.0 ? (std::int64_t) trimEnd : n, start, n);
                const std::int64_t f = std::min<std::int64_t>((std::int64_t) (0.005 * d.data->sampleRate()), (end - start) / 2);
                for (std::int64_t i = 0; i < f; ++i) {
                    const float g = (float) i / (float) f;
                    mono[(size_t) (start + i)] *= g;
                    mono[(size_t) (end - 1 - i)] *= g;
                }
            } else {   // tự trim (SilenceTrimmer, pre/post-roll 5 ms có fade)
                render::TrimRange tr;
                dsp::AudioDataPtr trimmed = render::trimSilence(*d.data, {}, &tr);
                if (trimmed == nullptr) return JobOutcome::fail(LE_ERR_INVALID_ARG, "không nghe thấy tiếng trong file");
                mono = mixMono(*trimmed);
                start = 0;
                end = (std::int64_t) mono.size();
            }
            if (end - start < 64) return JobOutcome::fail(LE_ERR_INVALID_ARG, "đoạn thu quá ngắn");
            const double sr = d.data->sampleRate();
            const std::uint64_t key = renderKey(mono, start, end, sr, rootNote);
            if (cacheDir != juce::File()) {
                sh->out = readZoneCache(cacheDir, key);   // P3-05: mở lại project → dùng zone đã cache
                if (sh->out.instrument != nullptr) return JobOutcome::ok({});
            }
            render::PitchRenderConfig cfg;
            cfg.rootNote = rootNote;
            cfg.cancel = &ctx.cancel;
            cfg.progress = &ctx.progress;
            render::PitchRenderResult r = render::renderPitchInstrument(mono.data() + start, end - start, sr, cfg);
            if (!r.ok) return JobOutcome::fail(r.error, r.message);
            sh->out.instrument = r.instrument;
            sh->out.rootNote = r.rootNote;
            sh->out.cents = r.rootCents;
            sh->out.confidence = r.confidence;
            if (cacheDir != juce::File()) writeZoneCache(cacheDir, key, *r.instrument, sh->out);
            return JobOutcome::ok({});
        },
        [this, id, sh](JobOutcome& o) {   // [main]
            const auto it = model_.userInstruments.find(id);
            if (it == model_.userInstruments.end() || it->second.request != sh->jobId) {
                if (o.error == LE_OK) o = JobOutcome::fail(LE_ERR_JOB_CANCELLED, "đã có lệnh tạo nhạc cụ mới hơn / project đã đóng");
                return;
            }
            it->second.request = 0;
            if (o.error != LE_OK) return;
            it->second.rendered = sh->out.instrument;
            applyUserInstrument(id);
            auto* r = new juce::DynamicObject();
            r->setProperty("rootNote", sh->out.rootNote);
            r->setProperty("cents", sh->out.cents);
            r->setProperty("confidence", sh->out.confidence);
            r->setProperty("zones", (int) sh->out.instrument->zones.size());
            r->setProperty("cached", sh->out.fromCache);
            o.result = juce::var(r);
        });
    sh->jobId = job;
    u.request = job;
    return job;
}

// {instrumentId, mode:"natural"|"classic"} → {}.
Reply Engine::opInstrumentSetMode(const juce::var& req) {
    std::string id, modeName;
    if (!args::getString(req, "instrumentId", id) || model_.userInstruments.count(id) == 0)
        return Reply::fail(LE_ERR_INVALID_ARG, "instrumentId chưa được tạo (instrument.createFromRecording)");
    dsp::Instrument::Mode mode{};
    if (!args::getString(req, "mode", modeName) || !parseMode(modeName, mode))
        return Reply::fail(LE_ERR_INVALID_ARG, "mode phải là \"natural\" hoặc \"classic\"");
    model_.userInstruments[id].mode = mode;
    applyUserInstrument(id);
    return Reply::ok();
}

// {instrumentId, a, d, s, r} (giây, s 0..1) → {}. Áp cho mọi zone; nốt mới dùng envelope mới.
Reply Engine::opInstrumentSetEnvelope(const juce::var& req) {
    std::string id;
    if (!args::getString(req, "instrumentId", id) || model_.userInstruments.count(id) == 0)
        return Reply::fail(LE_ERR_INVALID_ARG, "instrumentId chưa được tạo (instrument.createFromRecording)");
    double a = 0, d = 0, s = 0, r = 0;
    if (!args::getDouble(req, "a", a, 0.0, 10.0) || !args::getDouble(req, "d", d, 0.0, 10.0) ||
        !args::getDouble(req, "s", s, 0.0, 1.0) || !args::getDouble(req, "r", r, 0.0, 10.0))
        return Reply::fail(LE_ERR_INVALID_ARG, "a, d, r phải trong 0..10 giây; s trong 0..1");
    model_.userInstruments[id].env = dsp::AdsrParams{(float) a, (float) d, (float) s, (float) r};
    applyUserInstrument(id);
    return Reply::ok();
}

// track.setInstrument {kind:"user", id} → jobId (xong ngay). Nhạc cụ chưa render xong → track im tới khi xong.
Reply Engine::setUserInstrument(int track, const std::string& id) {
    if (model_.userInstruments.count(id) == 0)
        return Reply::fail(LE_ERR_INVALID_ARG, "instrument.id chưa được tạo (instrument.createFromRecording)");
    const auto job = jobs_->submit("instrument.assign", [](JobSystem::Context&) { return JobOutcome::ok({}); },
                                   [this, id](JobOutcome& o) {
                                       if (o.error != LE_OK) return;
                                       auto* r = new juce::DynamicObject();
                                       r->setProperty("ready", model_.userInstruments.count(id) != 0 &&
                                                                   model_.userInstruments[id].current != nullptr);
                                       o.result = juce::var(r);
                                   });
    model_.trackRequest[track] = job;   // job sfz đang chờ của track này bị bỏ
    model_.tracks[track].userInstrumentId = id;
    model_.tracks[track].kind = TrackKind::Instrument;
    model_.tracks[track].instrument = model_.userInstruments[id].current;
    UserInstrumentModel& u = model_.userInstruments[id];
    if (u.current == nullptr && u.request == 0 && !u.source.file.empty())
        (void) submitUserInstrumentRender(id);   // bị nhả lúc thiếu RAM (P4-17) → tạo lại (cache zone: nhanh)
    publishSnapshot();
    return Reply::job(job);
}

} // namespace le::core
