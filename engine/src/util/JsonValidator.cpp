#include "util/JsonValidator.h"

#include <cstdint>

namespace le::util {

namespace {

// Bộ đọc đệ quy xuống, dừng ở lỗi đầu tiên. Độ sâu đệ quy ≤ kMaxDepth nên stack có giới hạn.
class Parser {
public:
    Parser(const unsigned char* s, size_t n) : s_(s), n_(n) {}

    JsonCheck run() {
        skipWs();
        if (value(0)) {
            skipWs();
            if (p_ == n_) {
                JsonCheck ok;
                ok.ok = true;
                return ok;
            }
            error(p_, "trailing characters after JSON value");
        }
        return fail_;
    }

private:
    // Ghi lỗi ĐẦU TIÊN, luôn trả false để "return error(...)" dừng cả chuỗi đệ quy.
    bool error(size_t at, const char* why) {
        if (fail_.reason[0] == '\0') {
            fail_.ok = false;
            fail_.errorOffset = at;
            fail_.reason = why;
        }
        return false;
    }

    bool eof() const { return p_ >= n_; }
    unsigned char peek() const { return p_ < n_ ? s_[p_] : 0; }
    void skipWs() {
        while (p_ < n_ && (s_[p_] == ' ' || s_[p_] == '\t' || s_[p_] == '\n' || s_[p_] == '\r')) ++p_;
    }
    bool literal(const char* word) {
        const size_t at = p_;
        for (const char* w = word; *w != '\0'; ++w, ++p_)
            if (p_ >= n_ || s_[p_] != static_cast<unsigned char>(*w)) return error(p_ >= n_ ? n_ : at, "invalid literal");
        return true;
    }

    bool value(int depth) {
        if (eof()) return error(n_, "unexpected end of input");
        switch (peek()) {
            case '{': return object(depth + 1);
            case '[': return array(depth + 1);
            case '"': return string(false);
            case 't': return literal("true");
            case 'f': return literal("false");
            case 'n': return literal("null");
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) return number();
                return error(p_, peek() == '\'' ? "single-quoted strings are not JSON" : "unexpected character");
        }
    }

    bool object(int depth) {
        if (depth > JsonValidator::kMaxDepth) return error(p_, "nesting deeper than 32");
        ++p_;   // '{'
        skipWs();
        if (peek() == '}' && !eof()) {
            ++p_;
            return true;
        }
        for (;;) {
            skipWs();
            if (eof()) return error(n_, "unexpected end of input in object");
            if (peek() != '"') return error(p_, "expected a double-quoted key");
            if (!string(true)) return false;
            skipWs();
            if (eof()) return error(n_, "unexpected end of input in object");
            if (peek() != ':') return error(p_, "expected ':' after key");
            ++p_;
            skipWs();
            if (!value(depth)) return false;
            skipWs();
            if (eof()) return error(n_, "unexpected end of input in object");
            if (peek() == ',') {
                ++p_;
                continue;
            }
            if (peek() == '}') {
                ++p_;
                return true;
            }
            return error(p_, "expected ',' or '}' in object");
        }
    }

    bool array(int depth) {
        if (depth > JsonValidator::kMaxDepth) return error(p_, "nesting deeper than 32");
        ++p_;   // '['
        skipWs();
        if (peek() == ']' && !eof()) {
            ++p_;
            return true;
        }
        for (;;) {
            skipWs();
            if (!value(depth)) return false;
            skipWs();
            if (eof()) return error(n_, "unexpected end of input in array");
            if (peek() == ',') {
                ++p_;
                continue;
            }
            if (peek() == ']') {
                ++p_;
                return true;
            }
            return error(p_, "expected ',' or ']' in array");
        }
    }

    // Số RFC 8259: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?, phần nguyên ≤ kMaxIntegerDigits chữ số.
    bool number() {
        if (peek() == '-') ++p_;
        if (eof()) return error(n_, "number ends after '-'");
        if (peek() == '0') {
            ++p_;
            if (!eof() && peek() >= '0' && peek() <= '9') return error(p_, "leading zero in number");
        } else if (peek() >= '1' && peek() <= '9') {
            const size_t digits0 = p_;
            while (!eof() && peek() >= '0' && peek() <= '9') ++p_;
            if (p_ - digits0 > static_cast<size_t>(JsonValidator::kMaxIntegerDigits))
                return error(digits0, "integer part longer than 18 digits");
        } else {
            return error(p_, "expected digit in number");
        }
        if (!eof() && peek() == '.') {
            ++p_;
            if (eof() || peek() < '0' || peek() > '9') return error(eof() ? n_ : p_, "expected digit after '.'");
            while (!eof() && peek() >= '0' && peek() <= '9') ++p_;
        }
        if (!eof() && (peek() == 'e' || peek() == 'E')) {
            ++p_;
            if (!eof() && (peek() == '+' || peek() == '-')) ++p_;
            if (eof() || peek() < '0' || peek() > '9') return error(eof() ? n_ : p_, "expected digit in exponent");
            while (!eof() && peek() >= '0' && peek() <= '9') ++p_;
        }
        return true;
    }

