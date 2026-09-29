#pragma once
// Scenario runner (P1-03, 08 §3.2): chạy một kịch bản JSON bằng CÙNG lệnh mà app dùng (le_send / le_call),
// render offline qua OfflineDeviceIO, rồi kiểm các kỳ vọng (golden null test, firstNonSilentSample, ...).
// Dùng chung cho `le-harness render` và test Catch2 (tests/unit/sim/scenario_test.cpp).
//
// Định dạng (mọi trường ngoài "timeline"/"renderBeats" đều tuỳ chọn):
// {
//   "name": "launch_quantized_1bar", "sampleRate": 48000, "blockSize": 128, "bpm": 120,
//   "inputChannels": 0, "input": "fixtures/voice.wav",          // file mono nạp vào mic (kênh 0)
//   "setup":    [ {"call": {"op": "...", ...}}, {"send": {"type": "SET_BPM", "d0": 120}} ],
//   "timeline": [ {"atBeat": 1.3, "send": {...}}, {"atSample": 96000, "call": {...}} ],
//   "renderBeats": 12,                                           // hoặc "renderSeconds" / "renderFrames"
//   "expect": { "golden": "golden/x.wav", "nullTestMaxDb": -90,
//               "firstNonSilentSample": 96000, "silenceThresholdDb": -120 (mặc định),
//               "maxSampleJumpDb": -20, "peakMaxDb": -0.3, "peakMinDb": -40,
//               "onsetSamples": [0, 24000, 48000], "onsetMinGap": 64,
//               "stateAt": [{"frame": 96000, "track": 0, "slot": 0, "clipState": "QUEUED_PLAY"},
//                           {"frame": 96001, "track": 0, "playingSlot": 0}, {"frame": 0, "playing": false}] }
//   stateAt frame F = LeState SAU KHI render đúng F frame đầu tiên (runner chia block tại F).
//   "reference": {"file", "startSample", "skip", "loop", "gain", "maxDiffDb", "endSample"}: null test so với file nguồn.
//   "silentFromSample": N → mọi sample ≥ N dưới ngưỡng im lặng.
//   "clipInfo": [{track, slot, kind, lengthBeats}] → so với clip.info sau khi render.
// - "latencySamples": L → thiết bị (offline) báo latency L; "inputDelaySamples": D → input trễ D sample (round-trip thật);
//   "inputLoop": true → file input lặp lại (P1-19/20).
// - Đường dẫn tương đối (clip.setAudio file, track.setInstrument path) tính từ thư mục engine/tests/ (libraryDir).
// }
// - "send": {"type": "CLIP_LAUNCH" | "LE_CMD_CLIP_LAUNCH" | 10, "track", "slot", "i0", "f0", "f1", "d0"};
//   i0 nhận cả tên enum ("LE_Q_1_BAR").
// - atBeat được đổi sang sample theo "bpm" (và các SET_BPM đã gặp trong timeline). Sự kiện rơi giữa block
//   thì runner chia block tại đúng sample đó → kết quả không phụ thuộc blockSize.
// - "call" trả jobId thì runner chờ job xong (tối đa 30 giây) trước khi chạy tiếp.
// - "$TMP" trong chuỗi được thay bằng thư mục tạm của lượt chạy.
// - "requires": ["recording", "monitoring", …] → nếu engine (binary này) chưa có tính năng đó: lỗi
//   "scenario cần tính năng X", không chạy. Danh sách: supportedFeatures().
#include <cstdint>
#include <string>
#include <vector>

namespace le::sim {

struct RunOptions {
    int blockSize = 0;            // 0 = theo scenario (mặc định 128)
    int maxBlock = 0;             // 0 = max(blockSize, 1024). < blockSize → ép RtEngine chia khối (04 §1)
    // Khác nullptr → đo thời gian (steady_clock, ns) của MỖI lần render khối (chỉ phần RtEngine, không tính pump,
    // JSON hay kiểm kỳ vọng) rồi push_back vào đây. Cho tools/le-engine-bench (80).
    std::vector<double>* blockTimesNs = nullptr;
    // Khác nullptr → push_back số frame THẬT của mỗi lần render (runner chia block tại sự kiện lệch lưới), cùng thứ
    // tự với blockTimesNs → tính % CPU từng khối chính xác.
    std::vector<int>* blockFrames = nullptr;
    std::string baseDir;          // gốc cho đường dẫn tương đối trong scenario (mặc định: thư mục chứa file .json/..)
    bool checkExpectations = true;
    bool ignoreGolden = false;    // golden_update.sh: render mà không so golden
};

struct ScenarioResult {
    bool ok = false;                  // parse/setup/render thành công VÀ mọi kỳ vọng đạt
    std::string error;                // lỗi parse/setup (khác rỗng → không render)
    std::vector<std::string> failures;// kỳ vọng không đạt (mỗi dòng một lỗi, có số sample cụ thể)
    std::vector<std::string> notes;   // thông tin thêm (null test dB, sample đầu không im lặng...)
    std::string name;
    double sampleRate = 48000.0;
    int blockSize = 128;
    std::vector<float> left, right;
};

// Đọc file JSON rồi chạy. `baseDir` rỗng → thư mục cha của thư mục chứa scenario (tức engine/tests/).
ScenarioResult runScenarioFile(const std::string& path, const RunOptions& opt = {});
ScenarioResult runScenarioJson(const std::string& json, const RunOptions& opt = {});

// Tiện ích WAV (float 32-bit, stereo). [main/worker]
bool writeWavStereo(const std::string& path, const std::vector<float>& left, const std::vector<float>& right,
                    double sampleRate, std::string* error = nullptr);
bool readWav(const std::string& path, std::vector<std::vector<float>>& channels, double& sampleRate,
             std::string* error = nullptr);

// Tính năng engine mà scenario có thể đòi bằng "requires": [...] (binary cũ gặp scenario mới → lỗi rõ ràng).
const std::vector<std::string>& supportedFeatures();

// Tên lệnh ↔ mã (dùng cho scenario và harness).
int commandTypeFromName(const std::string& name);   // -1 nếu không có

} // namespace le::sim
