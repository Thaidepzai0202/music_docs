#include "core/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <atomic>
#include <limits>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include "core/CommandValidation.h"
#include "core/Json.h"
#include "io/AudioSession.h"
#include "io/CafWriter.h"
#include "io/JuceDeviceIO.h"

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
      configuredInputs_(std::max(1, cfg.numInputChannels)),
      rt_(commands_, toNrt_, midiToRt_, globalStatePublisher()),
      jobs_(std::make_unique<JobSystem>(2)),
      device_(std::move(device)) {
    cfg_.dataDir = nullptr;   // không giữ con trỏ của Dart
    cfg_.libraryDir = nullptr;
    io::session::installObservers();
    const auto& c = io::session::counters();
    seenInterruptBegan_ = c.interruptionBegan.load();
    seenInterruptEnded_ = c.interruptionEnded.load();
    seenRoute_ = c.routeChanged.load();
    seenMediaReset_ = c.mediaServicesReset.load();
    seenMemoryWarnings_ = c.memoryWarnings.load();
    rt_.setLaunchLog(&launchLog_);
    {   // le_destroy → le_create: LeState toàn cục còn giữ trạng thái engine cũ tới block đầu → xoá (audio chưa chạy:
        // main là writer duy nhất của seqlock lúc này).
        LeState empty;
        std::memset(&empty, 0, sizeof(empty));
        globalStatePublisher().publish(empty);
    }
    midi_ = std::make_unique<MidiDevices>();   // P4: mỗi thiết bị một queue SPSC (R10), đăng ký trước audio start
    rt_.setMidiSources(midi_->queues(), MidiDevices::kSlots);
    midi_->onDeviceListChanged = [this] {
        rebuildMidiMap();   // Launchpad cắm / rút, mapping theo tên thiết bị
        emitEvent(LE_EVT_MIDI_DEVICES, 0, 0, 0, 0.0);
    };
    rt_.setInitialSnapshot(buildSnapshot(model_, ++generation_));   // audio chưa chạy: gán thẳng
    registerOps();
    pump_ = std::make_unique<EventPump>(*this);
}

Engine::~Engine() {
    pump_.reset();
    jobs_.reset();   // huỷ job đang chạy và chờ worker dừng, trước khi rt_ / device bị huỷ
    audioStop();
    device_.reset();
    io::session::removeObservers();
    drainRtToNrt();   // audio đã dừng: xoá snapshot RT đã trả về (event còn lại vẫn được phát)
}

// [main] 04 §5.1: maxTakeSeconds 64 s + lề 2 s (bù latency + đuôi). Cấp phát 1 lần cho mỗi track, giữ tới khi
// engine huỷ (≤ 8 × 12.7 MB mono @48k). RT nhận con trỏ qua atomic trong Recorder, không bao giờ free.
// R7: buffer thu cấp theo SR lúc arm. Device chạy lại ở SR cao hơn (48k → 96k) thì take dài nhất chỉ còn một nửa →
// dừng audio, xả take đang chờ copy (TakeFinished), cấp lại cho đủ 66 s, rồi chạy lại. Hiếm (đổi route / thiết bị).
void Engine::fitRecordBuffersToRate() {
    if (device_ == nullptr || !device_->isRunning()) return;
    const double sr = device_->sampleRate();
    const auto need = (std::int64_t) std::ceil(66.0 * std::max(sr, 48000.0));
    bool small = false;
    for (const auto& b : takeBuffers_) small |= b != nullptr && b->numFrames() < need;
    if (!small) return;
    device_->stop();
    drainRtToNrt();   // take đã xong nhưng chưa copy → copy từ buffer cũ trước khi thay
    const int ch = std::clamp(numInputs_, 1, 2);
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        if (takeBuffers_[t] == nullptr || takeBuffers_[t]->numFrames() >= need) continue;
        takeBuffers_[t] = std::make_unique<dsp::AudioData>(ch, need, sr);
        rt_.recorder().setBuffer(t, takeBuffers_[t].get());   // audio dừng: RT không giữ con trỏ cũ (prepare reset)
    }
    io::DeviceConfig dc;
    dc.sampleRate = preferredRate_;
    dc.bufferSize = preferredBuffer_;
    dc.numInputs = numInputs_;
    dc.numOutputs = 2;
    if (device_->start(dc, &rt_) != LE_OK) emitEvent(LE_EVT_ERROR, LE_ERR_AUDIO_DEVICE, 0, 0, 0.0);
}

void Engine::ensureRecordBuffer(int track) {
    if (track < 0 || track >= LE_MAX_TRACKS || takeBuffers_[track] != nullptr) return;
    const double sr = device_ != nullptr && device_->isRunning() ? device_->sampleRate() : preferredRate_;
    const int ch = std::clamp(numInputs_, 1, 2);
    takeBuffers_[track] = std::make_unique<dsp::AudioData>(ch, (std::int64_t) std::ceil(66.0 * std::max(sr, 48000.0)), sr);
    rt_.recorder().setBuffer(track, takeBuffers_[track].get());
}

