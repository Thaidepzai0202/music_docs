// LatencyCalibrator — hiệu chỉnh latency round-trip bằng loopback loa → mic (P1-35 / P4-12 lõi, 04 §5.3).
// Bọc spike::LatencyProbe: chạy N lần đo (mỗi lượt probe phát 5 chirp), loại ngoại lai bằng MAD, trả
// L đo được, offset so với số device báo, độ tin cậy, và LÝ DO khi thất bại.
//
// Dùng ở đâu: 68 nối `latency.calibrate` (job trên worker gọi runCalibration) và `latency.setOffset`:
//   L = device.inputLatency + device.outputLatency + calibrationOffset,   calibrationOffset = offsetSamples.
//
// Loại ngoại lai bằng MAD (median absolute deviation) — vì sao không dùng trung bình ± độ lệch chuẩn:
//   Một lần đo hỏng (tiếng gõ, tiếng vọng mạnh) có thể lệch hàng trăm sample. Trung bình và độ lệch chuẩn đều
//   bị chính giá trị hỏng kéo đi, còn median và MAD thì gần như không đổi. σ̂ = 1.4826·MAD ước lượng độ lệch
//   chuẩn khi dữ liệu tốt có phân phối chuẩn. Lần đo x bị loại khi |x − median| > max(k·σ̂, sàn).
//   Ví dụ: 480, 481, 480, 700, 479 → median 480, |độ lệch| = 0 1 0 220 1 → MAD = 1 → ngưỡng = sàn 12 sample
//   (0.25 ms @48k) → 700 bị loại, L = 480.
//   Sàn cần thiết vì lag là số nguyên: khi đa số lần trùng nhau thì MAD = 0 và ±1 sample cũng thành "ngoại lai".
//
// Lý do thất bại (thứ tự kiểm):
//   NO_SIGNAL     đỉnh input < −60 dBFS (mic tắt / không có quyền mic), hoặc không đủ lần nghe thấy chirp
//                 trong phòng yên tĩnh (cắm tai nghe → loa không kêu, âm lượng 0).
//   TOO_NOISY     không đủ lần nghe thấy chirp VÀ nhiễu nền (đo trong pre-roll, trước chirp đầu) > −34 dBFS.
//   INCONSISTENT  nghe thấy nhưng các lần đo không khớp nhau: sau khi loại ngoại lai còn < minValidRuns,
//                 hoặc max − min > 1 ms (DoD P0-07).
//   DEVICE_CHANGED device prepare lại giữa chừng (đổi route / sample rate) → số của hai route không trộn được.
//   TIMEOUT / CANCELLED / NOT_PREPARED  từ runCalibration.
#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace le::spike {
class LatencyProbe;
}

namespace le::render {

enum class CalibrationError : uint8_t { None, NoSignal, TooNoisy, Inconsistent, DeviceChanged, Timeout, Cancelled, NotPrepared };
// "NONE" "NO_SIGNAL" "TOO_NOISY" "INCONSISTENT" "DEVICE_CHANGED" "TIMEOUT" "CANCELLED" "NOT_PREPARED"
const char* calibrationErrorName(CalibrationError e) noexcept;

struct CalibrationRun {           // một chirp
    int32_t lagSamples = -1;      // −1 = không tìm thấy (score < ngưỡng của probe)
    float   score = 0.0f;         // normalized correlation 0..1
};

struct CalibrationCriteria {
    float  minScore = 0.2f;       // "nghe thấy" khi score ≥ minScore (≥ Config::minScore của probe mới có nghĩa)
    float  minInputPeak = 0.001f; // −60 dBFS
    float  maxNoiseRms = 0.02f;   // −34 dBFS
    int    minValidRuns = 3;
    double madK = 3.0;
    double outlierFloorMs = 0.25;
    double maxSpreadMs = 1.0;
};

struct CalibrationResult {
    bool     ok = false;
    CalibrationError error = CalibrationError::None;
    int32_t  roundTripSamples = -1;   // median các lần hợp lệ = L đo được (−1 nếu không có lần nào)
    int32_t  reportedSamples = 0;     // device báo (input + output)
    int32_t  offsetSamples = 0;       // roundTrip − reported (0 khi thất bại)
    int32_t  spreadSamples = 0;       // max − min các lần hợp lệ
    float    confidence = 0.0f;       // 0..1 = tỉ lệ hợp lệ × chất lượng score × độ chụm (0 khi thất bại)
    int32_t  validRuns = 0;           // sau khi loại ngoại lai
    int32_t  heardRuns = 0;           // score ≥ minScore
    int32_t  totalRuns = 0;
    float    inputPeak = 0.0f;
    float    noiseRms = 0.0f;
    double   sampleRate = 0.0;
    std::vector<CalibrationRun> runs; // mọi lần đo theo thứ tự
    std::vector<uint8_t> inlier;      // 1 = dùng để tính L
};

// Hàm thuần: từ các lần đo → kết quả. Không phụ thuộc thread / thời gian (test trực tiếp).
CalibrationResult evaluateCalibration(const std::vector<CalibrationRun>& runs, float inputPeak, float noiseRms,
                                      int32_t reportedSamples, double sampleRate, const CalibrationCriteria& c = {});

struct CalibrateOptions {
    int     runs = 5;                 // số chirp; làm tròn LÊN bội của 5 (mỗi lượt LatencyProbe = 5 chirp, ~2.7 s)
    int32_t reportedSamples = 0;      // device->latencies().roundTrip()
    CalibrationCriteria criteria;
    double  timeoutMs = 0.0;          // mỗi lượt; 0 = tự tính (thời lượng lượt + 3 s)
    int     pollMs = 20;
};

// [worker] Audio phải đang chạy và RtEngine gọi probe.processRt mỗi block. Không ai khác start() probe trong lúc
// này (68: jobs_->anyRunning("latency")). progress 0..1 (nullable), cancel (nullable).
CalibrationResult runCalibration(spike::LatencyProbe& probe, const CalibrateOptions& opt,
                                 const std::atomic<bool>* cancel = nullptr, std::atomic<float>* progress = nullptr);

} // namespace le::render
