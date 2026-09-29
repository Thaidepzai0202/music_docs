// util::JsonValidator (05 §1, 29/09/2026): chấp nhận mọi JSON kiểu Dart jsonEncode, từ chối đúng những thứ làm parser
// JUCE assert / UB (key rỗng, số cụt, phần nguyên quá dài, lồng sâu), escape / surrogate / UTF-8 hỏng, quá 1 MB.
// Mọi input được chấp nhận đều phải được juce::JSON::parse đọc thành công (đối chiếu có seed).
#include <catch2/catch_test_macros.hpp>

#include "util/JsonValidator.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using le::util::JsonCheck;
using le::util::JsonValidator;

namespace {

JsonCheck check(const std::string& s) { return JsonValidator::validate(s.data(), s.size()); }

bool juceParses(const std::string& s) {
    juce::var v;
    return juce::JSON::parse(juce::String::fromUTF8(s.data(), static_cast<int>(s.size())), v).wasOk();
}

std::string nested(int depth, char open, char close, const std::string& inner) {
    std::string s;
    for (int i = 0; i < depth; ++i) s += open == '{' ? std::string("{\"k\":") : std::string(1, open);
    s += inner;
    for (int i = 0; i < depth; ++i) s += close;
    return s;
}

// Sinh JSON HỢP LỆ ngẫu nhiên kiểu Dart jsonEncode (tất định, LCG)
struct Gen {
    uint64_t s;
    uint32_t next() { return static_cast<uint32_t>((s = s * 6364136223846793005ull + 1442695040888963407ull) >> 33); }
    int below(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
    std::string str() {
        static const char* parts[] = {"a", "Trống", "🥁", "\\\"", "\\\\", "\\n", "\\u0001", "\\ud83e\\udd41", "é", "音", " ", "\\/", "x1"};
        std::string r = "\"";
        const int n = below(5);
        for (int i = 0; i < n; ++i) r += parts[below(13)];
        return r + "\"";
    }
    std::string key() {
        std::string k = str();
        return k.size() == 2 ? std::string("\"k\"") : k;   // key rỗng không được sinh
    }
    std::string num() {
        static const char* nums[] = {"0", "-0", "1", "-1", "0.5", "-0.0", "1e+22", "1.5e-7", "2.5E10", "123456789012345678",
                                     "3.141592653589793", "100", "-120.0", "1e999", "0.000001"};
        return nums[below(15)];
    }
    std::string value(int depth) {
        const int k = depth > 6 ? below(4) : below(7);
        switch (k) {
            case 0: return str();
            case 1: return num();
            case 2: return below(2) ? "true" : "false";
            case 3: return "null";
            case 4: {
                std::string r = "[";
                const int n = below(4);
                for (int i = 0; i < n; ++i) r += (i ? "," : "") + value(depth + 1);
                return r + "]";
            }
            default: {
                std::string r = "{";
                const int n = below(4);
                for (int i = 0; i < n; ++i) r += (i ? ", " : "") + key() + ": " + value(depth + 1);
                return r + "}";
            }
        }
    }
};

} // namespace

TEST_CASE("JsonValidator: chấp nhận JSON kiểu Dart jsonEncode", "[util][json]") {
    const std::vector<std::string> ok = {
        R"({"op":"engine.info"})",
        R"({"op":"clip.setMidi","track":0,"slot":1,"clipId":"m1","lengthBeats":4.0,"notes":[{"p":60,"v":100,"s":0.0,"d":0.5},{"p":64,"v":90,"s":1.25,"d":0.25}]})",
        R"({"op":"fx.set","track":2,"index":0,"type":"comp","params":{"0":-18.0,"1":4},"bypass":false})",
        R"({"op":"track.configure","track":3,"kind":"audio","name":"Trống cơm – Nguyễn Thị Hương 🥁"})",   // UTF-8 thô (Dart)
        "{\"name\":\"Tro\xcc\x82\xcc\x81ng\"}",                                                          // NFD
        R"({"s":"\"\\\/\b\f\n\r\t","u":"\u0001\u001F\u00e9\u1ED1","pair":"\ud83e\udd41"})",
        R"({"n":[0,-0,1,-1,0.5,-0.0,1e+22,1.5e-7,2.5E10,123456789012345678,-123456789012345678,3.141592653589793,1E-5]})",
        R"({"a":true,"b":false,"c":null,"d":{},"e":[],"f":[[],{}]})",
        R"({"dup":1,"dup":2})",                                                                        // trùng key: hợp lệ
        "{\n  \"op\": \"engine.info\",\n  \"list\": [\n    1,\n    2\n  ]\n}",                          // withIndent
        " \t\r\n{} \t\r\n", "[]", "[1,\"x\",null]", "\"chuỗi\"", "42", "true",
        "{\"max\":\"\xf4\x8f\xbf\xbf\"}",                                                              // U+10FFFF
    };
    for (const std::string& s : ok) {
        CAPTURE(s);
        const JsonCheck r = check(s);
        INFO("reason " << r.reason << " @" << r.errorOffset);
        CHECK(r.ok);
        CHECK(std::string(r.reason).empty());
        if (s[0] == '{' || s[0] == '[' || s[1] == '{') CHECK(juceParses(s));
    }
    CHECK(check(nested(32, '[', ']', "1")).ok);            // đúng 32 cấp
    CHECK(check(nested(32, '{', '}', "1")).ok);
}

