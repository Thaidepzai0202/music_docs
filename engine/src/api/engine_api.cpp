// C API (engine_api.h) → le::core::Engine. Mọi hàm gọi từ main thread, trừ le_read_state (05 §1).
#include "le/engine_api.h"

#include <cstdlib>
#include <cstring>
#include <memory>

#include "core/Engine.h"
#include "core/Json.h"

namespace {

std::unique_ptr<le::core::Engine> g_engine;   // [main]

char* dupString(const std::string& s) {
    auto* out = static_cast<char*>(std::malloc(s.size() + 1));   // giải phóng bằng le_free_string
    if (out != nullptr) std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

} // namespace

extern "C" {

LE_EXPORT int32_t le_api_version(void) { return LE_API_VERSION; }

LE_EXPORT int32_t le_create(const LeConfig* config) {
    if (g_engine != nullptr) return LE_ERR_ALREADY_CREATED;
    const int32_t err = le::core::Engine::validate(config);
    if (err != LE_OK) return err;
    g_engine = std::make_unique<le::core::Engine>(*config);
    return LE_OK;
}

LE_EXPORT void le_destroy(void) { g_engine.reset(); }

LE_EXPORT int32_t le_audio_start(void) {
    return g_engine != nullptr ? g_engine->audioStart() : LE_ERR_NOT_CREATED;
}

LE_EXPORT void le_audio_stop(void) {
    if (g_engine != nullptr) g_engine->audioStop();
}

LE_EXPORT bool le_send(const LeCommand* cmd) {
    return g_engine != nullptr && cmd != nullptr && g_engine->send(*cmd);   // engine copy ngay, Dart dùng lại được cmd
}

LE_EXPORT char* le_call(const char* requestJson) {
    if (g_engine == nullptr) return dupString(le::core::json::error(LE_ERR_NOT_CREATED, "le_create has not been called"));
    return dupString(g_engine->call(requestJson));
}

LE_EXPORT void le_free_string(char* s) { std::free(s); }

LE_EXPORT void le_read_state(LeState* out) {
    if (out != nullptr) le::core::globalStatePublisher().read(*out);
}

LE_EXPORT void le_set_event_callback(LeEventCallback cb) { le::core::setEventCallback(cb); }

LE_EXPORT int32_t le_get_peaks(const char* clipId, int32_t level, float* outMinMax, int32_t maxPairs) {
    (void) clipId;
    (void) level;
    (void) outMinMax;
    (void) maxPairs;
    return LE_ERR_NOT_IMPLEMENTED;   // P1-24
}

} // extern "C"
