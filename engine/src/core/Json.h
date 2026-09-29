#pragma once
// Envelope JSON cho le_call (05 §3): {"ok":true,"result":{...}} | {"ok":false,"error":{"code","message"}}
#include <cstdint>
#include <string>

#include <juce_core/juce_core.h>

namespace le::core::json {

const char* errorCodeName(std::int32_t err);   // LE_ERR_FILE_NOT_FOUND → "FILE_NOT_FOUND"

std::string ok(const juce::var& result);
std::string error(std::int32_t err, const std::string& message);

} // namespace le::core::json
