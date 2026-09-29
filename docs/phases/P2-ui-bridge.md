# P2 — UI Flutter + Bridge (W15–W22: 11/01 – 05/03/2027)

**Mục tiêu:** jam hoàn chỉnh trên iPad 8 bằng UI thật: grid, thu âm, pad và bàn phím, mixer, browser, lưu và mở project.
**Không làm ở P2:** sampler từ tiếng tự thu, warp, FX, export (để P3). MIDI controller, Link (để P4).
**Spec chính:** 05, 06, 07. **Mọi task UI** phải kiểm tra Highlight Repaints và 60fps trên iPad 8 trước khi đóng (07 §6).
**W19 (08–12/02) là tuần Tết**, không xếp task.

> **29/09/2026:** các task `[~]` đã code xong, chạy với FakeEngine (135 + 58 test xanh). Còn chờ **kiểm trên iPad 8** (60fps, repaint, cảm giác chạm) và **engine thật** có lệnh clip (P1-15+).

---

## Tổng quan task

| TT | Mã | Việc | Tuần | Ước lượng |
|---|---|---|---|---|
| [x] | P2-01 | Package `engine_ffi` v2: bindings đầy đủ, `EngineClient`, `JobTracker`, `EngineEvent` sealed | W15 | 1.5d |
| [x] | P2-02 | `EngineStateTicker` + 64 `ValueNotifier<ClipState>` | W15 | 1d |
| [x] | P2-03 | Khung app: router, theme tokens, khoá landscape, vòng đời engine theo `AppLifecycle` | W15 | 1d |
| [x] | P2-04 | Model (freezed + json) + hạ tầng migration + test round-trip | W15 | 1d |
| [x] | P2-05 | `ProjectController` + `FakeEngineClient` + test chuỗi lệnh | W15 | 1d |
| [~] | P2-06 | Layout màn Session (kích thước 07 §2, panel dưới thu gọn được) | W16 | 1d |
| [~] | P2-07 | `ClipCell` (painter theo trạng thái) + launch bằng pointer-down | W16 | 1.5d |
| [~] | P2-08 | Track header: tên, meter painter, arm, M/S | W16 | 1d |
| [~] | P2-09 | Cột scene + Stop all | W16 | 0.5d |
| [~] | P2-10 | Transport bar: play/stop, BPM (kéo + tap), quantize, metronome, count-in, CPU/xrun | W17 | 1.5d |
| [~] | P2-11 | Luồng thu trên ô trống + `RECORDING_FINISHED` → model | W17 | 1d |
| [~] | P2-12 | Chế độ Edit/Perform + menu ngữ cảnh (xoá, đổi tên, màu, nhân bản) | W17 | 1.5d |
| [~] | P2-13 | Haptic, wakelock, `EngineErrorBus` + banner | W17 | 0.5d |
| [~] | P2-14 | Panel Mixer | W18 | 1d |
| [~] | P2-15 | `ui_kit`: Fader, Knob, Meter (painter tối ưu, gom lệnh theo frame) | W18 | 1d |
| [~] | P2-16 | Pad 4×4 multi-touch (`Listener`) | W20 | 1d |
| [~] | P2-17 | Bàn phím 2 quãng tám + dịch quãng + trượt giữa các phím | W20 | 1.5d |
| [~] | P2-18 | Luồng thu MIDI + tuỳ chọn quantize khi thu | W20 | 0.5d |
| [~] | P2-19 | Xem MIDI clip: hiển thị nốt, quantize, xoá nốt, clear | W20 | 1.5d |
| [~] | P2-20 | Waveform view (peaks → cache `Picture`, lớp playhead riêng) | W21 | 1.5d |
| [~] | P2-21 | Sửa audio clip (vùng loop, gain, chế độ warp) + kéo, copy clip ở chế độ Edit | W21 | 1.5d |
| [~] | P2-22 | Browser: đọc manifest, kit/instrument/loop, gán vào track hoặc ô | W21 | 1.5d |
| [~] | P2-23 | Màn Projects: danh sách, tạo, đổi tên, nhân bản, xoá | W22 | 1d |
| [~] | P2-24 | `ProjectRepository` (lưu an toàn 06 §5) + khôi phục (06 §6) + progress | W22 | 1.5d |
| [~] | P2-25 | Autosave + vòng đời: nền, interruption, route, cảnh báo Bluetooth | W22 | 1d |
| [~] | P2-26 | Thư viện khởi đầu: 2 kit, 2 nhạc cụ, 10 loop (dùng để test) | W22 | 0.5d |
| [~] | P2-28 | **Nút LOOP + pedal mode (UI)** + độ dài thu tự do (07 §3.1b), người dùng yêu cầu 29/09 | W22 | 2d |
| [~] | P2-29 | **Vẽ nốt trong piano roll** + clip MIDI trống + nghe thử + undo (07 §4.1b), người dùng yêu cầu 29/09 | W22 | 2.5d |
| [ ] | P2-27 | Đo hiệu năng trên iPad 8 + **review M2** | W22 | 1d |

