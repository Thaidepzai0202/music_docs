// P4-17: bộ nhớ thấp. Hai đường vào: iOS UIApplicationDidReceiveMemoryWarningNotification (engine tự nghe, pump) và
// op memory.pressure {level} (app / test). Nhả thứ engine tạo lại được, rồi phát LE_EVT_MEMORY_WARNING(value = MB).
// [main]
#include <juce_core/juce_core.h>

#include "core/Engine.h"

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace le::core {

namespace {
std::int64_t bytesOf(const dsp::AudioData* d) {
    return d != nullptr ? d->numFrames() * d->numChannels() * (std::int64_t) sizeof(float) : 0;
}
} // namespace

// phys_footprint = con số iOS dùng để quyết định jetsam kill (đúng hơn resident size).
double Engine::memoryFootprintMB() {
#if defined(__APPLE__)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t) &info, &count) == KERN_SUCCESS)
        return (double) info.phys_footprint / (1024.0 * 1024.0);
#endif
    return 0.0;
}

// Nhả (theo thứ tự rẻ → đắt):
//  1) bản stretched của clip KHÔNG đang phát (cache trên đĩa còn → BPM đổi lần sau render lại nhanh);
//  2) nhạc cụ tự thu không track nào dùng (gán lại → tự tạo lại từ cache zone);
//  3) peaks của clip không còn trong model;
//  4) critical: thêm lớp undo overdub của ô không đang phát.
// Clip đang phát, take đang thu, nhạc cụ đang dùng: KHÔNG đụng.
std::int64_t Engine::releaseMemory(bool critical) {
    LeState st{};
    globalStatePublisher().read(st);
    std::int64_t freed = 0;
    bool snapshot = false;
    for (int t = 0; t < LE_MAX_TRACKS; ++t)
        for (int s = 0; s < LE_MAX_SCENES; ++s) {
            auto& c = model_.clips[t][s];
            const bool playing = st.trackPlayingSlot[t] == s;
            if (c && c->stretched != nullptr && !playing) {
                freed += bytesOf(c->stretched.get());
                c->stretched = nullptr;
                c->stretchedSource.reset();
                c->stretchedBpm = 0.0;
                snapshot = true;
            }
            if (critical && undoLayer_[t][s] != nullptr && !playing) {
                freed += bytesOf(undoLayer_[t][s].get());
                undoLayer_[t][s] = nullptr;
            }
        }
    for (auto& [id, u] : model_.userInstruments) {
        bool used = false;
        for (const auto& tr : model_.tracks) used |= tr.userInstrumentId == id;
        if (used || u.rendered == nullptr || u.source.file.empty()) continue;   // không có nguồn → không tạo lại được
        for (const auto& smp : u.rendered->samples) freed += bytesOf(smp.get());
        u.rendered = nullptr;
        u.current = nullptr;
    }
    for (auto it = peaks_.begin(); it != peaks_.end();) {
        bool inModel = false;
        for (const auto& row : model_.clips)
            for (const auto& c : row) inModel |= c && c->clipId == it->first;
        it = inModel ? std::next(it) : peaks_.erase(it);
    }
    if (snapshot) publishSnapshot();   // snapshot cũ (còn giữ bản stretched) được thu hồi ở block sau
    emitEvent(LE_EVT_MEMORY_WARNING, critical ? 1 : 0, 0, 0, memoryFootprintMB());
    return freed;
}

// {level:"warning"|"critical"} → {freedMB, usedMB}
Reply Engine::opMemoryPressure(const juce::var& req) {
    std::string level = "warning";
    if (req.hasProperty("level") && (!args::getString(req, "level", level) || (level != "warning" && level != "critical")))
        return Reply::fail(LE_ERR_INVALID_ARG, "level phải là \"warning\" hoặc \"critical\"");
    const std::int64_t freed = releaseMemory(level == "critical");
    auto* r = new juce::DynamicObject();
    r->setProperty("freedMB", (double) freed / (1024.0 * 1024.0));
    r->setProperty("usedMB", memoryFootprintMB());
    return Reply::ok(juce::var(r));
}

} // namespace le::core
