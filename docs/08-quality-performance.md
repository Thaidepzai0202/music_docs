# 08 — Chất lượng & hiệu năng

---

## 1. Ngân sách hiệu năng (đo trên iPad 8)

| Chỉ số | Mục tiêu | Cách đo |
|---|---|---|
| Buffer | 128 frame @ 48 kHz (dự phòng 256) | `LeState.bufferSize` |
| Round-trip latency (mic trong máy + loa) | ≤ 15 ms sau bù (spike chấp nhận ≤ 20ms) | `spike.latencyLoopback`, sau này `latency.calibrate` |
| CPU audio thread, trung bình | < 35% | `LeState.cpuLoad` |
| CPU audio thread, đỉnh (1 giây) | **< 50%** | `LeState.cpuPeak` |
| Xrun | **0** trong 30 phút | `LeState.xrunCount` |
| UI | 60 fps, không frame nào > 16.7ms khi đổi scene | Flutter DevTools, Performance overlay |
| Chạm tới lúc xếp lệnh | < 1 frame | Instruments `os_signpost` từ pointer-down tới `le_send` |
| Pre-render instrument (4 giây) | < 2 s | Thời gian job |
| Warp render 1 clip 8 bar | < 1 s | Thời gian job |
| RAM | < 600 MB | Instruments Allocations, `os_proc_available_memory` |
| Khởi động tới lúc vào Session | < 2 s (project nhỏ) | Stopwatch |

**Kịch bản tải chuẩn** (dùng để đo CPU mọi lúc): 8 track đang phát (4 audio stretch, 4 instrument), 64 voice active, mỗi track 3 FX (filter + delay + reverb), master EQ + limiter, 1 track đang thu.

---

## 2. Cách đo

- **CPU:** engine tự đo thời gian callback bằng `mach_absolute_time`, chia cho thời lượng buffer. Giữ trung bình và max trong cửa sổ 1 giây, đưa vào `LeState`.
- **Xrun:** (1) bộ đếm của JUCE `getXRunCount()` nếu có. (2) Tự phát hiện: khoảng cách host time giữa hai callback > 1.5 × thời lượng buffer. Mỗi xrun gửi event và tăng `xrunCount`. Bản debug hiện chấm đỏ ở top bar.
- **Profile:** Instruments → Time Profiler cộng System Trace (thấy được thread audio bị preempt). Đặt `os_signpost` quanh `process()` và từng subsystem.
- **UI:** `flutter run --profile` trên iPad 8, DevTools timeline, Highlight Repaints.

---

## 3. Chiến lược test

```
            ┌──────────────────────┐
            │  Thử tay trên iPad   │  mỗi mốc: checklist §6 + soak 30 phút
           ┌┴──────────────────────┴┐
           │ Test hợp đồng FFI      │  C++ và Dart, mỗi lần đổi header
          ┌┴────────────────────────┴┐
          │ Golden / scenario render │  harness, offline, deterministic
         ┌┴──────────────────────────┴┐
         │ Unit test C++ (Catch2)     │  toán transport, quantize, voice, YIN, queue…
         │ Unit/widget test Dart      │  model, migration, controller + FakeEngine
         └────────────────────────────┘
     + build sanitizer: RTSan · TSan · ASan/UBSan chạy trên cùng bộ test
```

### 3.1 Unit test C++: bắt buộc có
| Module | Test |
|---|---|
| Transport | beat ↔ sample, đổi BPM giữ neo, không trôi sau 10⁸ sample |
| Quantize | `boundary()` ở mọi mức q, trường hợp đứng đúng ranh giới, khi transport đang dừng |
| BlockSplitter | Ranh giới rơi giữa block, nhiều ranh giới trong một block, ranh giới ở frame 0 |
| ClipScheduler | Mọi chuyển trạng thái ở 04 §3.1, launch hai clip cùng track, scene với ô trống |
| Recorder | Bù latency (take bắt đầu đúng sample), take tự do được làm tròn lên |
| Sampler | Chọn zone, tính `inc`, stealing (không vượt 64 voice), choke group |
| Yin | Sine 110/220/440/880 Hz → sai số < 5 cent. Nhiễu trắng → confidence < 0.3 |
| SPSC / StatePublisher | Stress test 2 thread 10⁷ message, không mất, không rách dữ liệu (chạy dưới TSan) |

