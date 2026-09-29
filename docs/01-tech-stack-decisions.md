# Music Looper App — Quyết định công nghệ (v1)

> Chốt ngày 2026-09-29 sau phiên grill-me. Ưu tiên số 1: **hiệu năng âm thanh**.
> Bối cảnh: làm solo + AI, gần như chưa dùng C++, full-time, mục tiêu TestFlight sau 6–8 tháng.

---

## 1. Tóm tắt các quyết định

| # | Chủ đề | Quyết định |
|---|---|---|
| 1 | Nền tảng | **iPadOS** trước (Android/iPhone phase sau, lõi C++ port được) |
| 2 | "Tách tone" | **Chromatic sampler**: thu 1 âm → chơi được mọi cao độ |
| 3 | Pitch-shift | **Pre-render offline** mỗi 3 nửa cung, giữ formant + chế độ **Classic** (resample) |
| 4 | Looping | **Clip grid kiểu Session View** (track × scene, BPM, quantize, audio + MIDI clip) |
| 5, 8 | Kiến trúc | **Engine C++ (JUCE, headless)** + **UI Flutter** qua `dart:ffi` |
| 9 | Âm thanh có sẵn | **Chỉ sample** (drum kit, nhạc cụ multi-sample, loop); 1 sampler engine dùng chung |
| 10 | Máy tối thiểu | **A12** → iPad 8, iPad Air 3, iPad mini 5 |
| 11 | MVP có | FX cơ bản + Export, MIDI controller ngoài, Ableton Link |
| 12 | AUv3 | **Phase 2** (host), plugin mode phase 3. Engine thiết kế "slot" sẵn |
| 13 | Đổi BPM | **Hybrid**: Re-Pitch tức thì → render nền time-stretch → crossfade ở đầu ô nhịp |
| 14 | Kiếm tiền | Chưa. Offline 100%, **không backend** |
| 15 | Thời gian | Full-time, 6–8 tháng tới TestFlight |
| 16 | Form factor | **Chỉ iPad** (landscape) |
| 17 | Ngôn ngữ UI | **Tiếng Anh + tiếng Việt** (theo ngôn ngữ của iOS, mặc định tiếng Anh). Chốt 29/09/2026 |

**Nguyên tắc xuyên suốt:** audio thread chỉ **đọc buffer, resample, mix, chạy FX nhẹ**. Mọi DSP nặng (pitch-shift, time-stretch, dò cao độ, decode file) chạy trên background thread và kết quả được swap vào audio thread.

---

## 2. Kiến trúc tổng thể

```
┌───────────────────────────── Flutter UI (Dart) ─────────────────────────────┐
│  Grid · Sampler keyboard · Waveform · Mixer · Browser · Settings             │
│  Project model (source of truth khi soạn) · Riverpod/Bloc (tuỳ bạn quen)     │
└──────┬──────────────────────────────────────────────────────▲───────────────┘
       │ ① Lệnh: dart:ffi → C API → SPSC queue (không lock)    │ ③ Snapshot trạng thái:
       │                                                       │    playhead, meter, clip state
       │ ② Sự kiện hiếm: NativeCallable.listener ◄──────────── │    (shared memory + seqlock,
       │    (clip thu xong, render xong, lỗi)                  │     Dart đọc mỗi vsync)
┌──────▼───────────────────────── Engine C++20 (JUCE) ────────┴───────────────┐
│  Message/worker threads (không real-time)                                   │
│   • CommandProcessor → dựng AudioGraphSnapshot mới (immutable)              │
│   • PitchRenderer / WarpRenderer (Signalsmith Stretch)                      │
│   • Recorder writer (ghi đĩa) · Exporter (offline render) · File loader     │
│   • ReleasePool: huỷ object cũ mà audio thread đã trả lại                   │
│─────────────────────────────── atomic pointer swap ─────────────────────────│
│  AUDIO THREAD (Core Audio callback, 128 frames @ 48 kHz)                    │
│   Transport/Clock → ClipScheduler (quantize) → Tracks[8]                    │
│      Track = Source(AudioClipPlayer | Sampler) → FX slots → Mixer → Limiter │
│   ✗ không malloc/free · ✗ không lock · ✗ không I/O · ✗ không log · ✗ không ObjC│
└─────────────────────────────────────────────────────────────────────────────┘
```

