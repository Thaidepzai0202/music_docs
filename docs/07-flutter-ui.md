# 07 — Flutter UI

> Chỉ làm iPad, chỉ landscape, dark theme. Máy chuẩn là **iPad 8**: 1080×810 pt @2x, 60 Hz, không có ProMotion.
> Mục tiêu: **60 fps ổn định**, **chạm tới lúc lệnh được xếp hàng < 1 frame**.

---

## 1. Sơ đồ màn hình

```
Splash/Onboarding (lần đầu: xin quyền mic, mở project demo)
   └─► Projects (danh sách, tạo mới, đổi tên, nhân bản, xoá)
          └─► Session (màn chính)
                 ├─ Bottom panel: [Clip] [Instrument] [Mixer] [FX] [Browser]
                 ├─ Sheet: Record-to-Sampler (P3)
                 ├─ Sheet: Export (P3)
                 └─ Settings (Audio · Latency · MIDI · Link · Giấy phép · Giới thiệu)
```

---

## 2. Màn Session (wireframe, 1080×810 pt)

```
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│ ◀ My Jam   ▶ ■  ● REC   120.0 BPM [TAP]  Q: 1 Bar ▾  ♩ Metro  ⟲ Link(2)   CPU 23%  ✎ Edit │ 56pt
├──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬───┤
│ Drums    │ Bass     │ Vocal  ● │ La Synth │ Track 5  │ Track 6  │ Track 7  │ Track 8  │   │ 64pt
│ ▮▮▮▯ M S │ ▮▮▯▯ M S │ ▮▯▯▯ M S │ ▯▯▯▯ M S │          │          │          │          │   │ (tên, meter, arm, M/S)
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┼───┤
│[Beat A ▶]│[Bass A ▶]│[Hát 1  ▶]│[Melody  ]│[    ○   ]│          │          │          │ ▶ │ Scene 1
│[Beat B  ]│[Bass B  ]│[ ◌◌◌◌◌  ]│[Hợp âm  ]│          │          │          │          │ ▶ │ Scene 2 (◌ = queued)
│[Fill    ]│          │          │[Lead    ]│          │          │          │          │ ▶ │ Scene 3
│   …      │          │          │          │          │          │          │          │ ▶ │ … (8 hàng × ~48pt)
├──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┼───┤
│ [Clip] [Instrument] [Mixer] [FX] [Browser]                                    ▾ thu gọn│   │
│  (panel ngữ cảnh, cao 260pt, thu gọn còn 44pt → grid được giãn ra)                     │ ■ │ Stop all
└────────────────────────────────────────────────────────────────────────────────────────┴───┘
```
- Mỗi cột track rộng khoảng 118 pt. Cột scene rộng 56 pt. Ô clip cao tối thiểu 44 pt (theo chuẩn chạm của Apple).
- Màu ô clip lấy theo màu track. Trạng thái được vẽ như sau:

| Trạng thái | Hiển thị |
|---|---|
| Empty | Viền mờ. Có nút ○ khi track đang arm |
| Stopped | Nền màu track, độ đậm 40% |
| QueuedPlay / QueuedStop | Nhấp nháy theo phách (lấy `beat` từ state, không dùng timer riêng) |
| Playing | Nền đậm, có thanh tiến độ ở cạnh dưới (`trackClipProgress`) |
| QueuedRecord / Recording | Viền đỏ, đang thu thì nền đỏ và có thanh tiến độ |
| Overdubbing | Nền màu track, có sọc đỏ |
| Missing file | Xám, có biểu tượng ⚠ |

---

## 3. Tương tác

### 3.1 Hai chế độ (nút ✎ ở top bar)
| | **Perform** (mặc định) | **Edit** |
|---|---|---|
| Chạm ô có clip | **Pointer-down → launch ngay** | Chọn clip (mở panel Clip) |
| Chạm ô trống, track đang arm | Pointer-down → record | Chọn ô |
| Chạm ô trống, track không arm | Dừng track | Chọn ô |
| Nhấn giữ | – (không làm gì để tránh nhầm khi đang biểu diễn) | Menu: nhân bản, xoá, đổi tên, màu, copy/paste |
| Kéo thả | – | Di chuyển hoặc copy clip (P2-21) |

