# Ghi chú build engine (P0-02, P0-03, P0-11)

Bổ sung cho docs 09 §4. Chép sang 09 §4 khi điều phối đồng ý.

## Target CMake
| Target | Loại | Nội dung |
|---|---|---|
| `le_juce` | STATIC | 6 module JUCE (`core events audio_basics audio_devices audio_formats dsp`), mỗi module compile đúng 1 lần. Include dir và define lộ ra INTERFACE (mẫu "shared code" của JUCE) |
| `loopcore` | STATIC | `src/**/*.cpp` + `src/**/*.mm` (Apple, `-fobjc-arc`). Link PUBLIC `le_juce`, `signalsmith-stretch`, `signalsmith-linear`, SPSCQueue |
| `le-harness` | exe (Mac) | `harness/`, chỉ dùng C API |
| `le-tests` | exe (Mac) | `tests/unit/**/*.cpp` trừ `rtsan_selfcheck.cpp`, Catch2 v3 |
| `le-rtsan-selfcheck` | exe | Chỉ khi `LE_RTSAN_SELFCHECK=ON` (preset `mac-rtsan` bật sẵn) |
| tools/* | exe (Mac) | Của agent 80, `add_subdirectory(tools)` nếu có |
| `loopcore_dylib` → `libLoopCore.dylib` | SHARED (Mac) | `-force_load` loopcore + le_juce, chỉ export `_le_*`. Dùng cho test hợp đồng Dart với engine thật |

- `JUCE_MODULES_ONLY=ON`: không build `juceaide` lúc configure (không dùng `juce_add_*`).
- `src/spike/measure/StretchBench.cpp`, `src/render/*Renderer.cpp`, `src/render/Yin.cpp`: `-O2` ở mọi config
  (Signalsmith ở Debug chậm ~10×, Yin 4 s: 26 ms ở -O2 so với 775 ms ở -O0).
- `le-tests`: `-Wno-c2y-extensions` (chỉ clang upstream) đặt ở **mức source**. Lý do: macro `TEST_CASE` dùng `__COUNTER__`,
  còn `-Wpedantic` (interface của loopcore) đứng sau cờ target trên dòng lệnh nên sẽ bật lại cảnh báo.
- XCFramework: `libtool -static` gộp `libloopcore.a` + `lible_juce.a` thành một `libloopcore.a` cho mỗi slice.
- Framework iOS cần link: AVFoundation AudioToolbox CoreAudio CoreMIDI Accelerate QuartzCore Foundation UIKit, cùng `-lc++`.

## RTSan (`mac-rtsan`)
- Compiler: Homebrew LLVM (`/opt/homebrew/opt/llvm/bin/clang{,++}`, đã thử với **23.1.1**) cho cả C, C++, OBJC, OBJCXX.
  Apple clang 17 hiểu `[[clang::nonblocking]]` và `-Wfunction-effects`, nhưng **không có** `-fsanitize=realtime`.
- Cờ: `-fsanitize=realtime` cho compile **và** link.
- **libc++**: link bằng libc++ của Homebrew: `-L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++`.
  Header libc++ mới đi với dylib hệ thống cũ có thể thiếu symbol.
- **Deployment target** Mac = 15.0. Nếu để 13.0 thì `ld: warning: ... libc++.1.dylib which was built for newer version 15.0`.
- Sysroot: CMake tự lấy SDK của Xcode, không cần cờ. JUCE Obj-C++ build với clang Homebrew không lỗi.
- RTSan chỉ kiểm **bên trong** hàm `[[clang::nonblocking]]` (`RtEngine::process` và mọi hàm nó gọi). Lock của
  `AudioDeviceManager` nằm ở tầng gọi bên ngoài nên không bị báo (xem audio-session.md §4).
- `RTSAN_OPTIONS=halt_on_error=true` (test preset): vi phạm đầu tiên sẽ dừng chương trình kèm stack trace.
- **Self-check** `rtsan-selfcheck`: hàm nonblocking gọi `malloc`. Test PASS khi output có
  `RealtimeSanitizer: unsafe-library-call ... malloc`. Test đặt riêng `abort_on_error=0`: trên macOS sanitizer mặc định
  `abort()` → CTest coi là crash và **bỏ qua** `PASS_REGULAR_EXPRESSION`.
- Harness chạy real-time dưới RTSan: `RTSAN_OPTIONS=halt_on_error=1 build/mac-rtsan/harness/le-harness spike --seconds 300`.

## `-Wfunction-effects` (bật cho `loopcore`)
Một số hàm an toàn nhưng không được đánh dấu. Chỗ nào có lý do rõ thì tắt cảnh báo cục bộ bằng pragma, ghi lý do ngay tại chỗ:
- `mach_absolute_time()`: đọc bộ đếm phần cứng qua commpage, không syscall (`core/HostTime.h`).
- `rigtorp::SPSCQueue::pop()`: chỉ có `assert` ở bản debug (`core/RtEngine.cpp`).

## TSan / ASan
- `mac-tsan`: `TSAN_OPTIONS=halt_on_error=1`. StatePublisher dùng seqlock với dữ liệu là `std::atomic<uint64_t>`
  (relaxed) nên không có data race về mặt C++, và TSan không báo nhầm.
- `mac-asan`: ASan + UBSan, `detect_stack_use_after_return=1`.

## CMake option
| Option | Mặc định | Ý nghĩa |
|---|---|---|
| `LE_BUILD_HARNESS` | ON (iOS OFF) | harness + tools/ + libLoopCore.dylib |
| `LE_BUILD_TESTS` | ON (iOS OFF) | le-tests |
| `LE_RTSAN_SELFCHECK` | OFF (mac-rtsan ON) | test chứng minh RTSan đang chạy |
| `LE_ENABLE_SIM` | ON (iOS OFF) | op `sim.offline` / `sim.advance` (05 §3) cho test hợp đồng Dart qua dylib |

## Render offline & scenario (P1-01..03)
- `Engine(cfg, std::make_unique<io::OfflineDeviceIO>(maxBlock))`: không có thread audio. Người gọi tự `render()` từng khối
  trên thread của mình, host time giả lập tăng đúng theo frame → deterministic. Lệnh vẫn đi đúng đường `send()` / `call()` như app.
- `le::sim::runScenarioFile()` (08 §3.2): sự kiện `atBeat`/`atSample` rơi giữa block thì runner **chia block tại đúng sample đó**,
  nên output giống hệt nhau ở block 64/128/256/1024 (test `[scenario]` kiểm sai khác ≤ 1e-7).
- Kỳ vọng: `golden` + `nullTestMaxDb` (báo **sample đầu tiên bị lệch**), `firstNonSilentSample` (+`silenceThresholdDb`,
  `firstNonSilentTolerance`), `maxSampleJumpDb`, `peakMaxDb`, `peakMinDb`. Chưa có golden → failure kèm hướng dẫn chạy
  `scripts/golden_update.sh` rồi tự nghe.
- Kỳ vọng mới (P1-10..16): `onsetSamples` (+`onsetMinGap`), `stateAt` [{frame ≥ 1, track, slot, clipState, playingSlot,
  playing, beat}] = LeState SAU KHI render F frame, `reference` {file, startSample, skip, loop, gain, maxDiffDb} = null test
  so với CHÍNH file nguồn (không phải golden), `silentFromSample`.
- Đường dẫn tương đối trong scenario (clip.setAudio, track.setInstrument) tính từ `engine/tests/` (runner đặt libraryDir).
- Fixture tổng hợp: `scripts/gen_test_fixtures.py` → `engine/tests/fixtures/clip_*.wav` (float32 48 kHz). Không phải golden,
  tạo lại được. Fixture drum kit / nhạc cụ (P1-28) là của 80.

## Mixer / Limiter (P1-13/14)
- Limiter master KHÔNG lookahead (0 latency): g(n) = min(ceiling/|x(n)|, g nhả dần τ 50 ms) → luôn ≤ -0.3 dBFS. Lý do: lookahead
  làm lệch mọi mốc thời gian (metronome, launch) và cộng latency. Quá tải nặng thì méo nhẹ ở đỉnh (limiter an toàn).
- Meter: peak thô theo cửa sổ 25 ms (UI 60 Hz không sót đỉnh), ballistics làm ở UI (04 §11).
