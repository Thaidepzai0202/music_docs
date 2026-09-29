#pragma once
// RtEngine: xử lý audio 1 block (04 §1). Không phụ thuộc thiết bị: DeviceIO gọi vào qua AudioCallback,
// test và render offline gọi thẳng process().
#include <atomic>
#include <cstdint>
#include <memory>

#include "core/BlockSplitter.h"
#include "core/ClipScheduler.h"
#include "core/CpuMeter.h"
#include "core/FxChain.h"
#include "core/GraphSnapshot.h"
#include "core/HostTime.h"
#include "core/RtQueues.h"
#include "core/Metronome.h"
#include "core/Mixer.h"
#include "core/Recorder.h"
#include "core/Track.h"
#include "core/StatePublisher.h"
#include "core/Transport.h"
#include "core/XrunDetector.h"
#include "io/DeviceIO.h"
#include "le/engine_api.h"
#include "midi/MidiInputRouter.h"
#include "midi/MidiLearnMap.h"
#include "spike/SpikeProcessor.h"
#include "spike/measure/LatencyProbe.h"
#include "spike/measure/LoadGenerator.h"

namespace le::core {

using StatePublisher = SeqLockPublisher<LeState>;

class RtEngine final : public io::AudioCallback {
public:
    static constexpr int kMaxCommandsPerBlock = 64;

    static constexpr int kMaxRetiring = 4;
    static constexpr int kMaxGenerationUsers = 32;

    // Các queue và publisher do Engine sở hữu, sống lâu hơn RtEngine.
    RtEngine(CommandQueue& commands, RtToNrtQueue& toNrt, MidiQueue& midiIn, StatePublisher& publisher);
    ~RtEngine() override;   // [main, audio đã dừng] xoá snapshot RT còn giữ

    // ── Snapshot (P1-06) ──
    // [main, audio chưa chạy] RtEngine nhận quyền sở hữu snapshot đầu tiên.
    void setInitialSnapshot(std::unique_ptr<GraphSnapshot> s) noexcept;
    // [main] Kênh publish snapshot mới.
    SnapshotExchange& snapshots() noexcept { return exchange_; }
    // [main, audio chưa chạy] Đăng ký thứ đọc dữ liệu snapshot qua nhiều block (Sampler, clip player, test).
    void addGenerationUser(GenerationUser* u) noexcept;
    // [test / audio không chạy] Snapshot RT đang dùng và số bản đang chờ thu hồi.
    const GraphSnapshot* currentSnapshot() const noexcept { return current_; }
    int retiringCount() const noexcept { return retiringCount_; }

    // [main] Buffer thu cho spike (Engine sở hữu). Gọi trước prepare.
    void setRecordBuffer(float* data, int capacityFrames) noexcept;

    // [main] Khi audio chưa chạy. Cấp phát mọi thứ process() cần.
    void prepare(double sampleRate, int maxBlockSize) override;

    // [RT]
    void process(const float* const* in, int numIn, float* const* out, int numOut, int numFrames,
                 const io::CallbackContext& ctx) noexcept [[clang::nonblocking]] override;

    // [main → RT] Giá trị do main cập nhật định kỳ, audio thread đọc relaxed.
    void setLatencyRoundTrip(int samples) noexcept { latencyRoundTrip_.store(samples, std::memory_order_relaxed); }
    void setDeviceXruns(int count) noexcept { deviceXruns_.store(count, std::memory_order_relaxed); }
    // [main] Route có tai nghe có dây / interface (P1-23: monitoring Auto chỉ bật khi có).
    void setHeadphones(bool on) noexcept { headphones_.store(on, std::memory_order_relaxed); }

    // ── MIDI input (P4-01/02/04) ──
    // R10: mỗi nguồn (thiết bị) một queue SPSC riêng → đúng một producer mỗi queue dù CoreMIDI gọi callback từ thread
    // nào. Queue truyền vào ctor = nguồn 0 (sim / test). [main, audio chưa chạy] đăng ký queue của thiết bị.
    static constexpr int kMaxMidiSources = 8;
    void setMidiSources(MidiQueue* const* queues, int n) noexcept;
    // [main] Độ trễ lập lịch (MidiInputRouter): = 1 block → vị trí tương đối giữa các nốt đúng (không jitter).
    void setMidiScheduleLatencyNs(std::int64_t ns) noexcept { midiLatencyNs_.store(ns, std::memory_order_relaxed); }
    // [main] MIDI learn: message nốt / CC kế tiếp (bất kỳ nguồn) được báo về main (MidiLearned), không phát ra tiếng.
    void armMidiLearn(bool on) noexcept { learnArmed_.store(on, std::memory_order_release); }
    bool midiLearnArmed() const noexcept { return learnArmed_.load(std::memory_order_acquire); }
    int selectedTrack() const noexcept { return selectedTrack_; }   // [test]

