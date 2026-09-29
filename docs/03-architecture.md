# 03 — Kiến trúc hệ thống

> Đọc cùng: [04-engine-design.md](04-engine-design.md) (bên trong từng subsystem), [05-ffi-bridge.md](05-ffi-bridge.md) (hợp đồng giữa UI và engine).

---

## 1. Bối cảnh

```
                    ┌───────────────────────── iPad ─────────────────────────┐
  Người dùng ──────►│  Flutter UI (Dart, main thread)                        │
  (chạm, MIDI)      │        │ dart:ffi (gọi đồng bộ, rẻ)                    │
                    │        ▼                                               │
  Mic / interface ─►│  LoopCore engine (C++20 + JUCE, static lib)            │──► Loa / tai nghe
  MIDI controller ─►│        │                                               │
  Peer Link (Wi-Fi)◄┼────────┘ LinkKit                                       │
                    │  Documents/ (project, audio) ◄── app Files / iCloud    │
                    └────────────────────────────────────────────────────────┘
```
Không có backend. Không có mạng, trừ Ableton Link chạy trong mạng LAN.

---

## 2. Mô hình thread

| Thread | Ai tạo | Chạy gì | Được phép | Cấm |
|---|---|---|---|---|
| **Main** (iOS main = Dart UI, từ Flutter 3.29 = JUCE message thread) | iOS | Toàn bộ Dart, mọi hàm C API `le_*`, `CommandProcessor`, `juce::Timer` | Mọi thứ, nhưng mỗi thao tác phải < 2ms để UI không giật | Tính toán nặng (đẩy sang worker) |
| **Audio** (Core Audio IO) | Core Audio / JUCE | `RtEngine::process()` | Đọc snapshot, toán số, ghi vào buffer cấp phát sẵn, thao tác SPSC queue, `std::atomic` | `malloc/free/new/delete`, lock, I/O, log, Obj-C/Swift, gọi Dart, `std::function` cấp phát, chờ bất cứ thứ gì |
| **MIDI in** | CoreMIDI | Callback của `juce::MidiInput` | Đẩy vào SPSC `midiToRt` kèm timestamp | Giống audio thread (coi như RT) |
| **Worker ×2** | `juce::ThreadPool` | PitchRenderer, WarpRenderer, decode file, PeakBuilder, Exporter, SFZ loader | Mọi thứ | Chạm trực tiếp vào state của RT |
| **Disk writer** | `juce::TimeSliceThread` | `AudioFormatWriter::ThreadedWriter` khi thu âm | Ghi đĩa | — |

**Nguyên tắc vàng:** giữa audio thread và phần còn lại chỉ có **3 cơ chế**: (1) SPSC queue, (2) `std::atomic` (con trỏ snapshot, cờ), (3) state publisher (seqlock). Không có cơ chế thứ tư.

---

## 3. Luồng dữ liệu

```
 Dart (main)                       C++ main-thread side                     Audio thread
 ───────────                       ────────────────────                     ────────────
 le_send(cmd) ───────────────────► side effect phía NRT (nếu có) ─► SPSC rtCommands ──► drain đầu mỗi block
   (launch, note, gain...)          VD: SET_BPM → xếp job warp

 le_call(json) ──────────────────► CommandProcessor
   (setClip, setInstrument,          ├─ cập nhật EngineModel (NRT)
    loadProject, createInstr...)     ├─ dựng GraphSnapshot mới (hoặc tạo job worker)
                                     └─ atomic store(newSnapshot) ────────────► atomic load đầu mỗi block
                                                                                 │
 ReleasePool (Timer 30Hz) ◄──────── SPSC rtToNrt (snapshot cũ, event) ◄─────────┘
   delete snapshot cũ               ├─ RecordingFinished, TempoChanged(Link), Xrun
   ├─ event → NativeCallable ─────► Dart listener (bất đồng bộ)
   └─ job cho worker (VD: warp khi Link đổi tempo)

 le_read_state(&out) ◄──────────── StatePublisher (seqlock/triple buffer) ◄─── ghi cuối mỗi block
   (Ticker mỗi frame)                playhead, meter, clipState[8][8], cpu, xrun
```

### 3.1 Hai đường lệnh

| | Đường RT (nhanh) | Đường cấu trúc (chậm) |
|---|---|---|
| API | `le_send(const LeCommand*)` | `le_call(const char* json)` trả về JSON |
| Định dạng | Struct POD 32 byte | JSON (`juce::JSON`) |
| Tần suất | Cao: chạm, nốt, fader | Thấp: sửa cấu trúc project |
| Đích | SPSC tới audio thread | `CommandProcessor` → snapshot mới hoặc job |
| Ví dụ | launch clip, note on/off, gain, tham số FX, BPM | gán file vào clip, đổi instrument, thêm FX, load project, export |

