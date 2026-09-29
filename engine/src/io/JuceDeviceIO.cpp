#include "io/JuceDeviceIO.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include "le/engine_api.h"

namespace le::io {

namespace {

// Chuyển callback của JUCE sang AudioCallback (không phụ thuộc JUCE).
class CallbackAdapter final : public juce::AudioIODeviceCallback {
public:
    explicit CallbackAdapter(AudioCallback* cb) : cb_(cb) {}

    // [RT] Lưu ý: AudioDeviceManager giữ `audioCallbackLock` (CriticalSection) quanh lời gọi này.
    // Lock đó chỉ bị tranh chấp khi main đổi cấu hình device; ghi nhận trong engine/docs/audio-session.md.
    void audioDeviceIOCallbackWithContext(const float* const* in, int numIn, float* const* out, int numOut,
                                          int numFrames,
                                          const juce::AudioIODeviceCallbackContext& ctx) noexcept
        [[clang::nonblocking]] override {
        CallbackContext c;
        c.hostTimeNs = ctx.hostTimeNs != nullptr ? *ctx.hostTimeNs : 0;
        cb_->process(in, numIn, out, numOut, numFrames, c);
    }

    // [main] AudioDeviceManager gọi trong lúc giữ audioCallbackLock → audio callback không chạy song song.
    void audioDeviceAboutToStart(juce::AudioIODevice* d) override {
        const int buf = d->getCurrentBufferSizeSamples();
        cb_->prepare(d->getCurrentSampleRate(), buf > 4096 ? buf : 4096);   // iOS có thể gửi block lớn hơn buffer đặt
    }

    void audioDeviceStopped() override { cb_->released(); }

    // [thread bất kỳ] iOS gọi khi interruption/route change (từ thread của notification). Engine theo dõi
    // các sự kiện này bằng observer riêng (AudioSession.h), nên ở đây chỉ đếm.
    void audioDeviceError(const juce::String&) override { errors_.fetch_add(1, std::memory_order_relaxed); }

private:
    AudioCallback* cb_;
    std::atomic<std::uint32_t> errors_{0};
};

} // namespace

class JuceDeviceIOImpl final : private juce::ChangeListener {
public:
    JuceDeviceIOImpl() { dm_.addChangeListener(this); }
    ~JuceDeviceIOImpl() override {
        stop();
        dm_.removeChangeListener(this);
    }

    std::int32_t start(const DeviceConfig& cfg, AudioCallback* cb) {
        stop();
        cfg_ = cfg;

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.sampleRate = cfg.sampleRate;
        setup.bufferSize = cfg.bufferSize;
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        if (cfg.numInputs > 0) setup.inputChannels.setRange(0, cfg.numInputs, true);
        setup.useDefaultOutputChannels = false;
        setup.outputChannels.clear();
        setup.outputChannels.setRange(0, cfg.numOutputs, true);

        juce::String err;
        if (!initialised_) {
            err = dm_.initialise(cfg.numInputs, cfg.numOutputs, nullptr, true, {}, &setup);
            initialised_ = true;
        } else {
            // Lần start sau: giữ tên device đã chọn, chỉ đổi SR / buffer / kênh.
            auto prev = dm_.getAudioDeviceSetup();
            setup.inputDeviceName = cfg.numInputs > 0 ? prev.inputDeviceName : juce::String();
            setup.outputDeviceName = prev.outputDeviceName;
            err = dm_.setAudioDeviceSetup(setup, true);
        }
        if (err.isNotEmpty() || dm_.getCurrentAudioDevice() == nullptr) {
            lastError_ = err.isNotEmpty() ? err.toStdString() : std::string("no audio device");
            dm_.closeAudioDevice();
            return LE_ERR_AUDIO_DEVICE;
        }
        applySession();

        adapter_ = std::make_unique<CallbackAdapter>(cb);
        dm_.addAudioCallback(adapter_.get());   // → audioDeviceAboutToStart → prepare
        running_ = true;
        lastError_.clear();
        return LE_OK;
    }

