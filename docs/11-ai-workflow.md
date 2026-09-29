# 11 — Làm việc với AI (Claude)

> **Nguyên tắc:** AI viết code, **bạn** hiểu, nghe và đo. AI không nghe được tiếng click và không cảm nhận được độ trễ.
> Những việc **không bao giờ giao cho AI**:
> - Duyệt golden file (phải tự nghe)
> - Quyết định go/no-go
> - Quyết định liên quan tới license
> - Đánh giá "nghe có hay không"

---

## 1. Một task = một phiên

1. Mở phiên mới (hoặc `/clear`), đứng ở thư mục repo `music-app/`.
2. Dán prompt theo mẫu ở §3. Luôn kèm **mã task** và **mục spec** liên quan.
3. Yêu cầu **viết test trước** (hoặc cùng lúc với code). Task [RT] phải chạy `mac-rtsan`.
4. Đọc diff theo checklist ở §4 và 10 §4. Không hiểu chỗ nào thì hỏi đúng chỗ đó.
5. Đạt DoD thì commit `P1-xx: …`, đánh `[x]` trong file phase và ghi vào nhật ký tiến độ.

---

## 2. Mẫu `music-app/CLAUDE.md`

```markdown
# LoopCore / Music Looper — hướng dẫn cho AI

Docs (nguồn sự thật): docs/  — đọc 00-README.md trước.
Hợp đồng FFI: engine/include/le/engine_api.h ↔ docs/05-ffi-bridge.md (sửa cả hai cùng lúc).

## Luật cứng
- Code chạy trên audio thread đánh dấu [[clang::nonblocking]]; tuân thủ 08-quality-performance.md §4.
  KHÔNG: new/delete/malloc, std::vector grow, std::string, shared_ptr copy/destroy, std::function,
  mutex/lock, log/printf/DBG, file I/O, Obj-C/Swift, MessageManager::callAsync, IIR::Coefficients::make*.
- Giao tiếp RT ↔ NRT chỉ qua: SPSC queue, std::atomic, StatePublisher.
- Mỗi thay đổi engine phải có test (Catch2 hoặc scenario). Task [RT]: chạy scripts/test_engine.sh mac-debug mac-rtsan.
- Không sửa file golden (tests/golden/*.wav). Nếu output thay đổi có chủ đích → báo cho người dùng để họ tự nghe và cập nhật.
- Comment ghi rõ thread: // [RT] / // [main] / // [worker].
- C++20, namespace le::, không dùng exception trong engine.

## Lệnh
- Build + test: scripts/test_engine.sh mac-debug
- RTSan: scripts/test_engine.sh mac-rtsan
- iOS lib: scripts/build_engine_xcframework.sh
- Bindings: scripts/gen_bindings.sh
- Flutter: cd app && flutter test

## Người dùng
Mới học C++ → khi viết code RT hoặc concurrency, giải thích ngắn gọn vì sao nó an toàn.
Trao đổi bằng tiếng Việt.
```

---

## 3. Mẫu prompt cho một task

```
Làm task P1-15 (ClipScheduler) theo:
- docs/phases/P1-engine-core.md mục P1-15 (DoD)
- docs/04-engine-design.md §3
Ràng buộc: [RT], tuân thủ 08 §4. Viết unit test cho mọi chuyển trạng thái ở 04 §3.1 và
2 scenario JSON (launch quantized, scene với ô trống). Chạy mac-debug + mac-rtsan.
Cuối cùng: tóm tắt thay đổi, giải thích phần atomic/ownership bằng tiếng Việt, liệt kê
điều gì cần mình nghe thử.
```

---

## 4. Checklist review code RT (dán vào mỗi lần review)

- [ ] Mọi hàm được gọi từ `process()` đều có `[[clang::nonblocking]]` hoặc `noexcept` và không cấp phát
- [ ] Không có container nào tăng kích thước. Mọi buffer đã cấp phát ở `prepare()` hoặc trên main thread
- [ ] Không huỷ object nào trên audio thread. Object cũ đi qua `rtToNrt` → ReleasePool
- [ ] Vị trí phát được tính từ beat, không cộng dồn giá trị float qua từng block
- [ ] Có fade hoặc crossfade ở mọi chỗ bắt đầu, dừng hoặc chuyển đổi âm thanh
- [ ] Test chạy với nhiều `blockSize` (64/128/256/1024) cho ra kết quả giống nhau
- [ ] RTSan sạch, TSan sạch (nếu có đụng tới queue hoặc atomic)

