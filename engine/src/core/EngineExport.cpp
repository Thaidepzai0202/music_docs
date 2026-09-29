// P3-17 export.jamStart / jamStop (ghi master bus real-time) · P3-18/19 export.scene (offline, + stems, WAV / M4A)
// bằng io::ExportWriter của 80. [main] trừ phần ghi chú [worker].
#include <algorithm>
#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/ExportWriter.h"

namespace le::core {

namespace {

constexpr int kOfflineBlock = 1024;   // maxBlock của RtEngine offline (04 §12)
constexpr double kPreRollSeconds = 0.1;

bool formatFromName(const std::string& s, io::ExportFormat& out) {
    if (s == "wav") out = io::ExportFormat::Wav24;
    else if (s == "m4a") out = io::ExportFormat::M4aAac;
    else return false;
    return true;
}

io::ExportFormat formatFromPath(const std::string& p) {
    return juce::File(juce::String::fromUTF8(p.c_str())).hasFileExtension("m4a") ? io::ExportFormat::M4aAac : io::ExportFormat::Wav24;
}

// "<base>_t<n>.<ext>" (n = 1..8, đếm từ 1 cho người dùng, 05 §3).
std::string stemPath(const std::string& path, int track, io::ExportFormat f) {
    const juce::String p = juce::String::fromUTF8(path.c_str());
    const int dot = p.lastIndexOfChar('.');
    const int slash = std::max(p.lastIndexOfChar('/'), p.lastIndexOfChar('\\'));
    const juce::String base = dot > slash ? p.substring(0, dot) : p;
    return (base + "_t" + juce::String(track + 1) + "." + io::exportExtension(f)).toStdString();
}

LeCommand cmd(std::uint16_t type, int track, int slot, int i0, float f0 = 0.0f, double d0 = 0.0) {
    LeCommand c{};
    c.type = type;
    c.track = (std::int8_t) track;
    c.slot = (std::int8_t) slot;
    c.i0 = i0;
    c.f0 = f0;
    c.d0 = d0;
    return c;
}

// Một lượt render offline (bản mix, hoặc một stem = track đó solo).
struct SceneRender {
    std::unique_ptr<GraphSnapshot> snapshot;   // FX là instance RIÊNG (processor có state, không dùng chung với RT live)
    std::vector<LeCommand> setup;              // mixer / master / BPM (áp trong pre-roll)
    int scene = 0;
    std::string abs, file;                     // đường dẫn tuyệt đối / như Dart gửi
};

// [worker] RtEngine offline riêng (queue + publisher riêng) → ExportWriter. Không đụng gì của engine live.
JobOutcome renderScene(SceneRender& r, double sr, std::int64_t frames, const io::ExportOptions& opt, JobSystem::Context& ctx,
                       float progressFrom, float progressSpan, std::int64_t& clipped) {
    CommandQueue commands(1024);
    RtToNrtQueue toNrt(4096);
    MidiQueue midi(8);
    StatePublisher publisher;
    auto rt = std::make_unique<RtEngine>(commands, toNrt, midi, publisher);
    rt->setInitialSnapshot(std::move(r.snapshot));
    rt->prepare(sr, kOfflineBlock);
    for (const LeCommand& c : r.setup) (void) commands.try_push(c);
    std::vector<float> L((size_t) kOfflineBlock), R((size_t) kOfflineBlock);
    auto drain = [&] {   // snapshot cũ / event: bỏ (không có gì cần báo main)
        while (const RtMessage* m = toNrt.front()) {
            if (m->kind == RtMessage::Retire) delete m->snapshot;
            toNrt.pop();
        }
    };
    // Pre-roll im lặng (transport dừng): ramp gain / pan 20 ms, crossfade vào FX 20 ms, fade EQ master 10 ms chạy
    // xong TRƯỚC beat 0 — như engine live đã ổn định từ lâu. Output pre-roll bỏ đi.
    for (int done = 0, pre = (int) std::ceil(kPreRollSeconds * sr); done < pre; done += kOfflineBlock) {
        float* outs[2] = {L.data(), R.data()};
        rt->process(nullptr, 0, outs, 2, std::min(kOfflineBlock, pre - done), io::CallbackContext{});
        drain();
    }
    (void) commands.try_push(cmd(LE_CMD_SCENE_LAUNCH, -1, r.scene, 0));   // transport dừng → play tại beat 0 ngay

    juce::File(juce::String::fromUTF8(r.abs.c_str())).getParentDirectory().createDirectory();
    io::ExportWriter w;
    std::string err;
    if (const auto e = w.open(r.abs, sr, 2, opt, &err); e != LE_OK) return JobOutcome::fail(e, err);
    for (std::int64_t done = 0; done < frames;) {
        if (ctx.cancelled()) {
            w.abort();
            return JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
        }
        const int n = (int) std::min<std::int64_t>(kOfflineBlock, frames - done);
        float* outs[2] = {L.data(), R.data()};
        rt->process(nullptr, 0, outs, 2, n, io::CallbackContext{});
        const float* ins[2] = {L.data(), R.data()};
        if (const auto e = w.write(ins, n, &err); e != LE_OK) return JobOutcome::fail(e, err);
        drain();
        done += n;
        ctx.progress.store(progressFrom + progressSpan * (float) ((double) done / (double) frames), std::memory_order_relaxed);
    }
    if (const auto e = w.finish(&err); e != LE_OK) return JobOutcome::fail(e, err);
    clipped += w.clippedSamples();
    return JobOutcome::ok({});
}

} // namespace

// Snapshot cho render offline: như bản live (chung AudioData / Instrument bất biến) nhưng FX là instance MỚI
// (create → prepare → setParam theo model → reset), vì processor có state và RT live đang dùng instance kia.
std::unique_ptr<GraphSnapshot> Engine::buildOfflineSnapshot(double sampleRate) const {
    auto s = buildSnapshot(model_, 1);
    for (int t = 0; t < LE_MAX_TRACKS; ++t)
        for (int k = 0; k < kFxSlots; ++k) {
            const auto& f = model_.tracks[t].fx[k];
            if (!f) continue;
            std::shared_ptr<dsp::Processor> p = dsp::createProcessor(f->type);
            if (p == nullptr) continue;
            p->prepare(sampleRate, FxChain::kMaxFxBlock);
            for (const dsp::ParamInfo& pi : dsp::paramInfo(f->type)) p->setParam(pi.id, f->params[pi.id]);
            p->reset();
            s->tracks[t].fx[k].proc = std::move(p);
        }
    return s;
}

// Lệnh dựng lại state RT của project (không có trong snapshot): BPM, mixer, master. soloTrack ≥ 0 → stem.
std::vector<LeCommand> Engine::offlineSetupCommands(int soloTrack) const {
    std::vector<LeCommand> v;
    v.push_back(cmd(LE_CMD_SET_BPM, -1, -1, 0, 0.0f, bpm_));
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        const MixerStripModel& m = model_.mixer[t];
        v.push_back(cmd(LE_CMD_TRACK_GAIN, t, -1, 0, m.gainDb));
        v.push_back(cmd(LE_CMD_TRACK_PAN, t, -1, 0, m.pan));
        v.push_back(cmd(LE_CMD_TRACK_MUTE, t, -1, soloTrack == t ? 0 : (m.mute ? 1 : 0)));
        v.push_back(cmd(LE_CMD_TRACK_SOLO, t, -1, soloTrack >= 0 ? (soloTrack == t ? 1 : 0) : (m.solo ? 1 : 0)));
    }
    const MasterModel& ms = model_.master;
    v.push_back(cmd(LE_CMD_MASTER_GAIN, -1, -1, 0, ms.gainDb));
    for (int b = 0; b < 3; ++b) v.push_back(cmd(LE_CMD_FX_PARAM, -1, 0, b, ms.eq3[b]));
    v.push_back(cmd(LE_CMD_FX_BYPASS, -1, 0, ms.eqBypass ? 1 : 0));
    v.push_back(cmd(LE_CMD_FX_PARAM, -1, 1, 0, ms.limiterCeilingDb));
    v.push_back(cmd(LE_CMD_FX_PARAM, -1, 1, 1, ms.limiterReleaseMs));
    return v;
}

