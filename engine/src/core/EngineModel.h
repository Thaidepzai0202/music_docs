#pragma once
// EngineModel (03 §4.1): trạng thái cấu trúc có thể thay đổi. [main] CHỈ main thread đọc/ghi.
// Mỗi lệnh cấu trúc (le_call) sửa model rồi build lại GraphSnapshot (SnapshotBuilder) → publish.
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "core/GraphSnapshot.h"

namespace le::core {

struct ClipModel {
    std::string clipId;
    ClipKind kind = ClipKind::None;
    double lengthBeats = 0.0;
    double originalBpm = 0.0;
    float gainDb = 0.0f;
    WarpMode warp = WarpMode::Repitch;
    std::string file;              // audio: đường dẫn file gốc
    dsp::AudioDataPtr audio;
    dsp::MidiClipPtr midi;
    // P3-08: bản stretched gần nhất (mỗi clip giữ tối đa 1, 04 §10). Chỉ hợp lệ khi được render từ ĐÚNG `audio`
    // hiện tại: overdub / undo đổi audio → bản cũ tự bị loại khỏi snapshot (weak_ptr, không so con trỏ trần).
    dsp::AudioDataPtr stretched;
    double stretchedBpm = 0.0;
    std::weak_ptr<const dsp::AudioData> stretchedSource;
    bool stretchedValid() const {
        const auto src = stretchedSource.lock();
        return stretched != nullptr && src != nullptr && src == audio;
    }
};

// FX của một slot (P3-12). Tham số lưu Ở ĐÂY (nguồn sự thật): sau khi publish, main không đọc/ghi processor nữa.
struct FxModel {
    dsp::FxType type = dsp::FxType::Filter;
    float params[kMaxFxParams] = {};   // theo paramId (đã kẹp)
    bool bypass = false;
    std::uint32_t instanceId = 0;      // khớp FxSlotSnapshot::instanceId, main ghi vào d0 của lệnh FX
    std::shared_ptr<dsp::Processor> proc;
};

// Nhạc cụ tự thu (P3-05, 06 §2 "userInstruments"). Đăng ký ngay khi nhận createFromRecording.
struct UserInstrumentModel {
    dsp::Instrument::Mode mode = dsp::Instrument::Mode::Natural;
    dsp::AdsrParams env{0.005f, 0.2f, 0.8f, 0.3f};   // mặc định 06 §2
    dsp::InstrumentPtr rendered;                     // kết quả PitchRenderer / cache (env, mode gốc)
    dsp::InstrumentPtr current;                      // rendered + mode + env: bản track dùng
    std::int64_t request = 0;                        // job createFromRecording mới nhất
    struct Source {                                  // tham số lần tạo gần nhất (P4-17: tạo lại từ cache zone)
        std::string file;                            // đường dẫn tuyệt đối
        int rootNote = -1;
        double trimStart = -1.0, trimEnd = -1.0;
    } source;
};

struct TrackModel {
    TrackKind kind = TrackKind::Audio;
    std::string name;
    dsp::InstrumentPtr instrument;
    std::string userInstrumentId;
    std::uint32_t color = 0;        // 0xRRGGBB (track.configure color, 06 §2 tracks[].color)
    bool hasColor = false;   // ≠ rỗng: track dùng nhạc cụ tự thu này (cập nhật khi render xong / đổi env)
    std::optional<FxModel> fx[kFxSlots];
};

// Mixer track (06 §2 "mixer"): main ghi lại từ LE_CMD_TRACK_* đã push thành công → export offline dựng lại đúng mix.
struct MixerStripModel {
    float gainDb = 0.0f, pan = 0.0f;
    bool mute = false, solo = false;
};

// Master cố định (P3-15, 06 §2 "master"): chỉnh bằng LE_CMD_FX_PARAM track -1. Gain đi đường LE_CMD_MASTER_GAIN.
struct MasterModel {
    float gainDb = 0.0f;
    float eq3[3] = {0.0f, 0.0f, 0.0f};
    bool eqBypass = false;
    float limiterCeilingDb = -0.3f;
    float limiterReleaseMs = 50.0f;
};

struct EngineModel {
    std::string projectDir;
    int beatsPerBar = 4;
    int beatUnit = 4;
    TrackModel tracks[LE_MAX_TRACKS];
    std::optional<ClipModel> clips[LE_MAX_TRACKS][LE_MAX_SCENES];
    MasterModel master;
    MixerStripModel mixer[LE_MAX_TRACKS];
    bool tempoFirstLoop = false;   // transport.tempoMode (06 §2): "firstLoop" = pedal mode (04 §2.5)
    std::shared_ptr<const midi::MidiLearnMap> learnMap;   // dựng lại từ mappings (EngineMidi.cpp)
    std::map<std::string, UserInstrumentModel> userInstruments;

