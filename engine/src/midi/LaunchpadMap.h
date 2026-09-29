// LaunchpadMap — ánh xạ Launchpad X / Launchpad Mini [MK3] ở chế độ Programmer ↔ lưới clip 8×8 (04 §13, P4-05 lõi).
// Hàm thuần, không cấp phát (trừ buildLedSysEx dùng std::vector ở main). 68 mở MidiOutput và gửi từ Timer 30 Hz (main).
//
// NGUỒN (đã đối chiếu từng byte, 29/09/2026):
//   Novation "Launchpad X Programmer's reference manual" — Focusrite downloads
//     https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Launchpad%20X%20-%20Programmers%20Reference%20Manual.pdf
//     tr.6 SysEx header F0h 00h 20h 29h 02h 0Ch · tr.7 Programmer/Live switch F0h 00h 20h 29h 02h 0Ch 0Eh <mode> F7h
//     (mode 0 = Live, 1 = Programmer) · tr.10 Programmer mode layout · tr.12 Colour palette · tr.13 kênh màu ·
//     tr.15 LED lighting SysEx (lệnh 03h, tối đa 81 colourspec).
//   Novation "Launchpad Mini [MK3] Programmer's reference manual" — Focusrite downloads
//     https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Launchpad%20Mini%20-%20Programmers%20Reference%20Manual.pdf
//     tr.6 header F0h 00h 20h 29h 02h 0Dh · tr.7 cùng lệnh 0Eh <mode> · tr.10 cùng Programmer layout · tr.12 cùng kênh màu.
//
// Programmer layout (cả hai model):
//   Pad 8×8: note = 10·hàng + cột, hàng 1 (dưới) … 8 (trên), cột 1 (trái) … 8 (phải) → 11 … 88.
//   Cột nút bên phải: CC 19, 29, … 89 (hàng 1 … 8). Hàng nút trên cùng: CC 91 … 98. Logo: CC 99.
//   Pad gửi Note On 90h (velocity = lực bấm), nhả = Note On velocity 0 (hoặc Note Off). Nút gửi CC B0h 127 / 0.
// Màu LED (tr.13): kênh 1 (90h / B0h) = màu tĩnh, kênh 2 (91h / B1h) = NHẤP NHÁY giữa màu đang đặt ở kênh 1 (A)
//   và màu gửi ở kênh 2 (B), 50 % theo MIDI clock (hoặc 120 BPM); kênh 3 (92h / B2h) = "thở" (pulse). velocity =
//   chỉ số palette 0..127 (0 = tắt). Ví dụ trong tài liệu: 5 đỏ, 13 vàng, 21 xanh lá, 45 xanh dương, 37 ngọc.
//
// Ánh xạ lưới app (kiểu Session View của Ableton): CỘT = track 0..7 (trái → phải), HÀNG = scene 0..7 (trên → dưới).
//   Nút cột phải = launch scene cùng hàng. Hàng nút trên: [0] play/stop, [1] stop all (preset mặc định), còn lại trống.
#pragma once

#include "midi/MidiLearnMap.h"

#include <array>
#include <cstdint>
#include <vector>

