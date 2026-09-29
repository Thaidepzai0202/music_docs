# P0 — Spike kỹ thuật (W1–W2: 05/10 – 16/10/2026)

**Mục tiêu:** trong 2 tuần, trả lời được câu hỏi *"JUCE nhúng trong Flutter trên iPad 8 có đạt latency, CPU và chất lượng pitch mà kế hoạch cần không?"*
**Đầu ra:** app spike chạy trên iPad 8, số đo thật, file `spike-report.md`, quyết định **M0 go/no-go** (02 §3).
**Nguyên tắc:** code spike được phép xấu, nhưng **số đo phải thật**. Code tái sử dụng được (repo, CMake, FFI, StatePublisher) thì làm đúng chuẩn ngay từ đầu.

---

## Tổng quan task

| Trạng thái | Mã | Việc | Người làm | Ước lượng | Phụ thuộc |
|---|---|---|---|---|---|
| [ ] | P0-01 | Công cụ & hành chính | **Bạn** | 0.5d (+ chờ) | – |
| [x] | P0-02 | Repo, submodule, CMake preset, script | AI `68` | 1d | – |
| [x] | P0-03 | Engine spike: C API tối thiểu, JUCE device, StatePublisher, XCFramework | AI `68` | 2d | P0-02 |
| [~] | P0-04 | App Flutter + plugin `engine_ffi` + ffigen + EngineClient (**29/09: code xong, 26 test xanh, đang dùng Fake. Còn chờ XCFramework của 68 và `flutter run` trên iPad**) | AI `77` | 1.5d | header (có sẵn), P0-03 để link |
| [~] | P0-05 | UI spike + ticker state 60fps (**29/09: code xong, test repaint tự động xanh. Còn chờ bạn đo 60fps trên iPad**) | AI `77` | 1d | P0-04 |
| [~] | P0-06 | Mic: quyền, AVAudioSession, thu/phát lại (**29/09: phía Flutter xong. Còn chờ ghi chú session của 68 và bạn test thu/phát**) | AI `68` + `77` | 1d | P0-03, P0-04 |
| [~] | P0-07 | LatencyProbe (loopback + cross-correlation) (**code + nối vào xong, còn chờ bạn đo trên iPad**) | AI `80` (+`68` nối vào) | 1d | P0-03 |
| [~] | P0-08 | LoadGenerator + thử buffer 128/256 (**code xong, còn chờ bạn đo trên iPad**) | AI `80` (+`68`) | 1d | P0-03 |
| [~] | P0-09 | StretchBench (Signalsmith, 13 zone, formant) (**code xong, còn chờ bạn nghe và đo trên iPad**) | AI `80` (+`68`) | 1.5d | P0-02 |
| [ ] | P0-10 | Test interruption & route trên iPad | **Bạn** + `77` | 0.5d | P0-06 |
| [x] | P0-11 | Preset RTSan chạy được với JUCE | AI `68` | 0.5d | P0-02 |
| [ ] | P0-12 | Báo cáo spike & go/no-go | **Bạn** | 0.5d | tất cả |

> AI build và test được trên Mac, và build được cho iOS. **Chạy trên iPad 8, đo và nghe là việc của bạn.** Mỗi task AI phải ghi rõ "bước bạn cần làm trên máy thật".

---

## P0-01 · Công cụ & hành chính · Bạn · 0.5d
- Cài đủ công cụ theo 09 §1.
- Đăng ký hoặc gia hạn **Apple Developer Program**.
- **Nộp đơn xin entitlement multicast** cho LinkKit tại trang yêu cầu *Multicast Networking Entitlement* của Apple Developer. Ghi lại ngày nộp vào 02 §8.
- Tạo tài khoản juce.com, chọn gói **Starter** (miễn phí khi doanh thu < $20k).
- Đọc license SDK Ableton Link và UI guidelines của LinkKit.
- Mua **iPad 8** (A12) cũ, bật Developer Mode, chạy thử được app Flutter mẫu.

**DoD**
- [ ] `flutter doctor` sạch, `cmake --version` ≥ 3.25, có `/opt/homebrew/opt/llvm/bin/clang++`
- [ ] Đã nộp đơn entitlement (ghi ngày)
- [ ] iPad 8 chạy được `flutter run` với app mẫu

---

