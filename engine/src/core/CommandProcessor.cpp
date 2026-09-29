#include "core/CommandProcessor.h"

#include <cstring>
#include <limits>

#include <cmath>

#include "core/Json.h"
#include "util/JsonValidator.h"

namespace le::core {

Reply Reply::job(std::int64_t id) {
    auto* r = new juce::DynamicObject();
    r->setProperty("jobId", (juce::int64) id);
    return ok(juce::var(r));
}

std::string CommandProcessor::envelope(const Reply& r) {
    return r.error == LE_OK ? json::ok(r.result) : json::error(r.error, r.message);
}

std::string CommandProcessor::call(const char* requestJson) const {
    if (requestJson == nullptr) return json::error(LE_ERR_INVALID_ARG, "request is null");
    // 05 §1: mọi chuỗi qua FFI là UTF-8. Kiểm TRƯỚC khi đưa vào juce::String (UTF-8 hỏng → jassert / ký tự rác).
    if (!juce::CharPointer_UTF8::isValidString(requestJson, std::numeric_limits<int>::max()))
        return json::error(LE_ERR_INVALID_ARG, "invalid UTF-8");
    // Cú pháp RFC 8259 + giới hạn (key rỗng, số cụt / quá dài, lồng > 32, > 1 MB, \u0000…) TRƯỚC juce::JSON::parse:
    // JUCE jassert / trả giá trị sai với các input đó (le-fuzz-api của 80). util::JsonValidator của 80.
    const auto chk = le::util::JsonValidator::validate(requestJson, std::strlen(requestJson));
    if (!chk.ok)
        return json::error(LE_ERR_INVALID_ARG,
                           std::string("invalid JSON request: ") + chk.reason + " (byte " + std::to_string(chk.errorOffset) + ")");
    juce::var req;
    const auto parsed = juce::JSON::parse(juce::String::fromUTF8(requestJson), req);
    if (parsed.failed() || !req.isObject()) return json::error(LE_ERR_INVALID_ARG, "invalid JSON request");
    const juce::var op = req.getProperty("op", {});
    if (!op.isString() || op.toString().isEmpty()) return json::error(LE_ERR_INVALID_ARG, "missing \"op\"");

    const auto name = op.toString().toStdString();
    const auto it = handlers_.find(name);
    if (it == handlers_.end()) return json::error(LE_ERR_NOT_IMPLEMENTED, "op not implemented: " + name);
    return envelope(it->second(req));
}

namespace args {

bool isNumber(const juce::var& v) { return v.isInt() || v.isInt64() || v.isDouble(); }

bool getInt(const juce::var& req, const char* key, int& out, int lo, int hi) {
    const juce::var v = req.getProperty(key, {});
    if (!isNumber(v)) return false;
    const double d = (double) v;
    if (!std::isfinite(d) || std::fabs(d - std::round(d)) > 1e-9 || d < lo || d > hi) return false;
    out = (int) std::lround(d);
    return true;
}

bool getInt64(const juce::var& req, const char* key, std::int64_t& out, std::int64_t lo, std::int64_t hi) {
    const juce::var v = req.getProperty(key, {});
    if (!isNumber(v)) return false;
    const double d = (double) v;
    if (!std::isfinite(d) || std::fabs(d - std::round(d)) > 1e-9 || d < (double) lo || d > (double) hi) return false;
    out = (std::int64_t) std::llround(d);
    return true;
}

bool getDouble(const juce::var& req, const char* key, double& out, double lo, double hi) {
    const juce::var v = req.getProperty(key, {});
    if (!isNumber(v)) return false;
    const double d = (double) v;
    if (!std::isfinite(d) || d < lo || d > hi) return false;
    out = d;
    return true;
}

bool getString(const juce::var& req, const char* key, std::string& out) {
    const juce::var v = req.getProperty(key, {});
    if (!v.isString()) return false;
    out = v.toString().toStdString();
    return true;
}

bool getBool(const juce::var& req, const char* key, bool& out) {
    const juce::var v = req.getProperty(key, {});
    if (!v.isBool()) return false;
    out = (bool) v;
    return true;
}

} // namespace args

} // namespace le::core
