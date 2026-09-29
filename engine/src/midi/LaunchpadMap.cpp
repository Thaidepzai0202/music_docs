#include "midi/LaunchpadMap.h"

#include "le/engine_api.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

namespace le::midi {

namespace {
// Bảng palette 128 màu — Launchpad X Programmer's reference tr.12 "Colour palette" (Mini MK3 dùng cùng palette).
// Tài liệu KHÔNG in giá trị số, chỉ vẽ ô màu → các giá trị dưới đây được TRÍCH TỪ ẢNH của chính PDF đó (ảnh raster
// 864×432, object 118; bảng hex object 119 trùng khớp 100 %): trung vị 6×6 pixel ở góc trên-trái mỗi ô (tránh chữ số),
// 29/09/2026. Tài liệu vẽ màu với "sàn" 0x61: kênh LED 0 → 0x61 (ô 0 = tắt vẽ #616161), 255 → 0xFF; kênh nhỏ nhất của
// toàn bảng đúng bằng 0x61 → mức LED ≈ (v − 0x61) · 255 / 158 (launchpadPaletteRgb).
constexpr std::array<uint32_t, 128> kPaletteDocRgb = {
    0x616161, 0xB3B3B3, 0xDDDDDD, 0xFFFFFF, 0xFFB3B3, 0xFF6161, 0xDD6161, 0xB36161,   // 0..7
    0xFFF3D5, 0xFFB361, 0xDD8C61, 0xB37661, 0xFFEEA1, 0xFFFF61, 0xDDDD61, 0xB3B361,   // 8..15
    0xDDFFA1, 0xC2FF61, 0xA1DD61, 0x81B361, 0xC2FFB3, 0x61FF61, 0x61DD61, 0x61B361,   // 16..23
    0xC2FFC2, 0x61FF8C, 0x61DD76, 0x61B36B, 0xC2FFCC, 0x61FFCC, 0x61DDA1, 0x61B381,   // 24..31
    0xC2FFF3, 0x61FFE9, 0x61DDC2, 0x61B396, 0xC2F3FF, 0x61EEFF, 0x61C7DD, 0x61A1B3,   // 32..39
    0xC2DDFF, 0x61C7FF, 0x61A1DD, 0x6181B3, 0xA18CFF, 0x6161FF, 0x6161DD, 0x6161B3,   // 40..47
    0xCCB3FF, 0xA161FF, 0x8161DD, 0x7661B3, 0xFFB3FF, 0xFF61FF, 0xDD61DD, 0xB361B3,   // 48..55
    0xFFB3D5, 0xFF61C2, 0xDD61A1, 0xB3618C, 0xFF7661, 0xE9B361, 0xDDC261, 0xA1A161,   // 56..63
    0x61B361, 0x61B38C, 0x618CD5, 0x6161FF, 0x61B3B3, 0x8C61F3, 0xCCB3C2, 0x8C7681,   // 64..71
    0xFF6161, 0xF3FFA1, 0xEEFC61, 0xCCFF61, 0x76DD61, 0x61FFCC, 0x61E9FF, 0x61A1FF,   // 72..79
    0x8C61FF, 0xCC61FC, 0xEE8CDD, 0xA17661, 0xFFA161, 0xDDF961, 0xD5FF8C, 0x61FF61,   // 80..87
    0xB3FFA1, 0xCCFCD5, 0xB3FFF6, 0xCCE4FF, 0xA1C2F6, 0xD5C2F9, 0xF98CFF, 0xFF61CC,   // 88..95
    0xFFC261, 0xF3EE61, 0xE4FF61, 0xDDCC61, 0xB3A161, 0x61BA76, 0x76C28C, 0x8181A1,   // 96..103
    0x818CCC, 0xCCAA81, 0xDD6161, 0xF9B3A1, 0xF9BA76, 0xFFF38C, 0xE9F9A1, 0xD5EE76,   // 104..111
    0x8181A1, 0xF9F9D5, 0xDDFCE4, 0xE9E9FF, 0xE4D5FF, 0xB3B3B3, 0xD5D5D5, 0xF9FFFF,   // 112..119
    0xE96161, 0xAA6161, 0x81F661, 0x61B361, 0xF3EE61, 0xB3A161, 0xEEC261, 0xC27661,   // 120..127
};

double srgbToLinear(double c) {
    c /= 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

// OKLab (Björn Ottosson 2020): khoảng cách Euclid ≈ khác biệt cảm nhận, tốt hơn RGB / HSV cho "màu gần nhất".
std::array<double, 3> oklab(uint32_t rgb) {
    const double r = srgbToLinear(static_cast<double>((rgb >> 16) & 0xFF));
    const double g = srgbToLinear(static_cast<double>((rgb >> 8) & 0xFF));
    const double b = srgbToLinear(static_cast<double>(rgb & 0xFF));
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s, 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}
} // namespace

uint32_t launchpadPaletteRgb(uint8_t index) noexcept {
    const uint32_t d = kPaletteDocRgb[index & 0x7F];
    auto unlift = [](uint32_t v) {
        return static_cast<uint32_t>(std::lround(std::max(0.0, (static_cast<double>(v) - 97.0) * 255.0 / 158.0)));
    };
    return (unlift((d >> 16) & 0xFF) << 16) | (unlift((d >> 8) & 0xFF) << 8) | unlift(d & 0xFF);
}

uint8_t nearestPaletteColor(uint32_t rgb) noexcept {
    static const std::array<std::array<double, 3>, 128> lab = [] {
        std::array<std::array<double, 3>, 128> t{};
        for (int i = 0; i < 128; ++i) t[static_cast<size_t>(i)] = oklab(launchpadPaletteRgb(static_cast<uint8_t>(i)));
        return t;
    }();
    const auto want = oklab(rgb & 0xFFFFFFu);
    uint8_t best = 1;
    double bestD = 1e300;
    for (int i = 1; i < 128; ++i) {   // 0 = tắt: không bao giờ chọn. Trùng màu (VD 5 và 72) → chỉ số nhỏ hơn thắng.
        const auto& c = lab[static_cast<size_t>(i)];
        const double d = (c[0] - want[0]) * (c[0] - want[0]) + (c[1] - want[1]) * (c[1] - want[1]) + (c[2] - want[2]) * (c[2] - want[2]);
        if (d < bestD) {
            bestD = d;
            best = static_cast<uint8_t>(i);
        }
    }
    return best;
}

// Hàng Launchpad 1 (dưới) … 8 (trên); scene 0 là hàng trên cùng.
int padNote(int track, int scene) noexcept [[clang::nonblocking]] {
    if (track < 0 || track > 7 || scene < 0 || scene > 7) return -1;
    return 10 * (8 - scene) + (track + 1);
}

bool cellForNote(int note, int& track, int& scene) noexcept [[clang::nonblocking]] {
    const int row = note / 10, col = note % 10;
    if (row < 1 || row > 8 || col < 1 || col > 8) return false;
    track = col - 1;
    scene = 8 - row;
    return true;
}

int sceneButtonCc(int scene) noexcept [[clang::nonblocking]] {
    return (scene < 0 || scene > 7) ? -1 : 10 * (8 - scene) + 9;
}

bool sceneForCc(int cc, int& scene) noexcept [[clang::nonblocking]] {
    if (cc % 10 != 9 || cc < 19 || cc > 89) return false;
    scene = 8 - cc / 10;
    return true;
}

int topButtonCc(int index) noexcept [[clang::nonblocking]] { return (index < 0 || index > 7) ? -1 : 91 + index; }

bool topIndexForCc(int cc, int& index) noexcept [[clang::nonblocking]] {
    if (cc < 91 || cc > 98) return false;
    index = cc - 91;
    return true;
}

LaunchpadInput classifyLaunchpad(uint8_t status, uint8_t d1, uint8_t d2) noexcept [[clang::nonblocking]] {
    LaunchpadInput in;
    const uint8_t type = status & 0xF0;
    int a = 0, b = 0;
    if ((type == 0x90 || type == 0x80) && cellForNote(d1, a, b)) {
        in.kind = LaunchpadInput::Kind::Pad;
        in.track = static_cast<int8_t>(a);
        in.scene = static_cast<int8_t>(b);
        in.pressed = type == 0x90 && d2 > 0;     // Note On vel 0 = nhả (tr.6)
        in.velocity = in.pressed ? d2 : 0;
    } else if (type == 0xB0) {
        in.pressed = d2 > 0;
        if (sceneForCc(d1, a)) {
            in.kind = LaunchpadInput::Kind::Scene;
            in.scene = static_cast<int8_t>(a);
        } else if (topIndexForCc(d1, a)) {
            in.kind = LaunchpadInput::Kind::Top;
            in.index = static_cast<int8_t>(a);
        } else if (d1 == kLogoCc) {
            in.kind = LaunchpadInput::Kind::Logo;
        } else {
            in.pressed = false;
        }
    }
    return in;
}

LearnTarget defaultLaunchpadAction(const LaunchpadInput& in) noexcept [[clang::nonblocking]] {
    LearnTarget t;
    if (!in.pressed) return t;
    switch (in.kind) {
        case LaunchpadInput::Kind::Pad:
            t.action = LearnAction::ClipLaunch;
            t.track = in.track;
            t.slot = in.scene;
            break;
        case LaunchpadInput::Kind::Scene:
            t.action = LearnAction::SceneLaunch;
            t.slot = in.scene;
            break;
        case LaunchpadInput::Kind::Top:
            if (in.index == 0) t.action = LearnAction::TransportToggle;
            else if (in.index == 1) t.action = LearnAction::StopAll;
            break;
        case LaunchpadInput::Kind::Logo:
        case LaunchpadInput::Kind::None:
            break;
    }
    return t;
}

uint8_t launchpadDeviceId(LaunchpadModel m) noexcept { return m == LaunchpadModel::X ? 0x0C : 0x0D; }

SysExMessage programmerModeSysEx(LaunchpadModel m, bool programmer) noexcept {
    SysExMessage s;
    const uint8_t b[] = {0xF0, 0x00, 0x20, 0x29, 0x02, launchpadDeviceId(m), 0x0E, static_cast<uint8_t>(programmer ? 1 : 0), 0xF7};
    std::copy(std::begin(b), std::end(b), s.bytes.begin());
    s.size = static_cast<int>(sizeof(b));
    return s;
}

std::vector<std::vector<uint8_t>> buildLedSysEx(LaunchpadModel m, const LedSpec* specs, int count) {
    std::vector<std::vector<uint8_t>> out;
    constexpr int kMaxSpecs = 81;                // tr.15: tối đa 81 colourspec mỗi tin
    for (int start = 0; start < count; start += kMaxSpecs) {
        std::vector<uint8_t> msg = {0xF0, 0x00, 0x20, 0x29, 0x02, launchpadDeviceId(m), 0x03};
        for (int i = start; i < std::min(count, start + kMaxSpecs); ++i) {
            const LedSpec& s = specs[i];
            msg.push_back(static_cast<uint8_t>(s.type));
            msg.push_back(s.led);
            if (s.type == LedSpec::Type::Flash) {  // Flash: màu B rồi màu A (tr.15)
                msg.push_back(s.colorB & 0x7F);
                msg.push_back(s.colorA & 0x7F);
            } else {
                msg.push_back(s.colorB & 0x7F);
            }
        }
        msg.push_back(0xF7);
        out.push_back(std::move(msg));
    }
    return out;
}

namespace {
// Kênh 1 tĩnh, 2 nhấp nháy, 3 thở; Note (pad) hoặc CC (nút).
LedMessage led(bool isCc, int channel, int led, uint8_t color) noexcept [[clang::nonblocking]] {
    return {static_cast<uint8_t>((isCc ? 0xB0 : 0x90) | channel), static_cast<uint8_t>(led), static_cast<uint8_t>(color & 0x7F)};
}
LedCommands one(LedMessage a) noexcept [[clang::nonblocking]] {
    LedCommands c;
    c.msg[0] = a;
    c.count = 1;
    return c;
}
LedCommands flash(bool isCc, int ledIdx, uint8_t colorA, uint8_t colorB) noexcept [[clang::nonblocking]] {
    LedCommands c;
    c.msg[0] = led(isCc, 0, ledIdx, colorA);    // A: màu tĩnh làm nền
    c.msg[1] = led(isCc, 1, ledIdx, colorB);    // B: kênh 2 → nhấp nháy A ↔ B
    c.count = 2;
    return c;
}
} // namespace

LedCommands clipLed(int ledIdx, bool isCc, uint8_t state, uint8_t trackColor) noexcept [[clang::nonblocking]] {
    const uint8_t tc = trackColor == 0 ? lpcolor::kWhite : static_cast<uint8_t>(trackColor & 0x7F);   // 0 = "tắt" → dùng trắng
    switch (state) {
        case LE_CLIP_STOPPED:       return one(led(isCc, 0, ledIdx, tc));
        case LE_CLIP_QUEUED_PLAY:   return flash(isCc, ledIdx, tc, lpcolor::kGreen);
        case LE_CLIP_PLAYING:       return one(led(isCc, 2, ledIdx, lpcolor::kGreen));
        case LE_CLIP_QUEUED_STOP:   return flash(isCc, ledIdx, lpcolor::kGreen, lpcolor::kOff);
        case LE_CLIP_QUEUED_RECORD: return flash(isCc, ledIdx, tc, lpcolor::kRed);
        case LE_CLIP_RECORDING:     return one(led(isCc, 2, ledIdx, lpcolor::kRed));
        case LE_CLIP_OVERDUBBING:   return one(led(isCc, 2, ledIdx, lpcolor::kOrange));
        default:                    return one(led(isCc, 0, ledIdx, lpcolor::kOff));   // Empty / lạ
    }
}

LedCommands sceneLed(int scene, bool anyPlaying, bool anyQueued) noexcept [[clang::nonblocking]] {
    const int cc = sceneButtonCc(scene);
    if (cc < 0) return {};
    if (anyQueued) return flash(true, cc, anyPlaying ? lpcolor::kGreen : lpcolor::kOff, lpcolor::kGreen);
    return one(led(true, 0, cc, anyPlaying ? lpcolor::kGreen : lpcolor::kOff));
}

int LaunchpadLedState::update(const uint8_t (&clipState)[8][8], const uint8_t (&trackColor)[8],
                              std::array<LedMessage, kMaxMessages>& out, bool force) noexcept {
    int n = 0;
    auto emit = [&](const LedCommands& c) {
        for (int i = 0; i < c.count && n < kMaxMessages; ++i) out[static_cast<size_t>(n++)] = c.msg[static_cast<size_t>(i)];
    };
    const bool all = force || !valid_;
    for (int t = 0; t < 8; ++t)
        for (int s = 0; s < 8; ++s) {
            const uint8_t st = clipState[t][s];
            if (all || st != lastState_[t][s] || (trackColor[t] != lastColor_[t] && st != LE_CLIP_EMPTY))
                emit(clipLed(padNote(t, s), false, st, trackColor[t]));
            lastState_[t][s] = st;
        }
    for (int t = 0; t < 8; ++t) lastColor_[t] = trackColor[t];
    for (int s = 0; s < 8; ++s) {
        bool playing = false, queued = false;
        for (int t = 0; t < 8; ++t) {
            const uint8_t st = clipState[t][s];
            playing |= st == LE_CLIP_PLAYING || st == LE_CLIP_RECORDING || st == LE_CLIP_OVERDUBBING;
            queued |= st == LE_CLIP_QUEUED_PLAY || st == LE_CLIP_QUEUED_RECORD;
        }
        const uint8_t code = queued ? 2 : (playing ? 1 : 0);
        if (all || code != lastScene_[s]) emit(sceneLed(s, playing, queued));
        lastScene_[s] = code;
    }
    valid_ = true;
    return n;
}

} // namespace le::midi
