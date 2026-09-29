# 09 — Môi trường phát triển & cấu trúc repo

---

## 1. Công cụ cần cài

| Công cụ | Phiên bản | Ghi chú |
|---|---|---|
| macOS + **Xcode** | bản stable mới nhất | Kèm iOS SDK và Command Line Tools |
| Homebrew | – | `brew install cmake ninja llvm ccache git-lfs` |
| CMake | ≥ 3.25 | Cần preset và sinh project Xcode |
| **LLVM upstream** (Homebrew) | ≥ 20 | **Chỉ để build RTSan** (`/opt/homebrew/opt/llvm/bin/clang++`) |
| **Flutter** stable | **≥ 3.29** | Bắt buộc, vì cần Dart chạy trên main thread (05 §1) |
| CocoaPods | mới nhất | Plugin iOS của Flutter |
| Apple Developer Program | 99 USD/năm | Cần cho entitlement multicast, TestFlight, cài lên máy thật |
| iPad 8 | iPadOS ≥ 17 | Máy chuẩn |

**Deployment target:** iPadOS 17.0. `TARGETED_DEVICE_FAMILY = 2` (chỉ iPad). Chỉ landscape.

---

## 2. Vị trí & cấu trúc repo

- Repo (code + docs): **`/Users/apple/Desktop/MUSIC/music-app/`**, remote `origin` = `https://github.com/Thaidepzai0202/music_docs.git`
- Bộ docs này nằm **trong repo** tại `music-app/docs/`. `/Users/apple/Desktop/MUSIC/MUSIC docs` là symlink trỏ về đây, giữ lại để workspace VSCode và đường dẫn cũ vẫn dùng được (29/09/2026).

```
music-app/
├── CLAUDE.md                     # luật cho AI (mẫu ở 11 §2)
├── docs/                         # bộ tài liệu kế hoạch và spec (00–11, phases/)
├── README.md
├── .gitignore  .gitattributes    # LFS: *.wav *.flac *.caf
├── engine/
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── include/le/engine_api.h   # HỢP ĐỒNG (05 §2), nguồn cho ffigen
│   ├── src/
│   │   ├── api/                  # engine_api.cpp
│   │   ├── core/                 # Engine, RtEngine, CommandProcessor, Snapshot, ReleasePool, StatePublisher, Transport, ClipScheduler, JobSystem
│   │   ├── dsp/                  # Track, AudioClipPlayer, MidiClipPlayer, Sampler, Voice, Adsr, fx/, Mixer, Metronome
│   │   ├── io/                   # DeviceIO, Recorder, AudioFileIO, SfzLoader, Exporter
│   │   ├── render/               # PitchRenderer, WarpRenderer, Yin, SilenceTrimmer, PeakBuilder
│   │   ├── midi/  link/
│   │   └── spike/                # code chỉ dùng cho P0 (sine, tải giả lập), xoá sau M0
│   ├── harness/                  # le-harness CLI: `render <scenario.json>` | `play`
│   ├── tools/                    # công cụ đo: latency loopback, stretch bench (P0)
│   ├── tests/
│   │   ├── unit/                 # Catch2
│   │   ├── scenarios/*.json      # 08 §3.2
│   │   ├── golden/*.wav          # LFS
│   │   └── fixtures/*.wav        # LFS
│   └── third_party/              # submodule: JUCE, signalsmith-stretch, SPSCQueue, Catch2; LinkKit (vendored)
├── app/                          # Flutter app (iPad)
│   ├── lib/ …                    # 07 §7
│   ├── ios/
│   └── packages/engine_ffi/      # plugin FFI: ffigen + EngineClient + podspec
├── content/                      # nguồn thư viện âm thanh: sfz, flac, LICENSES/
└── scripts/
```

---

## 3. Thư viện bên thứ ba

