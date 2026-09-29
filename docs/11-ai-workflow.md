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
