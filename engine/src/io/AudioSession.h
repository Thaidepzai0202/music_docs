#pragma once
// Lớp mỏng quanh AVAudioSession (chỉ iOS). Trên macOS mọi hàm là no-op.
// Cấu hình của JUCE và lý do phải đặt lại: engine/docs/audio-session.md
#include <atomic>
#include <cstdint>
#include <string>

namespace le::io::session {

enum class Mode : std::int32_t { Default = 0, Measurement = 1 };

struct RouteInfo {
    bool wired = false;       // tai nghe có dây, line out, USB/Lightning interface
    bool bluetooth = false;   // A2DP / HFP / LE
};

// Bộ đếm do notification của hệ thống tăng lên (có thể từ thread bất kỳ). Main thread so sánh với
// giá trị đã thấy lần trước. Là biến tĩnh sống suốt đời process, nên block Obj-C chạy muộn sau khi
// Engine bị huỷ vẫn ghi vào vùng nhớ hợp lệ.
struct Counters {
    std::atomic<std::uint32_t> interruptionBegan{0};
    std::atomic<std::uint32_t> interruptionEnded{0};
    std::atomic<std::uint32_t> routeChanged{0};
    std::atomic<std::uint32_t> mediaServicesReset{0};
    std::atomic<std::uint32_t> memoryWarnings{0};   // UIApplicationDidReceiveMemoryWarningNotification (P4-17)
};
Counters& counters();

// [main] Đặt category playAndRecord + defaultToSpeaker + allowBluetoothA2DP + mixWithOthers
// (KHÔNG HFP) và mode. Phải gọi SAU mỗi lần JUCE mở device, vì JUCE tự đặt category có HFP trong open().
// withInput=false → category playback. Trả false nếu hệ thống từ chối (xem lastError()).
bool applyCategory(Mode mode, bool withInput);

// [main] Route hiện tại.
RouteInfo currentRoute();

// [main] Đăng ký / gỡ observer cho interruption, route change, media services reset.
void installObservers();
void removeObservers();

// [main] Cấu hình session hiện tại (category, options, mode, SR, IO buffer, latency, route) dạng JSON.
std::string describeJson();

bool isSupported();           // true trên iOS
std::string lastError();

} // namespace le::io::session