// [main] Take xong: copy vùng [offset, offset + frames) → AudioData đúng kích thước → clip trong model →
// snapshot mới (player crossfade cùng pha từ buffer thu sang bản copy) → worker ghi audio/<clipId>.caf →
// RECORDING_FINISHED(track, slot) SAU KHI đóng file (04 §5.6, P1-21).
void Engine::handleTakeFinished(const RtMessage& m) {
    const int t = m.a, s = m.b;
    dsp::AudioData* buf = (t >= 0 && t < LE_MAX_TRACKS) ? takeBuffers_[t].get() : nullptr;
    if (jobs_ == nullptr) {   // đang huỷ engine (~Engine xả queue): bỏ take
        if (buf != nullptr) rt_.recorder().releaseTake(t);
        return;
    }
    const bool stale = buf == nullptr || s < 0 || s >= LE_MAX_SCENES || m.projectEpoch != model_.projectEpoch ||
                       m.cellEpoch != model_.cellEpoch[t][s] || model_.clips[t][s].has_value();
    if (stale) {   // project.open / clip.clear / ô đã có clip trong lúc thu → bỏ take
        if (buf != nullptr) rt_.recorder().releaseTake(t);
        return;
    }
    auto take = std::make_shared<dsp::AudioData>(buf->numChannels(), m.i1, rt_.sampleRate());
    for (int c = 0; c < buf->numChannels(); ++c)
        std::copy(buf->channel(c) + m.i0, buf->channel(c) + m.i0 + m.i1, take->writePointer(c));
    rt_.recorder().releaseTake(t);   // vùng đó RT dùng lại được

    ClipModel c;
    c.clipId = "rec_" + std::to_string(juce::Time::currentTimeMillis()) + "_" + std::to_string(++takeCounter_);
    c.kind = ClipKind::Audio;
    c.lengthBeats = m.value;
    c.originalBpm = m.value2;
    c.warp = WarpMode::Repitch;
    std::string dir, file;
    if (!model_.projectDir.empty()) {
        dir = model_.projectDir;
        file = "audio/" + c.clipId + ".caf";   // tương đối với project (06 §1)
    } else if (!dataDir_.empty()) {
        dir = dataDir_;
        file = juce::File(juce::String::fromUTF8(dataDir_.c_str())).getChildFile("audio/" + c.clipId + ".caf").getFullPathName().toStdString();
    }
    c.file = file;
    c.audio = take;
    model_.clips[t][s] = std::move(c);
    publishSnapshot();

    persistClipAudio(t, s, std::move(take), m.i1);
}

void Engine::persistClipAudio(int t, int s, dsp::AudioDataPtr data, std::int64_t eventValue) {
    auto& cm = model_.clips[t][s];
    if (!cm) return;
    const std::string clipId = cm->clipId;
    std::string dir;
    if (!model_.projectDir.empty()) dir = model_.projectDir;
    else if (!dataDir_.empty()) dir = dataDir_;
    if (dir.empty()) {   // không có chỗ ghi (chưa project.open, không dataDir): chỉ nằm trong RAM
        peaks_[clipId] = peaksFor(*data, {}, nullptr);
        emitEvent(LE_EVT_RECORDING_FINISHED, t, s, 0, (double) eventValue);
        return;
    }
    const juce::File base(juce::String::fromUTF8(dir.c_str()));
    const std::string path = base.getChildFile(juce::String::fromUTF8(("audio/" + clipId + ".caf").c_str())).getFullPathName().toStdString();
    const std::string cache = model_.projectDir.empty() ? std::string()
        : base.getChildFile(juce::String::fromUTF8(("cache/" + clipId + ".peaks").c_str())).getFullPathName().toStdString();
    if (!cache.empty()) juce::File(juce::String::fromUTF8(cache.c_str())).deleteFile();   // dữ liệu đổi → cache cũ không còn đúng
    auto peaks = std::make_shared<std::shared_ptr<const render::Peaks>>();
    jobs_->submit(
        "writeTake",
        [path, cache, data, peaks](JobSystem::Context& ctx) {   // [worker] ghi file → tính peaks (04 §5.6)
            std::string err;
            if (!io::writeCafFloat32(path, *data, &err)) return JobOutcome::fail(LE_ERR_DISK_FULL, err);
            *peaks = peaksFor(*data, cache, &ctx.cancel);
            return JobOutcome::ok({});
        },
        [this, t, s, eventValue, clipId, peaks](JobOutcome& out) {   // [main] file đã đóng
            if (*peaks) peaks_[clipId] = *peaks;
            if (out.error != LE_OK) emitEvent(LE_EVT_ERROR, out.error, 0, 0, 0.0);
            emitEvent(LE_EVT_RECORDING_FINISHED, t, s, 0, (double) eventValue);
        },
        false);   // job nội bộ: không phát JOB_* cho Dart
}