> Lý do: trong Perform, dùng `Listener.onPointerDown` thay vì `onTap`. `onTap` chỉ chạy **khi nhấc tay** và còn phải chờ phân biệt với các gesture khác, nên thêm 50–300ms độ trễ. Đây là độ trễ quyết định cảm giác nhạy của app.

### 3.2 Pad & bàn phím (panel Instrument)
- **Pad 4×4** (drum kit) và **bàn phím 2 quãng tám** (có nút dịch quãng ±, khoá theo scale là tuỳ chọn ở phase 2).
- **Multi-touch:** một `Listener` bao cả panel, tự theo dõi `pointer id → note`. Pointer-down gửi `NOTE_ON` với velocity lấy từ vị trí chạm theo chiều dọc của phím, hoặc cố định 0.8. Pointer-up hoặc cancel gửi `NOTE_OFF`. Trượt sang phím khác thì gửi note-off cho phím cũ và note-on cho phím mới.
- Không dùng `GestureDetector` cho từng phím (chậm và tranh chấp gesture arena).

### 3.3 Fader & knob
- Kéo dọc để đổi giá trị. Kéo chậm để tinh chỉnh: giữ thêm một ngón hoặc kéo ra xa theo chiều ngang. Chạm đúp để về mặc định.
- Gửi `TRACK_GAIN` / `FX_PARAM` **tối đa 1 lần mỗi frame** (gom lại), vì engine có smoothing sẵn.

### 3.4 Phản hồi
- Haptic nhẹ khi launch hoặc record (`HapticFeedback.selectionClick`). Có thể tắt trong Settings.
- Màn hình luôn sáng khi transport đang chạy (`wakelock_plus`).

---

## 4. Các flow chính

### 4.1 Thu một clip audio
1. Bật arm (●) trên header track Vocal. App gửi `TRACK_ARM`; engine cấp phát buffer trước khi đưa lệnh vào hàng đợi.
2. Chạm vào ô trống, app gửi `CLIP_RECORD(track, slot, bars = mặc định trong Settings)`.
3. Ô chuyển sang QueuedRecord (nhấp nháy đỏ). Có count-in thì metronome kêu.
4. Recording, thanh tiến độ chạy. Đủ số bar thì chuyển sang Playing.
5. Event `RECORDING_FINISHED`: gọi `clip.info`, cập nhật model rồi autosave.

### 4.2 Thu tiếng rồi biến thành sampler (P3)
1. Mở panel Instrument trên track instrument, bấm "Thu âm mới".
2. Mở sheet có nút thu lớn, meter input, tự dừng sau 4 giây (hoặc bấm dừng).
3. Hiện waveform để chỉnh điểm đầu và cuối (đã trim sẵn).
4. Hiện nốt gốc dò được, ví dụ "A3 +12 cent". Người dùng có thể đổi. Confidence thấp thì bắt buộc phải chọn.
5. Bấm "Tạo nhạc cụ" (Natural hoặc Classic). Có progress bar trong lúc render (< 2 giây). Xong thì bàn phím chơi được luôn.

### 4.3 Mở và lưu project
Theo 06 §5–6. Màn loading hiện số job còn lại. Mọi lỗi được hiện dạng banner, không chặn app.

### 4.4 Export (P3)
Sheet có 2 tab. Tab "Ghi buổi jam" (nút REC master), tab "Export scene" (chọn scene, số bar, WAV hoặc M4A, stems). Có progress, xong thì mở share sheet.

---

## 5. Kiến trúc state

```
                    ┌──────────────── Riverpod ────────────────┐
 UI widgets ──────► │ projectControllerProvider (Notifier)     │── engine.call/send ──► EngineClient
   (đọc/sửa)        │   state: Project (freezed, bất biến)      │◄─ events ────────────┘
                    │ jobsProvider · settingsProvider · …      │
                    └──────────────────────────────────────────┘
 Widget tần suất cao ◄── EngineStateTicker (ChangeNotifier, KHÔNG dùng Riverpod)
   (meter, playhead,       ├─ Ticker mỗi frame: engine.readState() → cập nhật EngineState (mutable)
    trạng thái clip)       ├─ notifyListeners() → chỉ các CustomPainter đã đăng ký repaint
                           └─ 64 ValueNotifier<ClipState>: chỉ báo khi ô đó ĐỔI trạng thái
```
- **Mọi thay đổi project đi qua `ProjectController`.** Controller cập nhật model (bất biến), gửi lệnh tương ứng sang engine, rồi hẹn autosave. Widget không bao giờ gọi `EngineClient` trực tiếp, trừ đường nóng: nốt nhạc, launch, fader. Các đường này đi qua `PerformanceActions` để gọi `send` ngay mà không đợi rebuild.
- `EngineStateTicker` chạy một `Ticker` duy nhất cho toàn app, và **chỉ chạy khi màn Session đang hiển thị**.