### 3.2 Golden / scenario test (harness)
Kịch bản dạng JSON dùng **cùng lệnh** với app (05):
```json
{
  "name": "launch_quantized_1bar",
  "sampleRate": 48000, "blockSize": 128,
  "setup": [
    { "call": { "op": "project.open", "dir": "$TMP" } },
    { "call": { "op": "clip.setAudio", "track": 0, "slot": 0, "clipId": "c_1",
                "file": "fixtures/click_4beats_120.wav", "lengthBeats": 4, "originalBpm": 120, "warp": "repitch", "gain": 0 } },
    { "send": { "type": "SET_BPM", "d0": 120 } },
    { "send": { "type": "SET_QUANTIZE", "i0": "LE_Q_1_BAR" } }
  ],
  "timeline": [
    { "atBeat": 0.0, "send": { "type": "TRANSPORT_PLAY" } },
    { "atBeat": 1.3, "send": { "type": "CLIP_LAUNCH", "track": 0, "slot": 0 } }
  ],
  "renderBeats": 12,
  "expect": {
    "golden": "golden/launch_quantized_1bar.wav",
    "nullTestMaxDb": -90,
    "firstNonSilentSample": 96000
  }
}
```
- `firstNonSilentSample: 96000`: launch ở beat 1.3, quantize 1 bar nên clip vào tại beat 4. Ở 120 BPM và 48 kHz: 4 × 24000 = **96000**. Đây là cách kiểm tra "đúng nhịp tuyệt đối" bằng con số.
- **Null test:** lấy output trừ golden, RMS phải < −90 dBFS.
- **Cập nhật golden:** `scripts/golden_update.sh <name>` → **bắt buộc nghe lại** file mới rồi mới commit.
- Chạy mọi kịch bản với `blockSize` 64, 128, 256, 1024: kết quả phải giống nhau (bảo đảm chính xác tới từng sample bất kể kích thước block).

### 3.3 Build sanitizer
| Preset | Cờ | Khi nào |
|---|---|---|
| `mac-rtsan` | `-fsanitize=realtime`, LLVM upstream (Homebrew) | Mọi task [RT], trước khi đóng |
| `mac-tsan` | `-fsanitize=thread` | Mỗi tuần, và khi đụng tới queue hoặc publisher |
| `mac-asan` | `-fsanitize=address,undefined` | Mỗi tuần |

### 3.4 Test Dart
- Round-trip model: `Project → JSON → Project` bằng nhau. Mỗi migration có một fixture.
- `ProjectController` với `FakeEngineClient` ghi lại lệnh: kiểm tra đúng chuỗi lệnh khi mở project (06 §6).
- Widget test cho `ClipCell`: trạng thái → màu và nhãn đúng.

---

## 4. Quy tắc real-time (luật cứng)

**Trên audio thread (và thread MIDI in) KHÔNG ĐƯỢC:**

| Cấm | Vì sao | Thay bằng |
|---|---|---|
| `new`, `delete`, `malloc`, `free` | Thời gian không xác định, có lock bên trong | Cấp phát trong `prepare()` hoặc trên main thread |
| `std::vector::push_back/resize`, `std::string`, `std::map`, `juce::String`, `juce::Array::add` | Cấp phát ngầm | `std::array`, buffer cố định, ring buffer |
| `std::shared_ptr` copy hoặc huỷ, `std::atomic<std::shared_ptr>` | Refcount atomic + có thể `delete`; atomic shared_ptr có thể dùng lock | Con trỏ thô mượn từ snapshot |
| `std::function` (capture lớn), lambda gói vào `std::function` | Có thể cấp phát | Con trỏ hàm, template, virtual |
| `std::mutex`, `juce::CriticalSection`, `SpinLock`, `condition_variable` | Có thể bị chặn, gây đảo ưu tiên | SPSC queue, atomic |
| `printf`, `std::cout`, `DBG`, `juce::Logger`, `os_log` | I/O và lock | Đẩy mã sự kiện vào `rtToNrt` |
| Đọc/ghi file, gọi mạng | I/O | Worker thread |
| `MessageManager::callAsync`, `juce::Timer` | Cấp phát + lock | `rtToNrt` queue |
| `IIR::Coefficients::make*` | Cấp phát | Tự tính hệ số biquad (04 §9) |
| Obj-C, Swift, gọi vào Dart | Runtime có lock, autorelease | Không có ngoại lệ |
| `throw` / `try` | Có thể cấp phát | Mã lỗi |

