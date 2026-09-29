#include "core/Json.h"

#include "le/engine_api.h"

namespace le::core::json {

const char* errorCodeName(std::int32_t err) {
    switch (err) {
        case LE_OK: return "OK";
        case LE_ERR_INVALID_ARG: return "INVALID_ARG";
        case LE_ERR_NOT_CREATED: return "NOT_CREATED";
        case LE_ERR_ALREADY_CREATED: return "ALREADY_CREATED";
        case LE_ERR_API_VERSION: return "API_VERSION";
        case LE_ERR_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case LE_ERR_AUDIO_DEVICE: return "AUDIO_DEVICE";
        case LE_ERR_MIC_PERMISSION: return "MIC_PERMISSION";
        case LE_ERR_FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case LE_ERR_FILE_FORMAT: return "FILE_FORMAT";
        case LE_ERR_DISK_FULL: return "DISK_FULL";
        case LE_ERR_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case LE_ERR_QUEUE_FULL: return "QUEUE_FULL";
        case LE_ERR_JOB_CANCELLED: return "JOB_CANCELLED";
        case LE_ERR_JOB_NOT_FOUND: return "JOB_NOT_FOUND";
        case LE_ERR_PITCH_NOT_DETECTED: return "PITCH_NOT_DETECTED";
        case LE_ERR_INTERNAL: return "INTERNAL";
        default: return "INTERNAL";
    }
}

std::string ok(const juce::var& result) {
    auto* o = new juce::DynamicObject();   // var giữ refcount, tự giải phóng
    o->setProperty("ok", true);
    o->setProperty("result", result.isVoid() ? juce::var(new juce::DynamicObject()) : result);
    return juce::JSON::toString(juce::var(o), true).toStdString();
}

std::string error(std::int32_t err, const std::string& message) {
    auto* e = new juce::DynamicObject();
    e->setProperty("code", errorCodeName(err));
    e->setProperty("message", juce::String::fromUTF8(message.c_str()));
    auto* o = new juce::DynamicObject();
    o->setProperty("ok", false);
    o->setProperty("error", juce::var(e));
    return juce::JSON::toString(juce::var(o), true).toStdString();
}

} // namespace le::core::json