### Ba kênh giao tiếp giữa UI và engine
1. **Lệnh (UI → engine):** hàm C phẳng `extern "C"`. Lệnh là struct POD cỡ cố định, đi qua `rigtorp::SPSCQueue` (MIT, dung lượng cố định, không cấp phát bộ nhớ). Không bao giờ truyền con trỏ tới object Dart.
2. **Trạng thái tần suất cao (engine → UI):** audio thread publish struct `LeState` (playhead, meter từng track, trạng thái từng clip) qua **seqlock/triple buffer**. Mỗi frame, Flutter dùng `Ticker` để gọi `le_read_state(&out)`, và hàm C++ này copy an toàn với atomic đúng chuẩn vào một struct cấp phát sẵn. Dart không tự đọc trực tiếp vùng nhớ đang bị ghi, vì Dart FFI không có atomic load. Không dùng stream, không dùng platform channel.
   - Lệnh chia làm hai đường: **RT** (`le_send`, struct POD 32 byte) và **cấu trúc** (`le_call`, JSON). Chi tiết ở [05-ffi-bridge.md](05-ffi-bridge.md).
3. **Sự kiện tần suất thấp (engine → UI):** `NativeCallable.listener` gọi từ **worker thread**, không bao giờ gọi từ audio thread.

### Thay đổi project trong lúc đang phát
UI sửa project → worker thread dựng một `AudioGraphSnapshot` **bất biến** mới (danh sách clip, zone sample, tham số FX) → `std::atomic` swap con trỏ → audio thread dùng bản mới từ block kế tiếp → gửi con trỏ bản cũ qua queue về `ReleasePool` để huỷ ngoài audio thread. **Audio thread không bao giờ `delete`.**

---

## 3. Tech stack chi tiết

### 3.1 Engine (C++)
| Hạng mục | Chọn | Ghi chú |
|---|---|---|
| Ngôn ngữ | C++20 | |
| Build | CMake + JUCE CMake API → static lib → **XCFramework** | Flutter iOS tìm symbol bằng `DynamicLibrary.process()` |
| Framework | **JUCE 9** — chỉ lấy các module `juce_core`, `juce_events`, `juce_audio_basics`, `juce_audio_devices`, `juce_audio_formats`, `juce_dsp` (thêm `juce_audio_processors` ở phase 2) | Không dùng `juce_gui_*`. Khởi tạo bằng `ScopedJuceInitialiser_GUI` trên main thread |
| Audio I/O | `juce::AudioDeviceManager` (Core Audio / AVAudioSession) | **Engine là nơi duy nhất quản lý AVAudioSession**. Không dùng plugin audio nào của Flutter |
| Pitch-shift / time-stretch | **Signalsmith Stretch** (MIT): `setTransposeSemitones`, `setFormantFactor`, `seek()` + `flush()` khi xử lý offline | Nếu formant của giọng chưa đạt, dự phòng **Rubber Band R3** (license thương mại) |
| Dò cao độ gốc | **Tự viết YIN** (~150 dòng, thuật toán công khai) | Tránh aubio vì GPL |
| Queue lock-free | `rigtorp::SPSCQueue` | |
| Đọc/ghi file | `juce::AudioFormatManager`, `AudioFormatWriter::ThreadedWriter` khi thu | Project lưu CAF/FLAC; export M4A qua AVFoundation (bridge Obj-C++) |
| FX | `juce::dsp`: StateVariableTPTFilter, DelayLine, Reverb, IIR EQ, Limiter | Mỗi track 2–3 slot + limiter trên master |
| MIDI | `juce::MidiInput` (CoreMIDI) + Bluetooth MIDI pairing | Map Launchpad/controller vào grid và sampler |
| Tempo sync | **LinkKit** (SDK Link dành riêng cho iOS) | Cần entitlement multicast + `NSLocalNetworkUsageDescription` |
| Test | Catch2 · **offline render test** (render N ô nhịp → so với file golden) | Chạy trên macOS, không cần iPad |

