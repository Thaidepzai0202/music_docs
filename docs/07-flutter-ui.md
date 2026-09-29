# 07 — Flutter UI

> Chỉ làm iPad, chỉ landscape, dark theme. Máy chuẩn là **iPad 8**: 1080×810 pt @2x, 60 Hz, không có ProMotion.
> Mục tiêu: **60 fps ổn định**, **chạm tới lúc lệnh được xếp hàng < 1 frame**.

---

## 1. Sơ đồ màn hình

```
LaunchScreen gốc của iOS (không có màn Splash riêng) → Onboarding (lần đầu: xin quyền mic, mở project demo)
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
│ ◀ My Jam   ▶ ■  1.3.2  120.0 BPM [TAP]  Q: 1 Bar ▾  ♩▾  ● REC  ⟲ Link(2)  CPU 23%  ✎  ⋮ │ 56pt
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
- Top bar (bản thật): `♩▾` là menu metronome gồm bật/tắt/khi thu, **âm lượng** và count-in. `● REC` = ghi buổi jam. `⋮` = Export… và Cài đặt. `⟲ Link(n)` chỉ hiện khi đã tích hợp LinkKit.
- Mỗi cột track rộng `(1080 − 56) / 8 = 128 pt` (lấp đầy bề ngang). Cột scene rộng 56 pt. Ô clip cao tối thiểu 44 pt (theo chuẩn chạm của Apple).
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

### 3.1b Nút LOOP (pedal mode, người dùng chọn 29/09)
- Nút tròn lớn **● LOOP** (khoảng 72pt) ở cột phải của Session, cạnh Stop all. Nút tác động lên **track đang chọn**, và ô đích là ô đang chọn, nếu không có thì ô trống đầu tiên của track.
- **Chạm** (pointer-down) gửi `LE_CMD_LOOP_BUTTON(track, slot = ô đang chọn hoặc -1)`. Engine xoay vòng giống loop pedal (05 §3):
  1. Ô trống → **thu** (`CLIP_RECORD i0 = 0`).
  2. Đang thu → **chốt vòng và phát** (`RECORD_STOP`).
  3. Đang phát → **overdub** (`OVERDUB_TOGGLE`).
  4. Đang overdub → **phát** (`OVERDUB_TOGGLE`).
- **Nút LOOP chỉ có một cử chỉ là chạm**, không có chạm đúp hay nhấn giữ. Lệnh được gửi lúc pointer-down để giữ đúng thời điểm thu, nên mọi cử chỉ khác đều sẽ kích hoạt lệnh ở lần chạm đầu. Ví dụ nhấn giữ để hoàn tác sẽ **bật overdub mới** trước rồi mới hoàn tác đúng lớp vừa bật.
- **Hai nút riêng cạnh LOOP** (44pt, ngay dưới LOOP ở cột phải):
  - **■ Dừng track:** gửi `CLIP_STOP(track đang chọn)` theo quantize.
  - **↶ Hoàn tác:** gọi `clip.undoOverdub` cho ô đang phát của track. Không có lớp overdub thì hỏi "Xoá clip?" rồi mới `clip.clear`.
- Màu và nhãn đổi theo trạng thái: đỏ khi thu, xanh khi phát, cam khi overdub. Có vòng tiến độ của loop.
- **Chế độ tempo** ở menu ♩: `BPM cố định` | `Vòng đầu quyết định BPM`. Project mới **mặc định dùng pedal mode**. Khi đang chờ vòng đầu, top bar hiện "— BPM · chờ vòng đầu" thay cho số BPM, và metronome bị tắt.
- **Độ dài thu** ở Settings → Thu âm, dạng segmented `[1 · 2 · 4 · 8 bar · Tự do]`, là **thiết lập chung của app**. Khi chọn Tự do, chạm ô trống trên grid để bắt đầu thu (`CLIP_RECORD i0 = 0`), chạm lần nữa để chốt (`RECORD_STOP`).
- Nút LOOP là một đích MIDI learn (`{kind:"loopButton"}`), để nối với footswitch.

### 3.2 Pad & bàn phím (panel Instrument)
- **Pad 4×4** (drum kit) và **bàn phím 2 quãng tám** (có nút dịch quãng ±, khoá theo scale là tuỳ chọn ở phase 2).
- **Multi-touch:** một `Listener` bao cả panel, tự theo dõi `pointer id → note`. Pointer-down gửi `NOTE_ON` với velocity lấy từ vị trí chạm theo chiều dọc của phím, hoặc cố định 0.8. Pointer-up hoặc cancel gửi `NOTE_OFF`. Trượt sang phím khác thì gửi note-off cho phím cũ và note-on cho phím mới.
- Không dùng `GestureDetector` cho từng phím (chậm và tranh chấp gesture arena).

### 3.3 Fader & knob
- Kéo dọc để đổi giá trị. Kéo chậm để tinh chỉnh: giữ thêm một ngón hoặc kéo ra xa theo chiều ngang. Chạm đúp để về mặc định.
- Gửi `TRACK_GAIN` / `FX_PARAM` **tối đa 1 lần mỗi frame** (gom lại), vì engine có smoothing sẵn.
- **Trong lúc kéo chỉ gửi lệnh, CHƯA ghi model. Thả tay mới ghi model** (một lần). Nhãn giá trị dùng `ValueNotifier` trong ô kích thước cố định, có boundary riêng. Có test ngân sách rebuild khoá lại (`test/perf/rebuild_budget_test.dart`).

### 3.4 Phản hồi
- Haptic nhẹ khi launch hoặc record (`HapticFeedback.selectionClick`). Có thể tắt trong Settings.
- Màn hình luôn sáng khi transport đang chạy: plugin `engine_ffi` đặt `UIApplication.isIdleTimerDisabled` bằng Swift (không dùng `wakelock_plus`, bớt được một plugin bên thứ ba). Chỉ gọi native khi cờ `playing` đổi.

---

## 4. Các flow chính

### 4.0 Khi người dùng từ chối quyền mic
Người dùng chọn "Để sau" ở onboarding, hoặc từ chối quyền: app **không hỏi lại ngay**, mà **chỉ hỏi khi lần đầu làm việc cần micro** (arm track audio, nút LOOP trên track audio, Thu âm mới, đo trễ). Được cấp quyền thì làm tiếp thao tác đang dở. Trong lúc chưa có quyền, app **vẫn chạy ở chế độ chỉ phát**: gọi `audio.setInputEnabled {enabled:false}`, hiện banner "Chưa có quyền micro: chỉ phát được, chưa thu được" kèm nút "Mở Cài đặt", và khoá các nút thu. Khi quay lại app mà đã được cấp quyền thì gọi `audio.setInputEnabled {enabled:true}`.

### 4.1 Thu một clip audio
1. Bật arm (●) trên header track Vocal. App gửi `TRACK_ARM`; engine cấp phát buffer trước khi đưa lệnh vào hàng đợi.
2. Chạm vào ô trống, app gửi `CLIP_RECORD(track, slot, bars = mặc định trong Settings)`.
3. Ô chuyển sang QueuedRecord (nhấp nháy đỏ). Có count-in thì metronome kêu.
4. Recording, thanh tiến độ chạy. Đủ số bar thì chuyển sang Playing.
5. Event `RECORDING_FINISHED`: gọi `clip.info`, cập nhật model rồi autosave.

### 4.1b Vẽ nốt trong piano roll (người dùng chọn 29/09)
- Tab Clip của clip MIDI có hai chế độ: **✏️ Vẽ** và **⬚ Chọn** (chế độ Chọn là hành vi hiện tại).
- **Chế độ Vẽ:**
  - Chạm vào ô lưới trống → **thêm nốt**, snap theo lưới. Lưới chọn được 1/4 · 1/8 · **1/16** (mặc định) · 1/32. Độ dài mặc định bằng 1 ô lưới, velocity 100. Cao độ lấy theo hàng.
  - Chạm vào nốt có sẵn → **xoá** nốt đó.
  - Kéo mép phải của nốt → đổi độ dài. Kéo thân nốt → dời vị trí và cao độ (snap).
  - Mỗi nốt vừa thêm được **nghe thử** ngay (`NOTE_ON`, `NOTE_OFF` sau 150ms) trên track đó.
- **Trục cao độ:**
  - Track kit: mỗi hàng là một pad và hiện tên pad (lấy từ SFZ). Chỉ hiện các phím có sample.
  - Track nhạc cụ: hiện phím đàn, cuộn dọc được, có nút dịch ±1 quãng tám.
- **Trục thời gian:** zoom 1×/2×/4×, cuộn ngang. Có nút đổi độ dài clip (1/2/4/8 bar).
- Mỗi thao tác xong (thả tay) thì gửi `clip.setMidi` rồi ghi vào model. Kéo liên tục thì chỉ gửi khi thả tay. Có **undo/redo** cho các thao tác sửa nốt (giữ 50 bước, theo từng clip, chỉ trong phiên).
- **Nút ⤢ phóng to:** panel dưới cao lên khoảng 70% màn hình để vẽ nốt thoải mái. Bấm lại hoặc đổi tab thì trở về như cũ.
- **Tạo clip MIDI trống:** ở chế độ Edit, nhấn giữ ô trống của track instrument → "Clip MIDI trống (1/2/4 bar)" → mở piano roll ở chế độ Vẽ.
- Clip đang phát mà bị sửa: engine áp nội dung mới **ở lần đi qua kế tiếp**, không có nốt treo (04 §7).

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
- **Tầng gọi engine:** widget → `ProjectController` (mọi thay đổi **project**) hoặc **service theo tính năng** (`MidiService`, `ExportService`, `CaptureService`, `PeaksService`, `LatencyService`: các op không đổi project) → `EngineClient`. Widget không gọi `EngineClient` trực tiếp, trừ `PerformanceActions` cho đường nóng.
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
└── ui_kit/                     # Fader, Knob, Meter, PadButton, KeyboardView… (painter tối ưu). ClipCell nằm ở features/session/widgets
packages/engine_ffi/            # binding ffigen + EngineClient (05 §4) + podspec link XCFramework
```