// 05 §3: {scene, bars, path, format:"wav"|"m4a", stems:false} → jobId → {file, seconds, stems?:[…]}.
// RtEngine offline riêng, launch scene tại beat 0, render ĐÚNG N bar (±0 sample), nhanh hơn real-time.
Reply Engine::opExportScene(const juce::var& req) {
    int scene = 0, bars = 0;
    std::string path, formatName = "wav";
    bool stems = false;
    if (!args::getInt(req, "scene", scene, 0, LE_MAX_SCENES - 1)) return Reply::fail(LE_ERR_INVALID_ARG, "scene phải là 0..7");
    if (!args::getInt(req, "bars", bars, 1, 512)) return Reply::fail(LE_ERR_INVALID_ARG, "bars phải là 1..512");
    if (!args::getString(req, "path", path) || path.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"path\"");
    io::ExportFormat fmt{};
    if (req.hasProperty("format") && (!args::getString(req, "format", formatName) || !formatFromName(formatName, fmt)))
        return Reply::fail(LE_ERR_INVALID_ARG, "format phải là \"wav\" hoặc \"m4a\"");
    if (!req.hasProperty("format")) fmt = io::ExportFormat::Wav24;
    if (req.hasProperty("stems") && !args::getBool(req, "stems", stems)) return Reply::fail(LE_ERR_INVALID_ARG, "stems phải là bool");
    const std::string abs = resolvePath(path, false);
    if (abs.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "path tương đối nhưng chưa project.open");

    const double sr = rt_.sampleRate();
    const double spb = 60.0 * sr / bpm_;
    const auto frames = (std::int64_t) std::ceil((double) bars * model_.beatsPerBar * spb - 1e-6);   // như Transport::sampleAtBeat
    auto renders = std::make_shared<std::vector<SceneRender>>();
    auto add = [&](int solo, const std::string& file, const std::string& absFile) {
        SceneRender r;
        r.snapshot = buildOfflineSnapshot(sr);
        r.setup = offlineSetupCommands(solo);
        r.scene = scene;
        r.file = file;
        r.abs = absFile;
        renders->push_back(std::move(r));
    };
    add(-1, path, abs);
    juce::Array<juce::var> stemFiles;
    if (stems)
        for (int t = 0; t < LE_MAX_TRACKS; ++t) {
            if (!model_.clips[t][scene]) continue;   // track không có clip ở scene này → không có stem
            const std::string f = stemPath(path, t, fmt);
            add(t, f, stemPath(abs, t, fmt));
            stemFiles.add(juce::String::fromUTF8(f.c_str()));
        }
    io::ExportOptions opt;
    opt.format = fmt;
    const auto id = jobs_->submit("export", [renders, sr, frames, opt, path, stems, stemFiles](JobSystem::Context& ctx) {   // [worker]
        std::int64_t clipped = 0;
        const float span = 1.0f / (float) renders->size();
        for (size_t i = 0; i < renders->size(); ++i) {
            JobOutcome o = renderScene((*renders)[i], sr, frames, opt, ctx, span * (float) i, span, clipped);
            if (o.error != LE_OK) return o;
        }
        auto* r = new juce::DynamicObject();
        r->setProperty("file", juce::String::fromUTF8(path.c_str()));
        r->setProperty("seconds", (double) frames / sr);
        r->setProperty("frames", (juce::int64) frames);
        r->setProperty("clippedSamples", (juce::int64) clipped);
        if (stems) r->setProperty("stems", stemFiles);
        return JobOutcome::ok(juce::var(r));
    });
    return Reply::job(id);
}