// [main] P1-22 (04 §5.4): KHÔNG sửa buffer mà snapshot khác còn tham chiếu. Bật overdub trên clip audio đang Playing →
// copy thành target (RT sẽ cộng input vào), giữ bản gốc làm lớp undo, đưa target vào snapshot (cùng nội dung → player
// chuyển không nghe thấy) TRƯỚC khi push lệnh. Trạng thái đọc từ LeState (trễ ≤ 1 block); RT tự kiểm lại.
// ─────────────── Overdub audio (P1-22) — giao thức vé (R1, rt-review 2026-09-29) ───────────────
// Lệnh OVERDUB_TOGGLE của Dart không nói bật hay tắt. Main quyết định theo ý định của chính nó (overdubOn_) rồi ghi
// i0 = 1 (bật) / 0 (tắt); RT làm đúng lệnh (idempotent). Bật clip audio cần một VÉ mới (bản copy + session):
// - Vé RT chưa lấy (lượt bật trước bị bỏ qua) → main thu lại bằng exchange rồi đưa vé mới.
// - Vé RT đã lấy mà chưa trả (lượt trước còn ghi phần bù L / OverdubFinished chưa được pump) → HOÃN lệnh bật tới
//   khi RT trả vé. Nhờ vậy RT không bao giờ ghi vào target mà main đã nhả, và lượt nào cũng được lưu.
bool Engine::prepareOverdubCommand(LeCommand& c) {
    const int t = c.track;
    LeState st{};
    globalStatePublisher().read(st);
    bool rtOverdubbing = false;
    for (int s = 0; s < LE_MAX_SCENES; ++s) rtOverdubbing |= st.clipState[t][s] == LE_CLIP_OVERDUBBING;
    // RT tự thoát overdub (clip dừng, transport stop, launch clip khác): thấy qua LeState publish SAU lệnh bật.
    if (overdubOn_[t] && !overdubDeferred_[t] && !rtOverdubbing && overdubRounds_[t].empty() &&
        st.publishCounter >= overdubOnCounter_[t] + 2)
        overdubOn_[t] = false;
    if (overdubOn_[t]) {   // → TẮT (hoặc huỷ lệnh bật đang hoãn)
        overdubOn_[t] = false;
        if (overdubDeferred_[t]) {
            overdubDeferred_[t] = false;
            return false;
        }
        c.i0 = 0;
        return true;
    }
    overdubOn_[t] = true;
    overdubOnCounter_[t] = st.publishCounter;
    c.i0 = 1;
    const int slot = st.trackPlayingSlot[t];
    const auto* cm = slot >= 0 ? &model_.clips[t][slot] : nullptr;
    if (cm == nullptr || !*cm || (*cm)->kind != ClipKind::Audio || (*cm)->audio == nullptr)
        return true;   // clip MIDI (P1-30), chưa có clip / chưa vào model: RT tự quyết (có thể báo UNSUPPORTED)
    reclaimOverdubTicket(t);
    if (!overdubRounds_[t].empty()) {   // RT còn giữ vé của lượt trước
        overdubDeferred_[t] = true;
        return false;
    }
    offerOverdubTicket(t);
    return true;
}

void Engine::reclaimOverdubTicket(int t) {
    Recorder::OverdubTicket* back = rt_.recorder().offerOverdub(t, nullptr);
    if (back == nullptr) return;   // không có vé, hoặc RT đã lấy
    auto& rounds = overdubRounds_[t];
    rounds.erase(std::remove_if(rounds.begin(), rounds.end(), [back](const OverdubRound& r) { return r.ticket.get() == back; }),
                 rounds.end());   // RT chưa từng thấy target này → nhả được ngay (model vẫn giữ bản copy, cùng nội dung)
}

bool Engine::offerOverdubTicket(int t) {
    LeState st{};
    globalStatePublisher().read(st);
    const int slot = st.trackPlayingSlot[t];
    if (slot < 0) return false;
    auto& cm = model_.clips[t][slot];
    if (!cm || cm->kind != ClipKind::Audio || cm->audio == nullptr) return false;
    const dsp::AudioData& src = *cm->audio;
    auto target = std::make_shared<dsp::AudioData>(src.numChannels(), src.numFrames(), src.sampleRate());
    for (int c = 0; c < src.numChannels(); ++c) std::copy(src.channel(c), src.channel(c) + src.numFrames(), target->writePointer(c));
    OverdubRound r;
    r.ticket = std::make_unique<Recorder::OverdubTicket>();
    r.ticket->session = ++overdubSession_;
    r.ticket->slot = slot;
    r.ticket->target = target.get();
    r.target = target;
    r.before = cm->audio;
    cm->audio = target;
    publishSnapshot();   // snapshot có target TRƯỚC khi RT thấy vé (RT vẫn tự kiểm snapshot khớp rồi mới ghi)
    Recorder::OverdubTicket* prev = rt_.recorder().offerOverdub(t, r.ticket.get());
    overdubRounds_[t].push_back(std::move(r));
    if (prev != nullptr) {   // không xảy ra (đã reclaim, main là bên duy nhất đưa vé) — vẫn dọn cho chắc
        auto& rounds = overdubRounds_[t];
        rounds.erase(std::remove_if(rounds.begin(), rounds.end(), [prev](const OverdubRound& x) { return x.ticket.get() == prev; }),
                     rounds.end());
    }
    return true;
}

