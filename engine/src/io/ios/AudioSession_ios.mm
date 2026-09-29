// [main] AVAudioSession cho iOS. Biên dịch với -fobjc-arc (CMakeLists).
// KHÔNG gọi file này từ audio thread (Obj-C runtime có lock, autorelease).
#include "io/AudioSession.h"

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#include <sstream>

namespace le::io::session {
namespace {

std::string g_lastError;
NSMutableArray* g_observers = nil;   // ARC: tham chiếu mạnh tới các token observer

// iOS 26 SDK đổi tên AllowBluetooth → AllowBluetoothHFP (cùng giá trị). Chỉ dùng để ĐỌC/báo cáo.
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
constexpr AVAudioSessionCategoryOptions kOptionHFP = AVAudioSessionCategoryOptionAllowBluetoothHFP;
#else
constexpr AVAudioSessionCategoryOptions kOptionHFP = AVAudioSessionCategoryOptionAllowBluetooth;
#endif

std::string toStd(NSString* s) { return s != nil ? std::string([s UTF8String]) : std::string(); }

bool isWiredPort(NSString* t) {
    return [t isEqualToString:AVAudioSessionPortHeadphones] || [t isEqualToString:AVAudioSessionPortLineOut]
        || [t isEqualToString:AVAudioSessionPortUSBAudio]   || [t isEqualToString:AVAudioSessionPortHDMI]
        || [t isEqualToString:AVAudioSessionPortLineIn]     || [t isEqualToString:AVAudioSessionPortHeadsetMic];
}

bool isBluetoothPort(NSString* t) {
    return [t isEqualToString:AVAudioSessionPortBluetoothA2DP] || [t isEqualToString:AVAudioSessionPortBluetoothHFP]
        || [t isEqualToString:AVAudioSessionPortBluetoothLE];
}

void appendPorts(std::ostringstream& os, NSArray<AVAudioSessionPortDescription*>* ports) {
    os << "[";
    bool first = true;
    for (AVAudioSessionPortDescription* p in ports) {
        os << (first ? "" : ",") << "{\"type\":\"" << toStd(p.portType) << "\",\"name\":\"" << toStd(p.portName) << "\"}";
        first = false;
    }
    os << "]";
}

} // namespace

Counters& counters() {
    static Counters c;
    return c;
}

bool isSupported() { return true; }
std::string lastError() { return g_lastError; }

bool applyCategory(Mode mode, bool withInput) {
    AVAudioSession* s = [AVAudioSession sharedInstance];

    // Giữ MixWithOthers như mặc định của JUCE (chạy chung với app nhạc khác, cần cho Link sau này).
    AVAudioSessionCategoryOptions opts = AVAudioSessionCategoryOptionMixWithOthers;
    NSString* category = AVAudioSessionCategoryPlayback;
    if (withInput) {
        category = AVAudioSessionCategoryPlayAndRecord;
        // Không có AllowBluetoothHFP/AllowBluetooth: HFP kéo SR xuống 8–16 kHz, chất lượng kiểu cuộc gọi.
        opts |= AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetoothA2DP;
    }
    NSString* m = (mode == Mode::Measurement) ? AVAudioSessionModeMeasurement : AVAudioSessionModeDefault;

    NSError* err = nil;
    if (![s setCategory:category mode:m options:opts error:&err]) {
        g_lastError = toStd(err.localizedDescription);
        return false;
    }
    g_lastError.clear();
    return true;
}

RouteInfo currentRoute() {
    RouteInfo r;
    AVAudioSessionRouteDescription* route = [AVAudioSession sharedInstance].currentRoute;
    for (AVAudioSessionPortDescription* p in route.outputs) {
        if (isWiredPort(p.portType)) r.wired = true;
        if (isBluetoothPort(p.portType)) r.bluetooth = true;
    }
    for (AVAudioSessionPortDescription* p in route.inputs) {
        if (isWiredPort(p.portType)) r.wired = true;
        if (isBluetoothPort(p.portType)) r.bluetooth = true;
    }
    return r;
}

void installObservers() {
    removeObservers();
    NSNotificationCenter* centre = [NSNotificationCenter defaultCenter];
    AVAudioSession* s = [AVAudioSession sharedInstance];
    g_observers = [NSMutableArray array];

    // Block chỉ tăng atomic, không làm gì khác: notification có thể đến từ thread phụ.
    [g_observers addObject:[centre addObserverForName:AVAudioSessionInterruptionNotification object:s queue:nil
                                           usingBlock:^(NSNotification* n) {
        NSNumber* type = n.userInfo[AVAudioSessionInterruptionTypeKey];
        if (type != nil && type.unsignedIntegerValue == AVAudioSessionInterruptionTypeBegan)
            counters().interruptionBegan.fetch_add(1, std::memory_order_relaxed);
        else
            counters().interruptionEnded.fetch_add(1, std::memory_order_relaxed);
    }]];

    [g_observers addObject:[centre addObserverForName:AVAudioSessionRouteChangeNotification object:s queue:nil
                                           usingBlock:^(NSNotification* n) {
        NSNumber* reason = n.userInfo[AVAudioSessionRouteChangeReasonKey];
        const auto r = reason != nil ? (AVAudioSessionRouteChangeReason) reason.unsignedIntegerValue
                                     : AVAudioSessionRouteChangeReasonUnknown;
        // Chính engine đổi category sau khi JUCE mở device → bỏ qua, không phải người dùng cắm/rút gì.
        if (r == AVAudioSessionRouteChangeReasonCategoryChange) return;
        counters().routeChanged.fetch_add(1, std::memory_order_relaxed);
    }]];

    [g_observers addObject:[centre addObserverForName:AVAudioSessionMediaServicesWereResetNotification object:s queue:nil
                                           usingBlock:^(NSNotification*) {
        counters().mediaServicesReset.fetch_add(1, std::memory_order_relaxed);
    }]];

    // P4-17: hệ thống sắp thiếu RAM → main (pump) nhả cache của engine trước khi bị jetsam kill.
    [g_observers addObject:[centre addObserverForName:UIApplicationDidReceiveMemoryWarningNotification object:nil queue:nil
                                           usingBlock:^(NSNotification*) {
        counters().memoryWarnings.fetch_add(1, std::memory_order_relaxed);
    }]];
}

void removeObservers() {
    if (g_observers == nil) return;
    NSNotificationCenter* centre = [NSNotificationCenter defaultCenter];
    for (id token in g_observers) [centre removeObserver:token];
    g_observers = nil;
}

std::string describeJson() {
    AVAudioSession* s = [AVAudioSession sharedInstance];
    const NSUInteger o = s.categoryOptions;
    std::ostringstream os;
    os << "{\"category\":\"" << toStd(s.category) << "\""
       << ",\"mode\":\"" << toStd(s.mode) << "\""
       << ",\"options\":{"
       << "\"mixWithOthers\":" << ((o & AVAudioSessionCategoryOptionMixWithOthers) ? "true" : "false")
       << ",\"defaultToSpeaker\":" << ((o & AVAudioSessionCategoryOptionDefaultToSpeaker) ? "true" : "false")
       << ",\"allowBluetoothA2DP\":" << ((o & AVAudioSessionCategoryOptionAllowBluetoothA2DP) ? "true" : "false")
       << ",\"allowBluetoothHFP\":" << ((o & kOptionHFP) ? "true" : "false")
       << ",\"allowAirPlay\":" << ((o & AVAudioSessionCategoryOptionAllowAirPlay) ? "true" : "false")
       << ",\"raw\":" << (unsigned long) o << "}"
       << ",\"sampleRate\":" << s.sampleRate
       << ",\"ioBufferDuration\":" << s.IOBufferDuration
       << ",\"inputLatency\":" << s.inputLatency
       << ",\"outputLatency\":" << s.outputLatency
       << ",\"inputs\":";
    appendPorts(os, s.currentRoute.inputs);
    os << ",\"outputs\":";
    appendPorts(os, s.currentRoute.outputs);
    os << "}";
    return os.str();
}

} // namespace le::io::session

#endif // TARGET_OS_IPHONE