    int hex4(size_t at) const {   // −1 nếu không đủ 4 chữ số hex
        if (at + 4 > n_) return -1;
        int v = 0;
        for (size_t i = at; i < at + 4; ++i) {
            const unsigned char c = s_[i];
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return -1;
            v = v * 16 + d;
        }
        return v;
    }

    // Chuỗi: escape hợp lệ, cặp surrogate đủ đôi, UTF-8 đúng chuẩn, không ký tự điều khiển thô.
    // key: không rỗng, không chứa \u0000.
    bool string(bool key) {
        const size_t open = p_++;   // '"'
        size_t chars = 0;
        for (;;) {
            if (eof()) return error(n_, "unterminated string");
            const unsigned char c = s_[p_];
            if (c == '"') {
                ++p_;
                if (key && chars == 0) return error(open, "empty object key");
                return true;
            }
            if (c < 0x20) return error(p_, "raw control character in string");
            if (c == '\\') {
                if (p_ + 1 >= n_) return error(n_, "unterminated escape");
                const unsigned char e = s_[p_ + 1];
                if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't') {
                    p_ += 2;
                    ++chars;
                    continue;
                }
                if (e != 'u') return error(p_, "invalid escape");
                const int u = hex4(p_ + 2);
                if (u < 0) return error(p_, "\\u needs 4 hex digits");
                if (u >= 0xDC00 && u <= 0xDFFF) return error(p_, "lone low surrogate escape");
                if (u >= 0xD800 && u <= 0xDBFF) {   // phải có \uDC00..\uDFFF ngay sau
                    const int lo = (p_ + 7 < n_ && s_[p_ + 6] == '\\' && s_[p_ + 7] == 'u') ? hex4(p_ + 8) : -1;
                    if (lo < 0xDC00 || lo > 0xDFFF) return error(p_, "lone high surrogate escape");
                    p_ += 12;
                } else {
                    // juce::String không chứa được NUL: JUCE báo "Unexpected EOF in string constant" → chặn ở đây, lý do rõ
                    if (u == 0) return error(p_, key ? "\\u0000 in object key" : "\\u0000 in string (engine strings cannot hold NUL)");
                    p_ += 6;
                }
                ++chars;
                continue;
            }
            if (c < 0x80) {
                ++p_;
                ++chars;
                continue;
            }
            // UTF-8 nhiều byte (RFC 3629): chặn overlong, surrogate D800–DFFF, > U+10FFFF
            int len;
            uint32_t cp;
            if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1Fu; }
            else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0Fu; }
            else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07u; }
            else return error(p_, "invalid UTF-8 lead byte");
            if (p_ + static_cast<size_t>(len) > n_) return error(p_, "truncated UTF-8 sequence");
            for (int k = 1; k < len; ++k) {
                const unsigned char cc = s_[p_ + static_cast<size_t>(k)];
                if ((cc & 0xC0u) != 0x80u) return error(p_, "invalid UTF-8 continuation byte");
                cp = (cp << 6) | (cc & 0x3Fu);
            }
            if ((len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000)) return error(p_, "overlong UTF-8 sequence");
            if (cp >= 0xD800 && cp <= 0xDFFF) return error(p_, "UTF-8 encoded surrogate");
            if (cp > 0x10FFFF) return error(p_, "code point above U+10FFFF");
            p_ += static_cast<size_t>(len);
            ++chars;
        }
    }

    const unsigned char* s_;
    size_t n_;
    size_t p_ = 0;
    JsonCheck fail_{false, 0, ""};
};

} // namespace

JsonCheck JsonValidator::validate(const char* utf8, size_t len) noexcept {
    JsonCheck r;
    if (utf8 == nullptr) {
        r.reason = "null input";
        return r;
    }
    if (len > kMaxBytes) {
        r.errorOffset = kMaxBytes;
        r.reason = "request larger than 1 MB";
        return r;
    }
    return Parser(reinterpret_cast<const unsigned char*>(utf8), len).run();
}

} // namespace le::util