void Engine::handleOverdubFinished(const RtMessage& m) {
    const int t = m.a, s = m.b;
    if (t < 0 || t >= LE_MAX_TRACKS) return;
    auto& rounds = overdubRounds_[t];
    const auto it = std::find_if(rounds.begin(), rounds.end(),
                                 [&](const OverdubRound& r) { return r.ticket->session == (std::uint32_t) m.i0; });
    if (it == rounds.end()) return;
    OverdubRound r = std::move(*it);
    rounds.erase(it);   // RT đã trả vé: không còn con trỏ nào tới target trên RT
    if (m.i1 != 0 && s >= 0 && s < LE_MAX_SCENES) {
        auto& cm = model_.clips[t][s];
        if (cm && cm->audio.get() == r.target.get()) {   // ô không bị thay/xoá trong lúc overdub
            undoLayer_[t][s] = r.before;
            persistClipAudio(t, s, cm->audio, cm->audio->numFrames());
            refreshWarp(t, s);
        }
    }
    if (!rounds.empty()) return;
    if (overdubDeferred_[t]) {   // lệnh bật đã hoãn → bật ngay bây giờ
        overdubDeferred_[t] = false;
        if (offerOverdubTicket(t)) {
            LeCommand c{};
            c.type = LE_CMD_OVERDUB_TOGGLE;
            c.track = (std::int8_t) t;
            c.slot = -1;
            c.i0 = 1;
            if (!commands_.try_push(c)) ++queueFull_;
        } else {
            overdubOn_[t] = false;   // clip đã dừng trong lúc chờ
        }
    } else {
        overdubOn_[t] = false;   // RT đã trả mọi vé: không còn overdub audio (tắt, clip dừng, hoặc UNSUPPORTED)
    }
}