---

## 5. Debug lỗi âm thanh (click, lệch nhịp, mất tiếng)

1. **Tái hiện trong harness**: viết một scenario JSON tái hiện lỗi. Nếu tái hiện được offline thì lỗi là deterministic.
2. Render ra WAV rồi mở bằng Audacity: tìm **sample chính xác** nơi xảy ra lỗi.
3. Đổi `blockSize`: lỗi thay đổi theo block nghĩa là lỗi nằm ở chỗ chia block hoặc ranh giới.
4. Chỉ tái hiện được trên máy thật: xem CPU đỉnh, xrun, System Trace (preemption).
5. Dùng skill `vibe-debugger` để viết `BUG_REPORT.md`, rồi mới sửa. Sửa xong thì **thêm scenario đó vào bộ test**.

---

## 6. Nhiều agent làm song song

Khi giao việc cho nhiều phiên Claude cùng lúc (ví dụ `music-docs-68/77/80`):

| Quy tắc | Chi tiết |
|---|---|
| **Hợp đồng trước** | `engine_api.h` và các spec được chốt **trước** khi giao việc. Agent không tự đổi hợp đồng, muốn đổi thì nhắn cho điều phối |
| **Chia theo thư mục** | Mỗi agent **sở hữu** một tập thư mục riêng. Không ghi vào thư mục của agent khác |
| **Git** | Chỉ **một** agent được thao tác git (submodule). Các agent khác **không commit**. Người dùng hoặc điều phối commit theo từng phần |
| **Phụ thuộc** | Agent B cần kết quả của agent A (ví dụ XCFramework) thì dùng stub hoặc mock cho tới khi A báo xong qua SendMessage |
| **Báo cáo** | Xong việc thì mỗi agent báo lại: file đã tạo, lệnh đã chạy, kết quả test, việc còn lại, **điều gì cần người dùng làm tay** (quyền mic, máy thật, tài khoản Apple) |

### Phân công giai đoạn khởi động (P0 + đầu P1)
| Agent | Sở hữu | Việc |
|---|---|---|
| `music-docs-68` (**Engine core**) | Toàn bộ `engine/` **trừ** 3 thư mục của `80`. Thêm `scripts/` (trừ `gen_bindings.sh`), `README.md`, **git** | P0-02, P0-03, P0-06 (phía engine), nối P0-07/08/09 vào C API, P0-11, rồi P1-01 → P1-03 |
| `music-docs-77` (**Flutter + FFI**) | `app/**` (kể cả `app/packages/engine_ffi/`), `scripts/gen_bindings.sh` | P0-04, P0-05, P0-06 (quyền mic), rồi khung P2-01 → P2-05 (model, controller, FakeEngine) |
| `music-docs-80` (**Công cụ DSP & đo**) | `engine/src/spike/measure/**`, `engine/tools/**` (có CMakeLists riêng), `engine/tests/unit/spike/**` | P0-07, P0-08, P0-09 (lớp C++ thuần + CLI Mac + unit test), rồi P3-01 Yin (`engine/src/render/Yin.*` + test) |

**Giao diện giữa `80` và `68`:** `80` viết các class C++ thuần (không phụ thuộc C API):
- `LatencyProbe`: `prepare / processRt / analyze`
- `LoadGenerator`: `prepare / setVoices / processRt`
- `StretchBench`: `run(buffer, cfg) → result`

`68` gọi các class này từ `RtEngine` và từ `le_call`. `68` include `tools/` bằng `add_subdirectory` nếu thư mục tồn tại, và glob `tests/unit/**/*.cpp`.

### Phân công đợt 2 (từ 29/09/2026, sau commit `0300ff5`)
**Git:** điều phối (`music-docs-eb`) commit và push lên `origin` khi người dùng yêu cầu. Không agent nào chạy `git commit` hay `push`.