## P0-02 · Repo & build system · AI `68` · 1d
- Tạo `music-app/` theo 09 §2. Đã có sẵn khung thư mục và `engine/include/le/engine_api.h` (hợp đồng, **không được sửa** nếu chưa hỏi).
- `git init`, `.gitignore`, `.gitattributes` (LFS: `*.wav *.flac *.caf`).
- Submodule: JUCE (tag 9.x mới nhất), signalsmith-stretch, SPSCQueue, Catch2 v3. Ghi tag hoặc commit đã pin vào `engine/third_party/VERSIONS.md`.
- `engine/CMakeLists.txt` theo 09 §4.2. `CMakePresets.json` gồm đủ 7 preset ở 09 §4.1.
- `tests/CMakeLists.txt` glob `tests/unit/**/*.cpp` để agent khác thêm test không cần sửa CMake.
- Script: `bootstrap.sh`, `build_engine_mac.sh`, `test_engine.sh`.
- Một test Catch2 mẫu (`static_assert(sizeof(LeCommand) == 32)` và `le_api_version() == 1`).

**DoD**
- [ ] `scripts/bootstrap.sh && scripts/test_engine.sh mac-debug` xanh
- [ ] `cmake --preset ios-device` configure thành công
- [ ] `engine/third_party/VERSIONS.md` có đủ các pin

---

## P0-03 · Engine spike tối thiểu · AI `68` · 2d
Hiện thực tập con của 05 §2:
- `le_api_version`, `le_create` (kiểm tra `apiVersion`, khởi tạo JUCE: `ScopedJuceInitialiser_GUI` trên main thread), `le_destroy`
- `le_audio_start/stop`: `juce::AudioDeviceManager`, input 1 kênh và output 2 kênh, buffer theo config, 48 kHz
- `le_send` qua `rigtorp::SPSCQueue<LeCommand>` (dung lượng 1024). Lệnh spike: `SPIKE_SINE`, `SPIKE_LOAD_VOICES`, `SPIKE_RECORD`, `SPIKE_PLAY_RECORD`, `SPIKE_PASSTHROUGH`
- `le_call`: `engine.info` (trả thêm `stateSize`, `commandSize`), `spike.setBufferSize {frames}` (khởi động lại device). Mọi op khác trả `ok:false, NOT_IMPLEMENTED`
- `StatePublisher` (seqlock hoặc triple buffer) và `le_read_state`: điền các trường `sampleRate`, `bufferSize`, `cpuLoad`, `cpuPeak`, `xrunCount`, `inputPeak`, `masterPeak`, `latencyRoundTripSamples` (theo số device báo)
- CPU meter (08 §2) và phát hiện xrun theo khoảng cách host time
- `le_set_event_callback` + `juce::Timer` 30Hz đẩy event (`LE_EVT_XRUN`, `LE_EVT_AUDIO_INTERRUPTED`, `LE_EVT_ROUTE_CHANGED`)
- Buffer thu cấp phát sẵn 10 giây. `SPIKE_RECORD` ghi vào đó, `SPIKE_PLAY_RECORD` phát loop
- `harness/`: `le-harness spike` chạy real-time trên Mac (sine, in CPU và xrun mỗi giây)
- `scripts/build_engine_xcframework.sh` (09 §5)

**DoD**
- [ ] Trên Mac: `le-harness spike` phát ra tiếng sine, in đúng số CPU và xrun
- [ ] Test StatePublisher: 2 thread × 10⁶ lần publish/read, không đọc phải dữ liệu rách, chạy dưới **TSan** sạch
- [ ] Tạo ra được `LoopCore.xcframework`, `nm -gU` thấy đủ các symbol `le_*`
- [ ] **Bước cho bạn:** không có (kiểm tra trên máy ở P0-05/06)

---

## P0-04 · App Flutter & plugin FFI · AI `77` · 1.5d
- `flutter create app` (org `com.<you>`), iOS only, `TARGETED_DEVICE_FAMILY = 2`, chỉ landscape, deployment target 17.0
- `app/packages/engine_ffi`: tạo bằng template `plugin_ffi`, bỏ phần build C mặc định, podspec dùng `vendored_frameworks` + framework hệ thống + chống strip symbol (09 §5)
- `ffigen.yaml` + `scripts/gen_bindings.sh` sinh bindings từ `engine/include/le/engine_api.h`
- `EngineClient` theo 05 §4, `FakeEngineClient` (cùng interface) để test khi chưa có XCFramework
- `Info.plist`: `NSMicrophoneUsageDescription`, `UIBackgroundModes: audio`, orientation
- Test Dart: `sizeOf<LeCommand>() == 32`, cùng các test cho `EngineClient.call` (encode, decode, free) chạy bằng fake