### 3.2 UI (Flutter)
| Hạng mục | Chọn |
|---|---|
| Bridge | FFI plugin package `engine_ffi`, sinh binding bằng `ffigen` từ header C |
| Widget tần suất cao (meter, playhead, waveform) | `CustomPainter(repaint: <Listenable do Ticker kích>)` + `RepaintBoundary`. **Không gọi `setState`** cho dữ liệu 60–120Hz (xem skill `flutter-repaint-check`) |
| Waveform | Engine tính sẵn peak (min/max theo nhiều mức zoom) trên worker thread. Flutter chỉ vẽ mảng peak rồi cache thành `Picture`/`Image` |
| State app | Riverpod hoặc Bloc (dùng cái bạn quen). Project model nằm ở Dart; engine chỉ giữ bản snapshot |
| Lưu project | Mỗi project là thư mục `MyJam.loopproj/` gồm `project.json` (grid, clip, nốt MIDI, FX, BPM) và `audio/*.caf`. Nằm trong Documents, hiện trong app Files, iCloud Drive đồng bộ miễn phí |

### 3.3 Cấu trúc repo đề xuất
> Bản chi tiết và chính thức nằm ở [09-dev-environment.md](09-dev-environment.md) §2. Repo nằm ở `/Users/apple/Desktop/MUSIC/music-app/`.
```
music-app/
├── engine/                 # C++20 + JUCE (git submodule)
│   ├── src/core/           # Transport, Clock, ClipScheduler, AudioGraphSnapshot
│   ├── src/dsp/            # Sampler, Voice, AudioClipPlayer, FX
│   ├── src/io/             # Recorder, file load/save, Exporter
│   ├── src/render/         # PitchRenderer, WarpRenderer, YIN, peak builder
│   ├── src/bridge/         # engine_api.h (extern "C") ← nguồn cho ffigen
│   ├── harness/            # app console/standalone trên macOS để dev nhanh
│   └── tests/              # Catch2 + golden render
└── app/                    # Flutter (chỉ iPad)
    └── packages/engine_ffi/
```

---

## 4. Thiết kế các phần lõi của engine

### 4.1 Transport và clock
- Nguồn thời gian duy nhất là **số sample** kể từ lúc bắt đầu phát, kiểu `int64`. Beat được tính từ số sample: `beat = samples × bpm / (60 × sampleRate)`.
- Khi có Link: `ableton::Link::captureAudioSessionState()` ở **đầu mỗi callback**, rồi ánh xạ host time sang beat. LinkKit có test plan riêng, bắt buộc phải qua hết trước khi nộp App Store.

### 4.2 Clip và quantize
- Trạng thái clip: `Empty → Recording → Playing ⇄ Stopped`, và mỗi trạng thái đích có thêm dạng **Queued**, chờ tới ranh giới quantize kế tiếp.
- Ranh giới quantize (1 bar, 1/2, 1/4…) được tính **chính xác đến từng sample bên trong block**. Nếu ranh giới rơi giữa block 128 frame thì tách block làm hai đoạn.
- Mỗi track chỉ phát 1 clip tại một thời điểm. Bấm Scene là xếp hàng lệnh launch cho cả hàng.
- **Mọi lần launch/stop đều được ghi thành sự kiện `(beat, track, clip)`**, để phase 2 làm Arrangement mà không phải sửa lõi.

### 4.3 Thu âm và bù latency
- Độ trễ round-trip `L = inputLatency + outputLatency` (theo số device báo) `+ calibrationOffset` (đo bằng loopback). Engine **thu dư L sample rồi bỏ L sample đầu** của take, nhờ vậy vòng loop khớp nhịp tuyệt đối (chi tiết ở 04 §5.3).
- Thêm màn **hiệu chỉnh latency** thủ công (phát tiếng click, thu lại qua mic, tự đo độ lệch), vì số liệu hệ thống báo có thể sai khi dùng tai nghe hoặc audio interface.
- Audio thread ghi vào ring buffer cấp phát sẵn. `ThreadedWriter` ghi xuống đĩa trên thread khác.

### 4.4 Sampler (dùng chung cho drum, nhạc cụ, tiếng tự thu)
- **64 voice cấp phát sẵn**. Voice stealing ưu tiên voice cũ nhất hoặc đang ở pha release nhỏ nhất.
- `Instrument = [Zone]`, mỗi zone gồm `{keyRange, velRange, rootNote, buffer}`. Drum kit là trường hợp đặc biệt: mỗi pad là một zone chỉ có 1 phím.
- Envelope ADSR, interpolation dùng **Hermite 4 điểm** (rẻ và đủ tốt khi resample ±1.5 nửa cung).
- **Chế độ Classic:** bỏ qua các zone đã render, chỉ dùng 1 zone gốc rồi resample, kể cả với khoảng cách xa.