// [main] P1-30. Nốt đến theo thứ tự trong CÙNG queue với MidiTakeFinished → khi take xong đã có đủ nốt.
void Engine::handleMidiMessage(const RtMessage& m) {
    const int t = m.a, s = m.b;
    if (t < 0 || t >= LE_MAX_TRACKS || s < 0 || s >= LE_MAX_SCENES) return;
    auto closeNote = [](OpenNotes& acc, int pitch, double endBeat, double wrapLen) {
        if (!acc.open[pitch]) return;
        acc.open[pitch] = false;
        double d = endBeat - acc.start[pitch];
        if (wrapLen > 0.0 && d <= 0.0) d += wrapLen;   // overdub: nhả phím sau điểm loop
        dsp::MidiNote n;
        n.startBeat = acc.start[pitch];
        n.lengthBeats = std::max(d, 1e-3);
        n.pitch = (std::uint8_t) pitch;
        n.velocity = (std::uint8_t) acc.vel[pitch];
        acc.done.push_back(n);
    };
    const int pitch = (int) std::clamp<std::int64_t>(m.i0, 0, 127);

    if (m.kind == RtMessage::MidiNote) {   // nốt trong take (beat tính từ đầu take)
        OpenNotes& acc = midiTake_[t];
        if (m.i1 > 0) {
            closeNote(acc, pitch, m.value, 0.0);   // bấm lại cao độ đang giữ
            acc.open[pitch] = true;
            acc.start[pitch] = m.value;
            acc.vel[pitch] = (int) m.i1;
        } else {
            closeNote(acc, pitch, m.value, 0.0);
        }
        return;
    }

    if (m.kind == RtMessage::MidiTakeFinished) {
        OpenNotes acc = std::move(midiTake_[t]);
        midiTake_[t] = OpenNotes{};
        const double len = m.value;
        const bool stale = m.projectEpoch != model_.projectEpoch || m.cellEpoch != model_.cellEpoch[t][s] ||
                           model_.clips[t][s].has_value() || len <= 0.0;
        if (stale) return;
        for (int p = 0; p < 128; ++p) closeNote(acc, p, len, 0.0);   // phím còn giữ → kéo tới cuối take
        auto clip = std::make_shared<dsp::MidiClip>();
        clip->lengthBeats = len;
        for (dsp::MidiNote n : acc.done) {
            if (recordQuantize_ > 0.0) {   // quantize khi thu: điểm bắt đầu về lưới gần nhất, giữ độ dài
                n.startBeat = std::round(n.startBeat / recordQuantize_) * recordQuantize_;
                if (n.startBeat >= len) n.startBeat -= len;
            }
            n.lengthBeats = std::min(n.lengthBeats, len);
            clip->notes.push_back(n);
        }
        std::stable_sort(clip->notes.begin(), clip->notes.end(),
                         [](const dsp::MidiNote& a, const dsp::MidiNote& b) { return a.startBeat < b.startBeat; });
        ClipModel c;
        c.clipId = "rec_" + std::to_string(juce::Time::currentTimeMillis()) + "_" + std::to_string(++takeCounter_);
        c.kind = ClipKind::Midi;
        c.lengthBeats = len;
        const int count = (int) clip->notes.size();
        c.midi = std::move(clip);
        model_.clips[t][s] = std::move(c);
        publishSnapshot();
        emitEvent(LE_EVT_RECORDING_FINISHED, t, s, 0, (double) count);
        return;
    }

    // MidiOverdubNote: vị trí trong clip đang phát; nốt hoàn chỉnh được trộn ngay vào clip (snapshot mới,
    // MidiClipPlayer phát tiếp cùng pha).
    OpenNotes& acc = midiOverdub_[t];
    if (m.kind == RtMessage::MidiOverdubFinished) {   // đóng nốt còn giữ tại điểm kết thúc, rồi báo Dart lưu
        for (int p = 0; p < 128; ++p) closeNote(acc, p, m.value, m.value2);
        auto& cm = model_.clips[t][s];
        if (!acc.done.empty() && cm && cm->kind == ClipKind::Midi && cm->midi != nullptr) {
            auto clip = std::make_shared<dsp::MidiClip>(*cm->midi);
            for (auto& n : acc.done) {
                n.lengthBeats = std::min(n.lengthBeats, clip->lengthBeats);
                clip->notes.push_back(n);
            }
            midiOverdubNotes_[t] += (int) acc.done.size();
            std::stable_sort(clip->notes.begin(), clip->notes.end(),
                             [](const dsp::MidiNote& a, const dsp::MidiNote& b) { return a.startBeat < b.startBeat; });
            cm->midi = std::move(clip);
            publishSnapshot();
        }
        acc.done.clear();
        if (cm) {
            clipChanged(t, s, true);   // piano roll có đủ nốt TRƯỚC khi Dart lưu clip
            emitEvent(LE_EVT_RECORDING_FINISHED, t, s, 0, (double) midiOverdubNotes_[t]);
        }
        midiOverdubNotes_[t] = 0;
        return;
    }
    if (m.i1 > 0) {
        acc.open[pitch] = true;
        acc.start[pitch] = m.value;
        acc.vel[pitch] = (int) m.i1;
        return;
    }
    closeNote(acc, pitch, m.value, m.value2);
    auto& cm = model_.clips[t][s];
    if (acc.done.empty() || !cm || cm->kind != ClipKind::Midi || cm->midi == nullptr) {
        acc.done.clear();
        return;
    }
    auto clip = std::make_shared<dsp::MidiClip>(*cm->midi);
    for (auto& n : acc.done) {
        n.lengthBeats = std::min(n.lengthBeats, clip->lengthBeats);
        clip->notes.push_back(n);
    }
    midiOverdubNotes_[t] += (int) acc.done.size();
    acc.done.clear();
    std::stable_sort(clip->notes.begin(), clip->notes.end(),
                     [](const dsp::MidiNote& a, const dsp::MidiNote& b) { return a.startBeat < b.startBeat; });
    cm->midi = std::move(clip);
    publishSnapshot();
    clipChanged(t, s, false);   // ghi Live: nốt hiện trên piano roll gần như ngay (≤ 100 ms + 1 tick pump)
}

void Engine::clipChanged(int t, int s, bool final) {
    const double now = debounceClockMs();
    if (!final && clipChangedSent_[t][s] && now - clipChangedMs_[t][s] < kClipChangedMs) {
        clipChangedPending_[t][s] = true;
        anyClipChangedPending_ = true;
        return;
    }
    clipChangedMs_[t][s] = now;
    clipChangedSent_[t][s] = true;
    clipChangedPending_[t][s] = false;
    emitEvent(LE_EVT_CLIP_CHANGED, t, s, 0, 0.0);
}

void Engine::flushClipChanged() {
    if (!anyClipChangedPending_) return;
    const double now = debounceClockMs();
    bool still = false;
    for (int t = 0; t < LE_MAX_TRACKS; ++t)
        for (int s = 0; s < LE_MAX_SCENES; ++s) {
            if (!clipChangedPending_[t][s]) continue;
            if (now - clipChangedMs_[t][s] >= kClipChangedMs) clipChanged(t, s, true);
            else still = true;
        }
    anyClipChangedPending_ = still;
}

std::shared_ptr<const render::Peaks> Engine::peaksFor(const dsp::AudioData& data, const std::string& cachePath,
                                                      const std::atomic<bool>* cancel) {
    auto p = std::make_shared<render::Peaks>();
    if (!cachePath.empty() && render::readPeaksFile(cachePath, *p, nullptr, data.numFrames(), data.sampleRate())) return p;
    *p = render::buildPeaks(data, cancel);
    if (!cachePath.empty()) {
        juce::File(juce::String::fromUTF8(cachePath.c_str())).getParentDirectory().createDirectory();
        render::writePeaksFile(cachePath, *p);   // lỗi ghi cache không chặn gì: lần sau tính lại
    }
    return p;
}