Lý do tách hai đường: đường RT cần độ trễ thấp và không được cấp phát bộ nhớ. Đường cấu trúc cần linh hoạt và dễ mở rộng. JSON cho phép thêm lệnh mới mà không phải sửa FFI binding.

---

## 4. Ownership & vòng đời dữ liệu

### 4.1 Ba loại dữ liệu trong engine
| Loại | Ví dụ | Ai sở hữu | Ai được sửa |
|---|---|---|---|
| **EngineModel** (NRT, có thể thay đổi) | Danh sách clip, instrument, FX config, đường dẫn file | `CommandProcessor` | Chỉ main thread |
| **GraphSnapshot** (bất biến) | Bản "đóng băng" của model, gồm con trỏ tới các buffer âm thanh | Tạo trên main/worker, audio thread chỉ **mượn** | Không ai sửa. Muốn thay đổi thì tạo bản mới |
| **RtState** (chỉ audio thread) | Vị trí phát, trạng thái clip, voice, trạng thái filter | `RtEngine` | Chỉ audio thread |

### 4.2 Swap snapshot (kiểu RCU)
```
main:   auto next = builder.build(model);          // cấp phát thoải mái
        pending.store(next.release(), release);    // std::atomic<GraphSnapshot*>

audio:  if (auto* p = pending.exchange(nullptr, acq_rel)) {
            rtToNrt.push(Retire{current});          // không delete!
            current = p;
            rtState.remap(*current);                // khớp vị trí phát sang snapshot mới
        }

main (Timer 30Hz): while (rtToNrt.pop(msg)) if (msg is Retire) delete msg.ptr;
```
- Buffer âm thanh (sample, clip) nằm trong `std::shared_ptr<const AudioData>`, **do snapshot giữ**. Audio thread chỉ dùng **con trỏ thô**, không bao giờ copy hay huỷ `shared_ptr`.
- Snapshot cũ vẫn giữ tham chiếu tới buffer cho tới khi ReleasePool huỷ nó. Vì vậy audio thread luôn đọc được dữ liệu hợp lệ.
- Nếu main tạo 2 snapshot liên tiếp trước khi audio thread kịp lấy bản đầu thì dùng `exchange` trên `pending`: bản chưa được dùng bị lấy ra và huỷ ngay trên main thread.

### 4.3 Buffer lớn khi thu âm
Mỗi lần arm để thu, bộ nhớ đã được **cấp phát sẵn trên main thread** (độ dài tối đa của take cộng thêm lề latency). Audio thread chỉ ghi vào vùng đó. Thu xong thì buffer được chuyển sang NRT qua `rtToNrt` để gắn vào clip.

---

## 5. Vòng đời engine

```
App khởi động
 └─ Dart: le_create(config)                       // main thread
     ├─ ScopedJuceInitialiser_GUI (MessageManager gắn vào main run loop)
     ├─ Cấp phát: queue, pool 64+8 voice, buffer thu, StatePublisher
     ├─ Tạo ThreadPool(2), TimeSliceThread (disk)
     └─ Snapshot rỗng
 └─ Dart: le_audio_start()
     ├─ Xin quyền mic (phía Dart/Swift, trước bước này)
     ├─ Cấu hình AVAudioSession (playAndRecord, buffer mong muốn 128). Engine ĐẶT LẠI sau mỗi lần JUCE open():
     │    options = defaultToSpeaker | allowBluetoothA2DP | mixWithOthers*   (KHÔNG có HFP, KHÔNG có AirPlay)
     │    mode = default | measurement (chọn sau khi nghe thử ở P0-06)    *chờ người dùng xác nhận
     └─ AudioDeviceManager mở thiết bị → callback bắt đầu chạy
 └─ Dart: le_call({"op":"project.open", ...}) → khôi phục trạng thái bằng các lệnh cấu trúc

Chạy nền (không phát) → le_audio_stop() để tiết kiệm pin
Chạy nền (đang phát hoặc có Link) → giữ audio (UIBackgroundModes: audio)
Interruption (cuộc gọi, Siri) → engine dừng, gửi event → UI hiện trạng thái; hết interruption → khởi động lại
Route change (cắm hoặc rút tai nghe) → đo lại latency; nếu sample rate đổi thì prepare lại
Thoát app → le_audio_stop() → le_destroy() (dừng worker, xả ReleasePool)
```

---

## 6. Xử lý lỗi

