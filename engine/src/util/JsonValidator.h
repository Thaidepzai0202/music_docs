// JsonValidator — kiểm JSON NGHIÊM (RFC 8259) trước khi đưa cho juce::JSON::parse (05 §1, 29/09/2026). [NRT, hàm thuần]
//
// Vì sao cần: parser JSON của JUCE có jassert / UB với input mà le-fuzz-api tìm ra (tools/docs/fuzz-soak.md):
//   • key rỗng {"": 1} — JSON HỢP LỆ nhưng JUCE tạo Identifier("") → jassert (juce_Identifier.cpp:61)
//   • số cụt "-" ở cuối input → parseNumber đọc '\0' như chữ số → jassert (juce_JSON.cpp:253)
//   • phần nguyên ≥ 19 chữ số (vd 12345678901234567890 hay 123456789012345680000.0) → cộng dồn int64 tràn: UB
//   • lồng quá sâu → đệ quy không giới hạn
// CommandProcessor::call (68) gọi validate() sau bước kiểm UTF-8, TRƯỚC juce::JSON::parse; ok == false → INVALID_ARG.
//
// Chấp nhận: mọi JSON mà Dart jsonEncode sinh ra cho request của engine (object / mảng / chuỗi UTF-8 thô hoặc escape,
//   \uXXXX kể cả cặp surrogate, số nguyên / thực / mũ, true / false / null, khoảng trắng " \t\n\r").
// Từ chối (reason là chuỗi ASCII tĩnh — an toàn cho juce::String(const char*)):
//   cú pháp sai / cụt / rác sau giá trị · số sai dạng ("-", "1e", "01", "1.", ".5", "+1") · phần nguyên > 18 chữ số ·
//   KEY RỖNG · \u lẻ (thiếu hex) · surrogate lẻ (\ud800 không có \udc00…) · \u0000 ở BẤT KỲ chuỗi nào (key hay giá trị:
//   juce::String không chứa được NUL, JUCE báo "Unexpected EOF") · ký tự điều khiển thô trong
//   chuỗi · UTF-8 hỏng trong chuỗi (lẻ, cụt, overlong, surrogate mã hoá, > U+10FFFF) · lồng sâu > 32 · tổng > 1 MB ·
//   BOM / khoảng trắng không phải của JSON · dấu nháy đơn.
// Trùng key: hợp lệ theo RFC (JUCE giữ giá trị cuối) → chấp nhận.
#pragma once

#include <cstddef>

namespace le::util {

struct JsonCheck {
    bool        ok = false;
    size_t      errorOffset = 0;    // byte đầu tiên gây lỗi (== len nếu input bị cụt)
    const char* reason = "";        // ASCII tĩnh, "" khi ok
};

class JsonValidator {
public:
    static constexpr size_t kMaxBytes = 1u << 20;   // 1 MB
    static constexpr int    kMaxDepth = 32;         // object / mảng lồng nhau
    static constexpr int    kMaxIntegerDigits = 18; // phần nguyên (trước '.', 'e'): 19 chữ số đã có thể tràn int64 của JUCE

    // [any, NRT] Không cấp phát, không đọc quá len (không cần '\0' ở cuối). utf8 == nullptr → lỗi.
    static JsonCheck validate(const char* utf8, size_t len) noexcept;
};

} // namespace le::util