std::int32_t Engine::getPeaks(const char* clipId, std::int32_t level, float* outMinMax, std::int32_t maxPairs) const {
    if (clipId == nullptr || outMinMax == nullptr || maxPairs < 0 || level < 0 || level >= render::Peaks::kLevels)
        return LE_ERR_INVALID_ARG;
    if (!juce::CharPointer_UTF8::isValidString(clipId, std::numeric_limits<int>::max())) return LE_ERR_INVALID_ARG;   // 05 §1
    const std::string id(clipId);
    const auto it = peaks_.find(id);
    if (it != peaks_.end()) return it->second->copyTo(level, outMinMax, maxPairs);
    for (const auto& row : model_.clips)   // clip có trong model nhưng peaks đang tính → 0 cặp
        for (const auto& c : row)
            if (c && c->clipId == id) return 0;
    return LE_ERR_INVALID_ARG;
}

void Engine::publishSnapshot() {
    // Bản cũ chưa được RT lấy (nếu có) được trả lại và huỷ ngay ở đây, trên main.
    rt_.snapshots().publish(buildSnapshot(model_, ++generation_));
    syncTempoToRt();   // có / hết clip → pedal mode có / chưa có tempo
}

bool Engine::hasTempo() const {
    if (!model_.tempoFirstLoop) return true;
    if (tempoFound_) return true;
    for (const auto& row : model_.clips)
        for (const auto& c : row)
            if (c) return true;
    return false;
}

// Nút LOOP (07 §3.1b, 05 §2). Chọn ô: slot ≥ 0 → ô đó; −1 → ô Recording / Overdubbing / Playing của track, không có
// thì ô trống đầu tiên. Empty → thu tự do (pedal chưa có tempo: vòng đầu 04 §2.5) · QueuedRecord / Recording → chốt ·
// Playing → overdub bật · Overdubbing → overdub tắt · Stopped / Queued → launch. Đi qua send() như lệnh của Dart →
// có đủ side effect (buffer thu, vé overdub). Trạng thái lấy từ LeState (trễ ≤ 1 block).
bool Engine::loopButton(int t, int slot) {
    LeState st{};
    globalStatePublisher().read(st);
    int s = slot;
    if (s < 0) {
        for (const int want : {LE_CLIP_RECORDING, LE_CLIP_OVERDUBBING, LE_CLIP_PLAYING, LE_CLIP_QUEUED_RECORD})
            for (int k = 0; k < LE_MAX_SCENES && s < 0; ++k)
                if (st.clipState[t][k] == want) s = k;
        for (int k = 0; k < LE_MAX_SCENES && s < 0; ++k)
            if (st.clipState[t][k] == LE_CLIP_EMPTY && !model_.clips[t][k]) s = k;
        if (s < 0) return false;   // track đầy, không ô nào đang chạy
    }
    LeCommand c{};
    c.track = (std::int8_t) t;
    c.slot = (std::int8_t) s;
    switch (st.clipState[t][s]) {
        case LE_CLIP_EMPTY:
            c.type = LE_CMD_CLIP_RECORD;   // i0 = 0: tự do
            break;
        case LE_CLIP_QUEUED_RECORD:
        case LE_CLIP_RECORDING:
            c.type = LE_CMD_RECORD_STOP;
            break;
        case LE_CLIP_PLAYING:
            overdubOn_[t] = false;   // RT đang Playing → lệnh toggle này là BẬT
            c.type = LE_CMD_OVERDUB_TOGGLE;
            break;
        case LE_CLIP_OVERDUBBING:
            overdubOn_[t] = true;    // → TẮT
            overdubDeferred_[t] = false;
            c.type = LE_CMD_OVERDUB_TOGGLE;
            break;
        default:   // Stopped / QueuedPlay / QueuedStop
            c.type = LE_CMD_CLIP_LAUNCH;
            break;
    }
    return send(c);
}

bool Engine::hasClipInModel() const {
    for (const auto& row : model_.clips)
        for (const auto& c : row)
            if (c) return true;
    return false;
}

void Engine::syncTempoToRt() { rt_.setPedalState(model_.tempoFirstLoop, hasTempo(), firstLoopBeats_); }

// 05 §3: {mode:"fixed"|"firstLoop", firstLoopBeats?} → {hasTempo, firstLoopBeats}. State project (06 §2
// transport.tempoMode / firstLoopBeats): mở lại project → vòng sau vẫn làm tròn theo bội số vòng đầu. 0 = chưa biết
// (làm tròn theo 1 bar).
Reply Engine::opTransportSetTempoMode(const juce::var& req) {
    std::string mode;
    if (!args::getString(req, "mode", mode) || (mode != "fixed" && mode != "firstLoop"))
        return Reply::fail(LE_ERR_INVALID_ARG, "mode phải là \"fixed\" hoặc \"firstLoop\"");
    double beats = firstLoopBeats_;
    if (req.hasProperty("firstLoopBeats") && !args::getDouble(req, "firstLoopBeats", beats, 0.0, 4096.0))
        return Reply::fail(LE_ERR_INVALID_ARG, "firstLoopBeats phải trong 0..4096 (0 = chưa biết)");
    model_.tempoFirstLoop = mode == "firstLoop";
    firstLoopBeats_ = beats;
    syncTempoToRt();
    auto* r = new juce::DynamicObject();
    r->setProperty("hasTempo", hasTempo());
    r->setProperty("firstLoopBeats", firstLoopBeats_);
    return Reply::ok(juce::var(r));
}