    // ── Pedal mode FromFirstLoop (P1-39, 04 §2.5) ── [main] trạng thái do main tính (mode, đã có tempo, độ dài vòng
    // đầu theo beat; 0 = chưa biết → làm tròn theo 1 bar). RT tự chuyển hasTempo = true khi vòng đầu xong.
    void setPedalState(bool firstLoopMode, bool hasTempo, double loopBeats) noexcept {
        pedalLoopBeats_.store(loopBeats, std::memory_order_relaxed);
        pedalHasTempo_.store(hasTempo, std::memory_order_relaxed);
        pedalMode_.store(firstLoopMode, std::memory_order_release);
    }

    // [main] P3-17 ghi buổi jam: RT chép master (sau limiter) vào ring main cấp sẵn, một job worker xả ra file.
    // Trao ring bằng exchange; RT báo JamStopped khi thôi dùng → main mới được nhả. Ring đầy → bỏ mẫu, đếm.
    static constexpr double kJamRingSeconds = 10.0;
    struct JamRing {
        std::uint32_t id = 0;
        std::int64_t capacity = 0;
        std::unique_ptr<float[]> L, R;
        std::atomic<std::int64_t> written{0};    // [RT] tổng frame đã ghi (release)
        std::atomic<std::int64_t> read{0};       // [worker] tổng frame đã đọc (release)
        std::atomic<std::uint32_t> dropped{0};   // [RT] frame bỏ vì ring đầy
    };
    JamRing* offerJam(JamRing* r) noexcept { return jamOffer_.exchange(r, std::memory_order_acq_rel); }
    void stopJam(std::uint32_t id) noexcept { jamStop_.store(id, std::memory_order_release); }

    // [main] Cho spike.* trong le_call (P0-06/P0-09).
    const spike::SpikeProcessor& spikeProcessor() const noexcept { return spike_; }
    // [main] start() / [worker] isDone(), analyze() — luật thread: spike/measure/LatencyProbe.h
    spike::LatencyProbe& latencyProbe() noexcept { return probe_; }
    const spike::LoadGenerator& loadGenerator() const noexcept { return load_; }
    // [test] Đọc trạng thái RT khi audio KHÔNG chạy (render offline trên cùng thread).
    const Transport& transport() const noexcept { return transport_; }
    const Metronome& metronome() const noexcept { return metronome_; }
    const ClipScheduler& scheduler() const noexcept { return scheduler_; }
    const Mixer& mixer() const noexcept { return mixer_; }
    const Track& track(int t) const noexcept { return tracks_[t]; }
    const FxChain& fx(int t) const noexcept { return fx_[t]; }
    const MasterEq& masterEq() const noexcept { return masterEq_; }
    bool armed(int t) const noexcept { return armed_[t]; }
    int monitorMode(int t) const noexcept { return monitor_[t]; }
    // [main] setBuffer / releaseTake (xem Recorder.h). [test] trạng thái.
    Recorder& recorder() noexcept { return recorder_; }
    // [main, audio chưa chạy] Queue LaunchLog (Engine sở hữu).
    void setLaunchLog(LaunchLogQueue* q) noexcept { scheduler_.setLaunchLog(q); }

    double sampleRate() const noexcept { return sampleRate_; }
    int maxBlockSize() const noexcept { return maxBlock_; }
    std::uint32_t droppedEvents() const noexcept { return droppedEvents_.load(std::memory_order_relaxed); }
    // Lệnh hợp lệ nhưng RT chưa xử lý (subsystem chưa có, VD TRACK_GAIN trước P1-13).
    std::uint32_t ignoredCommands() const noexcept { return ignoredCommands_.load(std::memory_order_relaxed); }

private:
    void processChunk(const float* in0, const float* in1, float* outL, float* outR, int numFrames, int blockOffset) noexcept
        [[clang::nonblocking]];
    void adoptPendingSnapshot() noexcept [[clang::nonblocking]];
    void retireUnused() noexcept [[clang::nonblocking]];
    void remap(const GraphSnapshot& s) noexcept [[clang::nonblocking]];
    bool generationInUse(std::uint32_t g) const noexcept [[clang::nonblocking]];
    void drainCommands() noexcept [[clang::nonblocking]];
    void handleCommand(const LeCommand& c) noexcept [[clang::nonblocking]];
    void noteEvent(int track, int pitch, float velocity01, bool on, int blockOffset) noexcept [[clang::nonblocking]];
    double beatAtBlockOffset(int offset) const noexcept [[clang::nonblocking]];
    void handleMidiEvent(const midi::MidiInputEvent& e) noexcept [[clang::nonblocking]];
    void applyMapping(const midi::LearnTarget& t, const midi::ParsedMidi& m, int blockOffset) noexcept [[clang::nonblocking]];
    void notifyMapped(int kind, int track, int slot, int param, double value) noexcept [[clang::nonblocking]];
    void overdubCommand(int track, bool on) noexcept [[clang::nonblocking]];
    void recordJam(const float* L, const float* R, int n) noexcept [[clang::nonblocking]];
    void finishFirstLoop(int track) noexcept [[clang::nonblocking]];
    void drainMidiInput(std::uint64_t blockHostNs, int numFrames) noexcept [[clang::nonblocking]];
    void pushEvent(std::int32_t type, std::int32_t a, std::int32_t b, double value) noexcept [[clang::nonblocking]];
    void publishState(int numFrames) noexcept [[clang::nonblocking]];