**Được phép:** toán số, đọc snapshot, ghi vào buffer cấp phát sẵn, `std::atomic` kiểu nguyên thuỷ, SPSC `try_push/try_pop`, gọi virtual.

**Để máy bắt lỗi hộ:**
1. Đánh dấu `[[clang::nonblocking]]` cho `RtEngine::process` và mọi hàm nó gọi.
2. Build `mac-rtsan` rồi chạy toàn bộ golden test cộng harness `play` 5 phút. Chỉ cần vi phạm là crash kèm stack trace.
3. `-Wfunction-effects` (Clang) cảnh báo ngay lúc compile.

---

## 5. Ma trận thiết bị

| Thiết bị | Vai trò |
|---|---|
| **iPad 8 (A12, 3 GB)** | Máy chuẩn: mọi số đo đều lấy ở đây |
| iPad Air/Pro (M-series) | Kiểm tra sample rate và độ trễ khác, màn hình 120 Hz |
| Tai nghe có dây (Lightning/USB-C) | Kiểm tra monitoring, latency thấp |
| AirPods / tai nghe Bluetooth | Kiểm tra cảnh báo latency, monitor tự tắt |
| Audio interface USB (2 in) | Input stereo, sample rate 44.1/96 kHz |
| Launchpad (Mini/X) hoặc MIDI keyboard USB/BLE | MIDI (P4) |
| Mac chạy Ableton Live hoặc iPad thứ hai có app Link | Test plan Link (P4) |

---

## 6. Checklist thử tay trên máy (mỗi mốc)

- [ ] Cold start → mở project demo → play: không có click ở đầu
- [ ] Launch 8 clip liên tục thật nhanh: mọi clip vào đúng ranh giới
- [ ] Thu 4 bar có count-in: nghe lại khớp metronome (bù latency đúng)
- [ ] Cắm và rút tai nghe khi đang phát: không crash, âm thanh tiếp tục
- [ ] Có cuộc gọi hoặc Siri chen ngang: engine dừng và khởi động lại đúng
- [ ] Chuyển app xuống nền khi đang phát: vẫn phát. Khi dừng thì tắt audio
- [ ] Bộ nhớ thấp (mở nhiều app nặng): không mất project
- [ ] Soak 30 phút ở kịch bản tải chuẩn: 0 xrun, RAM không tăng dần

---

## 7. Mức độ lỗi

| Mức | Định nghĩa | Xử lý |
|---|---|---|
| S1 | Crash, mất dữ liệu, xrun ở kịch bản tải chuẩn, lệch nhịp | Dừng mọi việc để sửa |
| S2 | Tính năng chính hỏng nhưng có cách lách | Sửa trong tuần |
| S3 | Lỗi UI, lỗi hiển thị | Đưa vào backlog, sửa ở tuần polish |

---

## 8. Kết quả đo (cập nhật mỗi mốc)

| Mốc | Buffer | RT latency | CPU TB / đỉnh | Xrun/30' | UI fps | RAM | Ghi chú |
|---|---|---|---|---|---|---|---|
| M0 | | | | | | | |
| M1 | | | | | | | |
| M2 | | | | | | | |
| M3 | | | | | | | |
| M4 | | | | | | | |
