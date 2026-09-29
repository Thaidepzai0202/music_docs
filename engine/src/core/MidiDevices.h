#pragma once
// MidiDevices (P4-01/02/04/05) [main] trừ callback thiết bị: mở juce::MidiInput cho thiết bị được bật, gom message về
// RtEngine qua MỘT queue SPSC riêng cho mỗi thiết bị (R10: mỗi queue đúng một producer), MIDI learn + mapping
// (MidiLearnMap của 80 trong snapshot), Launchpad (SysEx Programmer mode + LED 30 Hz, preset mặc định).
//
// Nguồn (LearnKey::source): 0 = ảo (sim.midiIn / test), 1..kMaxMidiSources = slot thiết bị đang mở.
#include <atomic>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <juce_audio_devices/juce_audio_devices.h>

#include "core/HostTime.h"
#include "core/RtEngine.h"
#include "core/RtQueues.h"
#include "midi/LaunchpadMap.h"
#include "midi/MidiLearnMap.h"

namespace le::core {

struct UserMidiMapping {        // 05 §3 midi.setMappings / 06 §2 "midiMappings"
    std::string device;         // "" = mọi thiết bị
    midi::LearnKind kind = midi::LearnKind::Note;
    int channel = -1;           // −1 = mọi kênh
    int number = 0;
    midi::LearnTarget target;
    juce::var targetJson;       // giữ nguyên để trả lại / lưu
};

class MidiDevices {
public:
    static constexpr int kSlots = RtEngine::kMaxMidiSources;

    MidiDevices();
    ~MidiDevices();   // đóng mọi thiết bị (sau đó không còn callback nào ghi vào queue)

    MidiQueue* const* queues() noexcept { return queuePtrs_; }   // [main, trước audio start] cho RtEngine
    MidiQueue& queue(int slot) noexcept { return *queuePtrs_[slot]; }

    // [main]
    juce::var listDevices() const;                           // {inputs:[{id,name,enabled,open}], outputs:[{id,name}]}
    bool enableDevice(const std::string& id, bool on);       // false = id không có trong danh sách hiện tại (vẫn nhớ)
    void reconcile();                                        // mở / đóng theo danh sách + tập đã bật
    std::string deviceName(int source) const;                // "" cho nguồn 0 / slot trống
    std::string deviceId(int source) const;
    int sourceForName(const std::string& name) const;        // −1 nếu thiết bị chưa mở
    bool isLaunchpad(int source) const;
    std::uint32_t droppedMessages() const noexcept { return dropped_.load(std::memory_order_relaxed); }

    // Launchpad: mapping mặc định (pad → clip, cột phải → scene, top[0] play/stop, top[1] stop all) cho nguồn này.
    void addLaunchpadDefaults(midi::MidiLearnMap& map) const;
    static std::uint8_t defaultTrackColor(int track) noexcept;   // track chưa có color
    // [main, Timer 30 Hz] LED theo LeState.clipState.
    void updateLaunchpadLeds(const LeState& s, const std::uint8_t (&trackColors)[8]);

    // Callback test: thay cho CoreMIDI (tests / sim) — cùng đường enqueue.
    bool enqueue(int slot, std::int64_t hostNs, const std::uint8_t* bytes, int size) noexcept;

    std::function<void()> onDeviceListChanged;               // [main] Engine phát LE_EVT_MIDI_DEVICES

private:
    struct Slot;
    void open(int slot, const juce::MidiDeviceInfo& info);
    void close(int slot);

    std::unique_ptr<MidiQueue> queueStore_[kSlots];
    MidiQueue* queuePtrs_[kSlots] = {};
    std::unique_ptr<Slot> slots_[kSlots];
    std::set<std::string> enabled_;                          // id thiết bị người dùng bật (thiết lập toàn cục)
    juce::MidiDeviceListConnection listConn_;
    std::atomic<std::uint32_t> dropped_{0};                  // queue đầy
};

} // namespace le::core
