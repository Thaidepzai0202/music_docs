# Thư viện bên thứ ba — phiên bản đã pin

Cập nhật: 2026-09-29 (P0-02). Đổi pin thì sửa file này, rồi chạy `scripts/test_engine.sh mac-debug mac-rtsan`.

| Submodule | Đường dẫn | Pin (tag) | Commit | Ngày commit | License |
|---|---|---|---|---|---|
| JUCE | `engine/third_party/JUCE` | `9.0.3` (bản 9.x mới nhất) | `be29c81492b6151c8ea8d14c840e1311963b3a83` | 2026-09-28 | JUCE 9 (AGPLv3 / thương mại; dùng gói **Starter**, xem P0-01) |
| Signalsmith Stretch | `engine/third_party/signalsmith-stretch` | `1.4.0` | `a670068d9aeb64913331d5cc29337b19a457a7df` | 2026-09-25 | MIT |
| Signalsmith Linear | `engine/third_party/signalsmith-linear` | `0.6.4` | `de55e6a50ffcf6f8f43f649692d94691c7025151` | 2026-09-25 | MIT |
| SPSCQueue (rigtorp) | `engine/third_party/SPSCQueue` | commit cố định (sau `v1.1`) | `1053918dbd251fbff69b24ef27fa5d51c29ec2af` | 2023-09-24 | MIT |
| Catch2 | `engine/third_party/Catch2` | `v3.16.0` | `317ac1ed4c0bb6e6b91eafc817e05c488feffcb3` | 2026-08-25 | BSL-1.0 |
| LinkKit | `engine/third_party/LinkKit` | — (P4, vendored) | — | — | — |

## Ghi chú
- **signalsmith-linear** là submodule thứ 5, không có trong danh sách ban đầu ở 09 §3. Từ bản 1.4.0, Stretch
  `#include "signalsmith-linear/stft.h"`, và CMakeLists của nó sẽ tự `FetchContent` linear 0.6.4 nếu chưa có target
  `signalsmith-linear`. Engine `add_subdirectory` linear **trước** stretch, nên build không cần mạng (kể cả iOS).
  Linear bật `SIGNALSMITH_USE_ACCELERATE` (FFT qua vDSP) trên Apple.
- Submodule lồng `signalsmith-stretch/cmd/util` (chỉ dùng cho CLI mẫu của Stretch) **không** được init.
- JUCE được build với `JUCE_MODULES_ONLY=ON` (không build `juceaide`, không dùng `juce_add_*`).
- Module JUCE dùng: `juce_core juce_events juce_audio_basics juce_audio_devices juce_audio_formats juce_dsp`.
