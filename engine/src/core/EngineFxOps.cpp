// FX (P3-12) + master cố định (P3-15): op fx.set / fx.remove và phần main của LE_CMD_FX_PARAM / LE_CMD_FX_BYPASS.
// [main] Mọi hàm ở đây chạy trên main thread.
#include <algorithm>
#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"

namespace le::core {

namespace {

// 05 §3: "comp" là tên chuẩn, "compressor" là bí danh.
bool parseFxType(const std::string& name, dsp::FxType& out) {
    if (dsp::fxTypeFromName(name, out)) return true;
    if (name == "comp" || name == "compressor") {
        out = dsp::FxType::Compressor;
        return true;
    }
    return false;
}

bool hasParam(dsp::FxType type, int id) {
    for (const dsp::ParamInfo& p : dsp::paramInfo(type))
        if (p.id == id) return true;
    return false;
}

LeCommand fxCommand(std::uint16_t type, int track, int slot, int i0, float f0, std::uint32_t instanceId) {
    LeCommand c{};
    c.type = type;
    c.track = (std::int8_t) track;
    c.slot = (std::int8_t) slot;
    c.i0 = i0;
    c.f0 = f0;
    c.d0 = (double) instanceId;
    return c;
}

} // namespace

// Tất cả hoặc không: main là producer duy nhất nên chỗ trống chỉ có thể tăng giữa lúc kiểm và lúc push.
bool Engine::pushCommands(const LeCommand* cmds, int n) {
    if (commands_.capacity() - commands_.size() < (std::size_t) n) {
        ++queueFull_;
        return false;
    }
    for (int i = 0; i < n; ++i) (void) commands_.try_push(cmds[i]);
    return true;
}

// Kiểm theo model rồi ghi instanceId vào d0: RT chỉ áp lệnh cho đúng instance đó (FxChain.h).
bool Engine::prepareFxCommand(LeCommand& c) const {
    if (c.track < 0) {   // master: dải đã kiểm ở isValidCommand, RT tự kẹp giá trị
        c.d0 = 0.0;
        return true;
    }
    const auto& fx = model_.tracks[c.track].fx[c.slot];
    if (!fx) return false;   // slot trống
    if (c.type == LE_CMD_FX_PARAM && !hasParam(fx->type, c.i0)) return false;
    c.d0 = (double) fx->instanceId;
    return true;
}

// Sau khi push thành công: model là nguồn sự thật của tham số (không đọc lại processor).
void Engine::commitFxCommand(const LeCommand& c) {
    if (c.track < 0) {
        MasterModel& m = model_.master;
        if (c.type == LE_CMD_FX_BYPASS) m.eqBypass = c.i0 != 0;
        else if (c.slot == 0) m.eq3[c.i0] = dsp::clampParam(dsp::FxType::EQ3, c.i0, c.f0);
        else if (c.i0 == 0) m.limiterCeilingDb = std::clamp(c.f0, Limiter::kMinCeilingDb, Limiter::kMaxCeilingDb);
        else m.limiterReleaseMs = std::clamp(c.f0, Limiter::kMinReleaseMs, Limiter::kMaxReleaseMs);
        return;
    }
    FxModel& f = *model_.tracks[c.track].fx[c.slot];
    if (c.type == LE_CMD_FX_BYPASS) f.bypass = c.i0 != 0;
    else f.params[c.i0] = dsp::clampParam(f.type, c.i0, c.f0);
}

// project.open / close (chốt 05 §3): mọi state RT thuộc PROJECT về mặc định — transport (dừng, 120 BPM, quantize
// 1 bar, metronome tắt, count-in 0), mixer (gain 0 dB, pan giữa, không mute/solo/arm, monitor off), master (gain
// 0 dB, EQ3 phẳng, limiter −0.3 dBFS / 50 ms). Dart chỉ gửi những gì khác mặc định (VD EQ master khi có band ≠ 0)
// → không được để project trước lọt sang project sau. Thiết lập TOÀN CỤC (record quantize, buffer, input…) giữ nguyên.
// 4/4 và track/clip/FX/nhạc cụ đi theo model (model_.reset() + snapshot).
void Engine::resetProjectRtState() {
    model_.master = MasterModel{};
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {   // lượt overdub RT còn giữ vé vẫn được giữ tới khi RT trả vé
        overdubOn_[t] = false;
        overdubDeferred_[t] = false;
        for (auto& w : warp_[t]) {
            if (w.id != 0) jobs_->cancel(w.id);
            w = WarpJob{};
        }
    }
    bpm_ = 120.0;
    warpDueMs_ = -1.0;
    tempoFound_ = false;       // pedal mode: đọc lại từ model (Dart gửi transport.setTempoMode khi mở project)
    firstLoopBeats_ = 0.0;
    midiMappings_.clear();   // mapping MIDI là state project (06 §2); thiết bị đã bật là thiết lập toàn cục
    learnPending_ = false;
    rt_.armMidiLearn(false);
    rebuildMidiMap();
    const MasterModel& m = model_.master;
    std::vector<LeCommand> cmds;
    auto add = [&](std::uint16_t type, int track, int slot, int i0, float f0 = 0.0f, double d0 = 0.0) {
        LeCommand c = fxCommand(type, track, slot, i0, f0, 0);
        c.d0 = d0;
        cmds.push_back(c);
    };
    add(LE_CMD_TRANSPORT_STOP, -1, -1, 0);
    add(LE_CMD_SET_BPM, -1, -1, 0, 0.0f, 120.0);
    add(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_1_BAR);
    add(LE_CMD_METRONOME, -1, -1, 0, Metronome::kDefaultVolume);
    add(LE_CMD_SET_COUNT_IN, -1, -1, 0);
    add(LE_CMD_ALL_NOTES_OFF, -1, -1, 0);
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        add(LE_CMD_TRACK_GAIN, t, -1, 0, 0.0f);
        add(LE_CMD_TRACK_PAN, t, -1, 0, 0.0f);
        add(LE_CMD_TRACK_MUTE, t, -1, 0);
        add(LE_CMD_TRACK_SOLO, t, -1, 0);
        add(LE_CMD_TRACK_ARM, t, -1, 0);
        add(LE_CMD_TRACK_MONITOR, t, -1, 0);
    }
    add(LE_CMD_MASTER_GAIN, -1, -1, 0, 0.0f);
    for (int band = 0; band < 3; ++band) add(LE_CMD_FX_PARAM, -1, 0, band, m.eq3[band]);
    add(LE_CMD_FX_BYPASS, -1, 0, m.eqBypass ? 1 : 0);
    add(LE_CMD_FX_PARAM, -1, 1, 0, m.limiterCeilingDb);
    add(LE_CMD_FX_PARAM, -1, 1, 1, m.limiterReleaseMs);
    (void) pushCommands(cmds.data(), (int) cmds.size());   // ~60 lệnh / 1024 chỗ; RT xả 64 lệnh mỗi block
}