TEST_CASE("JsonValidator: từ chối cú pháp sai, số sai dạng, key rỗng — kèm vị trí và lý do", "[util][json]") {
    struct Bad { std::string s; size_t at; const char* why; };
    const std::vector<Bad> bad = {
        {"", 0, "unexpected end of input"},
        {"{", 1, "unexpected end of input in object"},
        {"[1,", 3, "unexpected end of input"},
        {R"({"a")", 4, "unexpected end of input in object"},
        {R"({"a":)", 5, "unexpected end of input"},
        {R"({"a":1)", 6, "unexpected end of input in object"},
        {R"("abc)", 4, "unterminated string"},
        {R"({"op": "track.setInstrument", "track": -)", 40, "number ends after '-'"},   // seed 27 / 158
        {R"({"op": "clip.info", ""track": "x", "slot": 5})", 20, "empty object key"},  // seed 5 / 298
        {R"({"":1})", 1, "empty object key"},
        {R"({"a":{"b":[{"":0}]}})", 12, "empty object key"},
        {"[-]", 2, "expected digit in number"},
        {"[1e]", 3, "expected digit in exponent"},
        {"[1e+]", 4, "expected digit in exponent"},
        {"[01]", 2, "leading zero in number"},
        {"[-01]", 3, "leading zero in number"},
        {"[1.]", 3, "expected digit after '.'"},
        {"[.5]", 1, "unexpected character"},
        {"[+1]", 1, "unexpected character"},
        {"[1234567890123456789]", 1, "integer part longer than 18 digits"},     // tràn int64 trong JUCE
        {"[-1234567890123456789.5]", 2, "integer part longer than 18 digits"},
        {"[123456789012345680000.0]", 1, "integer part longer than 18 digits"},  // Dart in double lớn kiểu này
        {"{} x", 3, "trailing characters after JSON value"},
        {R"({"op":"engine.info"}{"op":"engine.info"})", 20, "trailing characters after JSON value"},
        {"{'op':'engine.info'}", 1, "expected a double-quoted key"},
        {"['x']", 1, "single-quoted strings are not JSON"},
        {"[NaN]", 1, "unexpected character"},
        {"[tru]", 1, "invalid literal"},
        {"\xef\xbb\xbf{}", 0, "unexpected character"},                           // BOM
        {"\x0b{}", 0, "unexpected character"},                                   // VT không phải khoảng trắng JSON
        {"{\"a\":1,}", 7, "expected a double-quoted key"},
        {"[1 2]", 3, "expected ',' or ']' in array"},
    };
    for (const Bad& b : bad) {
        CAPTURE(b.s);
        const JsonCheck r = check(b.s);
        CHECK_FALSE(r.ok);
        CHECK(std::string(r.reason) == b.why);
        CHECK(r.errorOffset == b.at);
    }
    CHECK(check("[123456789012345678]").ok);   // 18 chữ số: được
    CHECK(check("[0.12345678901234567890123]").ok);   // phần thập phân dài: JUCE đọc bằng readDoubleValue, an toàn
    const JsonCheck n = JsonValidator::validate(nullptr, 0);
    CHECK_FALSE(n.ok);
    CHECK(std::string(n.reason) == "null input");
}

