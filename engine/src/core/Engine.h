#pragma once
// Engine: facade mà C API gọi vào (03 §7). Mọi method chạy trên main thread (05 §1).
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/CommandProcessor.h"
#include "core/EngineModel.h"
#include "render/PeakBuilder.h"
#include "core/JobSystem.h"
#include "core/MidiDevices.h"
#include "core/RtEngine.h"
#include "le/engine_api.h"

namespace juce { class ScopedJuceInitialiser_GUI; }

namespace le::io { class DeviceIO; }

namespace le::core {

// [any] Publisher sống suốt đời process → le_read_state an toàn cả trước le_create / sau le_destroy.
StatePublisher& globalStatePublisher();

// [main] Callback event do Dart đăng ký. Lưu toàn cục vì Dart gọi le_set_event_callback TRƯỚC le_create.
void setEventCallback(LeEventCallback cb);
void emitEvent(std::int32_t type, std::int32_t a, std::int32_t b, std::int64_t jobId, double value);

class EventPump;

class Engine {
public:
    // `device` = nullptr → JuceDeviceIO (tạo lúc audioStart). Test / render offline truyền OfflineDeviceIO.
    explicit Engine(const LeConfig& cfg, std::unique_ptr<io::DeviceIO> device = nullptr);   // validate trước
    ~Engine();

    static std::int32_t validate(const LeConfig* cfg);   // LeError

    std::int32_t audioStart();
    void audioStop();
    bool send(const LeCommand& cmd);
    std::string call(const char* requestJson);   // luôn trả JSON envelope (05 §3)
    // [main] le_get_peaks (P1-24): số cặp (min,max) đã ghi, 0 nếu clip có nhưng peaks chưa tính xong, <0 = LeError.
    std::int32_t getPeaks(const char* clipId, std::int32_t level, float* outMinMax, std::int32_t maxPairs) const;

    // [main] Timer 30Hz: xả rtToNrt, phát event, theo dõi interruption/route/xrun.
    void pump();

    // [main] Build snapshot từ model và đưa cho RT (03 §4.2). Gọi sau mỗi lệnh cấu trúc.
    void publishSnapshot();
    const EngineModel& model() const noexcept { return model_; }
    RtEngine& rt() noexcept { return rt_; }   // [test]
    std::uint32_t releasedSnapshots() const noexcept { return releasedSnapshots_; }
    // [main] LaunchLog đã gom từ RT (04 §3.5). Chỉ số = vị trí trong vector.
    const std::vector<LaunchEvent>& launchLog() const noexcept { return launchLogEntries_; }