| Thư viện | Cách lấy | Pin |
|---|---|---|
| JUCE 9 | `git submodule add https://github.com/juce-framework/JUCE engine/third_party/JUCE` | Tag release mới nhất của 9.x |
| Signalsmith Stretch | `git submodule add https://github.com/Signalsmith-Audio/signalsmith-stretch engine/third_party/signalsmith-stretch` | Tag mới nhất (có formant API) |
| SPSCQueue | `git submodule add https://github.com/rigtorp/SPSCQueue engine/third_party/SPSCQueue` | Commit cố định |
| Catch2 v3 | `git submodule add https://github.com/catchorg/Catch2 engine/third_party/Catch2` | Tag v3.x |
| signalsmith-linear | `git submodule add https://github.com/Signalsmith-Audio/linear engine/third_party/signalsmith-linear` | **Bắt buộc** với signalsmith-stretch 1.4.0 (thiếu thì nó sẽ FetchContent) |
| LinkKit (P4) | Tải bản release từ `github.com/Ableton/LinkKit` → `engine/third_party/LinkKit/` | Bản release chính thức (không dùng pre-release) |

**Đã pin (29/09/2026, P0-02):** JUCE **9.0.3** (`be29c81`) · signalsmith-stretch **1.4.0** (`a670068`) · signalsmith-linear **0.6.4** (`de55e6a`) · SPSCQueue `1053918` · Catch2 **v3.16.0** (`317ac1e`). Nguồn chính thức: `engine/third_party/VERSIONS.md`.

---

## 4. CMake

### 4.1 Preset
| Preset | Generator | Compiler | Mục đích |
|---|---|---|---|
| `mac-debug` | Ninja | Apple clang | Dev hằng ngày, harness, test |
| `mac-release` | Ninja | Apple clang | Đo hiệu năng trên Mac |
| `mac-rtsan` | Ninja | **Homebrew clang** + `-fsanitize=realtime` | Kiểm tra task [RT] |
| `mac-tsan` | Ninja | Apple clang + `-fsanitize=thread` | Queue, publisher |
| `mac-asan` | Ninja | Apple clang + `-fsanitize=address,undefined` | Lỗi bộ nhớ |
| `ios-device` | Xcode | Apple clang, `CMAKE_SYSTEM_NAME=iOS`, `arm64` | Static lib cho iPad |
| `ios-sim` | Xcode | Apple clang, sdk iphonesimulator, `arm64` | Chạy simulator (chỉ để test UI, không đo audio) |

### 4.2 Các dòng chính trong `engine/CMakeLists.txt`
```cmake
cmake_minimum_required(VERSION 3.25)
project(LoopCore VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party/JUCE)          # chỉ dùng module, không dùng juce_add_gui_app

add_library(loopcore STATIC ${LE_SOURCES})
target_include_directories(loopcore PUBLIC include PRIVATE src
    third_party/signalsmith-stretch third_party/SPSCQueue/include)
target_link_libraries(loopcore
    PRIVATE juce::juce_core juce::juce_events juce::juce_audio_basics
            juce::juce_audio_devices juce::juce_audio_formats juce::juce_dsp
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
target_compile_definitions(loopcore PUBLIC
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_STANDALONE_APPLICATION=0
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
set_target_properties(loopcore PROPERTIES CXX_VISIBILITY_PRESET hidden)  # chỉ symbol LE_EXPORT lộ ra

option(LE_BUILD_HARNESS "Build harness + tools" ON)   # tắt khi build iOS
option(LE_BUILD_TESTS   "Build tests" ON)
if(LE_BUILD_HARNESS)
  add_subdirectory(harness)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt)
    add_subdirectory(tools)
  endif()
endif()
if(LE_BUILD_TESTS)
  add_subdirectory(tests)
endif()
```
### 4.3 Thực tế sau P0-02/03/11 (chi tiết: `engine/docs/build-notes.md`)
- **Target:**
  - `le_juce` (STATIC): 6 module JUCE, mỗi module compile **đúng một lần**, bật `JUCE_MODULES_ONLY=ON` (không build `juceaide`).
  - `loopcore` (STATIC): `src/**/*.cpp` + `*.mm` (`-fobjc-arc`).
  - Các executable: `le-harness`, `le-tests`, `le-rtsan-selfcheck`, và `tools/*` của agent 80.
