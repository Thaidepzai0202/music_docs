# P4 — Kết nối & hoàn thiện (W29–W35: 19/04 – 04/06/2027)

**Mục tiêu:** MIDI controller, Ableton Link, ổn định, rồi **TestFlight ngày 04/06/2027**.
**Điều kiện trước:** entitlement multicast **đã được Apple duyệt** (nộp từ P0-01). Nếu tới W30 vẫn chưa có: làm Link trên build dev bằng provisioning có entitlement khi Apple duyệt xong. Trường hợp xấu nhất, TestFlight đầu tiên tắt Link.

---

## Tổng quan task

| TT | Mã | Việc | Tuần | Ước lượng |
|---|---|---|---|---|
| [~] | P4-01 | `MidiInputRouter`: thiết bị, bật/tắt, SPSC, timestamp → offset [RT] | W29 | 1.5d |
| [~] | P4-02 | Nốt từ controller → track đang chọn (`SELECT_TRACK`) | W29 | 0.5d |
| [~] | P4-03 | Ghép nối BLE MIDI (Swift `CABTMIDICentralViewController`) | W29 | 0.5d |
| [~] | P4-04 | MIDI learn (clip, scene, transport, gain track, tham số FX) + lưu mapping | W29 | 1.5d |
| [~] | P4-05 | Preset Launchpad + đèn LED phản hồi (Timer 30Hz trên main) | W29 | 1d |
| [ ] | P4-06 | Tích hợp LinkKit: vendored, entitlement, plist | W30 | 0.5d |
| [ ] | P4-07 | [RT] `LinkSync`: capture state, beat tại thời điểm output, set/commit tempo | W30 | 2d |
| [ ] | P4-08 | Đồng bộ start/stop + quantum | W30 | 1d |
| [ ] | P4-09 | UI Link: `ABLLinkSettingsViewController` (Swift), số peer trên top bar | W30 | 1d |
| [ ] | P4-10 | **Qua 100% test plan của LinkKit** (Ableton Live + một app Link khác) | W31 | 2.5d |
| [ ] | P4-11 | Link đổi tempo → warp (P3-08) → không glitch | W31 | 1d |
| [~] | P4-12 | UI hiệu chỉnh latency (loopback) + chỉnh offset thủ công | W32 | 1d |
| [~] | P4-13 | Settings: audio (buffer, input), monitoring, metronome, haptic, mặc định khi thu | W32 | 1.5d |
| [~] | P4-14 | Onboarding: giải thích quyền mic, project demo | W32 | 1d |
| [ ] | P4-15 | Chốt thư viện: 4 kit, 6–8 nhạc cụ, khoảng 40 loop + dữ liệu license | W32 | 1.5d |
| [ ] | P4-16 | Soak 30 phút + kiểm tra leak (Instruments Leaks/Allocations) | W33 | 1d |
| [~] | P4-17 | Xử lý bộ nhớ thấp: nhả instrument không dùng, `LE_EVT_MEMORY_WARNING` | W33 | 1d |
| [ ] | P4-18 | Checklist interruption và route đầy đủ (08 §6) | W33 | 1d |
| [~] | P4-19 | An toàn khi bị kill: đang thu hoặc đang lưu → khôi phục được | W33 | 1d |
| [ ] | P4-26 | Preview / cue đi đường riêng, KHÔNG vào bản ghi jam (`export.jamStart`) — như cue của Ableton (05 §3 preview.play) | W33 | 1d |
| [ ] | P4-20 | Bug bash: chạy checklist 08 §6 hai lượt, sửa hết lỗi S1/S2 | W34 | 2d |
| [~] | P4-21 | `PrivacyInfo.xcprivacy` (required-reason APIs) | W34 | 0.5d |
| [~] | P4-22 | Màn Giấy phép (JUCE, Signalsmith MIT, Link, nội dung CC-BY) | W34 | 0.5d |
| [ ] | P4-23 | App Store Connect: icon, ảnh chụp màn hình, thông tin beta review | W34 | 1d |
| [ ] | P4-24 | Archive → upload → internal testing → external beta review | W35 | 1.5d |
| [ ] | P4-25 | **Review M4** + retro + backlog phase 2 | W35 | 1d |

---

## DoD chính

- **P4-01 → P4-02:**
  - [ ] Nốt từ keyboard USB vào sai lệch ≤ 1 block so với lúc gõ (đo bằng loopback MIDI → audio)
  - [ ] Không dùng `MidiMessageCollector`
- **P4-04 → P4-05:**
  - [ ] Learn → mapping được lưu vào project, mở lại vẫn còn
  - [ ] Đèn Launchpad phản ánh đúng trạng thái clip, trễ < 50ms
- **P4-07 → P4-08:**
  - [ ] Hai iPad cùng phát metronome: lệch nhau < 3ms (đo bằng cách thu cả hai vào một mic)
  - [ ] Đổi tempo ở máy kia → máy này theo trong 1 block
- **P4-10:**
  - [ ] Tick đủ **mọi** test case trong test plan của LinkKit
  - [ ] Lưu bảng kết quả vào `docs/link-test-results.md`
- **P4-11:**
  - [ ] Scenario tempo từ Link (giả lập) giống kết quả P3-10
- **P4-16 → P4-19:**
  - [ ] 0 xrun / 30 phút, 0 leak
  - [ ] Kill app lúc đang thu → mở lại, take được giữ nguyên (phần đã ghi tới lúc bị kill) hoặc bị bỏ sạch, không làm hỏng project
- **P4-21 → P4-23:**
  - [ ] Build qua được bước validate của App Store Connect
- **P4-24 → P4-25 (M4):**
  - [ ] Đạt checklist M4 (02 §3)
  - [ ] Đã ghi 08 §8

---

## Backlog phase 2 (sau TestFlight)

| Tính năng | Điểm nối đã có |
|---|---|
| AUv3 host (nạp plugin của người khác) | `Processor`, `latencySamples()` (04 §9) |
| Arrangement view (timeline) | `LaunchLog` (04 §3.5) |
| Slice theo transient (C) | `PeakBuilder`, `Sampler` zone |
| Melody → MIDI (D) | `Yin` (P3-01) |
| Synth subtractive | Là một `Processor` mới |
| Warp chế độ "Beats" (theo lát cắt, cho loop trống) | WarpRenderer, SilenceTrimmer/onset |
| Tách stems bằng AI (Core ML) | Job system |
| iPhone layout | Engine và model dùng chung |
| Android | JUCE (Oboe), Flutter UI dùng chung |
| AUv3 plugin mode | UI riêng bằng JUCE hoặc SwiftUI |
| Kiếm tiền (IAP Pro + sound pack) | StoreKit 2, không cần backend |