---

## 6. Quy tắc hiệu năng UI (bắt buộc)

1. Meter, playhead, thanh tiến độ, nhấp nháy queued: `CustomPainter(repaint: ticker)` bọc trong `RepaintBoundary`. **Không gọi `setState` hay dùng Riverpod** cho dữ liệu 60 Hz.
2. Mỗi ô clip là một `RepaintBoundary` và chỉ rebuild khi `ValueNotifier<ClipState>` của chính nó đổi.
3. Waveform: vẽ từ mảng peaks **một lần** vào `ui.Picture` hoặc `Image`, cache theo `(clipId, zoom)`. Playhead vẽ ở một lớp riêng phía trên.
4. Cấm dùng `BackdropFilter` (blur), `Opacity` động, `saveLayer`, `ClipRRect` lồng nhau trong grid. Các thứ này rất tốn trên GPU A12.
5. Dùng widget `const` tối đa. Không tạo `Paint` hoặc `Path` mới trong `paint()`: tạo sẵn một lần ở field.
6. Mọi lệnh gửi từ gesture phải được gửi **trước** khi cập nhật UI (UI được cập nhật theo state đọc ở frame sau).
7. Kiểm tra bằng "Highlight repaints" và Performance overlay trên iPad 8 (skill `flutter-repaint-check`) trước khi đóng mỗi task UI.

---

## 7. Cấu trúc thư mục `app/lib`

```
lib/
├── main.dart                   # khởi tạo EngineClient, ProviderScope, khoá landscape
├── app/                        # router, theme (tokens), l10n
├── engine/                     # wrapper cấp cao: EngineService, EngineStateTicker, PerformanceActions, JobTracker
├── model/                      # Project, Track, Clip, Note, Instrument… (freezed + json) + migrations/
├── data/                       # ProjectRepository (đọc/ghi an toàn), LibraryRepository (manifest)
├── features/
│   ├── projects/               # danh sách project
│   ├── session/                # top bar, grid, track header, scene column
│   ├── instrument/             # pad, keyboard, record-to-sampler sheet
│   ├── clip/                   # waveform, trình xem/sửa MIDI
│   ├── mixer/
│   ├── fx/
│   ├── browser/
│   ├── export/
│   └── settings/               # audio, latency, midi, link, giấy phép
└── ui_kit/                     # Fader, Knob, Meter, ClipCell, PadButton, KeyboardView… (painter tối ưu)
packages/engine_ffi/            # binding ffigen + EngineClient (05 §4) + podspec link XCFramework
```

### Package dùng
`flutter_riverpod`, `freezed` + `json_serializable`, `ffi`, `ffigen` (dev), `path_provider`, `share_plus`, `uuid`, `wakelock_plus`. Quyền mic và mở màn Link hoặc BLE MIDI được làm trong code Swift của plugin `engine_ffi` qua MethodChannel. Các thao tác này hiếm, nên độ trễ của MethodChannel không thành vấn đề.

---

## 8. Design tokens (tối thiểu)
- Nền `#0E0F12`, bề mặt `#17191E`, viền `#262A31`, chữ chính `#E8EAED`, chữ phụ `#9AA0A6`
- Nhấn: đỏ record `#FF3B30`, xanh play `#34C759`, vàng queued `#FFD60A`
- 8 màu track (có độ tương phản tốt trên nền tối): `#FF7A59 #FFB547 #F5E663 #7BD88F #59C3FF #7A8CFF #B98CFF #FF7AC6`
- Font: SF Pro (hệ thống). Số (BPM, dB) dùng `FontFeature.tabularFigures()` để không nhảy chữ.