    CommandQueue& commands_;
    RtToNrtQueue& toNrt_;
    MidiQueue& midiIn_;
    StatePublisher& publisher_;

    double sampleRate_ = 48000.0;
    int maxBlock_ = 0;
    std::unique_ptr<float[]> scratch_;   // [L | R], mỗi kênh maxBlock_ frame (dùng khi device có < 2 output)

    HostClock clock_{};
    SnapshotExchange exchange_;
    GraphSnapshot* current_ = nullptr;                  // [RT] sở hữu (trả về main qua Retire)
    GraphSnapshot* retiring_[kMaxRetiring] = {};       // [RT] cũ, còn người dùng
    int retiringCount_ = 0;
    GenerationUser* users_[kMaxGenerationUsers] = {};
    int numUsers_ = 0;
    Transport transport_;
    BlockSplitter splitter_;
    Segment segs_[BlockSplitter::kMaxSegments];
    Metronome metronome_;
    ClipScheduler scheduler_;
    Track tracks_[LE_MAX_TRACKS];
    Recorder recorder_;
    bool armed_[LE_MAX_TRACKS] = {};    // TRACK_ARM
    int midiOdSlot_[LE_MAX_TRACKS] = {-1, -1, -1, -1, -1, -1, -1, -1};   // ô đang overdub MIDI (-1 = không)
    double midiOdLaunch_[LE_MAX_TRACKS] = {};
    int monitor_[LE_MAX_TRACKS] = {};   // TRACK_MONITOR: 0 off / 1 auto / 2 on
    LinearRamp monitorGain_[LE_MAX_TRACKS];
    std::atomic<bool> headphones_{false};
    MidiQueue* midiSources_[kMaxMidiSources] = {};
    int numMidiSources_ = 0;
    midi::MidiInputRouter midiRouter_;
    midi::MidiInputEventList midiEvents_;
    std::atomic<std::int64_t> midiLatencyNs_{0};
    std::atomic<bool> learnArmed_{false};
    std::atomic<bool> pedalMode_{false}, pedalHasTempo_{true};
    std::atomic<double> pedalLoopBeats_{0.0};
    int pedalTrack_ = -1;               // [RT] track đang thu vòng đầu (transport "tạm": UI thấy chưa chạy)
    std::atomic<JamRing*> jamOffer_{nullptr};
    std::atomic<std::uint32_t> jamStop_{0};
    JamRing* jam_ = nullptr;            // [RT] ring đang ghi
    FxChain fx_[LE_MAX_TRACKS];         // P3-12: bus track → FX → mixer
    MasterEq masterEq_;                 // P3-15: master gain → EQ3 → limiter
    double fxRate_ = 48000.0;           // [main] sample rate mà mọi Processor trong snapshot đã được prepare
    Mixer mixer_;
    PeakMeter inputMeter_;
    std::unique_ptr<float[]> mixScratch_;   // [L | R] maxBlock_ mỗi kênh, cho Mixer::mixTrack
    double blockStartBeat_ = 0.0;
    int selectedTrack_ = 0;
    CpuMeter cpu_;
    XrunDetector xruns_;
    spike::SpikeProcessor spike_;
    spike::LoadGenerator load_;        // P0-08 (agent 80)
    spike::LatencyProbe probe_;        // P0-07 (agent 80)
    double probeRate_ = 0.0;           // [main] SR lúc prepare probe gần nhất
    float* recordBuf_ = nullptr;
    int recordCapacity_ = 0;

    int lastBlock_ = 0;
    std::uint32_t publishCounter_ = 0;

    std::atomic<int> latencyRoundTrip_{0};
    std::atomic<int> deviceXruns_{0};
    std::atomic<std::uint32_t> droppedEvents_{0};
    std::atomic<std::uint32_t> ignoredCommands_{0};
};

} // namespace le::core