TEST_CASE("JsonValidator: escape, surrogate, \\u0000 trong key, ký tự điều khiển, UTF-8 hỏng", "[util][json][utf8]") {
    struct Bad { std::string s; const char* why; };
    const std::vector<Bad> bad = {
        {R"(["\x"])", "invalid escape"},
        {R"(["\u12"])", "\\u needs 4 hex digits"},
        {R"(["\u12g4"])", "\\u needs 4 hex digits"},
        {R"(["\ud800"])", "lone high surrogate escape"},
        {R"(["\ud800x"])", "lone high surrogate escape"},
        {R"(["\ud800\u0041"])", "lone high surrogate escape"},
        {R"(["\ud83d"])", "lone high surrogate escape"},
        {R"(["\udc00x"])", "lone low surrogate escape"},
        {R"({"\u0000":1})", "\\u0000 in object key"},
        {R"({"a\u0000b":1})", "\\u0000 in object key"},
        {"[\"a\nb\"]", "raw control character in string"},
        {"[\"a\tb\"]", "raw control character in string"},
        {"[\"\x80\"]", "invalid UTF-8 lead byte"},
        {"[\"abc\xbf\"]", "invalid UTF-8 lead byte"},
        {"[\"\xc3\"]", "invalid UTF-8 continuation byte"},
        {"[\"\xe2\x82\"]", "invalid UTF-8 continuation byte"},
        {"[\"\xc0\xaf\"]", "invalid UTF-8 lead byte"},
        {"[\"\xe0\x80\xaf\"]", "overlong UTF-8 sequence"},
        {"[\"\xf0\x80\x80\xaf\"]", "overlong UTF-8 sequence"},
        {"[\"\xed\xa0\x80\"]", "UTF-8 encoded surrogate"},
        {"[\"\xed\xbf\xbf\"]", "UTF-8 encoded surrogate"},
        {"[\"\xf4\x90\x80\x80\"]", "code point above U+10FFFF"},
        {"[\"\xf5\x80\x80\x80\"]", "invalid UTF-8 lead byte"},
        {"[\"\xfe\xff\"]", "invalid UTF-8 lead byte"},
        {"[\"Tr\xe1\xbb\"]", "invalid UTF-8 continuation byte"},
        {std::string("[\"\xe1\xbb", 4), "truncated UTF-8 sequence"},
    };
    for (const Bad& b : bad) {
        CAPTURE(b.s);
        const JsonCheck r = check(b.s);
        CHECK_FALSE(r.ok);
        CHECK(std::string(r.reason) == b.why);
    }
    CHECK(check(R"(["\uD83E\uDD41 \u00E9"])").ok);   // chữ hoa hex
    // NUL trong GIÁ TRỊ: JSON hợp lệ nhưng juce::String không chứa được → từ chối với lý do rõ (JUCE sẽ báo lỗi mơ hồ)
    const JsonCheck nul = check(R"(["a\u0000b"])");
    CHECK_FALSE(nul.ok);
    CHECK(std::string(nul.reason) == "\\u0000 in string (engine strings cannot hold NUL)");
}

TEST_CASE("JsonValidator: giới hạn độ sâu 32 và kích thước 1 MB", "[util][json]") {
    const JsonCheck deepA = check(nested(33, '[', ']', "1"));
    CHECK_FALSE(deepA.ok);
    CHECK(std::string(deepA.reason) == "nesting deeper than 32");
    CHECK(deepA.errorOffset == 32);
    CHECK_FALSE(check(nested(33, '{', '}', "1")).ok);
    CHECK_FALSE(check(nested(100000, '[', ']', "")).ok);   // không đệ quy tới 100 000 cấp (dừng ở 33)

    std::string big = "[\"" + std::string(JsonValidator::kMaxBytes - 4, 'x') + "\"]";
    REQUIRE(big.size() == JsonValidator::kMaxBytes);
    CHECK(check(big).ok);                                  // đúng 1 MB: được
    big.insert(2, "y");
    const JsonCheck tooBig = check(big);
    CHECK_FALSE(tooBig.ok);
    CHECK(std::string(tooBig.reason) == "request larger than 1 MB");
}

TEST_CASE("JsonValidator: JSON hợp lệ ngẫu nhiên (kiểu Dart) luôn được nhận và JUCE đọc được; bản đột biến không làm crash", "[util][json]") {
    Gen g{20260929};
    int accepted = 0, mutatedRejected = 0, mutatedAccepted = 0;
    for (int i = 0; i < 3000; ++i) {
        const std::string doc = "{\"op\": \"x\", \"v\": " + g.value(0) + "}";
        const JsonCheck r = check(doc);
        INFO(doc << " → " << r.reason << " @" << r.errorOffset);
        REQUIRE(r.ok);
        REQUIRE(juceParses(doc));
        ++accepted;
        // Đột biến: cắt / chèn / xoá 1 byte. Được nhận thì JUCE phải đọc được; bị từ chối thì thôi (không đưa cho JUCE).
        std::string m = doc;
        const size_t at = static_cast<size_t>(g.below(static_cast<int>(m.size())));
        switch (g.below(3)) {
            case 0: m.resize(at); break;
            case 1: m.insert(at, 1, "{}[],:\"\\-0e.\x80"[g.below(14)]); break;
            default: m.erase(at, 1); break;
        }
        const JsonCheck mr = check(m);
        if (mr.ok) {
            ++mutatedAccepted;
            INFO("đột biến được nhận: " << m);
            CHECK(juceParses(m));
        } else {
            ++mutatedRejected;
            CHECK(mr.errorOffset <= m.size());
        }
    }
    CHECK(accepted == 3000);
    CHECK(mutatedRejected > 1000);
    CHECK(mutatedAccepted > 0);
}