| Agent | Luồng | Sở hữu | Việc |
|---|---|---|---|
| `68` | A: core/time | `engine/src/{api,core,io,sim,spike}/**` (trừ `io/SfzLoader.*`, `spike/measure/**`), `engine/harness/`, CMake, `scripts/`, `engine/tests/**` (trừ thư mục của 80) | P1-04 → P1-10, **P1-11 (AudioFileIO), P1-13 (Track + Mixer), P1-14 (Limiter + meter)**, rồi P1-15 → P1-17. Xử lý media services reset. Nối các class DSP của 80 vào snapshot và RtEngine, kể cả lõi P1-12 |
| `80` | B: DSP | `engine/src/dsp/**`, `engine/src/io/SfzLoader.*`, `engine/src/render/**`, `engine/src/spike/measure/**`, `engine/tools/**`, `engine/tests/unit/{dsp,render,spike}/**`, `engine/tests/unit/io/Sfz*` | Interpolators (Hermite), P1-25, P1-26, P1-27. Nếu còn thời gian: lõi thuật toán của P1-12 (AudioClipPlayer) ở dạng class độc lập |
| `77` | UI | `app/**`, `scripts/gen_bindings.sh` | P2-15 (ui_kit), P2-06 → P2-09 (grid, chạy với FakeEngine có mô phỏng quantize), P2-23, P2-25. Main **vẫn mở màn spike** cho tới M0 |

### Đợt 3 cho `80` (29/09/2026, sau khi xong đợt 2)
Vẫn là luồng DSP, các class độc lập có unit test, `68` nối vào sau:
1. **P1-29 (lõi):** `dsp/MidiClipPlayer`: nốt chính xác từng sample trong segment, bảng note-off 128 phần tử, cắt nốt ở điểm loop, all-notes-off.
2. **P1-24:** `render/PeakBuilder` (3 mức, min/max) + định dạng cache `.peaks`.
3. **P1-28:** fixture **tổng hợp bằng code**: drum kit 4 hit (kick/snare/hat đóng/hat mở) và nhạc cụ 3 zone, kèm file `.sfz`. Không dùng sample có license, file WAV qua LFS.
4. **P3-04 (lõi, làm trước lịch):** `render/PitchRenderer`: 13 zone, giữ formant, **đo lại bằng Yin → `tuneCents`**, **chuẩn hoá RMS**, deterministic, có cancel và progress.
5. **P3-12 → P3-14 (lõi, làm trước lịch):** `dsp/Processor.h` (04 §9) + Filter, Delay (tempo-sync), Reverb, EQ3 (**tự tính biquad**), Compressor. `68` làm FxChain và snapshot.

### Đợt 4 cho `80` (29/09/2026)
Sở hữu thêm: `engine/src/midi/**`, `engine/tests/unit/midi/**`.
1. **P3-09 (lõi):** `render/WarpRenderer`: Signalsmith offline, ratio = `originalBpm / newBpm`, giữ cao độ, độ dài output chính xác từng sample, cancel < 100ms, progress. Cache `stretched/<clipId>@<bpm>.caf` (định dạng tên theo 06 §1).
2. **P3-10 (phía player):** `AudioClipPlayer` nhận thêm `stretchedData` + `stretchedBpm`. Khi BPM hiện tại khớp `stretchedBpm` thì **chuyển từ Re-Pitch sang Stretched ở ranh giới bar kế tiếp**, crossfade equal-power 10ms cùng pha. Có test không bị bước nhảy sample. Phần 68 làm: debounce trên NRT, xếp job, đưa kết quả vào snapshot.
3. **P3-02 (lõi):** `render/SilenceTrimmer` (ngưỡng −45 dBFS, giữ pre-roll 5ms).
4. **P4-01 (lõi, làm trước lịch):** `midi/MidiInputRouter`: callback thread CoreMIDI đẩy `{timestampNs, bytes[3]}` vào SPSC (không dùng `MidiMessageCollector`). Hàm [RT] đổi timestamp sang frame offset theo host time của block. `midi/MidiLearnMap` (bảng tra cố định). Test bằng message tổng hợp. Mở thiết bị thật thì `68` nối sau.

