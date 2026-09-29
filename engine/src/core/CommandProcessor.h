#pragma once
// CommandProcessor (P1-05): đường lệnh cấu trúc `le_call` (03 §3.1).
// Bảng dispatch op → handler. Handler nhận request đã parse, trả Reply. Envelope JSON (05 §3) làm ở một chỗ:
//   thành công {"ok":true,"result":{...}}, lỗi {"ok":false,"error":{"code","message"}}.
// [main] Mọi thứ ở đây chạy trên main thread.
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include <juce_core/juce_core.h>

#include "le/engine_api.h"

namespace le::core {

struct Reply {
    std::int32_t error = LE_OK;
    std::string message;
    juce::var result;   // void → {}

    static Reply ok(juce::var r = {}) { return {LE_OK, {}, std::move(r)}; }
    static Reply fail(std::int32_t err, std::string msg) { return {err, std::move(msg), {}}; }
    static Reply job(std::int64_t id);   // {"jobId": id}
};

class CommandProcessor {
public:
    using Handler = std::function<Reply(const juce::var& request)>;

    void add(const std::string& op, Handler h) { handlers_[op] = std::move(h); }
    bool has(const std::string& op) const { return handlers_.count(op) != 0; }

    // Parse → dispatch → envelope. JSON hỏng / thiếu op → INVALID_ARG; op chưa có handler → NOT_IMPLEMENTED.
    std::string call(const char* requestJson) const;

    static std::string envelope(const Reply& r);

private:
    std::unordered_map<std::string, Handler> handlers_;
};

// ── Tiện ích đọc tham số (trả false nếu thiếu hoặc sai kiểu) ──
namespace args {
bool getInt(const juce::var& req, const char* key, int& out, int lo, int hi);
// Số nguyên trong [lo, hi] (hi ≤ 2^53: double biểu diễn đúng). Kiểm TRƯỚC khi ép kiểu → không UB với input lạ.
bool getInt64(const juce::var& req, const char* key, std::int64_t& out, std::int64_t lo, std::int64_t hi);
bool getDouble(const juce::var& req, const char* key, double& out, double lo, double hi);
bool getString(const juce::var& req, const char* key, std::string& out);
bool getBool(const juce::var& req, const char* key, bool& out);
bool isNumber(const juce::var& v);
} // namespace args

} // namespace le::core