- **XCFramework:** dùng `libtool -static` gộp `libloopcore.a` với `lible_juce.a` thành một `libloopcore.a` cho mỗi slice.
- **Signalsmith** luôn build với `-O2`, kể cả bản Debug, vì ở `-O0` nó chậm khoảng 10 lần.
- **RTSan:**
  - Apple clang 17 hiểu `[[clang::nonblocking]]` và `-Wfunction-effects`, nhưng **không có** `-fsanitize=realtime`. Vì vậy phải dùng Homebrew LLVM (đã thử 23.1.1) cho cả C/C++/ObjC/ObjC++.
  - Link libc++ của Homebrew: `-L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,…`.
  - Đặt deployment target Mac là 15.0.
  - `RTSAN_OPTIONS=halt_on_error=true`. Riêng test self-check dùng `abort_on_error=0`, nếu không CTest sẽ coi là crash.
- **`-Wfunction-effects`** được bật cho `loopcore`. Chỗ nào an toàn nhưng chưa được đánh dấu thì tắt cảnh báo cục bộ bằng pragma, kèm lý do (`mach_absolute_time`, `SPSCQueue::pop`).
- **Lưu ý:** RTSan chỉ kiểm tra **bên trong** `RtEngine::process`. Lock của JUCE `AudioDeviceManager` nằm ở tầng gọi bên ngoài, nên RTSan không thấy (03 §8).

---

## 5. Tích hợp vào Flutter (iOS)

1. `scripts/build_engine_xcframework.sh`:
   ```bash
   cmake --preset ios-device && cmake --build --preset ios-device --config Release
   cmake --preset ios-sim    && cmake --build --preset ios-sim    --config Release
   # libtool gộp libloopcore.a với các object JUCE nếu cần → 1 file .a cho mỗi platform
   xcodebuild -create-xcframework \
     -library build/ios-device/Release-iphoneos/libloopcore.a   -headers engine/include \
     -library build/ios-sim/Release-iphonesimulator/libloopcore.a -headers engine/include \
     -output app/packages/engine_ffi/ios/Frameworks/LoopCore.xcframework
   ```
2. `app/packages/engine_ffi` tạo bằng `flutter create --template=plugin_ffi --platforms=ios engine_ffi`, sau đó **bỏ phần build C mặc định** của template và thay bằng XCFramework:
   - `ios/engine_ffi.podspec`: `s.vendored_frameworks = 'Frameworks/LoopCore.xcframework'`, `s.frameworks = 'AVFoundation', 'AudioToolbox', 'CoreAudio', 'CoreMIDI', 'Accelerate', 'QuartzCore', 'UIKit', 'Foundation'` (CoreAudioKit chưa cần, chỉ thêm khi làm BLE MIDI ở P4), `s.libraries = 'c++'`
   - **Chống strip symbol:** Dart tìm `le_*` lúc chạy qua `DynamicLibrary.process()`, nên linker không thấy ai gọi và sẽ bỏ các hàm đó.
     - **Đã chọn:** file `ios/Classes/LoopCoreKeepAlive.c` tham chiếu tới từng hàm `le_*`. File này do `scripts/gen_bindings.sh` **tự sinh** từ các dòng `LE_EXPORT` trong header, nên không bị lệch khi header đổi.
     - Không dùng `-force_load`, để khỏi kéo toàn bộ object JUCE vào binary.
     - Set thêm `STRIP_STYLE = non-global`. Kiểm chứng bằng `nm -gU` trên binary của app.
   - Tên thư viện: `libloopcore.a`. Đường dẫn XCFramework: `app/packages/engine_ffi/ios/Frameworks/LoopCore.xcframework` (đã gitignore).
   - Pin phiên bản phía Dart: `ffigen ^21.0.0` (bản 22 xung đột `meta` với Flutter SDK), `freezed 3.2.5` (bản 4 cần Dart 3.13).