    std::uint32_t rejectedCommands() const noexcept { return rejectedCommands_; }   // lệnh sai tham số
    std::uint32_t queueFullCount() const noexcept { return queueFull_; }           // queue RT đầy
    std::uint32_t mediaServicesResets() const noexcept { return mediaResets_; }    // số lần restart device vì reset
    io::DeviceIO* device() noexcept { return device_.get(); }
    // [main / test / sim] MIDI vào nguồn 0 (ảo), cùng đường với thiết bị thật. hostNs = 0 → block kế tiếp.
    bool injectMidi(const std::uint8_t* bytes, int size, std::int64_t hostNs = 0);
    // [test] Processor của slot FX trong model. Hết hạn khi slot bị xoá/thay VÀ mọi snapshot còn giữ nó đã được thu
    // hồi trên main (ReleasePool).
    std::weak_ptr<dsp::Processor> fxProcessor(int track, int slot) const;
    // [test] Số frame của buffer thu track t (0 = chưa cấp).
    std::int64_t recordBufferFrames(int t) const { return takeBuffers_[t] != nullptr ? takeBuffers_[t]->numFrames() : 0; }
    // [test] preview: job nạp đang chờ (0 = không), số vé RT có thể còn giữ (tới PreviewReleased), số mục cache LRU.
    std::int64_t previewJob() const noexcept { return previewJob_; }
    std::size_t previewLiveTickets() const noexcept { return previewLive_.size(); }
    std::size_t previewCacheSize() const noexcept { return previewCache_.size(); }
    const dsp::Instrument* previewCachedInstrument(std::size_t i) const noexcept {
        return i < previewCache_.size() ? previewCache_[i].instrument.get() : nullptr;
    }

private:
    // EngineOps.cpp
    void registerOps();
    Reply opEngineInfo(const juce::var& req);
    Reply opSetBufferSize(const juce::var& req);
    Reply opSetInputEnabled(const juce::var& req);
    Reply opSetSessionMode(const juce::var& req);
    Reply opSessionInfo(const juce::var& req);
    Reply opLatencyLoopback(const juce::var& req);
    Reply opStretchBench(const juce::var& req);
    Reply opJobResult(const juce::var& req);
    Reply opJobCancel(const juce::var& req);
    Reply opProjectOpen(const juce::var& req);
    Reply opProjectClose(const juce::var& req);
    Reply opSetTimeSignature(const juce::var& req);
    Reply opTrackConfigure(const juce::var& req);
    Reply opClipSetMidi(const juce::var& req);
    Reply opClipGetMidi(const juce::var& req);
    Reply opClipClear(const juce::var& req);
    Reply opClipInfo(const juce::var& req);
    Reply opLaunchLogRead(const juce::var& req);
    Reply opClipSetAudio(const juce::var& req);
    Reply opTrackSetInstrument(const juce::var& req);
    std::string resolvePath(const std::string& p, bool preferLibrary) const;   // tương đối → projectDir / libraryDir
    // src/sim/EngineSimOps.cpp — chỉ có khi LE_ENABLE_SIM (Mac); iOS → không đăng ký → NOT_IMPLEMENTED
    void registerSimOps();
    Reply opSimOffline(const juce::var& req);
    Reply opSimAdvance(const juce::var& req);
    Reply opSimMidiIn(const juce::var& req);
    void drainRtToNrt();   // [main] event → Dart, Retire → delete (ReleasePool), TakeFinished → clip
    void ensureRecordBuffer(int track);                 // [main] P1-18: cấp phát buffer thu TRƯỚC khi push lệnh
    void fitRecordBuffersToRate();                      // [main] R7: sau khi device (re)start
    void handleTakeFinished(const RtMessage& m);        // [main] P1-21
    void handleMidiMessage(const RtMessage& m);         // [main] P1-30: nốt thu / take MIDI xong / overdub
    // [main] LE_EVT_CLIP_CHANGED: ô (t, s) vừa đổi nội dung MIDI. Phát ngay nếu lần trước đã ≥ kClipChangedMs, không thì
    // dồn lại cho pump(); final = hết lượt overdub → luôn phát (trước RECORDING_FINISHED).
    static constexpr double kClipChangedMs = 100.0;
    void clipChanged(int t, int s, bool final);
    void flushClipChanged();
    // [main] P1-22 + R1: quyết định BẬT/TẮT (ghi i0) và đưa vé cho lượt bật. false = không push bây giờ (hoãn).
    bool prepareOverdubCommand(LeCommand& c);
    bool offerOverdubTicket(int track);                 // [main] copy clip → target, snapshot, đưa vé cho RT
    void reclaimOverdubTicket(int track);               // [main] thu lại vé RT chưa lấy
    void handleOverdubFinished(const RtMessage& m);     // [main] RT trả vé (đã ghi / không ghi)
    // [main] Ghi audio/<clipId>.caf + peaks trên worker, rồi RECORDING_FINISHED(track, slot) khi file đã đóng.
    void persistClipAudio(int track, int slot, dsp::AudioDataPtr data, std::int64_t eventValue);
    Reply opClipUndoOverdub(const juce::var& req);
    Reply undoOverdub(int track, int slot);   // clip.undoOverdub + MIDI {kind:"undoOverdub"}
    Reply opSetRecordQuantize(const juce::var& req);
    Reply opClipSetParams(const juce::var& req);
    Reply opMidiClipQuantize(const juce::var& req);
    // EngineFxOps.cpp (P3-12/15)
    Reply opFxSet(const juce::var& req);
    Reply opFxRemove(const juce::var& req);
    bool prepareFxCommand(LeCommand& c) const;   // kiểm theo model + ghi instanceId vào d0
    void commitFxCommand(const LeCommand& c);    // sau khi push: ghi tham số / bypass vào model
    bool pushCommands(const LeCommand* cmds, int n);   // tất cả hoặc không
    void resetProjectRtState();
    // EngineCapture.cpp (P3-02, P3-05..07)
    Reply opCaptureStart(const juce::var& req);
    Reply opCaptureStop(const juce::var& req);
    Reply opCaptureAnalyze(const juce::var& req);
    Reply opInstrumentCreateFromRecording(const juce::var& req);
    Reply opInstrumentSetMode(const juce::var& req);
    Reply opInstrumentSetEnvelope(const juce::var& req);
    Reply setUserInstrument(int track, const std::string& id);   // track.setInstrument {kind:"user"}
    void applyUserInstrument(const std::string& id);
    std::int64_t submitUserInstrumentRender(const std::string& id);   // job theo UserInstrumentModel::source
    // EnginePreview.cpp (05 §3 preview.*, Browser)
    static constexpr std::size_t kPreviewCacheSize = 3;
    struct PreviewEntry {   // [main] một mục nghe thử đã nạp (instrument HOẶC audio)
        std::string key;    // kind + ":" + đường dẫn tuyệt đối
        dsp::InstrumentPtr instrument;
        dsp::AudioDataPtr audio;
        // Kết quả SfzLoader (sfz): track.setInstrument dùng lại bản cache → job.result y như nạp mới
        std::string name;
        int regions = 0, samplesLoaded = 0;
        std::vector<std::string> warnings;
    };
    Reply opPreviewPlay(const juce::var& req);
    Reply opPreviewStop(const juce::var& req);
    void offerPreview(const PreviewEntry& e, std::uint32_t id, int note, double durationMs);
    void releasePreviewTicket(std::uint32_t id);
    void handlePreviewReleased(const RtMessage& m) { releasePreviewTicket((std::uint32_t) m.a); }
    // [main] Mục cache của đường dẫn SFZ tuyệt đối (nullptr = chưa có); trúng → đưa lên đầu LRU.
    const PreviewEntry* previewCachedSfz(const std::string& absPath);
    // EngineMemory.cpp (P4-17)
    Reply opMemoryPressure(const juce::var& req);
    std::int64_t releaseMemory(bool critical);   // byte đã nhả (ước lượng)
    static double memoryFootprintMB();
    bool finalizeCapture(std::int64_t frames);
    void handleCaptureFinished(const RtMessage& m);
    // EngineExport.cpp (P3-17..19)
    Reply opExportScene(const juce::var& req);
    Reply opExportJamStart(const juce::var& req);
    Reply opExportJamStop(const juce::var& req);
    std::unique_ptr<GraphSnapshot> buildOfflineSnapshot(double sampleRate) const;
    std::vector<LeCommand> offlineSetupCommands(int soloTrack) const;
    void handleJamStopped(const RtMessage& m);
    // EngineLatency.cpp (P1-35 / P4-12)
    static constexpr int kMaxLatencyOffset = 96000;   // ±1 s @96k
    Reply opLatencyCalibrate(const juce::var& req);
    Reply opLatencySetOffset(const juce::var& req);
    // EngineMidi.cpp (P4-01/02/04/05)
    static constexpr const char* kVirtualMidiDevice = "virtual";   // nguồn 0: sim.midiIn / test
    Reply opMidiListDevices(const juce::var& req);
    Reply opMidiEnableDevice(const juce::var& req);
    Reply opMidiLearnStart(const juce::var& req);
    Reply opMidiLearnCancel(const juce::var& req);
    Reply opMidiLearnResult(const juce::var& req);
    Reply opMidiSetMappings(const juce::var& req);
    std::string parseMidiTarget(const juce::var& t, midi::LearnTarget& out) const;
    void rebuildMidiMap();
    void handleMidiLearned(const RtMessage& m);
    void handleMappedChange(const RtMessage& m);
    // Pedal mode (P1-39, 04 §2.5) — Engine.cpp
    Reply opTransportSetTempoMode(const juce::var& req);
    bool hasTempo() const;         // fixed → luôn có; firstLoop → vòng đầu đã chốt, hoặc project đã có clip
    bool hasClipInModel() const;
    bool loopButton(int track, int slot);   // LE_CMD_LOOP_BUTTON → lệnh cụ thể theo trạng thái ô (07 §3.1b)
    void syncTempoToRt();
    // EngineWarp.cpp (P3-08, 04 §10)
    static constexpr double kWarpDebounceMs = 300.0;
    static constexpr double kBpmMatch = 0.005;   // cùng dung sai với AudioClipPlayer
    void refreshWarp(int track, int slot);       // [main] xếp / huỷ job WarpRenderer cho ô nếu cần
    void refreshAllWarps();
    double debounceClockMs() const;              // offline: theo audio đã render (tất định); thật: đồng hồ hệ thống
    bool offlineDevice() const;                  // project.open/close: transport, mixer, master về mặc định
    void refreshDeviceInfo();