### Đợt 5 cho `80` (29/09/2026)
Sở hữu thêm: `engine/src/io/Export*`, `engine/tests/unit/io/Export*`.
1. **P3-18/19 (lõi):** `io/ExportWriter`: WAV 24-bit (float → 24-bit có **TPDF dither**), M4A/AAC 256k bằng `ExtAudioFile` (Obj-C++ `.mm`), ghi an toàn qua `.tmp`. Test trên Mac: đọc lại đúng độ dài, đúng sample rate, WAV sai lệch ≤ 1 LSB so với nguồn. Phần offline `RtEngine` và op `export.*` do `68` làm.
2. **Chuẩn bị P1-34/P3-21:** `tools/le-engine-bench` + scenario `standard_load.json` (08 §1: 8 track, 64 voice, sau này thêm 3 FX mỗi track khi có FxChain). Đo thời gian `process` mỗi block (trung bình / p99 / max, tính theo % thời lượng buffer) ở 128/256 frame, bản Release trên Mac. Dùng để bắt hồi quy hiệu năng.
3. **P4-05 (lõi):** `midi/LaunchpadMap`: note ↔ ô `(track, slot)` cho Launchpad X/Mini ở chế độ Programmer, SysEx để vào chế độ này, màu LED từ `LeClipState` + màu track (bảng palette), hàm thuần có test.

### Đợt 6 cho `80` (29/09/2026)
1. **Rà soát chéo tính an toàn RT** (chỉ đọc) cho code của `68`: `src/core/**`, `src/io/JuceDeviceIO*`, `src/api/**`, theo checklist 11 §4 và 08 §4. Tìm những gì RTSan **không bắt được**: lock ở tầng gọi ngoài hàm nonblocking, `shared_ptr` bị copy trên RT, container có thể grow, thiếu `[[clang::nonblocking]]`, memory order sai. Kết quả ghi vào `engine/tools/docs/rt-review-2026-09-29.md` (file, dòng, mức độ, đề xuất sửa), rồi gửi `68`. **Không sửa code của 68.**
2. **P1-35/P4-12 (lõi):** `render/LatencyCalibrator` bọc `LatencyProbe`: N lần đo, loại ngoại lai (MAD), trả `{offsetSamples, spreadSamples, confidence, validRuns}` và lý do khi thất bại (quá ồn, không nghe thấy chirp). `68` nối op `latency.calibrate` / `latency.setOffset`.

Sở hữu thêm của `80` (29/09): `engine/src/util/**`, `engine/tests/unit/util/**` (bắt đầu với `JsonValidator`).

**Cây source luôn build được:** CMake glob toàn bộ `src/**`, nên bất kỳ file `.cpp` nào lưu vào repo cũng được build của mọi agent compile. **Không lưu vào repo code dở làm vỡ build hoặc link.** Viết nháp ở scratchpad, hoặc giữ tối thiểu một định nghĩa rỗng compile được. Trước khi báo xong, chạy thử build `mac-debug` và `libLoopCore.dylib`.

**Thư mục `content/`** (nguồn thư viện âm thanh và `content/LICENSES/`) thuộc `77`.

**Sửa header trong lúc agent khác đang build:** object file có thể được compile theo hai layout khác nhau (đã gặp: `LatencyProbe` 152 và 120 byte) và gây segfault "vô lý". Gặp segfault ngay sau khi có người vừa sửa header thì **build lại trước khi debug**. Header dùng chung (`dsp/*.h`, `core/*.h`) chỉ đổi layout khi thật cần, và phải báo agent kia.

**Thư mục build:** `build/` trong repo thuộc về `68`. Các agent khác build ở thư mục riêng (ví dụ `cmake --preset <x> -B <scratchpad>/cmake-<x>`) để không có hai tiến trình ninja chạy trên cùng một thư mục. Mỗi báo cáo ghi rõ đã test ở build nào và preset nào.

**Hợp đồng nội bộ 68 ↔ 80:** `80` viết `engine/src/dsp/AudioData.h` và `Instrument.h` (Zone, Instrument, AudioData) **trước tiên**, gửi cho `68` duyệt, rồi mới hiện thực. Snapshot của 68 dùng lại đúng các struct này.
