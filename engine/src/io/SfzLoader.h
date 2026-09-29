// SfzLoader — đọc nhạc cụ SFZ (tập con ở docs/06 §4) thành dsp::Instrument (P1-27). [worker]
// Chạy trên worker thread (đọc file, cấp phát, std::function). KHÔNG gọi từ audio thread.
//
// Hỗ trợ:
//   <control>                 default_path
//   <global> <group> <region> sample lokey hikey key pitch_keycenter lovel hivel tune volume pan
//                             loop_mode (no_loop | one_shot | loop_continuous) loop_start loop_end
//                             ampeg_attack ampeg_decay ampeg_sustain ampeg_release group off_by
//   Kế thừa: <global> → <group> → <region> (mức sau ghi đè mức trước). Mỗi <group> mới bắt đầu lại
//   từ <global>. Phím nhận số MIDI hoặc tên nốt (c4 = 60, c#4 = db4 = 61). Comment // và /* */.
// Không hỗ trợ → bỏ qua + CẢNH BÁO (Result::warnings): opcode khác, header khác, #define, #include,
//   loop_sustain (xử lý như loop_continuous).
// LỖI (ok = false, không crash): thiếu file .sfz, thiếu file sample, region có lokey > hikey…
//
// Nạp sample: TIÊM từ ngoài vào qua Options::loadSample (io::SampleLoader). Engine truyền
// &io::decodeAudioFile (P1-11, của 68); test truyền hàm giả tự sinh sample. Mỗi file chỉ nạp 1 lần dù
// nhiều region dùng chung. Đường dẫn sample = thư mục .sfz / default_path / sample ('\' đổi thành '/').
#pragma once

#include "dsp/Instrument.h"
#include "io/AudioFileIO.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace le::io {

struct SfzLoadOptions {
    SampleLoader loadSample;                     // [worker] bắt buộc. Lỗi của loader được giữ nguyên mã
    const std::atomic<bool>* cancel = nullptr;   // true → dừng giữa chừng, trả LE_ERR_JOB_CANCELLED
};

struct SfzLoadResult {
    bool ok = false;
    int32_t error = 0;                  // LeError: LE_OK / FILE_NOT_FOUND / FILE_FORMAT / INVALID_ARG / JOB_CANCELLED
    std::string message;                // mô tả lỗi cho người đọc (tiếng Việt, có số dòng)
    std::vector<std::string> warnings;  // opcode/header bị bỏ qua…
    dsp::InstrumentPtr instrument;      // khi ok
    int regions = 0;                    // số zone đã tạo
    int samplesLoaded = 0;              // số file sample khác nhau đã nạp
};

// Một region đã kế thừa đủ giá trị, chưa nạp sample (Zone::data = nullptr).
struct SfzRegion {
    std::string samplePath;             // đã ghép thư mục + default_path, dấu '/'
    std::string sampleName;             // như trong file (để báo lỗi)
    dsp::Zone zone;
    int line = 0;                       // dòng của <region> trong file
};

struct SfzParseResult {
    bool ok = false;
    int32_t error = 0;
    std::string message;
    std::vector<SfzRegion> regions;
    std::vector<std::string> warnings;
};

// [worker] Chỉ phân tích text (không đọc sample). baseDir = thư mục chứa file .sfz ("" = thư mục hiện tại).
SfzParseResult parseSfz(std::string_view text, const std::string& baseDir);

// [worker] Phân tích text rồi nạp sample → Instrument. name dùng cho Instrument::name.
SfzLoadResult loadSfzText(std::string_view text, const std::string& baseDir, const std::string& name,
                          const SfzLoadOptions& options);

// [worker] Đọc file .sfz từ đĩa rồi như loadSfzText (baseDir = thư mục của file, name = tên file).
SfzLoadResult loadSfzFile(const std::string& sfzPath, const SfzLoadOptions& options);

// Tên nốt SFZ → số MIDI: "60" → 60, "c4" → 60, "C#4"/"db4" → 61, "a-1" → 9. Sai → −1.
int parseSfzNote(std::string_view s);

} // namespace le::io