    // jobId của lệnh bất đồng bộ MỚI NHẤT cho từng ô / track (clip.setAudio, track.setInstrument).
    // Job xong mà id không còn khớp (đã có lệnh mới hơn, clip.clear, project.close) → kết quả bị bỏ.
    std::int64_t cellRequest[LE_MAX_TRACKS][LE_MAX_SCENES] = {};
    std::int64_t trackRequest[LE_MAX_TRACKS] = {};

    std::uint32_t projectEpoch = 0;
    std::uint32_t cellEpoch[LE_MAX_TRACKS][LE_MAX_SCENES] = {};

    // Xoá sạch model nhưng TĂNG projectEpoch → RT bỏ mọi take nó đang giữ.
    void reset() {
        const std::uint32_t epoch = projectEpoch + 1;
        *this = EngineModel{};
        projectEpoch = epoch;
    }
    void touchCell(int t, int s) { ++cellEpoch[t][s]; }
};

// [main] Dựng snapshot bất biến từ model (copy shared_ptr, không copy dữ liệu âm thanh).
inline std::unique_ptr<GraphSnapshot> buildSnapshot(const EngineModel& m, std::uint32_t generation) {
    auto s = std::make_unique<GraphSnapshot>();
    s->generation = generation;
    s->beatsPerBar = m.beatsPerBar;
    s->beatUnit = m.beatUnit;
    s->projectEpoch = m.projectEpoch;
    s->learnMap = m.learnMap;
    for (int t = 0; t < LE_MAX_TRACKS; ++t)
        for (int c = 0; c < LE_MAX_SCENES; ++c) s->cellEpoch[t][c] = m.cellEpoch[t][c];
    for (int t = 0; t < LE_MAX_TRACKS; ++t) {
        s->tracks[t].kind = m.tracks[t].kind;
        s->tracks[t].instrument = m.tracks[t].instrument;
        for (int k = 0; k < kFxSlots; ++k) {
            if (!m.tracks[t].fx[k]) continue;
            const FxModel& f = *m.tracks[t].fx[k];
            s->tracks[t].fx[k].proc = f.proc;   // cùng instance → RT không fade, đuôi giữ nguyên
            s->tracks[t].fx[k].instanceId = f.instanceId;
            s->tracks[t].fx[k].bypass = f.bypass;
        }
        for (int c = 0; c < LE_MAX_SCENES; ++c) {
            if (!m.clips[t][c]) continue;
            const ClipModel& src = *m.clips[t][c];
            ClipSnapshot& dst = s->clips[t][c];
            dst.kind = src.kind;
            dst.lengthBeats = src.lengthBeats;
            dst.originalBpm = src.originalBpm;
            dst.gainDb = src.gainDb;
            dst.warp = src.warp;
            dst.audio = src.audio;
            dst.midi = src.midi;
            if (src.kind == ClipKind::Audio && src.warp == WarpMode::Stretch && src.stretchedValid()) {
                dst.stretched = src.stretched;
                dst.stretchedBpm = src.stretchedBpm;
            }
        }
    }
    return s;
}

} // namespace le::core