### 4.5 Pipeline pre-render cho tiếng người dùng thu
```
Thu xong → trim khoảng lặng → YIN tìm rootNote (VD: A3, sai lệch +12 cent)
→ Signalsmith Stretch render ~8–12 zone (mỗi zone cách 3 nửa cung, giữ formant, giữ độ dài)
→ dựng Instrument mới → atomic swap vào audio thread
```
- Ước lượng: dưới 2 giây trên A12 với sample khoảng 4 giây (**phải đo thật ở tuần spike**).
- RAM: tiếng thu từ mic là mono, 12 zone × 4 giây × 48 kHz × float32 ≈ **9 MB mỗi instrument**.

### 4.6 Warp hybrid khi đổi BPM
1. BPM đổi (do bạn hoặc do Link) → clip audio lập tức chuyển sang **Re-Pitch** với `rate = bpmMới / bpmGốc`. Tiếng không bị ngắt, chỉ cao hoặc trầm hơn một chút.
2. Worker thread render lại clip bằng Signalsmith Stretch ở tempo mới, giữ nguyên cao độ.
3. Render xong → tới **đầu ô nhịp kế tiếp** thì crossfade khoảng 10ms sang bản mới, rồi đưa bản cũ vào ReleasePool.
4. Nếu BPM đổi liên tục (người dùng đang kéo thanh BPM) thì **debounce khoảng 300ms** mới bắt đầu render.

### 4.7 Interface slot (chuẩn bị cho AUv3)
```cpp
struct Processor {                       // Sampler, FX và AUv3 (phase 2) đều cài đặt interface này
    virtual void prepare(double sampleRate, int maxBlock) = 0;   // gọi ngoài audio thread
    virtual void process(AudioBlock&, MidiBuffer&) noexcept = 0; // [[clang::nonblocking]]
    virtual int  latencySamples() const noexcept = 0;            // để làm delay compensation sau này
    virtual ~Processor() = default;
};
```

---

## 5. Ngân sách hiệu năng (đo trên iPad 8 / A12)

| Chỉ số | Mục tiêu |
|---|---|
| Buffer | 128 frame @ 48 kHz (~2.7 ms). Dự phòng 256 nếu máy không cho |
| Round-trip latency (mic trong máy) | ≤ 15 ms sau khi bù |
| Tải CPU của audio thread | **< 50%** thời gian cho phép của mỗi callback, với 8 track đang phát + 64 voice + FX |
| Xrun (mất buffer) | **0** trong 30 phút jam liên tục |
| UI | 60 fps ổn định (iPad 8 không có ProMotion), không tụt frame khi đổi scene |
| RAM sample | < 400 MB tổng (iPad 8 có 3 GB RAM) |
| Pre-render 1 tiếng thu | < 2 s |

Công cụ đo: Instruments (Time Profiler, System Trace), `os_signpost` quanh mỗi callback, và bộ đếm xrun do engine tự hiển thị trên UI ở bản debug.

---

## 6. Quy tắc real-time và cách để máy kiểm tra hộ

Bạn mới học C++, nên **đừng dựa vào trí nhớ, hãy để công cụ bắt lỗi**:

1. Đánh dấu mọi hàm chạy trên audio thread bằng `[[clang::nonblocking]]`.
2. Build bản test trên macOS bằng **LLVM upstream (Homebrew)** với `-fsanitize=realtime` (RealtimeSanitizer). Chỉ cần có `malloc`, `free`, `mutex` hay I/O lọt vào audio thread là crash ngay kèm stack trace.
3. Chạy thêm các bản build với `-fsanitize=thread` và `-fsanitize=address` định kỳ.
4. Golden render test: mỗi thay đổi lớn phải render lại các project mẫu rồi so sánh (null test).
5. Cấm trên audio thread: `new/delete`, `std::vector::push_back`, `std::string`, `std::function` (có thể cấp phát), `std::shared_ptr` copy/destroy, `mutex`, log, Obj-C/Swift, gọi vào Dart.

