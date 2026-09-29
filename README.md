# Music Looper (codename LoopCore)

Live looper kiểu Session View cho iPad, có chromatic sampler tạo từ tiếng người dùng tự thu.
Engine viết bằng C++20 + JUCE (headless), UI viết bằng Flutter qua `dart:ffi`.

- Kế hoạch và spec: [`docs/`](docs/00-README.md)
- Hợp đồng FFI: [`engine/include/le/engine_api.h`](engine/include/le/engine_api.h)
- Hướng dẫn cho AI: [`CLAUDE.md`](CLAUDE.md)
- Ghi chú engine: [`engine/docs/`](engine/docs/) (build, RTSan, AVAudioSession), phiên bản thư viện: [`engine/third_party/VERSIONS.md`](engine/third_party/VERSIONS.md)

## Bắt đầu
```bash
scripts/bootstrap.sh                      # submodule, LFS, kiểm tra công cụ (cmake, ninja, llvm, xcode, flutter)
scripts/test_engine.sh mac-debug          # build + test engine trên Mac
scripts/test_engine.sh                    # mặc định: mac-debug + mac-rtsan (bắt buộc cho task [RT])
scripts/test_engine.sh mac-tsan mac-asan  # sanitizer luồng / bộ nhớ (hằng tuần, hoặc khi đụng queue/atomic)
```

## Lệnh thường dùng
| Lệnh | Việc |
|---|---|
| `scripts/build_engine_mac.sh [preset]` | Configure + build (preset: `mac-debug` `mac-release` `mac-rtsan` `mac-tsan` `mac-asan`) |
| `scripts/build_engine_xcframework.sh` | Build iOS (device + simulator) → `app/packages/engine_ffi/ios/Frameworks/LoopCore.xcframework`, kiểm symbol `le_*` |
| `scripts/run_harness.sh spike --seconds 10 --voices 64` | Chạy engine real-time qua loa Mac, in CPU/xrun mỗi giây |
| `scripts/run_harness.sh play` | Real-time, điều khiển bằng bàn phím (xem `--help`) |
| `scripts/run_harness.sh render engine/tests/scenarios/<x>.json -o out.wav` | Render offline một scenario, kiểm kỳ vọng |
| `scripts/golden_update.sh <scenario>` | Render lại golden, **in đường dẫn để bạn tự nghe** trước khi commit |
| `scripts/gen_bindings.sh` | ffigen → bindings Dart (app) |

## Cấu trúc
```
engine/
  include/le/engine_api.h   hợp đồng C API (không sửa khi chưa hỏi)
  src/api/                  C API → le::core::Engine
  src/core/                 Engine, RtEngine, StatePublisher, JobSystem, CpuMeter, XrunDetector, ...
  src/io/                   DeviceIO (JuceDeviceIO, OfflineDeviceIO), AudioSession (iOS)
  src/sim/                  ScenarioRunner (render offline, golden)
  src/spike/                code chỉ dùng cho P0 (xoá ở P1-37)
  harness/  tools/  tests/{unit,scenarios,golden,fixtures}
app/                        Flutter (iPad) + packages/engine_ffi
scripts/
```
Build output nằm ở `build/<preset>/` (không commit).
