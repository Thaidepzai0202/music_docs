// StretchBench — đo thời gian + tạo file nghe thử pitch-shift bằng Signalsmith Stretch (P0-09).
// [worker] Toàn bộ chạy trên worker thread (cấp phát, file I/O). KHÔNG gọi từ audio thread.
//
// Render 13 zone (-18, -15, …, +15, +18 nửa cung) từ một buffer mono, chế độ offline
// seek → process → flush, output dài ĐÚNG bằng input và thẳng hàng với input (không có pre-roll).
// Cách giữ formant đã chốt khi đọc header, xem engine/tools/docs/signalsmith-notes.md:
//   setTransposeSemitones(k) + setFormantFactor(1, compensatePitch=true) + setFormantBase(f0/sr).
//
// Dùng:
//   StretchBench::Config cfg;  cfg.formant = true;  cfg.outDir = ".../Documents/spike";
//   StretchBench::Result r = StretchBench::run(buf.data(), n, 48000.0, cfg);
//   // r.msPerZone[i] ứng với r.semitones[i]; r.files[i] là đường dẫn WAV (nếu có outDir)
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace le::spike {

class StretchBench {
public:
    struct Config {
        int    semitoneMin = -18;
        int    semitoneMax = 18;
        int    semitoneStep = 3;      // → 13 zone
        std::vector<int> semitones;   // khác rỗng → dùng đúng danh sách này, bỏ qua min/max/step
        bool   formant = false;       // true: giữ formant (chất giọng) khi dịch cao độ
        double formantBaseHz = 0.0;   // f0 ước lượng (lấy từ Yin). 0 = để Signalsmith tự đoán
        double tonalityLimitHz = 0.0; // 0 = tắt. Ví dụ 8000: phần trên 8 kHz dịch ít hơn (xem notes)
        bool   cheaper = false;       // presetCheaper (block 100 ms) thay vì presetDefault (120 ms)
        // Tự chọn block/interval STFT (ms), > 0 thì bỏ qua preset. Block dài → cao độ chính xác hơn
        // (đo trên sine: default lệch tới 22 cent, 200/50 ms còn ~8 cent) nhưng transient nhoè hơn.
        double blockMs = 0.0;
        double intervalMs = 0.0;
        std::string outDir;           // rỗng = không ghi file
        bool   keepAudio = false;     // giữ output trong Result::audio (dùng cho test)

        // Tuỳ chọn cho JobSystem (04 §15). Có thể để nullptr.
        const std::atomic<bool>* cancel = nullptr;   // true → dừng sau zone đang render
        std::atomic<float>*      progress = nullptr; // 0..1 sau mỗi zone
    };

    struct Result {
        bool ok = false;
        std::string error;
        std::vector<int>         semitones;
        std::vector<double>      msPerZone;   // chỉ thời gian render (không tính ghi file)
        std::vector<float>       peakPerZone; // biên độ lớn nhất, > 1.0 nghĩa là sẽ clip nếu xuất 16/24-bit
        std::vector<float>       rmsDbPerZone;// RMS (dBFS) để so âm lượng giữa các zone
        float                    inputRmsDb = 0.0f;
        std::vector<std::string> files;       // đường dẫn tuyệt đối hoặc như outDir đã cho
        double msTotal = 0.0;                 // tổng thời gian render 13 zone
        double msSetup = 0.0;                 // configure() (cấp phát STFT), 1 lần
        double msWrite = 0.0;                 // ghi WAV
        int    inputLatency = 0, outputLatency = 0;
        int    blockSamples = 0, intervalSamples = 0;  // cấu hình STFT thực tế
        int64_t numSamples = 0;
        bool   cancelled = false;
        std::vector<std::vector<float>> audio; // khi Config::keepAudio
    };

    static Result run(const float* mono, int64_t numSamples, double sampleRate, const Config& cfg);

    // Danh sách nửa cung sẽ render theo cfg (rỗng nếu cấu hình không hợp lệ).
    static std::vector<int> zoneSemitones(const Config& cfg);

    // Tên file cho zone thứ `index` (theo thứ tự render): "z00_-18st_formant.wav", "z06_+00st_plain.wav"
    static std::string zoneFileName(int index, int semitones, bool formant);
};

} // namespace le::spike