**DoD**
- [ ] `flutter test` xanh
- [ ] `flutter build ios --no-codesign` thành công khi có XCFramework (nếu chưa có thì báo "chờ `68`" và dùng fake)
- [ ] **Bước cho bạn:** `flutter run` trên iPad 8 → màn hình hiện `apiVersion = 1`

---

## P0-05 · UI spike · AI `77` · 1d
Một màn hình duy nhất, không cần đẹp:
- Nút Start/Stop audio, chọn buffer 128/256 (`spike.setBufferSize`)
- Sine: tần số (50–2000 Hz, thang log), gain
- Tải giả lập: slider 0–128 voice (`SPIKE_LOAD_VOICES`)
- Thu 4 giây, phát loop, bật/tắt passthrough (có cảnh báo "cắm tai nghe")
- Nút "Đo latency" (`spike.latencyLoopback`) và "Stretch bench" (`spike.stretchBench`): hiện kết quả JSON
- **Bảng live 60 fps** (Ticker + `le_read_state`): SR, buffer, CPU trung bình/đỉnh, xrun, meter input và master (`CustomPainter` + `RepaintBoundary`)

**DoD**
- [ ] Mọi control gửi đúng lệnh (test bằng `FakeEngineClient` ghi lại lệnh)
- [ ] Không gọi `setState` cho dữ liệu 60Hz (07 §6)
- [ ] **Bước cho bạn:** chạy `--profile` trên iPad 8, Performance overlay ổn định 60fps

---

## P0-06 · Mic & AVAudioSession · AI `68` + `77` · 1d
- `77`: xin quyền mic bằng Swift trong plugin (`AVAudioApplication.requestRecordPermission`) **trước** `le_audio_start`. Bị từ chối thì hiện hướng dẫn mở Settings.
- `68`: đọc code iOS audio session của JUCE (`juce_audio_devices/native/juce_ios_Audio.cpp`) và ghi lại JUCE đặt category, options và mode gì. Bảo đảm dùng `playAndRecord` + `defaultToSpeaker` + `allowBluetoothA2DP`, **không** dùng HFP. So sánh mode `default` và `measurement` (chất lượng thu). Ghi kết luận vào `engine/docs/audio-session.md`.
- Bảo đảm **không** có plugin Flutter nào khác đụng vào AVAudioSession.

**DoD**
- [ ] Ghi chú về session đã viết xong
- [ ] **Bước cho bạn:** trên iPad 8, thu 4 giây → phát loop ra loa ngoài; cắm tai nghe → passthrough hoạt động

---

## P0-07 · LatencyProbe · AI `80` (code) + `68` (nối vào) · 1d
- `engine/src/spike/measure/LatencyProbe.{h,cpp}`:
  - Phía RT: phát chirp hoặc chuỗi 5 click (mỗi click cách nhau 500ms) và ghi input vào buffer cấp phát sẵn.
  - Phía NRT: cross-correlation giữa tín hiệu phát và tín hiệu thu → round-trip tính bằng sample. Chạy 5 lần, lấy median.
- Unit test: tín hiệu tổng hợp trễ 523 sample cộng nhiễu -30 dB → phát hiện đúng ±1 sample.
- `68` nối vào `le_call("spike.latencyLoopback")` → `jobId` → `{measuredSamples, reportedSamples, runs:[...]}`

**DoD**
- [ ] Unit test xanh (mac-debug + mac-rtsan)
- [ ] **Bước cho bạn:** đo 3 lần trên iPad 8 (loa → mic trong máy, phòng yên tĩnh). Độ lệch giữa các lần ≤ 1ms. Ghi vào 08 §8

---

## P0-08 · LoadGenerator & buffer · AI `80` + `68` · 1d
- `engine/src/spike/measure/LoadGenerator.{h,cpp}`: N "voice giả", mỗi voice **mô phỏng chi phí voice sampler thật**: đọc buffer 1 giây với bước phân số, nội suy Hermite, envelope, cộng dồn stereo.
- Harness Mac: `le-harness spike --voices 64` in CPU theo N.
- Quy trình đo trên iPad (ghi vào báo cáo): với buffer 128 và 256, lần lượt 0/16/32/64/96/128 voice → CPU trung bình và đỉnh. Chạy 10 phút ở mức 64 voice.