namespace le::midi {

enum class LaunchpadModel : uint8_t { X, MiniMk3 };

// Chỉ số palette hay dùng (tr.12, tr.14)
namespace lpcolor {
inline constexpr uint8_t kOff = 0, kWhite = 3, kRed = 5, kRedDim = 7, kOrange = 9, kYellow = 13, kGreen = 21,
                         kGreenDim = 23, kCyan = 37, kBlue = 45, kPurple = 53;
}

// ── Màu track (#RRGGBB) → chỉ số palette (P1-39, track.configure {color}) ──
// [main] Chỉ số 1..127 gần nhất về cảm nhận (không bao giờ trả 0 = tắt). Bảng 128 màu: xem LaunchpadMap.cpp.
uint8_t nearestPaletteColor(uint32_t rgb) noexcept;
// [any] Màu LED ước lượng của chỉ số palette (0xRRGGBB, đã bỏ "sàn" hiển thị của tài liệu) — cho UI xem trước.
uint32_t launchpadPaletteRgb(uint8_t index) noexcept;

// ── Vị trí ↔ MIDI ──
int  padNote(int track, int scene) noexcept [[clang::nonblocking]];            // 11..88, −1 nếu ngoài lưới
bool cellForNote(int note, int& track, int& scene) noexcept [[clang::nonblocking]];
int  sceneButtonCc(int scene) noexcept [[clang::nonblocking]];                  // scene 0 → 89 … 7 → 19
bool sceneForCc(int cc, int& scene) noexcept [[clang::nonblocking]];
int  topButtonCc(int index) noexcept [[clang::nonblocking]];                    // 0 → 91 … 7 → 98
bool topIndexForCc(int cc, int& index) noexcept [[clang::nonblocking]];
inline constexpr int kLogoCc = 99;

// ── Input từ Launchpad ──
struct LaunchpadInput {
    enum class Kind : uint8_t { None, Pad, Scene, Top, Logo };
    Kind    kind = Kind::None;
    int8_t  track = -1, scene = -1, index = -1;   // Pad: track + scene; Scene: scene; Top: index
    bool    pressed = false;                       // false = nhả
    uint8_t velocity = 0;                          // Pad: lực bấm 1..127
};
LaunchpadInput classifyLaunchpad(uint8_t status, uint8_t data1, uint8_t data2) noexcept [[clang::nonblocking]];

// Preset mặc định: pad → launch clip, cột phải → launch scene, top[0] → play/stop, top[1] → stop all.
// Chỉ khi nhấn (pressed); nhả → action None.
LearnTarget defaultLaunchpadAction(const LaunchpadInput& in) noexcept [[clang::nonblocking]];

// ── SysEx ──
struct SysExMessage {
    std::array<uint8_t, 16> bytes{};
    int size = 0;
};
uint8_t launchpadDeviceId(LaunchpadModel m) noexcept;                            // X 0Ch, Mini MK3 0Dh
SysExMessage programmerModeSysEx(LaunchpadModel m, bool programmer) noexcept;   // F0 00 20 29 02 id 0E mode F7

struct LedSpec {                                  // colourspec của LED lighting SysEx (lệnh 03h, tr.15)
    enum class Type : uint8_t { Static = 0, Flash = 1, Pulse = 2 };
    Type    type = Type::Static;
    uint8_t led = 0;                               // chỉ số theo Programmer layout (11..99)
    uint8_t colorB = 0;                            // Static/Pulse: màu; Flash: màu B
    uint8_t colorA = 0;                            // Flash: màu A
};
// [main] Gói nhiều LED vào 1 SysEx (tối đa 81 spec/tin; nhiều hơn → cắt thành nhiều tin).
std::vector<std::vector<uint8_t>> buildLedSysEx(LaunchpadModel m, const LedSpec* specs, int count);

// ── LED theo trạng thái clip ──
struct LedMessage {                               // 1 tin MIDI 3 byte gửi ra MidiOutput
    uint8_t status = 0, data1 = 0, data2 = 0;
};
struct LedCommands {                              // 1 LED cần tối đa 2 tin (màu A tĩnh, rồi màu B nhấp nháy)
    std::array<LedMessage, 2> msg{};
    int count = 0;
};
// clipState = LeClipState (0..7). trackColor = chỉ số palette của track (UI chọn). isCc: LED là nút CC (cột phải).
//   Empty → tắt · Stopped → màu track · QueuedPlay → nhấp nháy màu track ↔ xanh lá · Playing → xanh lá "thở"
//   QueuedStop → nhấp nháy xanh lá ↔ tắt · QueuedRecord → nhấp nháy màu track ↔ đỏ · Recording → đỏ "thở"
//   Overdubbing → cam "thở"
LedCommands clipLed(int led, bool isCc, uint8_t clipState, uint8_t trackColor) noexcept [[clang::nonblocking]];
// Nút scene: có clip đang phát → xanh lá; có clip đang chờ → nhấp nháy xanh lá; không → tắt.
LedCommands sceneLed(int scene, bool anyPlaying, bool anyQueued) noexcept [[clang::nonblocking]];

// [main] Nhớ LED đã gửi, mỗi lần update chỉ sinh tin cho ô ĐỔI (Timer 30 Hz không làm ngập cổng MIDI).
class LaunchpadLedState {
public:
    static constexpr int kMaxMessages = 64 * 2 + 8 * 2;
    // clipState[track][scene], trackColor[track]. Trả số tin ghi vào out (≤ kMaxMessages). force = gửi lại tất cả.
    int update(const uint8_t (&clipState)[8][8], const uint8_t (&trackColor)[8], std::array<LedMessage, kMaxMessages>& out,
               bool force = false) noexcept;
    void invalidate() noexcept { valid_ = false; }   // VD sau khi cắm lại thiết bị

private:
    uint8_t lastState_[8][8]{}, lastColor_[8]{};
    uint8_t lastScene_[8]{};                          // 0 tắt, 1 phát, 2 chờ
    bool valid_ = false;
};

} // namespace le::midi