| Nơi xảy ra | Cách xử lý |
|---|---|
| Audio thread | Không dùng exception, không assert crash ở bản release. Lỗi thì ghi mã vào `rtToNrt` và tiếp tục chạy (xuất im lặng nếu cần) |
| `le_call` | Trả JSON `{"ok":false,"error":{"code":"FILE_NOT_FOUND","message":"..."}}` |
| Job worker | Event `JOB_FAILED(jobId, code)`. Dart gọi `le_call({"op":"job.result"})` để lấy chi tiết |
| Toàn engine | Mã lỗi đánh số trong `engine_api.h` (05 §5) |
| Dart | Mọi lỗi từ engine đưa vào một `EngineErrorBus` để hiện toast hoặc banner |

Build debug: bật `jassert` và RTSan. Build release: tắt assert nhưng giữ bộ đếm xrun.

---

## 7. Module map (C++)

```
le::api      engine_api.cpp          C API → Engine facade
le::core     Engine, RtEngine, CommandProcessor, EngineModel, GraphSnapshot, SnapshotBuilder,
             ReleasePool, StatePublisher, RtQueues, Transport, ClipScheduler, JobSystem
le::dsp      Track, AudioClipPlayer, MidiClipPlayer, Sampler, Voice, Instrument/Zone, Adsr,
             Interpolators, FxChain, Processor(interface), fx/*, Mixer, Metronome, Limiter
le::io       DeviceIO (JUCE AudioDeviceManager | Plan B: CoreAudioIO), Recorder, AudioFileIO,
             SfzLoader, Exporter
le::render   PitchRenderer, WarpRenderer, Yin, SilenceTrimmer, PeakBuilder
le::midi     MidiInputRouter, MidiLearnMap
le::link     LinkSync (LinkKit)                                     [P4]
```
Chiều phụ thuộc: `api → core → dsp/io/render/midi/link`. Riêng `dsp` không phụ thuộc vào `io` hay JUCE device, nên test được bằng offline render.

---

## 8. Plan B: nếu JUCE I/O không chạy được trong Flutter

Chuyển sang Plan B khi P0 cho thấy JUCE `AudioDeviceManager` hoặc `MessageManager` xung đột với Flutter (crash, mất session, không khởi động lại được sau interruption).

**Điểm yếu của JUCE 9.0.3 mà agent 68 tìm ra khi đọc code (P0-06):**
1. `AudioDeviceManager::audioDeviceIOCallbackInt` giữ một `ScopedLock` (lock chặn thật) **trên audio thread**. Lock này chỉ bị tranh chấp khi main thread đổi callback hoặc cấu hình device → **quy tắc: không đổi cấu hình device trong lúc đang biểu diễn**.
2. Nếu iOS gửi block **lớn hơn** buffer đã cấp phát, JUCE resize buffer ngay trên audio thread (có cấp phát bộ nhớ).
3. Sau khi hết interruption, JUCE tự chạy lại RemoteIO nhưng không gọi `audioDeviceAboutToStart`.
4. Sau **media services reset**, JUCE không tạo lại AudioUnit → nhiều khả năng mất tiếng. Engine phải tự `stop()` rồi `start()` lại.

→ **Tín hiệu để kích hoạt Plan B:** P0-10 hoặc soak test (P1-36) thấy xrun khi khoá màn hình hay đổi route, hoặc không khôi phục được sau interruption hay media reset, trong khi đã xử lý điểm 4.

- Thay `le::io::DeviceIO` bằng `CoreAudioIO` tự viết bằng Obj-C++: AVAudioSession cộng với AudioUnit RemoteIO, khoảng 400–600 dòng. Callback gọi đúng `RtEngine::process()` như cũ.
- **Vẫn giữ JUCE** cho `juce_audio_basics`, `juce_audio_formats`, `juce_dsp`, `juce_core` (không cần message loop).
- Mọi phần khác không đổi, vì `RtEngine` không biết thiết bị đến từ đâu. Đây là lý do DeviceIO được tách thành interface ngay từ P1-01.

---

## 9. Chuẩn bị cho phase sau (không làm trong MVP)

| Tính năng | Điểm nối đã chuẩn bị sẵn |
|---|---|
| AUv3 host | Interface `Processor` (04 §9), `latencySamples()` để làm delay compensation |
| Arrangement view | `LaunchLog`: mọi launch/stop được ghi thành sự kiện `(beat, track, slot)` (04 §3.5) |
| Android | Engine không phụ thuộc iOS ngoài `DeviceIO`. JUCE có Oboe |
| Pedal mode | Transport có chế độ "BPM lấy từ loop đầu tiên" (04 §2.4) |