void Engine::drainRtToNrt() {
    while (const RtMessage* m = toNrt_.front()) {
        const RtMessage msg = *m;
        toNrt_.pop();
        if (msg.kind == RtMessage::Retire) {
            delete msg.snapshot;   // ReleasePool: shared_ptr (AudioData, Instrument) được huỷ trên main
            ++releasedSnapshots_;
        } else if (msg.kind == RtMessage::TakeFinished) {
            handleTakeFinished(msg);
        } else if (msg.kind == RtMessage::OverdubFinished) {
            handleOverdubFinished(msg);
        } else if (msg.kind == RtMessage::CaptureFinished) {
            handleCaptureFinished(msg);
        } else if (msg.kind == RtMessage::JamStopped) {
            handleJamStopped(msg);
        } else if (msg.kind == RtMessage::PreviewReleased) {
            handlePreviewReleased(msg);
        } else if (msg.kind == RtMessage::MidiLearned) {
            handleMidiLearned(msg);
        } else if (msg.kind == RtMessage::MappedChange) {
            handleMappedChange(msg);
        } else if (msg.kind == RtMessage::UndoOverdub) {   // footswitch: ô đang phát; không có lớp undo → bỏ qua
            if (msg.a >= 0 && msg.a < LE_MAX_TRACKS) {
                LeState st{};
                globalStatePublisher().read(st);
                if (st.trackPlayingSlot[msg.a] >= 0) (void) undoOverdub(msg.a, st.trackPlayingSlot[msg.a]);
            }
        } else if (msg.kind == RtMessage::LoopButton) {   // footswitch qua MIDI learn → như LE_CMD_LOOP_BUTTON
            if (msg.a >= 0 && msg.a < LE_MAX_TRACKS) (void) loopButton(msg.a, -1);
        } else if (msg.kind == RtMessage::MidiNote || msg.kind == RtMessage::MidiTakeFinished ||
                   msg.kind == RtMessage::MidiOverdubNote || msg.kind == RtMessage::MidiOverdubFinished) {
            handleMidiMessage(msg);
        } else {
            if (msg.type == LE_EVT_TEMPO_CHANGED) {   // pedal mode: vòng đầu chốt tempo (04 §2.5)
                bpm_ = msg.value;
                tempoFound_ = true;
                firstLoopBeats_ = msg.value2;
                syncTempoToRt();
            }
            emitEvent(msg.type, msg.a, msg.b, 0, msg.value);
        }
    }
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
    fitRecordBuffersToRate();
    startGraceTicks_ = 15;   // 0.5 giây
    return LE_OK;
}

void Engine::audioStop() {
    if (device_ != nullptr) device_->stop();
}

bool Engine::send(const LeCommand& cmd) {
    // [main] P1-04: validate → side effect NRT (P1-18: TRACK_ARM cấp phát buffer thu; P1-? SET_BPM xếp job warp) → push.
    if (!isValidCommand(cmd)) {
        ++rejectedCommands_;
        return false;
    }
    if (cmd.type == LE_CMD_LOOP_BUTTON) return loopButton(cmd.track, cmd.slot);   // dịch ở main (P1-39)
    LeCommand c = cmd;   // bản copy: lệnh FX của track được ghi instanceId vào d0 (P3-12)
    const bool fx = c.type == LE_CMD_FX_PARAM || c.type == LE_CMD_FX_BYPASS;
    if (fx && !prepareFxCommand(c)) {   // slot trống / paramId không có ở loại FX này
        ++rejectedCommands_;
        return false;
    }
    // Side effect NRT trước khi push (03 §3): thu âm cần buffer cấp phát sẵn trên main (04 §5.1).
    if ((c.type == LE_CMD_TRACK_ARM && c.i0 != 0) || c.type == LE_CMD_CLIP_RECORD) ensureRecordBuffer(c.track);
    if (c.type == LE_CMD_OVERDUB_TOGGLE && !prepareOverdubCommand(c)) return true;   // lệnh bật được hoãn (R1)
    if (!commands_.try_push(c)) {   // queue đầy: báo false, không chờ
        ++queueFull_;
        return false;
    }
    if (fx) commitFxCommand(c);
    switch (c.type) {   // mixer vào model (export offline dựng lại đúng mix, 04 §12)
        case LE_CMD_TRACK_GAIN: model_.mixer[c.track].gainDb = c.f0; break;
        case LE_CMD_TRACK_PAN: model_.mixer[c.track].pan = c.f0; break;
        case LE_CMD_TRACK_MUTE: model_.mixer[c.track].mute = c.i0 != 0; break;
        case LE_CMD_TRACK_SOLO: model_.mixer[c.track].solo = c.i0 != 0; break;
        case LE_CMD_MASTER_GAIN: model_.master.gainDb = c.f0; break;
        default: break;
    }
    if (c.type == LE_CMD_SET_BPM) {   // P3-08: [RT] Re-Pitch ngay; [NRT] BPM đứng yên 300 ms → render bản stretched
        bpm_ = c.d0;
        warpDueMs_ = debounceClockMs() + kWarpDebounceMs;
    }
    return true;
}

