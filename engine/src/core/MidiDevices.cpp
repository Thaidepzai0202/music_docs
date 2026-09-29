#include "core/MidiDevices.h"

#include <algorithm>
#include <cmath>

#include "midi/MidiInputRouter.h"

namespace le::core {

namespace {
bool launchpadModelFor(const juce::String& name, midi::LaunchpadModel& out) {
    if (name.containsIgnoreCase("Launchpad X")) out = midi::LaunchpadModel::X;
    else if (name.containsIgnoreCase("Launchpad Mini")) out = midi::LaunchpadModel::MiniMk3;
    else return false;
    return true;
}
constexpr std::uint8_t kTrackColors[8] = {midi::lpcolor::kRed,  midi::lpcolor::kOrange, midi::lpcolor::kYellow, midi::lpcolor::kGreen,
                                          midi::lpcolor::kCyan, midi::lpcolor::kBlue,   midi::lpcolor::kPurple, midi::lpcolor::kWhite};
} // namespace

// Một thiết bị đang mở. Callback chạy trên thread CoreMIDI, chỉ ghi vào queue của slot này (một producer).
struct MidiDevices::Slot final : juce::MidiInputCallback {
    MidiDevices* owner = nullptr;
    int index = 0;                       // slot 0..kSlots−1 → source = index + 1
    juce::MidiDeviceInfo info;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;   // Launchpad: LED + SysEx
    bool launchpad = false;
    midi::LaunchpadModel model = midi::LaunchpadModel::X;
    midi::LaunchpadLedState leds;
    HostClock clock;
    std::atomic<std::thread::id> producer{};   // R10: bản debug assert đúng MỘT thread gọi callback

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m) override {   // [thread CoreMIDI]
#if JUCE_DEBUG
        std::thread::id expected{};
        const auto me = std::this_thread::get_id();
        if (!producer.compare_exchange_strong(expected, me)) jassert(expected == me);   // 2 thread cùng push = hỏng SPSC
#endif
        // Timestamp của JUCE (giây, theo Time::getMillisecondCounterHiRes) → mach host time (ns) như block audio.
        const double ageSec = std::max(0.0, juce::Time::getMillisecondCounterHiRes() * 0.001 - m.getTimeStamp());
        const auto hostNs = (std::int64_t) clock.nowNs() - (std::int64_t) (ageSec * 1e9);
        owner->enqueue(index, hostNs, m.getRawData(), m.getRawDataSize());
    }
};

MidiDevices::MidiDevices() {
    for (int i = 0; i < kSlots; ++i) {
        queueStore_[i] = std::make_unique<MidiQueue>(kMidiToRtCapacity);
        queuePtrs_[i] = queueStore_[i].get();
    }
    listConn_ = juce::MidiDeviceListConnection::make([this] {   // [main] cắm / rút thiết bị
        reconcile();
        if (onDeviceListChanged) onDeviceListChanged();
    });
}

MidiDevices::~MidiDevices() {
    for (int i = 0; i < kSlots; ++i) close(i);
}

bool MidiDevices::enqueue(int slot, std::int64_t hostNs, const std::uint8_t* bytes, int size) noexcept {
    if (slot < 0 || slot >= kSlots) return false;
    const bool ok = midi::MidiInputRouter::enqueue(*queuePtrs_[slot], hostNs, bytes, size, (std::uint16_t) (slot + 1));
    if (!ok && midi::MidiInputRouter::accepts(bytes, size)) dropped_.fetch_add(1, std::memory_order_relaxed);
    return ok;
}

void MidiDevices::open(int i, const juce::MidiDeviceInfo& info) {
    auto s = std::make_unique<Slot>();
    s->owner = this;
    s->index = i;
    s->info = info;
    s->clock.init();
    s->input = juce::MidiInput::openDevice(info.identifier, s.get());
    if (s->input == nullptr) return;
    s->launchpad = launchpadModelFor(info.name, s->model);
    if (s->launchpad) {   // P4-05: cổng ra cùng tên → Programmer mode + LED
        for (const auto& o : juce::MidiOutput::getAvailableDevices())
            if (o.name == info.name) {
                s->output = juce::MidiOutput::openDevice(o.identifier);
                break;
            }
        if (s->output != nullptr) {
            const midi::SysExMessage sx = midi::programmerModeSysEx(s->model, true);
            s->output->sendMessageNow(juce::MidiMessage(sx.bytes.data(), sx.size));
            s->leds.invalidate();
        }
    }
    s->input->start();
    slots_[i] = std::move(s);
}

void MidiDevices::close(int i) {
    auto& s = slots_[i];
    if (s == nullptr) return;
    if (s->input != nullptr) s->input->stop();   // sau stop + huỷ: CoreMIDI không gọi callback nữa
    if (s->output != nullptr) {
        const midi::SysExMessage sx = midi::programmerModeSysEx(s->model, false);   // trả Launchpad về Live mode
        s->output->sendMessageNow(juce::MidiMessage(sx.bytes.data(), sx.size));
    }
    s.reset();
}

