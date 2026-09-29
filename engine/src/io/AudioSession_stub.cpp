// macOS / nền tảng khác: không có AVAudioSession, mọi hàm là no-op.
#include "io/AudioSession.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !(defined(__APPLE__) && TARGET_OS_IPHONE)

namespace le::io::session {

Counters& counters() {
    static Counters c;
    return c;
}

bool applyCategory(Mode, bool) { return true; }
RouteInfo currentRoute() { return {}; }
void installObservers() {}
void removeObservers() {}
std::string describeJson() { return "{}"; }
bool isSupported() { return false; }
std::string lastError() { return {}; }

} // namespace le::io::session

#endif