// ─────────────────────────── Ghi buổi jam (P3-17) ───────────────────────────

// {path} → {}. RT chép master (sau limiter) vào ring cấp phát sẵn; một job worker xả ring ra ExportWriter.
Reply Engine::opExportJamStart(const juce::var& req) {
    std::string path;
    if (!args::getString(req, "path", path) || path.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"path\"");
    if (jam_.ring != nullptr) return Reply::fail(LE_ERR_INVALID_ARG, "đang ghi jam (gọi export.jamStop trước)");
    if (device_ == nullptr || !device_->isRunning()) return Reply::fail(LE_ERR_AUDIO_DEVICE, "audio chưa chạy (le_audio_start)");
    const std::string abs = resolvePath(path, false);
    if (abs.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "path tương đối nhưng chưa project.open");
    const double sr = rt_.sampleRate();

    io::ExportOptions opt;
    opt.format = formatFromPath(abs);
    juce::File(juce::String::fromUTF8(abs.c_str())).getParentDirectory().createDirectory();
    auto writer = std::make_shared<io::ExportWriter>();   // mở trên main → lỗi (đĩa, định dạng) trả về ngay
    std::string err;
    if (const auto e = writer->open(abs, sr, 2, opt, &err); e != LE_OK) return Reply::fail(e, err);

    auto ring = std::make_shared<RtEngine::JamRing>();
    ring->id = ++jamCounter_;
    ring->capacity = (std::int64_t) (RtEngine::kJamRingSeconds * sr);
    ring->L = std::make_unique<float[]>((size_t) ring->capacity);
    ring->R = std::make_unique<float[]>((size_t) ring->capacity);
    auto stopAt = std::make_shared<std::atomic<std::int64_t>>(-1);   // −1 = đang ghi
    auto failed = std::make_shared<std::atomic<std::int32_t>>(LE_OK);
    const auto job = jobs_->submit("jam", [ring, stopAt, failed, writer](JobSystem::Context& ctx) {   // [worker]
        std::string e2;
        const std::int64_t cap = ring->capacity;
        std::vector<float> a(4096), b(4096);
        for (;;) {
            if (ctx.cancelled()) {
                writer->abort();
                return JobOutcome::fail(LE_ERR_JOB_CANCELLED, "cancelled");
            }
            const std::int64_t stop = stopAt->load(std::memory_order_acquire);
            const std::int64_t end = stop >= 0 ? stop : ring->written.load(std::memory_order_acquire);
            const std::int64_t r = ring->read.load(std::memory_order_relaxed);
            if (r >= end) {
                if (stop >= 0) break;
                juce::Thread::sleep(5);
                continue;
            }
            const int n = (int) std::min<std::int64_t>(4096, end - r);
            for (int i = 0; i < n; ++i) {
                const auto idx = (size_t) ((r + i) % cap);
                a[(size_t) i] = ring->L[idx];
                b[(size_t) i] = ring->R[idx];
            }
            const float* ch[2] = {a.data(), b.data()};
            if (const auto e = writer->write(ch, n, &e2); e != LE_OK) {
                failed->store(e, std::memory_order_relaxed);
                return JobOutcome::fail(e, e2);
            }
            ring->read.store(r + n, std::memory_order_release);   // RT được ghi đè vùng này
        }
        if (const auto e = writer->finish(&e2); e != LE_OK) {
            failed->store(e, std::memory_order_relaxed);
            return JobOutcome::fail(e, e2);
        }
        return JobOutcome::ok({});
    }, {}, false);
    (void) rt_.offerJam(ring.get());   // chỉ một phiên: không có vé cũ
    jam_ = JamSession{ring, stopAt, failed, job, path, sr};
    jamRings_.push_back(ring);   // sống tới khi RT báo JamStopped
    return Reply::ok();
}