std::weak_ptr<dsp::Processor> Engine::fxProcessor(int track, int slot) const {
    if (track < 0 || track >= LE_MAX_TRACKS || slot < 0 || slot >= kFxSlots || !model_.tracks[track].fx[slot]) return {};
    return model_.tracks[track].fx[slot]->proc;
}

// 05 §3: {track 0..7, index 0..2, type: "filter"|"delay"|"reverb"|"eq3"|"comp", params: {"id": value}, bypass}.
// Tham số không có trong params = mặc định của loại đó.
// - Cùng loại với FX đang ở slot → GIỮ instance (đuôi reverb/delay không bị cắt); tham số + bypass đi đường lệnh RT.
// - Slot trống / khác loại → Processor mới: tạo → prepare → setParam → reset (nhảy tới đích, không nghe "quét" từ
//   mặc định) ở main, rồi snapshot mới. RT crossfade 20 ms; instance cũ được huỷ trên main khi snapshot cũ thu hồi.
Reply Engine::opFxSet(const juce::var& req) {
    int track = 0, index = 0;
    if (!args::getInt(req, "track", track, 0, LE_MAX_TRACKS - 1))
        return Reply::fail(LE_ERR_INVALID_ARG, "track phải là 0..7 (master chỉnh bằng LE_CMD_FX_PARAM track -1)");
    if (!args::getInt(req, "index", index, 0, kFxSlots - 1)) return Reply::fail(LE_ERR_INVALID_ARG, "index phải là 0..2");
    std::string typeName;
    dsp::FxType type{};
    if (!args::getString(req, "type", typeName) || !parseFxType(typeName, type))
        return Reply::fail(LE_ERR_INVALID_ARG, "type phải là \"filter\", \"delay\", \"reverb\", \"eq3\" hoặc \"comp\"");
    bool bypass = false;
    if (req.hasProperty("bypass") && !args::getBool(req, "bypass", bypass))
        return Reply::fail(LE_ERR_INVALID_ARG, "bypass phải là true/false");

    float values[kMaxFxParams] = {};
    for (const dsp::ParamInfo& p : dsp::paramInfo(type)) values[p.id] = p.defaultValue;
    if (req.hasProperty("params")) {
        const juce::var& ps = req["params"];
        const juce::DynamicObject* obj = ps.getDynamicObject();
        if (obj == nullptr || ps.isArray()) return Reply::fail(LE_ERR_INVALID_ARG, "params phải là object {\"paramId\": value}");
        for (const auto& kv : obj->getProperties()) {
            const juce::String key = kv.name.toString();
            const int id = key.getIntValue();
            if (key.isEmpty() || !key.containsOnly("0123456789") || !hasParam(type, id))
                return Reply::fail(LE_ERR_INVALID_ARG, "params: FX " + typeName + " không có tham số \"" + key.toStdString() + "\"");
            if (!args::isNumber(kv.value) || !std::isfinite((double) kv.value))
                return Reply::fail(LE_ERR_INVALID_ARG, "params." + key.toStdString() + " phải là số");
            values[id] = dsp::clampParam(type, id, (float) (double) kv.value);
        }
    }

    auto& slot = model_.tracks[track].fx[index];
    if (slot && slot->type == type && slot->proc != nullptr) {
        LeCommand cmds[kMaxFxParams + 1];
        int n = 0;
        for (const dsp::ParamInfo& p : dsp::paramInfo(type))
            cmds[n++] = fxCommand(LE_CMD_FX_PARAM, track, index, p.id, values[p.id], slot->instanceId);
        cmds[n++] = fxCommand(LE_CMD_FX_BYPASS, track, index, bypass ? 1 : 0, 0.0f, slot->instanceId);
        if (pushCommands(cmds, n)) {
            std::copy(std::begin(values), std::end(values), std::begin(slot->params));
            slot->bypass = bypass;
            return Reply::ok();
        }
        // queue RT đầy → tạo instance mới (luôn đúng, chỉ mất đuôi)
    }

    std::shared_ptr<dsp::Processor> proc = dsp::createProcessor(type);
    if (proc == nullptr) return Reply::fail(LE_ERR_INTERNAL, "không tạo được FX " + typeName);
    proc->prepare(rt_.sampleRate(), FxChain::kMaxFxBlock);   // RtEngine::prepare sẽ prepare lại nếu SR đổi
    for (const dsp::ParamInfo& p : dsp::paramInfo(type)) proc->setParam(p.id, values[p.id]);
    proc->reset();
    FxModel f;
    f.type = type;
    std::copy(std::begin(values), std::end(values), std::begin(f.params));
    f.bypass = bypass;
    f.instanceId = ++fxInstanceCounter_;
    f.proc = std::move(proc);
    slot = std::move(f);   // instance cũ (nếu có) vẫn sống trong snapshot RT tới khi fade xong
    publishSnapshot();
    return Reply::ok();
}

// 05 §3: {track 0..7, index 0..2}. Slot cố định (không dồn), xoá slot trống vẫn ok.
Reply Engine::opFxRemove(const juce::var& req) {
    int track = 0, index = 0;
    if (!args::getInt(req, "track", track, 0, LE_MAX_TRACKS - 1))
        return Reply::fail(LE_ERR_INVALID_ARG, "track phải là 0..7 (FX master cố định, không xoá được)");
    if (!args::getInt(req, "index", index, 0, kFxSlots - 1)) return Reply::fail(LE_ERR_INVALID_ARG, "index phải là 0..2");
    auto& slot = model_.tracks[track].fx[index];
    if (!slot) return Reply::ok();
    slot.reset();
    publishSnapshot();   // RT fade instance → dry 20 ms rồi trả snapshot cũ về main
    return Reply::ok();
}

} // namespace le::core
