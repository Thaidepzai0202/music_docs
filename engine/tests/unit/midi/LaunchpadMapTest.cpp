// P4-05 (lõi) — LaunchpadMap: note ↔ ô, nút scene/top, SysEx Programmer mode (đúng byte trong Programmer's
// Reference của Novation), phân loại input, preset mặc định, LED theo LeClipState, chỉ gửi ô đổi.
#include <catch2/catch_test_macros.hpp>

#include "le/engine_api.h"
#include "midi/LaunchpadMap.h"

#include <array>
#include <cmath>
#include <set>
#include <vector>

using namespace le::midi;

TEST_CASE("LaunchpadMap: 64 ô ↔ note 11..88 (cột = track, hàng trên = scene 0), khứ hồi đúng", "[midi][launchpad]") {
    CHECK(padNote(0, 0) == 81);           // track 0, scene 0 = góc trên trái
    CHECK(padNote(7, 0) == 88);
    CHECK(padNote(0, 7) == 11);           // góc dưới trái (tài liệu tr.14: "lower left pad" = 0Bh = 11)
    CHECK(padNote(7, 7) == 18);           // góc dưới phải (tr.14: 12h = 18)
    CHECK(padNote(8, 0) == -1);
    CHECK(padNote(0, -1) == -1);
    std::set<int> seen;
    for (int t = 0; t < 8; ++t)
        for (int s = 0; s < 8; ++s) {
            const int n = padNote(t, s);
            REQUIRE(n >= 11);
            REQUIRE(n <= 88);
            seen.insert(n);
            int tt = -1, ss = -1;
            REQUIRE(cellForNote(n, tt, ss));
            CHECK(tt == t);
            CHECK(ss == s);
        }
    CHECK(seen.size() == 64);
    int a = 0, b = 0;
    CHECK_FALSE(cellForNote(19, a, b));   // cột nút phải, không phải pad
    CHECK_FALSE(cellForNote(90, a, b));
    CHECK_FALSE(cellForNote(10, a, b));

    CHECK(sceneButtonCc(0) == 89);
    CHECK(sceneButtonCc(7) == 19);
    int sc = -1;
    REQUIRE(sceneForCc(59, sc));
    CHECK(sc == 3);
    CHECK_FALSE(sceneForCc(58, sc));
    CHECK(topButtonCc(0) == 91);
    CHECK(topButtonCc(7) == 98);
    int ti = -1;
    REQUIRE(topIndexForCc(95, ti));
    CHECK(ti == 4);
    CHECK_FALSE(topIndexForCc(99, ti));   // 99 là logo
}

TEST_CASE("LaunchpadMap: SysEx Programmer/Live đúng byte cho Launchpad X (0Ch) và Mini MK3 (0Dh)", "[midi][launchpad]") {
    const auto x = programmerModeSysEx(LaunchpadModel::X, true);
    const std::vector<uint8_t> expectX = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0C, 0x0E, 0x01, 0xF7};   // Launchpad X tr.7
    CHECK(std::vector<uint8_t>(x.bytes.begin(), x.bytes.begin() + x.size) == expectX);
    const auto m = programmerModeSysEx(LaunchpadModel::MiniMk3, false);
    const std::vector<uint8_t> expectM = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x00, 0xF7};   // Mini MK3 tr.7, Live
    CHECK(std::vector<uint8_t>(m.bytes.begin(), m.bytes.begin() + m.size) == expectM);

    // LED lighting SysEx — đúng ví dụ ở tr.15: dưới trái vàng tĩnh, kế bên nhấp nháy xanh, kế nữa thở ngọc
    const LedSpec specs[] = {{LedSpec::Type::Static, 11, 13, 0}, {LedSpec::Type::Flash, 12, 21, 23}, {LedSpec::Type::Pulse, 13, 37, 0}};
    const auto msgs = buildLedSysEx(LaunchpadModel::X, specs, 3);
    REQUIRE(msgs.size() == 1);
    const std::vector<uint8_t> expectLed = {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0C, 0x03, 0x00, 0x0B, 0x0D,
                                            0x01, 0x0C, 0x15, 0x17, 0x02, 0x0D, 0x25, 0xF7};
    CHECK(msgs[0] == expectLed);
    std::vector<LedSpec> many(100, LedSpec{LedSpec::Type::Static, 11, 5, 0});
    CHECK(buildLedSysEx(LaunchpadModel::MiniMk3, many.data(), 100).size() == 2);   // > 81 spec → 2 tin
}