void MidiDevices::reconcile() {
    const auto inputs = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < kSlots; ++i) {   // đóng: rút ra, hoặc người dùng tắt
        if (slots_[i] == nullptr) continue;
        const auto& id = slots_[i]->info.identifier;
        const bool present = std::any_of(inputs.begin(), inputs.end(), [&](const auto& d) { return d.identifier == id; });
        if (!present || enabled_.count(id.toStdString()) == 0) close(i);
    }
    for (const auto& d : inputs) {   // mở: đã bật mà chưa mở (cắm lại → tự mở lại)
        const std::string id = d.identifier.toStdString();
        if (enabled_.count(id) == 0) continue;
        bool open_ = false;
        for (const auto& s : slots_) open_ |= s != nullptr && s->info.identifier == d.identifier;
        if (open_) continue;
        for (int i = 0; i < kSlots; ++i)
            if (slots_[i] == nullptr) {
                open(i, d);
                break;
            }
    }
}

bool MidiDevices::enableDevice(const std::string& id, bool on) {
    if (on) enabled_.insert(id);
    else enabled_.erase(id);
    const auto inputs = juce::MidiInput::getAvailableDevices();
    const bool known = std::any_of(inputs.begin(), inputs.end(), [&](const auto& d) { return d.identifier.toStdString() == id; });
    reconcile();
    return known;
}

juce::var MidiDevices::listDevices() const {
    juce::Array<juce::var> ins, outs;
    for (const auto& d : juce::MidiInput::getAvailableDevices()) {
        auto* o = new juce::DynamicObject();
        o->setProperty("id", d.identifier);
        o->setProperty("name", d.name);
        o->setProperty("enabled", enabled_.count(d.identifier.toStdString()) != 0);
        bool isOpen = false;
        for (const auto& s : slots_) isOpen |= s != nullptr && s->info.identifier == d.identifier;
        o->setProperty("open", isOpen);
        ins.add(juce::var(o));
    }
    for (const auto& d : juce::MidiOutput::getAvailableDevices()) {
        auto* o = new juce::DynamicObject();
        o->setProperty("id", d.identifier);
        o->setProperty("name", d.name);
        outs.add(juce::var(o));
    }
    auto* r = new juce::DynamicObject();
    r->setProperty("inputs", ins);
    r->setProperty("outputs", outs);
    return juce::var(r);
}

std::string MidiDevices::deviceName(int source) const {
    const int i = source - 1;
    return (i >= 0 && i < kSlots && slots_[i] != nullptr) ? slots_[i]->info.name.toStdString() : std::string();
}

std::string MidiDevices::deviceId(int source) const {
    const int i = source - 1;
    return (i >= 0 && i < kSlots && slots_[i] != nullptr) ? slots_[i]->info.identifier.toStdString() : std::string();
}

int MidiDevices::sourceForName(const std::string& name) const {
    for (int i = 0; i < kSlots; ++i)
        if (slots_[i] != nullptr && slots_[i]->info.name.toStdString() == name) return i + 1;
    return -1;
}

bool MidiDevices::isLaunchpad(int source) const {
    const int i = source - 1;
    return i >= 0 && i < kSlots && slots_[i] != nullptr && slots_[i]->launchpad;
}

void MidiDevices::addLaunchpadDefaults(midi::MidiLearnMap& map) const {
    for (int i = 0; i < kSlots; ++i) {
        if (slots_[i] == nullptr || !slots_[i]->launchpad) continue;
        const auto src = (std::uint16_t) (i + 1);
        auto add = [&](midi::LearnKind kind, int number, const midi::LaunchpadInput& in) {
            const midi::LearnTarget t = midi::defaultLaunchpadAction(in);
            if (t.action == midi::LearnAction::None) return;
            midi::LearnKey k;
            k.source = src;
            k.kind = kind;
            k.channel = 0;
            k.number = (std::uint8_t) number;
            (void) map.set(k, t);
        };
        for (int t = 0; t < 8; ++t)
            for (int s = 0; s < 8; ++s) {
                midi::LaunchpadInput in;
                in.kind = midi::LaunchpadInput::Kind::Pad;
                in.track = (std::int8_t) t;
                in.scene = (std::int8_t) s;
                in.pressed = true;
                in.velocity = 127;
                add(midi::LearnKind::Note, midi::padNote(t, s), in);
            }
        for (int s = 0; s < 8; ++s) {
            midi::LaunchpadInput in;
            in.kind = midi::LaunchpadInput::Kind::Scene;
            in.scene = (std::int8_t) s;
            in.pressed = true;
            add(midi::LearnKind::CC, midi::sceneButtonCc(s), in);
        }
        for (int k = 0; k < 8; ++k) {
            midi::LaunchpadInput in;
            in.kind = midi::LaunchpadInput::Kind::Top;
            in.index = (std::int8_t) k;
            in.pressed = true;
            add(midi::LearnKind::CC, midi::topButtonCc(k), in);
        }
    }
}

std::uint8_t MidiDevices::defaultTrackColor(int t) noexcept { return kTrackColors[t & 7]; }

void MidiDevices::updateLaunchpadLeds(const LeState& st, const std::uint8_t (&trackColors)[8]) {
    std::uint8_t cs[8][8];
    for (int t = 0; t < 8; ++t)
        for (int s = 0; s < 8; ++s) cs[t][s] = st.clipState[t][s];
    std::uint8_t colors[8];
    std::copy(std::begin(trackColors), std::end(trackColors), colors);
    std::array<midi::LedMessage, midi::LaunchpadLedState::kMaxMessages> out{};
    for (auto& s : slots_) {
        if (s == nullptr || s->output == nullptr) continue;
        const int n = s->leds.update(cs, colors, out);
        for (int i = 0; i < n; ++i) s->output->sendMessageNow(juce::MidiMessage(out[(size_t) i].status, out[(size_t) i].data1, out[(size_t) i].data2));
    }
}

} // namespace le::core