3. `ffigen.yaml` trỏ tới `engine/include/le/engine_api.h`, sinh `lib/src/loopcore_bindings.g.dart`. Chạy bằng `scripts/gen_bindings.sh`.
4. `app/ios/Runner/Info.plist`:
   - `NSMicrophoneUsageDescription`
   - `UIBackgroundModes: [audio]`
   - `UIFileSharingEnabled` + `LSSupportsOpeningDocumentsInPlace`
   - `UISupportedInterfaceOrientations~ipad`: chỉ landscape
   - `UIRequiresFullScreen: YES`
   - Từ P4 thêm `NSLocalNetworkUsageDescription`
5. Entitlement từ P4: `com.apple.developer.networking.multicast`.

---

## 5b. Cài lên iPad thật (đã chốt 29/09/2026)
- **Ký bằng Apple ID cá nhân miễn phí:** team `3NAKR5T93Y` (thethai2019@icloud.com), bundle id `com.thethai.musiclooper`.
  - App ký kiểu này **chỉ chạy được 7 ngày**, sau đó phải cài lại.
  - Chưa làm được TestFlight và Link. Muốn làm thì phải vào Apple Developer Program và đổi team.
- **Máy test:** iPad Air 3 (A12, iPad11,3), iOS 26.6.
- **Cài hoặc cài lại:** `scripts/install_ipad.sh`. Script build engine và app ở chế độ profile, cài bằng `xcrun devicectl`, rồi mở app. Không đi qua phiên debug của Xcode, vì cách đó hay bị lỗi "Timed out waiting for CONFIGURATION_BUILD_DIR".
  - Tuỳ chọn: `release`; `--dart-define=LOOPCORE_START=projects` để mở thẳng app đầy đủ; `SKIP_ENGINE=1` để bỏ qua bước build engine.
- **Lần đầu trên iPad:** Settings → General → VPN & Device Management → Apple Development: thethai2019@icloud.com → **Trust**.
- **Simulator:** slice simulator của XCFramework **chỉ có arm64** (Mac Apple Silicon). Dùng `flutter run -d <simulator>` / `flutter test -d <simulator>`, **không** dùng `flutter build ios --simulator` (đích chung cần cả x86_64). Số xrun và CPU trên simulator không có giá trị, vì audio đi qua Mac.
- **Không chạy hai lần build Xcode cùng lúc** trên project app, và không `flutter clean` khi người khác đang build: DerivedData dùng chung sẽ hỏng (lỗi `build.db: disk I/O error`).

## 6. Script

| Script | Việc |
|---|---|
| `scripts/install_ipad.sh [profile\|release] [--dart-define…]` | Build engine + app rồi cài và mở trên iPad đang cắm (§5b) |
| `scripts/bootstrap.sh` | `git submodule update --init --recursive`, `git lfs pull`, kiểm tra công cụ, `flutter pub get` |
| `scripts/build_engine_mac.sh [preset]` | Configure và build engine (mặc định `mac-debug`) |
| `scripts/test_engine.sh [preset…]` | Build rồi chạy unit test và mọi scenario, mặc định chạy `mac-debug mac-rtsan` |
| `scripts/build_engine_xcframework.sh` | Build static lib iOS thành XCFramework trong package FFI |
| `scripts/gen_bindings.sh` | ffigen, sau đó chạy test hợp đồng Dart |
| `scripts/golden_update.sh <scenario>` | Render lại golden, **in ra đường dẫn file để nghe** |
| `scripts/run_harness.sh play` | Chạy harness real-time qua loa của Mac |

---

## 7. Quy ước git

- Branch: `p1-15-clip-scheduler` (mã task + tên ngắn). Commit: `P1-15: quantized launch state machine`.
- Nhánh chính là `main`. Mỗi task merge khi đạt DoD. Không có nhánh dài hạn.
- Pre-push hook (local CI): `scripts/test_engine.sh mac-debug` + `flutter test`. Task [RT] chạy thêm `mac-rtsan`.
- LFS cho audio. **Không commit** `build/`, `.dart_tool/`, `Pods/`, `cache/`.