**DoD**
- [ ] Code sạch dưới RTSan (không cấp phát trong `process`)
- [ ] **Bước cho bạn:** bảng CPU trên iPad 8. Chạy 10 phút ở 64 voice → xrun = 0 (ở buffer đã chọn)

---

## P0-09 · StretchBench · AI `80` + `68` · 1.5d
- `engine/src/spike/measure/StretchBench.{h,cpp}`: input là buffer mono. Render 13 zone (−18…+18, bước 3 nửa cung) bằng Signalsmith Stretch ở chế độ **offline** (`seek` → `process` → `flush`), có 2 biến thể **formant on/off**. Đo thời gian từng zone và tổng.
- **Xác định đúng cách dùng API formant** (`setFormantFactor` / `setFormantSemitones` / `compensatePitch` / `setFormantBase`) bằng cách đọc header của thư viện, rồi ghi kết luận vào `engine/docs/signalsmith-notes.md`. `68` sẽ cập nhật 04 §8.
- CLI Mac: `le-stretch-bench in.wav outdir [--formant]`
- `68` nối vào `le_call("spike.stretchBench")`: dùng buffer vừa thu ở P0-06, ghi WAV vào `Documents/spike/` (xem được trong app Files)

**DoD**
- [ ] Trên Mac, CLI tạo đủ 26 file WAV, có in thời gian
- [ ] **Bước cho bạn:** trên iPad 8, tổng thời gian cho 13 zone với mẫu 4 giây < 3 giây. **Tự nghe**: giọng hát ±12 nửa cung có formant nghe chấp nhận được không. Ghi nhận xét

---

## P0-10 · Interruption & route · Bạn + `77` · 0.5d
Thử từng tình huống trong lúc đang phát sine hoặc loop:

| Tình huống | Kỳ vọng | Kết quả |
|---|---|---|
| Cắm / rút tai nghe có dây | Không crash, phát tiếp, có event route | |
| Kết nối AirPods | Có cảnh báo latency | |
| Gọi Siri | Dừng, rồi tự khởi động lại khi xong | |
| Cuộc gọi FaceTime Audio | Như trên | |
| Về Home trong lúc đang phát | Vẫn phát (background audio) | |
| Khoá màn hình | Vẫn phát | |
| Mở lại app sau 10 phút | Audio chạy bình thường | |
| **Reset Media Services** (Cài đặt → Nhà phát triển) | Engine tự stop + start lại, có tiếng trở lại (JUCE không tự làm, 03 §8) | |
| Khoá màn hình 5 phút khi đang phát loop | 0 xrun (nghi phạm: JUCE resize buffer trên RT) | |
| AirPods: `spike.sessionInfo` | output là BluetoothA2DP, input là mic trong máy, `allowBluetoothHFP=false` | |

**DoD**
- [ ] Điền xong bảng. Lỗi chặn (blocker) được ghi vào báo cáo

---

## P0-11 · RTSan preset · AI `68` · 0.5d
- Preset `mac-rtsan` build được JUCE + engine + harness bằng Homebrew clang (xử lý sysroot và Obj-C++ nếu có lỗi).
- `tests/unit/rtsan_selfcheck.cpp` (bật bằng option `LE_RTSAN_SELFCHECK`): hàm `[[clang::nonblocking]]` gọi `malloc`. RTSan **phải** bắt được lỗi này.
- Ghi lại cờ và lỗi thường gặp vào 09 §4.

**DoD**
- [ ] `scripts/test_engine.sh mac-rtsan` xanh
- [ ] Bật self-check → RTSan báo lỗi đúng như mong đợi

---

## P0-12 · Báo cáo & M0 · Bạn · 0.5d
Tạo `docs/spike-report.md` gồm:
- Bảng số đo: latency (đo và báo), CPU theo số voice (128/256), thời gian stretch, xrun 10 phút
- Nhận xét nghe: formant on/off, thu âm với mode default và measurement
- Bảng interruption (P0-10)
- **Quyết định:** buffer 128 hay 256? Signalsmith hay Rubber Band? JUCE I/O hay Plan B?
- Checklist M0 (02 §3) → **GO / NO-GO**
- Cập nhật 01, 04 §8 và 08 §8 theo kết quả

**DoD**
- [ ] Đã ghi quyết định vào 02 §9 (Nhật ký thay đổi)