void Engine::refreshDeviceInfo() {
    if (device_ == nullptr || !device_->isRunning()) return;
    rt_.setLatencyRoundTrip(std::max(0, device_->latencies().roundTrip() + latencyOffset_));   // + offset đo / chỉnh tay
    rt_.setHeadphones(device_->route().wired);   // P1-23: monitoring Auto
    rt_.setDeviceXruns(device_->xrunCount());
    // MIDI (P4-02): trễ lập lịch 1 buffer → vị trí tương đối giữa các nốt đúng, không jitter. Offline: 0 (tất định).
    rt_.setMidiScheduleLatencyNs(offlineDevice() ? 0
                                                 : (std::int64_t) (1e9 * device_->bufferSize() / std::max(1.0, device_->sampleRate())));
}

void Engine::pump() {
    jobs_->pump();

    // 1) Event + snapshot cũ + LaunchLog từ audio thread
    drainRtToNrt();
    flushClipChanged();   // CLIP_CHANGED đã dồn (≤ 10 lần/s mỗi ô)
    while (const LaunchEvent* e = launchLog_.front()) {
        if (launchLogEntries_.size() < 1'000'000) launchLogEntries_.push_back(*e);
        launchLog_.pop();
    }

    // 2) Xrun: so với lần trước, tối đa 30 event/giây
    if (device_ != nullptr && device_->isRunning()) rt_.setDeviceXruns(device_->xrunCount());
    LeState s;
    globalStatePublisher().read(s);
    std::uint8_t lpColors[8];   // P4-05: màu track (06 §2 color) → palette Launchpad gần nhất
    for (int t = 0; t < 8; ++t)
        lpColors[t] = model_.tracks[t].hasColor ? midi::nearestPaletteColor(model_.tracks[t].color) : MidiDevices::defaultTrackColor(t);
    midi_->updateLaunchpadLeds(s, lpColors);   // LED theo clipState (chỉ gửi ô đổi)
    // Pedal mode: transport dừng VÀ project hết clip → về "chưa có tempo" (04 §2.5)
    if (model_.tempoFirstLoop && tempoFound_ && s.playing == 0 && s.anyRecording == 0 && !hasClipInModel()) {
        tempoFound_ = false;
        firstLoopBeats_ = 0.0;
        syncTempoToRt();
        emitEvent(LE_EVT_TEMPO_CHANGED, 0, 0, 0, 0.0);   // value = 0: về "chưa có tempo" (05 §2) — UI theo event
    }
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

    // 4) Media services reset (03 §8, audio-session.md §4): JUCE không tạo lại AudioUnit → stop + start trên main.
    if (const auto v = c.mediaServicesReset.load(); v != seenMediaReset_) {
        seenMediaReset_ = v;
        if (device_ != nullptr && device_->isRunning()) {
            device_->stop();
            io::DeviceConfig dc;
            dc.sampleRate = preferredRate_;
            dc.bufferSize = preferredBuffer_;
            dc.numInputs = numInputs_;
            dc.numOutputs = 2;
            const std::int32_t err = device_->start(dc, &rt_);
            ++mediaResets_;
            if (err != LE_OK) {
                emitEvent(LE_EVT_ERROR, err, 0, 0, 0.0);
            } else {
                refreshDeviceInfo();
                fitRecordBuffersToRate();
                const auto r = io::session::currentRoute();
                emitEvent(LE_EVT_ROUTE_CHANGED, r.wired ? 1 : 0, r.bluetooth ? 1 : 0, 0, 0.0);
            }
        }
    }

    // 5b) P3-08: debounce đổi tempo xong → WarpRenderer cho mọi clip warp = stretch lệch BPM
    if (warpDueMs_ >= 0.0 && debounceClockMs() >= warpDueMs_) {
        warpDueMs_ = -1.0;
        refreshAllWarps();
    }

    // 4b) P4-17: iOS báo thiếu RAM → nhả cache (mức critical: hệ thống đã cảnh báo, sắp jetsam)
    if (const auto v = c.memoryWarnings.load(); v != seenMemoryWarnings_) {
        seenMemoryWarnings_ = v;
        (void) releaseMemory(true);
    }

    // 5) Latency đọc lại mỗi giây (getter rẻ)
    if (++pumpTicks_ >= 30) {
        pumpTicks_ = 0;
        refreshDeviceInfo();
    }
}

// ─────────────────────────── le_call ───────────────────────────
// Các handler nằm ở EngineOps.cpp (registerOps). CommandProcessor làm parse + envelope.

std::string Engine::call(const char* requestJson) { return ops_.call(requestJson); }

} // namespace le::core