TEST_CASE("LaunchpadMap: phân loại input + preset mặc định", "[midi][launchpad]") {
    auto p = classifyLaunchpad(0x90, 83, 100);    // pad hàng 8 cột 3 → track 2, scene 0
    CHECK(p.kind == LaunchpadInput::Kind::Pad);
    CHECK(p.track == 2);
    CHECK(p.scene == 0);
    CHECK(p.pressed);
    CHECK(p.velocity == 100);
    auto t = defaultLaunchpadAction(p);
    CHECK(t.action == LearnAction::ClipLaunch);
    CHECK(t.track == 2);
    CHECK(t.slot == 0);

    p = classifyLaunchpad(0x90, 83, 0);           // nhả = Note On vel 0
    CHECK(p.kind == LaunchpadInput::Kind::Pad);
    CHECK_FALSE(p.pressed);
    CHECK(defaultLaunchpadAction(p).action == LearnAction::None);
    CHECK_FALSE(classifyLaunchpad(0x80, 83, 64).pressed);

    p = classifyLaunchpad(0xB0, 69, 127);         // nút phải hàng 6 → scene 2
    CHECK(p.kind == LaunchpadInput::Kind::Scene);
    CHECK(p.scene == 2);
    t = defaultLaunchpadAction(p);
    CHECK(t.action == LearnAction::SceneLaunch);
    CHECK(t.slot == 2);

    CHECK(defaultLaunchpadAction(classifyLaunchpad(0xB0, 91, 127)).action == LearnAction::TransportToggle);
    CHECK(defaultLaunchpadAction(classifyLaunchpad(0xB0, 92, 127)).action == LearnAction::StopAll);
    CHECK(defaultLaunchpadAction(classifyLaunchpad(0xB0, 93, 127)).action == LearnAction::None);
    CHECK(classifyLaunchpad(0xB0, 99, 127).kind == LaunchpadInput::Kind::Logo);
    CHECK(classifyLaunchpad(0xB0, 7, 100).kind == LaunchpadInput::Kind::None);   // CC khác
    CHECK(classifyLaunchpad(0xE0, 0, 64).kind == LaunchpadInput::Kind::None);
}

TEST_CASE("LaunchpadMap: màu LED theo trạng thái clip (kênh 1 tĩnh, 2 nhấp nháy A↔B, 3 thở)", "[midi][launchpad]") {
    const uint8_t blue = lpcolor::kBlue;
    auto c = clipLed(81, false, LE_CLIP_EMPTY, blue);
    REQUIRE(c.count == 1);
    CHECK((c.msg[0].status == 0x90 && c.msg[0].data1 == 81 && c.msg[0].data2 == 0));
    c = clipLed(81, false, LE_CLIP_STOPPED, blue);
    CHECK((c.count == 1 && c.msg[0].status == 0x90 && c.msg[0].data2 == blue));
    c = clipLed(81, false, LE_CLIP_QUEUED_PLAY, blue);   // nền màu track, nhấp nháy sang xanh lá
    REQUIRE(c.count == 2);
    CHECK((c.msg[0].status == 0x90 && c.msg[0].data2 == blue));
    CHECK((c.msg[1].status == 0x91 && c.msg[1].data1 == 81 && c.msg[1].data2 == lpcolor::kGreen));
    c = clipLed(81, false, LE_CLIP_PLAYING, blue);
    CHECK((c.count == 1 && c.msg[0].status == 0x92 && c.msg[0].data2 == lpcolor::kGreen));
    c = clipLed(81, false, LE_CLIP_QUEUED_STOP, blue);
    CHECK((c.count == 2 && c.msg[0].data2 == lpcolor::kGreen && c.msg[1].status == 0x91 && c.msg[1].data2 == 0));
    c = clipLed(81, false, LE_CLIP_QUEUED_RECORD, blue);
    CHECK((c.count == 2 && c.msg[1].data2 == lpcolor::kRed));
    c = clipLed(81, false, LE_CLIP_RECORDING, blue);
    CHECK((c.count == 1 && c.msg[0].status == 0x92 && c.msg[0].data2 == lpcolor::kRed));
    c = clipLed(81, false, LE_CLIP_OVERDUBBING, blue);
    CHECK((c.count == 1 && c.msg[0].status == 0x92 && c.msg[0].data2 == lpcolor::kOrange));
    c = clipLed(19, true, LE_CLIP_STOPPED, 0);           // nút CC; màu track 0 → trắng để còn thấy
    CHECK((c.msg[0].status == 0xB0 && c.msg[0].data2 == lpcolor::kWhite));

    const auto s = sceneLed(0, true, true);
    CHECK((s.count == 2 && s.msg[0].status == 0xB0 && s.msg[0].data1 == 89 && s.msg[1].status == 0xB1));
    CHECK(sceneLed(3, false, false).msg[0].data2 == 0);
}

