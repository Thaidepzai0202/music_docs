# LoopCore / Music Looper — hướng dẫn cho AI

Docs (nguồn sự thật): `docs/` (thư mục thật trong repo; `../MUSIC docs` là symlink trỏ về đây). Đọc `docs/00-README.md` trước.
Hợp đồng FFI: `engine/include/le/engine_api.h` ↔ `docs/05-ffi-bridge.md` (sửa cả hai cùng lúc, hỏi trước khi sửa).
Kế hoạch task: `docs/phases/P*.md`. Luôn làm theo mã task (VD `P0-03`) và DoD của task đó.

## Luật cứng
- Code chạy trên audio thread: đánh dấu `[[clang::nonblocking]]`, tuân thủ `docs/08-quality-performance.md` §4.
  KHÔNG dùng: new/delete/malloc, std::vector grow, std::string, shared_ptr copy/destroy, std::function,
  mutex/lock, log/printf/DBG, file I/O, Obj-C/Swift, MessageManager::callAsync, IIR::Coefficients::make*.
- RT ↔ NRT chỉ giao tiếp qua: SPSC queue, std::atomic, StatePublisher.
- Mỗi thay đổi engine phải có test (Catch2 hoặc scenario JSON). Task [RT]: chạy `scripts/test_engine.sh mac-debug mac-rtsan`.
- Không sửa file golden (`engine/tests/golden/*.wav`). Output đổi có chủ đích → báo người dùng tự nghe rồi cập nhật.
- Comment ghi rõ thread: `// [RT]` / `// [main]` / `// [worker]`.
- C++20, namespace `le::`, không dùng exception trong engine.
- Làm việc song song nhiều agent: chỉ ghi vào thư mục mình sở hữu (bảng ở `docs/11-ai-workflow.md` §6).
  Chỉ agent Engine core được chạy lệnh git.

## Lệnh
- Bootstrap: `scripts/bootstrap.sh`
- Build + test: `scripts/test_engine.sh mac-debug`
- RTSan: `scripts/test_engine.sh mac-rtsan`
- iOS lib: `scripts/build_engine_xcframework.sh`
- Bindings: `scripts/gen_bindings.sh`
- Flutter: `cd app && flutter test`

## Người dùng
Làm solo, mới học C++ → khi viết code RT hoặc concurrency, giải thích ngắn gọn vì sao nó an toàn.
Việc chỉ người dùng làm được (chạy trên iPad 8, nghe, đo, tài khoản Apple) → liệt kê rõ ở cuối mỗi báo cáo.
Trao đổi bằng tiếng Việt.
