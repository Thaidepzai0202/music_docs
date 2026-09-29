// P4-01/02 midi.listDevices / enableDevice · P4-04 midi.learnStart / learnCancel / learnResult / setMappings.
// [main] Thiết bị: MidiDevices. Bảng mapping (MidiLearnMap của 80) dựng ở đây, đưa vào snapshot; RT tra + áp.
#include <algorithm>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "core/MidiDevices.h"

namespace le::core {

namespace {

float num(const juce::var& o, const char* key, float def) {
    const juce::var v = o.getProperty(key, {});
    return args::isNumber(v) && std::isfinite((double) v) ? (float) (double) v : def;
}

} // namespace

// target (05 §3, khớp midi::LearnAction): {kind:"clip",track,slot} · {kind:"scene",slot} ·
// {kind:"transport",action:"play"|"stop"|"toggle"} · {kind:"stopAll"} · {kind:"trackGain",track,minDb?,maxDb?} ·
// {kind:"trackMute",track} · {kind:"fx",track(-1=master),slot,param,min?,max?}. Sai → chuỗi lỗi (rỗng = ok).
std::string Engine::parseMidiTarget(const juce::var& t, midi::LearnTarget& out) const {
    if (!t.isObject()) return "target phải là object";
    std::string kind;
    if (!args::getString(t, "kind", kind)) return "thiếu target.kind";
    out = midi::LearnTarget{};
    using A = midi::LearnAction;
    int track = 0, slot = 0, param = 0;
    if (kind == "clip") {
        if (!args::getInt(t, "track", track, 0, LE_MAX_TRACKS - 1) || !args::getInt(t, "slot", slot, 0, LE_MAX_SCENES - 1))
            return "clip: track 0..7, slot 0..7";
        out.action = A::ClipLaunch;
    } else if (kind == "scene") {
        if (!args::getInt(t, "slot", slot, 0, LE_MAX_SCENES - 1)) return "scene: slot 0..7";
        out.action = A::SceneLaunch;
    } else if (kind == "transport") {
        std::string a;
        if (!args::getString(t, "action", a)) return "transport: thiếu action";
        if (a == "play") out.action = A::TransportPlay;
        else if (a == "stop") out.action = A::TransportStop;
        else if (a == "toggle") out.action = A::TransportToggle;
        else return "transport.action phải là play | stop | toggle";
    } else if (kind == "stopAll") {
        out.action = A::StopAll;
    } else if (kind == "loopButton") {   // P1-39: = LE_CMD_LOOP_BUTTON(track đang chọn, slot −1)
        out.action = A::LoopButton;
    } else if (kind == "trackStop") {    // footswitch: dừng track đang chọn (theo quantize)
        out.action = A::TrackStop;
    } else if (kind == "undoOverdub") {  // footswitch: hoàn tác overdub ô đang phát của track đang chọn
        out.action = A::UndoOverdub;
    } else if (kind == "trackGain") {
        if (!args::getInt(t, "track", track, 0, LE_MAX_TRACKS - 1)) return "trackGain: track 0..7";
        out.action = A::TrackGain;
        out.minValue = std::clamp(num(t, "minDb", -60.0f), -120.0f, 6.0f);
        out.maxValue = std::clamp(num(t, "maxDb", 6.0f), -120.0f, 6.0f);
    } else if (kind == "trackMute") {
        if (!args::getInt(t, "track", track, 0, LE_MAX_TRACKS - 1)) return "trackMute: track 0..7";
        out.action = A::TrackMute;
    } else if (kind == "fx") {
        if (!args::getInt(t, "track", track, -1, LE_MAX_TRACKS - 1)) return "fx: track -1..7";
        float lo = 0.0f, hi = 1.0f;
        if (track < 0) {   // master cố định: slot 0 EQ3 (p 0..2), slot 1 limiter (p 0 trần dB, 1 release ms)
            if (!args::getInt(t, "slot", slot, 0, 1)) return "fx master: slot 0 (EQ3) hoặc 1 (limiter)";
            if (!args::getInt(t, "param", param, 0, slot == 0 ? 2 : 1)) return "fx master: param không hợp lệ";
            if (slot == 0) lo = -15.0f, hi = 15.0f;
            else if (param == 0) lo = Limiter::kMinCeilingDb, hi = Limiter::kMaxCeilingDb;
            else lo = Limiter::kMinReleaseMs, hi = Limiter::kMaxReleaseMs;
        } else {
            if (!args::getInt(t, "slot", slot, 0, kFxSlots - 1)) return "fx: slot 0..2";
            if (!args::getInt(t, "param", param, 0, kMaxFxParams - 1)) return "fx: param 0..7";
            if (const auto& f = model_.tracks[track].fx[slot])   // dải mặc định theo loại FX đang ở slot
                for (const dsp::ParamInfo& p : dsp::paramInfo(f->type))
                    if (p.id == param) lo = p.min, hi = p.max;
        }
        out.action = A::FxParam;
        out.minValue = num(t, "min", lo);
        out.maxValue = num(t, "max", hi);
        out.paramId = (std::int16_t) param;
    } else {
        return "target.kind không hợp lệ: " + kind;
    }
    out.track = (std::int8_t) track;
    out.slot = (std::int8_t) slot;
    return {};
}

// Dựng lại bảng: preset Launchpad (nguồn đang mở) rồi mapping người dùng (ghi đè). device "" / channel −1 = wildcard.
// Mapping cho thiết bị chưa cắm được giữ trong danh sách, vào bảng khi thiết bị mở.
void Engine::rebuildMidiMap() {
    auto map = std::make_shared<midi::MidiLearnMap>();
    midi_->addLaunchpadDefaults(*map);
    for (const UserMidiMapping& m : midiMappings_) {
        midi::LearnKey k;
        if (m.device.empty()) {
            k.source = kAnyMidiSource;
        } else if (m.device == kVirtualMidiDevice) {
            k.source = 0;
        } else {
            const int src = midi_->sourceForName(m.device);
            if (src < 0) continue;
            k.source = (std::uint16_t) src;
        }
        k.kind = m.kind;
        k.channel = m.channel < 0 ? kAnyMidiChannel : (std::uint8_t) m.channel;
        k.number = (std::uint8_t) m.number;
        (void) map->set(k, m.target);
    }
    model_.learnMap = map->size() > 0 ? std::shared_ptr<const midi::MidiLearnMap>(std::move(map)) : nullptr;
    publishSnapshot();
}

// – → {inputs:[{id,name,enabled,open}], outputs:[{id,name}]}
Reply Engine::opMidiListDevices(const juce::var&) { return Reply::ok(midi_->listDevices()); }

// {id, enabled} → {}. Thiết lập toàn cục (project.open không đổi). Thiết bị chưa cắm vẫn nhớ, cắm vào tự mở.
Reply Engine::opMidiEnableDevice(const juce::var& req) {
    std::string id;
    bool enabled = false;
    if (!args::getString(req, "id", id) || id.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"id\"");
    if (!args::getBool(req, "enabled", enabled)) return Reply::fail(LE_ERR_INVALID_ARG, "enabled phải là true/false");
    (void) midi_->enableDevice(id, enabled);
    rebuildMidiMap();   // Launchpad vừa mở / đóng, mapping theo tên thiết bị
    return Reply::ok();
}

// {target} → {}. Message nốt / CC kế tiếp (mọi nguồn) thành mapping → LE_EVT_MIDI_LEARNED(a = kind, b = number).
Reply Engine::opMidiLearnStart(const juce::var& req) {
    midi::LearnTarget t;
    const std::string err = parseMidiTarget(req["target"], t);
    if (!err.empty()) return Reply::fail(LE_ERR_INVALID_ARG, err);
    learnTarget_ = t;
    learnTargetJson_ = req["target"];
    learnPending_ = true;
    rt_.armMidiLearn(true);
    return Reply::ok();
}

Reply Engine::opMidiLearnCancel(const juce::var&) {
    learnPending_ = false;
    rt_.armMidiLearn(false);
    return Reply::ok();
}

// – → {deviceId, deviceName, kind:"note"|"cc", channel, number} của lần learn gần nhất.
Reply Engine::opMidiLearnResult(const juce::var&) {
    if (!lastLearn_.has_value()) return Reply::fail(LE_ERR_INVALID_ARG, "chưa có lần learn nào");
    auto* r = new juce::DynamicObject();
    r->setProperty("deviceId", juce::String::fromUTF8(lastLearn_->deviceId.c_str()));
    r->setProperty("deviceName", juce::String::fromUTF8(lastLearn_->deviceName.c_str()));
    r->setProperty("kind", lastLearn_->kind == midi::LearnKind::CC ? "cc" : "note");
    r->setProperty("channel", lastLearn_->channel);
    r->setProperty("number", lastLearn_->number);
    return Reply::ok(juce::var(r));
}

// {mappings:[{src:{device, kind:"note"|"cc", channel, number}, target}]} → {}. Thay TOÀN BỘ danh sách (state project).
Reply Engine::opMidiSetMappings(const juce::var& req) {
    const auto* arr = req["mappings"].getArray();
    if (arr == nullptr) return Reply::fail(LE_ERR_INVALID_ARG, "mappings phải là mảng");
    if (arr->size() > midi::MidiLearnMap::kCapacity) return Reply::fail(LE_ERR_INVALID_ARG, "tối đa 256 mapping");
    std::vector<UserMidiMapping> next;
    for (int i = 0; i < arr->size(); ++i) {
        const juce::var& e = arr->getReference(i);
        const juce::var src = e["src"];
        const std::string at = "mappings[" + std::to_string(i) + "]: ";
        UserMidiMapping m;
        std::string kind;
        if (!src.isObject() || !args::getString(src, "device", m.device)) return Reply::fail(LE_ERR_INVALID_ARG, at + "thiếu src.device");
        if (!args::getString(src, "kind", kind) || (kind != "note" && kind != "cc"))
            return Reply::fail(LE_ERR_INVALID_ARG, at + "src.kind phải là note | cc");
        if (!args::getInt(src, "channel", m.channel, -1, 15)) return Reply::fail(LE_ERR_INVALID_ARG, at + "src.channel -1..15");
        if (!args::getInt(src, "number", m.number, 0, 127)) return Reply::fail(LE_ERR_INVALID_ARG, at + "src.number 0..127");
        m.kind = kind == "cc" ? midi::LearnKind::CC : midi::LearnKind::Note;
        const std::string err = parseMidiTarget(e["target"], m.target);
        if (!err.empty()) return Reply::fail(LE_ERR_INVALID_ARG, at + err);
        m.targetJson = e["target"];
        next.push_back(std::move(m));
    }
    midiMappings_ = std::move(next);
    rebuildMidiMap();
    return Reply::ok();
}

// RT bắt được message khi đang learn → mapping mới (thay mapping cùng nguồn), báo Dart.
void Engine::handleMidiLearned(const RtMessage& m) {
    if (!learnPending_) return;   // learnCancel tới trước
    learnPending_ = false;
    LastLearn l;
    l.deviceId = m.a == 0 ? std::string(kVirtualMidiDevice) : midi_->deviceId(m.a);
    l.deviceName = m.a == 0 ? std::string(kVirtualMidiDevice) : midi_->deviceName(m.a);
    l.kind = m.b == 1 ? midi::LearnKind::CC : midi::LearnKind::Note;
    l.channel = (int) m.i0;
    l.number = (int) m.i1;
    lastLearn_ = l;
    UserMidiMapping mm;
    mm.device = l.deviceName;
    mm.kind = l.kind;
    mm.channel = l.channel;
    mm.number = l.number;
    mm.target = learnTarget_;
    mm.targetJson = learnTargetJson_;
    midiMappings_.erase(std::remove_if(midiMappings_.begin(), midiMappings_.end(),
                                       [&](const UserMidiMapping& x) {
                                           return x.device == mm.device && x.kind == mm.kind && x.channel == mm.channel &&
                                                  x.number == mm.number;
                                       }),
                        midiMappings_.end());
    midiMappings_.push_back(std::move(mm));
    rebuildMidiMap();
    emitEvent(LE_EVT_MIDI_LEARNED, (std::int32_t) l.kind, l.number, 0, 0.0);
}

// Mapping MIDI đã đổi state RT → model khớp (lưu project / export offline dùng model).
void Engine::handleMappedChange(const RtMessage& m) {
    const int t = m.b;
    switch (m.a) {
        case 0:
            if (t >= 0 && t < LE_MAX_TRACKS) model_.mixer[t].gainDb = (float) m.value;
            break;
        case 1:
            if (t >= 0 && t < LE_MAX_TRACKS) model_.mixer[t].mute = m.value > 0.5;
            break;
        case 2:
            if (t < 0) {
                if (m.i0 == 0 && m.i1 >= 0 && m.i1 < 3) model_.master.eq3[m.i1] = dsp::clampParam(dsp::FxType::EQ3, (int) m.i1, (float) m.value);
                else if (m.i0 == 1 && m.i1 == 0) model_.master.limiterCeilingDb = (float) m.value;
                else if (m.i0 == 1 && m.i1 == 1) model_.master.limiterReleaseMs = (float) m.value;
            } else if (t < LE_MAX_TRACKS && m.i0 >= 0 && m.i0 < kFxSlots && model_.tracks[t].fx[m.i0] && m.i1 >= 0 &&
                       m.i1 < kMaxFxParams) {
                FxModel& f = *model_.tracks[t].fx[m.i0];
                f.params[m.i1] = dsp::clampParam(f.type, (int) m.i1, (float) m.value);
            }
            break;
        default: break;
    }
}

// [main / test / sim] Message MIDI vào nguồn 0 (ảo) — cùng đường với thiết bị thật. hostNs = 0 → phát ngay block kế.
bool Engine::injectMidi(const std::uint8_t* bytes, int size, std::int64_t hostNs) {
    return midi::MidiInputRouter::enqueue(midiToRt_, hostNs, bytes, size, 0);
}

} // namespace le::core