    void stop() {
        if (adapter_ != nullptr) {
            dm_.removeAudioCallback(adapter_.get());   // chờ callback đang chạy xong (JUCE giữ lock)
            adapter_.reset();
        }
        dm_.closeAudioDevice();
        running_ = false;
    }

    std::int32_t restart(const DeviceConfig& cfg) {
        if (!running_) {
            cfg_ = cfg;
            return LE_OK;
        }
        auto setup = dm_.getAudioDeviceSetup();
        setup.bufferSize = cfg.bufferSize;
        setup.sampleRate = cfg.sampleRate;
        const juce::String err = dm_.setAudioDeviceSetup(setup, true);   // đóng rồi mở lại device
        if (err.isNotEmpty()) {
            lastError_ = err.toStdString();
            return LE_ERR_AUDIO_DEVICE;
        }
        cfg_ = cfg;
        applySession();
        return LE_OK;
    }

    bool setSessionMode(session::Mode mode) {
        mode_ = mode;
        return running_ ? applySession() : true;
    }

    bool isRunning() const { return running_; }
    juce::AudioIODevice* device() const { return dm_.getCurrentAudioDevice(); }
    const DeviceConfig& config() const { return cfg_; }
    session::Mode mode() const { return mode_; }
    std::uint32_t changes() const { return changes_; }
    std::string lastError() const { return lastError_; }

private:
    // JUCE đặt category kèm HFP trong mỗi lần open() → đặt lại theo ý mình (engine/docs/audio-session.md).
    bool applySession() {
        if (!session::isSupported()) return true;
        const bool ok = session::applyCategory(mode_, cfg_.numInputs > 0);
        if (!ok) lastError_ = "setCategory: " + session::lastError();
        return ok;
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override { ++changes_; }   // [main]

    juce::AudioDeviceManager dm_;
    std::unique_ptr<CallbackAdapter> adapter_;
    DeviceConfig cfg_{};
    session::Mode mode_ = session::Mode::Default;
    bool running_ = false;
    bool initialised_ = false;
    std::uint32_t changes_ = 0;
    std::string lastError_;
};

JuceDeviceIO::JuceDeviceIO() : impl_(std::make_unique<JuceDeviceIOImpl>()) {}
JuceDeviceIO::~JuceDeviceIO() = default;

std::int32_t JuceDeviceIO::start(const DeviceConfig& cfg, AudioCallback* cb) { return impl_->start(cfg, cb); }
void JuceDeviceIO::stop() { impl_->stop(); }
std::int32_t JuceDeviceIO::restart(const DeviceConfig& cfg) { return impl_->restart(cfg); }
bool JuceDeviceIO::isRunning() const { return impl_->isRunning(); }

double JuceDeviceIO::sampleRate() const {
    auto* d = impl_->device();
    return d != nullptr ? d->getCurrentSampleRate() : impl_->config().sampleRate;
}

int JuceDeviceIO::bufferSize() const {
    auto* d = impl_->device();
    return d != nullptr ? d->getCurrentBufferSizeSamples() : impl_->config().bufferSize;
}

int JuceDeviceIO::numInputs() const {
    auto* d = impl_->device();
    return d != nullptr ? d->getActiveInputChannels().countNumberOfSetBits() : 0;
}

DeviceLatencies JuceDeviceIO::latencies() const {
    auto* d = impl_->device();
    if (d == nullptr) return {};
    return {d->getInputLatencyInSamples(), d->getOutputLatencyInSamples()};
}

int JuceDeviceIO::xrunCount() const {
    auto* d = impl_->device();
    return d != nullptr ? d->getXRunCount() : -1;
}

std::string JuceDeviceIO::deviceName() const {
    auto* d = impl_->device();
    return d != nullptr ? (d->getTypeName() + ": " + d->getName()).toStdString() : std::string();
}

std::string JuceDeviceIO::lastError() const { return impl_->lastError(); }
bool JuceDeviceIO::setSessionMode(session::Mode mode) { return impl_->setSessionMode(mode); }
session::Mode JuceDeviceIO::sessionMode() const { return impl_->mode(); }
std::uint32_t JuceDeviceIO::deviceChangeCount() const { return impl_->changes(); }

} // namespace le::io