TEST_CASE("LaunchpadLedState: lần đầu gửi tất cả, sau đó chỉ gửi ô đổi", "[midi][launchpad]") {
    uint8_t state[8][8] = {};
    uint8_t color[8] = {5, 9, 13, 21, 37, 45, 53, 3};
    LaunchpadLedState leds;
    std::array<LedMessage, LaunchpadLedState::kMaxMessages> out{};
    CHECK(leds.update(state, color, out) == 64 + 8);      // lần đầu: 64 ô + 8 nút scene
    CHECK(leds.update(state, color, out) == 0);           // không đổi → không gửi gì

    state[2][0] = LE_CLIP_QUEUED_PLAY;                    // 1 ô chờ phát → 2 tin ô + 2 tin nút scene 0 (nhấp nháy)
    CHECK(leds.update(state, color, out) == 4);
    CHECK((out[0].data1 == padNote(2, 0) && out[1].status == 0x91));
    state[2][0] = LE_CLIP_PLAYING;
    CHECK(leds.update(state, color, out) == 2);           // ô: thở xanh; nút scene: xanh tĩnh
    color[2] = 45;                                        // đổi màu track 2 → gửi lại ô CÓ clip của track đó
    state[2][1] = LE_CLIP_STOPPED;
    const int n = leds.update(state, color, out);
    CHECK(n == 2);                                        // ô (2,0) phát + ô (2,1) mới dừng; ô trống không gửi
    CHECK(leds.update(state, color, out, true) == 64 + 8 + 0);   // force: gửi lại tất cả (ô phát 1 tin)
}

TEST_CASE("LaunchpadMap: màu track #RRGGBB → chỉ số palette gần nhất (OKLab), không bao giờ 0", "[midi][launchpad][color]") {
    CHECK(nearestPaletteColor(0xFF0000) == lpcolor::kRed);      // 5
    CHECK(nearestPaletteColor(0x00FF00) == lpcolor::kGreen);    // 21
    CHECK(nearestPaletteColor(0x0000FF) == lpcolor::kBlue);     // 45
    CHECK(nearestPaletteColor(0xFFFFFF) == lpcolor::kWhite);    // 3
    CHECK(nearestPaletteColor(0xFFFF00) == lpcolor::kYellow);   // 13
    CHECK(nearestPaletteColor(0xFF00FF) == lpcolor::kPurple);   // 53
    CHECK(nearestPaletteColor(0xFF8000) == lpcolor::kOrange);   // 9
    CHECK(nearestPaletteColor(0x808080) == 1);                  // xám
    CHECK(nearestPaletteColor(0x00FFFF) == 33);                 // ngọc thuần: ô 33 (#61FFE9) gần hơn ô 37
    // Màu UI phổ biến vẫn ra đúng họ màu
    // Màu UI phổ biến vẫn ra đúng HỌ màu: hue của ô được chọn lệch < 20° so với màu vào (không nhất thiết là ô "chuẩn":
    // #E53935 hơi ấm → ô 60 = LED 255/34/0; #1E88E5 thiên xanh trời → ô 41 = LED 0/165/255)
    auto hueOf = [](uint32_t rgb) {
        const double r = static_cast<double>((rgb >> 16) & 0xFF), g = static_cast<double>((rgb >> 8) & 0xFF), b = static_cast<double>(rgb & 0xFF);
        return std::atan2(std::sqrt(3.0) * (g - b), 2.0 * r - g - b) * 180.0 / 3.14159265358979;
    };
    for (const uint32_t ui : {0xE53935u, 0x43A047u, 0x1E88E5u, 0xFB8C00u, 0x8E24AAu, 0xFDD835u}) {   // Material 600
        const uint8_t i = nearestPaletteColor(ui);
        double dh = std::fabs(hueOf(launchpadPaletteRgb(i)) - hueOf(ui));
        if (dh > 180.0) dh = 360.0 - dh;
        CAPTURE(ui, static_cast<int>(i), dh);
        CHECK(dh < 20.0);
    }
    // Không bao giờ 0 (tắt), mọi chỉ số trong 1..127, kể cả đen và bit rác phía trên
    for (uint32_t rgb : {0x000000u, 0x010101u, 0xFF000000u, 0x123456u, 0xFFFFFFFFu}) {
        const uint8_t i = nearestPaletteColor(rgb);
        CHECK(i >= 1);
        CHECK(i <= 127);
    }
    CHECK(nearestPaletteColor(0xFF123456u) == nearestPaletteColor(0x123456u));   // byte cao bị bỏ qua
    // Màu LED ước lượng của các chỉ số tài liệu dùng làm ví dụ (tr.12–15): 5 đỏ, 21 xanh lá, 45 xanh dương, 3 trắng
    CHECK(launchpadPaletteRgb(lpcolor::kRed) == 0xFF0000u);
    CHECK(launchpadPaletteRgb(lpcolor::kGreen) == 0x00FF00u);
    CHECK(launchpadPaletteRgb(lpcolor::kBlue) == 0x0000FFu);
    CHECK(launchpadPaletteRgb(lpcolor::kWhite) == 0xFFFFFFu);
    CHECK(launchpadPaletteRgb(0) == 0x000000u);                 // tắt
    // Khứ hồi: màu LED của mọi chỉ số (trừ các ô trùng màu) → chính chỉ số đó hoặc một ô cùng màu
    for (int i = 1; i < 128; ++i) {
        const uint8_t back = nearestPaletteColor(launchpadPaletteRgb(static_cast<uint8_t>(i)));
        CAPTURE(i);
        CHECK(launchpadPaletteRgb(back) == launchpadPaletteRgb(static_cast<uint8_t>(i)));
    }
}