---

## DoD từng task

- **P2-01:**
  - [ ] Test hợp đồng Dart ↔ C (kích thước struct, mọi op trả envelope đúng)
  - [ ] `JobTracker.await(jobId)` trả kết quả hoặc lỗi đúng
- **P2-02:**
  - [ ] Ticker chỉ chạy khi màn Session đang hiển thị
  - [ ] Đổi trạng thái 1 ô thì chỉ đúng 1 `ClipCell` repaint (Highlight Repaints)
- **P2-03:**
  - [ ] App xuống nền khi không phát thì gọi `le_audio_stop`; lên lại thì khởi động lại
  - [ ] Đang phát thì giữ audio
- **P2-04:**
  - [ ] Round-trip mọi fixture trong `test/fixtures/projects/`
  - [ ] Có migrator khung `v1 → v1` (no-op) cùng test
- **P2-05:**
  - [ ] Mở một fixture project → `FakeEngineClient` ghi đúng chuỗi lệnh ở 06 §6 (so với snapshot của test)
- **P2-06:**
  - [ ] Đúng kích thước ở 07 §2 trên màn 1080×810
  - [ ] Thu gọn panel → grid giãn ra, không bị rebuild toàn cây
- **P2-07:**
  - [ ] Pointer-down tới `le_send` < 1 frame (đo bằng `os_signpost`)
  - [ ] 8 trạng thái vẽ đúng 07 §2
  - [ ] Nhấp nháy khi queued bám theo `beat`
- **P2-08 → P2-09:**
  - [ ] Meter chạy 60fps, không rebuild widget
  - [ ] M/S/Arm gửi đúng lệnh
- **P2-10:**
  - [ ] Tap tempo lấy trung bình 4 lần chạm, dải 20–300
  - [ ] Kéo BPM gửi tối đa 1 lệnh mỗi frame
- **P2-11:**
  - [ ] Thu 4 bar → clip xuất hiện, được autosave, mở lại vẫn còn
- **P2-12:**
  - [ ] Perform: nhấn giữ không làm gì
  - [ ] Edit: tap để chọn, nhấn giữ mở menu
  - [ ] Mọi thao tác cập nhật cả model lẫn engine
- **P2-14 → P2-15:**
  - [ ] Fader và knob mượt khi kéo nhiều ngón cùng lúc
  - [ ] Chạm đúp về mặc định
- **P2-16 → P2-17:**
  - [ ] 10 ngón chạm cùng lúc → 10 nốt, không có nốt treo khi nhấc tay hoặc khi gesture bị huỷ
- **P2-18 → P2-19:**
  - [ ] Thu MIDI 2 bar → hiện đúng nốt
  - [ ] Quantize sửa được, lưu được
- **P2-20:**
  - [ ] Waveform 60 giây, zoom 3 mức, không giật
  - [ ] Picture được cache theo `(clipId, zoom)`
- **P2-21:**
  - [ ] Đổi vùng loop → engine phát đúng, lưu đúng
- **P2-22:**
  - [ ] Gán kit vào track instrument, gán loop vào ô, và nghe được ngay
- **P2-23 → P2-25:**
  - [ ] Kill app ngay trong lúc lưu → mở lại không mất project (dùng `.bak`)
  - [ ] Cảnh báo khi dùng tai nghe Bluetooth
- **P2-26:**
  - [ ] Có file license cho mọi nội dung trong `content/LICENSES/`
- **P2-27 (M2):**
  - [ ] Đạt checklist M2 (02 §3)
  - [ ] Đã ghi 08 §8