### Package dùng
`flutter_riverpod`, `freezed` + `json_serializable`, `ffi`, `ffigen` (dev), `path_provider`, `share_plus`, `uuid`, `clock` (để test được thời gian). Quyền mic và mở màn Link hoặc BLE MIDI được làm trong code Swift của plugin `engine_ffi` qua MethodChannel. Các thao tác này hiếm, nên độ trễ của MethodChannel không thành vấn đề.

---

## 8. Ngôn ngữ (l10n)
- **Hỗ trợ `en` + `vi`.** App theo ngôn ngữ iOS. Ngôn ngữ khác `vi` thì **dùng `en`**.
- Khai báo `CFBundleLocalizations = [en, vi]` trong Info.plist. Nhờ vậy người dùng đổi được ngôn ngữ riêng cho app trong Cài đặt iOS (Settings → Music Looper → Language), và app **không cần** màn chọn ngôn ngữ riêng.
- Dùng `flutter_localizations` + `gen-l10n` với `lib/l10n/app_en.arb` (file mẫu) và `app_vi.arb`. Key giữ đúng như slug trong `strings.dart` hiện có. Chuỗi có tham số dùng placeholder ICU (số nhiều cho tiếng Anh, ví dụ "1 peer" / "2 peers").
- **Thông báo lỗi:** UI map từ **mã lỗi** (`LeError`, `error.code`) sang chuỗi đã dịch. **Không bao giờ** hiện nguyên `message` của engine, vì đó là chuỗi cho dev.
- Test bắt buộc:
  - Hai file ARB có cùng tập key và cùng placeholder.
  - Không có chuỗi viết cứng trong `lib/features` và `lib/ui_kit` (luật tĩnh sẵn có).
  - Layout stress test chạy ở **cả hai ngôn ngữ**, vì tiếng Anh và tiếng Việt dài ngắn khác nhau.
- Metadata App Store (P4-23) cũng làm cả hai ngôn ngữ.

## 9. Design tokens (tối thiểu)
- Nền `#0E0F12`, bề mặt `#17191E`, viền `#262A31`, chữ chính `#E8EAED`, chữ phụ `#9AA0A6`
- Nhấn: đỏ record `#FF3B30`, xanh play `#34C759`, vàng queued `#FFD60A`
- 8 màu track (có độ tương phản tốt trên nền tối): `#FF7A59 #FFB547 #F5E663 #7BD88F #59C3FF #7A8CFF #B98CFF #FF7AC6`
- Font: SF Pro (hệ thống). Số (BPM, dB) dùng `FontFeature.tabularFigures()` để không nhảy chữ.