// – → {file, seconds}. Chưa start → INVALID_ARG.
Reply Engine::opExportJamStop(const juce::var&) {
    if (jam_.ring == nullptr) return Reply::fail(LE_ERR_INVALID_ARG, "chưa export.jamStart");
    JamSession s = std::move(jam_);
    jam_ = JamSession{};
    const std::int64_t end = s.ring->written.load(std::memory_order_acquire);   // file = đúng những gì RT đã ghi tới lúc này
    s.stopAt->store(end, std::memory_order_release);
    rt_.stopJam(s.ring->id);
    (void) jobs_->waitWorker(s.job, 30000);   // writer xả nốt phần còn lại rồi đóng file (.tmp → tên thật)
    jobs_->pump();                            // job nội bộ: không phát event
    if (const auto e = s.failed->load(std::memory_order_relaxed); e != LE_OK) return Reply::fail(e, "ghi file jam lỗi");
    auto* r = new juce::DynamicObject();
    r->setProperty("file", juce::String::fromUTF8(s.path.c_str()));
    r->setProperty("seconds", (double) end / s.sampleRate);
    r->setProperty("droppedFrames", (juce::int64) s.ring->dropped.load(std::memory_order_relaxed));
    return Reply::ok(juce::var(r));
}

void Engine::handleJamStopped(const RtMessage& m) {
    const auto id = (std::uint32_t) m.a;
    jamRings_.erase(std::remove_if(jamRings_.begin(), jamRings_.end(), [id](const auto& r) { return r->id == id; }), jamRings_.end());
}

} // namespace le::core