Nên xem trước khi code: bài talk *"C++ in the Audio Industry"* và *"Real-time 101"* (Timur Doumler, Fabian Renn-Giles), cùng tài liệu RealtimeSanitizer.

---

## 7. Lộ trình (full-time)

> Lịch chi tiết theo tuần và các mốc go/no-go nằm ở [02-roadmap.md](02-roadmap.md). Task và DoD của từng phase nằm trong `phases/`.

| Giai đoạn | Thời gian | Nội dung | Mốc go/no-go |
|---|---|---|---|
| **0. Spike** | Tuần 1–2 | App Flutter iPad + JUCE static lib + FFI: phát sine, thu mic rồi phát lại, đo latency và CPU trên iPad 8. Chạy thử Signalsmith Stretch với 1 tiếng thu | JUCE chạy ổn khi nhúng trong Flutter, không tranh AVAudioSession. **Nếu fail → cân nhắc thay JUCE bằng Core Audio thuần + Oboe sau này** |
| **1. Engine headless** | Tháng 1–3 | Transport, clip + quantize, thu âm có bù latency, audio clip, sampler 64 voice, MIDI clip, mixer. Harness trên macOS, RTSan, golden test | Demo trên macOS: jam 8 track bằng phím tắt |
| **2. UI + bridge** | Tháng 3–5 | Grid, launch/record, sampler keyboard, waveform, mixer, lưu/mở project | Jam được trên iPad 8, đạt ngân sách ở mục 5 |
| **3. DSP nâng cao** | Tháng 5–6.5 | Pipeline pre-render pitch, warp hybrid, FX, Export WAV/M4A | Hát "la" rồi chơi giai điệu, đổi BPM không bị glitch |
| **4. Kết nối + hoàn thiện** | Tháng 6.5–8 | MIDI controller, LinkKit (qua test plan), tối ưu hiệu năng, sửa lỗi, TestFlight | 0 xrun trong 30 phút, qua hết test Link |
| Phase 2 | Sau MVP | AUv3 host, Arrangement view, slice (C), melody→MIDI (D), synth, tách stems bằng AI | |
| Phase 3 | | AUv3 plugin mode, iPhone, Android (JUCE → Oboe/AAudio) | |

---

## 8. Việc hành chính nên làm ngay (tốn thời gian chờ)

- [ ] **Xin entitlement `com.apple.developer.networking.multicast`** từ Apple cho LinkKit. Apple duyệt thủ công, có thể mất vài tuần.
- [ ] Đọc **Ableton Link SDK license** và UI guidelines của LinkKit.
- [ ] Đăng ký **JUCE Starter** (miễn phí khi doanh thu dưới $20k). Nếu sau này có doanh thu thì chuyển sang Indie hoặc Pro.
- [ ] Chọn nguồn sound pack có **quyền phân phối lại bên trong app** (CC0 hoặc license thương mại ghi rõ "redistribution in software"). Lưu file license vào repo.
- [ ] Mua **iPad 8 cũ** làm máy test chuẩn.

---

## 9. Rủi ro chính và cách giảm

| Rủi ro | Mức | Giảm thiểu |
|---|---|---|
| Học C++ real-time trong lúc làm | Cao | C++ chỉ nằm trong engine. RTSan/TSan/ASan + golden test. Làm engine headless trên macOS trước |
| Nhúng JUCE trong Flutter gặp lỗi AVAudioSession hoặc message loop | Trung bình | Spike tuần 1–2 là **go/no-go** |
| Chất lượng formant của Signalsmith chưa đủ với giọng hát | Trung bình | Thử ở tuần spike, dự phòng Rubber Band R3 |
| Latency thực tế khác số hệ thống báo | Trung bình | Màn hiệu chỉnh latency bằng click loopback |
| Mở rộng phạm vi giữa chừng | Cao | Bảng phase ở mục 7 là giới hạn cứng. Ý tưởng mới đưa vào phase 2 |

---

## Nguồn đã kiểm tra (2026-09-29)
- JUCE licensing: https://juce.com/get-juce/
- Signalsmith Stretch (MIT, formant API): https://github.com/Signalsmith-Audio/signalsmith-stretch
- Ableton Link (dual license, iOS dùng LinkKit): https://github.com/Ableton/link · https://ableton.github.io/linkkit/
- RealtimeSanitizer: https://clang.llvm.org/docs/RealtimeSanitizer.html