    // Thứ tự khai báo = thứ tự khởi tạo; huỷ theo thứ tự ngược lại. JUCE phải sống lâu nhất.
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juce_;

    LeConfig cfg_{};
    std::string dataDir_, libraryDir_;
    int preferredBuffer_ = 128;
    double preferredRate_ = 48000.0;
    int numInputs_ = 1;             // kênh input đang dùng (0 = chỉ phát, audio.setInputEnabled false)
    int configuredInputs_ = 1;      // LeConfig.numInputChannels (≥ 1): số kênh khi bật lại input

    EngineModel model_;                  // [main]
    std::uint32_t generation_ = 0;       // [main] generation của snapshot build gần nhất
    std::uint32_t releasedSnapshots_ = 0;

    CommandQueue commands_{kRtCommandCapacity};
    RtToNrtQueue toNrt_{kRtToNrtCapacity};
    MidiQueue midiToRt_{kMidiToRtCapacity};   // producer: CoreMIDI thread (P4)
    LaunchLogQueue launchLog_{kLaunchLogCapacity};
    std::vector<LaunchEvent> launchLogEntries_;   // [main]
    std::unique_ptr<float[]> recordBuf_;   // spike P0
    int recordCapacity_ = 0;
    std::unique_ptr<dsp::AudioData> takeBuffers_[LE_MAX_TRACKS];   // [main] sở hữu; RT chỉ ghi (Recorder)
    std::uint32_t takeCounter_ = 0;
    // [main] P1-30: ghép nốt on/off đang thu (mỗi track 1 take) và đang overdub.
    struct OpenNotes {
        bool open[128] = {};
        double start[128] = {};
        int vel[128] = {};
        std::vector<dsp::MidiNote> done;
    };
    OpenNotes midiTake_[LE_MAX_TRACKS];
    OpenNotes midiOverdub_[LE_MAX_TRACKS];
    double recordQuantize_ = 0.0;   // midi.setRecordQuantize (beat), 0 = tắt
    std::uint32_t fxInstanceCounter_ = 0;   // instanceId của Processor FX, không lặp lại trong đời engine
    // P3-08: BPM project mới nhất main biết (SET_BPM), hạn debounce, job WarpRenderer đang chạy của từng ô.
    double bpm_ = 120.0;
    double warpDueMs_ = -1.0;
    struct WarpJob {
        std::int64_t id = 0;
        double bpm = 0.0;
        std::weak_ptr<const dsp::AudioData> source;
    };
    WarpJob warp_[LE_MAX_TRACKS][LE_MAX_SCENES];
    // P3-02: buffer capture RT có thể còn giữ (tới CaptureFinished) + kết quả lượt hiện tại (capture.stop idempotent).
    struct CaptureState {
        std::unique_ptr<Recorder::CaptureTicket> ticket;
        std::shared_ptr<dsp::AudioData> buf;
    };
    std::vector<CaptureState> captures_;
    std::uint32_t captureId_ = 0;   // lượt hiện tại (0 = chưa có)
    bool captureDone_ = false;      // lượt hiện tại đã ghi file
    std::string captureFile_, captureAbs_;
    double captureSeconds_ = 0.0;
    int midiOverdubNotes_[LE_MAX_TRACKS] = {};   // nốt đã trộn trong lượt overdub MIDI hiện tại
    double clipChangedMs_[LE_MAX_TRACKS][LE_MAX_SCENES] = {};    // lần phát CLIP_CHANGED gần nhất (debounceClockMs)
    bool clipChangedSent_[LE_MAX_TRACKS][LE_MAX_SCENES] = {};    // đã từng phát (clipChangedMs_ có nghĩa)
    bool clipChangedPending_[LE_MAX_TRACKS][LE_MAX_SCENES] = {}; // đổi sau lần phát gần nhất, chờ hết khoảng 100 ms
    bool anyClipChangedPending_ = false;                         // pump() rảnh: không đọc đồng hồ, không quét 64 ô
    // P3-17: phiên ghi jam hiện tại + ring RT có thể còn giữ (tới JamStopped).
    struct JamSession {
        std::shared_ptr<RtEngine::JamRing> ring;
        std::shared_ptr<std::atomic<std::int64_t>> stopAt;
        std::shared_ptr<std::atomic<std::int32_t>> failed;
        std::int64_t job = 0;
        std::string path;
        double sampleRate = 48000.0;
    };
    JamSession jam_;
    std::vector<std::shared_ptr<RtEngine::JamRing>> jamRings_;
    std::uint32_t jamCounter_ = 0;
    int latencyOffset_ = 0;         // latency.setOffset / calibrate: cộng vào roundTrip device báo (toàn cục)
    bool tempoFound_ = false;       // pedal mode: vòng đầu đã chốt trong phiên này
    double firstLoopBeats_ = 0.0;   // độ dài vòng đầu (beat) — vòng sau làm tròn lên bội số của nó
    // P4: thiết bị MIDI + mapping (state project) + learn đang chờ + kết quả learn gần nhất.
    std::unique_ptr<MidiDevices> midi_;
    std::vector<UserMidiMapping> midiMappings_;
    midi::LearnTarget learnTarget_;
    juce::var learnTargetJson_;
    bool learnPending_ = false;
    struct LastLearn {
        std::string deviceId, deviceName;
        midi::LearnKind kind = midi::LearnKind::Note;
        int channel = 0, number = 0;
    };
    std::optional<LastLearn> lastLearn_;
    // Preview (EnginePreview.cpp): cache LRU (front = dùng gần nhất) + vé RT có thể còn đọc (giữ dữ liệu riêng, không
    // phụ thuộc cache) + lượt mới nhất (id vé; job nạp xong mà lượt đã đổi thì chỉ vào cache, không phát).
    std::vector<PreviewEntry> previewCache_;
    struct PreviewLive {
        std::unique_ptr<PreviewPlayer::Ticket> ticket;
        dsp::InstrumentPtr instrument;
        dsp::AudioDataPtr audio;
    };
    std::vector<PreviewLive> previewLive_;
    std::uint32_t previewSeq_ = 0;
    std::int64_t previewJob_ = 0;   // job nạp của lượt mới nhất (0 = không có)
    // P1-22 + R1: mỗi lượt overdub audio = một vé đã đưa cho RT. Main giữ vé + target sống cho tới khi RT trả vé
    // (OverdubFinished cùng session) — kể cả khi ô đã bị clip.clear / project.open trong lúc RT còn ghi.
    struct OverdubRound {
        std::unique_ptr<Recorder::OverdubTicket> ticket;
        std::shared_ptr<dsp::AudioData> target;   // RT cộng input vào; model giữ cùng object dạng const
        dsp::AudioDataPtr before;                 // → undo layer khi lượt xong
    };
    std::vector<OverdubRound> overdubRounds_[LE_MAX_TRACKS];
    bool overdubOn_[LE_MAX_TRACKS] = {};              // ý định của main: toggle kế tiếp là TẮT
    bool overdubDeferred_[LE_MAX_TRACKS] = {};        // bật khi lượt trước còn giữ vé → bật lúc RT trả vé
    std::uint32_t overdubOnCounter_[LE_MAX_TRACKS] = {};   // LeState.publishCounter lúc gửi lệnh bật
    std::uint32_t overdubSession_ = 0;
    dsp::AudioDataPtr undoLayer_[LE_MAX_TRACKS][LE_MAX_SCENES];
    std::unordered_map<std::string, std::shared_ptr<const render::Peaks>> peaks_;   // [main] clipId → peaks
    // [worker] Đọc cache/<clipId>.peaks nếu khớp nguồn, không thì tính và ghi cache (projectDir rỗng → không cache).
    static std::shared_ptr<const render::Peaks> peaksFor(const dsp::AudioData& data, const std::string& cachePath,
                                                          const std::atomic<bool>* cancel);
    RtEngine rt_;
    std::unique_ptr<JobSystem> jobs_;   // sau rt_: job tham chiếu LatencyProbe trong rt_ → phải huỷ trước
    std::unique_ptr<io::DeviceIO> device_;
    std::unique_ptr<EventPump> pump_;
    CommandProcessor ops_;

    std::uint32_t rejectedCommands_ = 0;
    std::uint32_t queueFull_ = 0;
    std::uint32_t lastXruns_ = 0;
    std::uint32_t seenInterruptBegan_ = 0, seenInterruptEnded_ = 0, seenRoute_ = 0, seenDeviceChanges_ = 0;
    std::uint32_t seenMediaReset_ = 0;
    std::uint32_t seenMemoryWarnings_ = 0;
    std::uint32_t mediaResets_ = 0;
    int pumpTicks_ = 0;
    int startGraceTicks_ = 0;
};

} // namespace le::core
