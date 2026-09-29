# Music Looper — Bộ tài liệu kế hoạch

- **Codename engine:** `LoopCore` (C++ namespace `le`, prefix C API là `le_`)
- **App:** chưa đặt tên. Loại app: live looper kiểu Session View của Ableton, có chromatic sampler, chạy trên iPad
- **Cập nhật:** 2026-09-29
- **Trạng thái:** kế hoạch đã xong, bắt đầu P0 vào **thứ Hai 05/10/2026**
- **Mốc TestFlight:** **04/06/2027**

---

## Đọc theo thứ tự nào

| # | File | Nội dung | Khi nào đọc |
|---|---|---|---|
| 01 | [01-tech-stack-decisions.md](01-tech-stack-decisions.md) | 16 quyết định đã chốt và lý do | Trước tiên, và mỗi khi muốn đổi hướng |
| 02 | [02-roadmap.md](02-roadmap.md) | Lịch theo tuần, mốc go/no-go, thứ tự cắt phạm vi khi trễ | Mỗi thứ Hai |
| 03 | [03-architecture.md](03-architecture.md) | Mô hình thread, luồng dữ liệu, ownership, vòng đời, Plan B | Trước P0 và P1 |
| 04 | [04-engine-design.md](04-engine-design.md) | Thiết kế chi tiết từng subsystem của engine | Trước mỗi task engine |
| 05 | [05-ffi-bridge.md](05-ffi-bridge.md) | C API, lệnh, event, state struct, wrapper Dart | P0, P2 |
| 06 | [06-data-model.md](06-data-model.md) | Định dạng project, thư viện âm thanh, cấu trúc file | P1 (W11), P2 |
| 07 | [07-flutter-ui.md](07-flutter-ui.md) | Màn hình, tương tác, chiến lược repaint, state | P2 |
| 08 | [08-quality-performance.md](08-quality-performance.md) | Ngân sách hiệu năng, chiến lược test, công cụ đo | Luôn luôn |
| 09 | [09-dev-environment.md](09-dev-environment.md) | Cài đặt, cấu trúc repo, lệnh build, script | P0 ngày 1 |
| 10 | [10-cpp-learning-path.md](10-cpp-learning-path.md) | Lộ trình học C++ và audio chạy song song với dự án | Ngay bây giờ |
| 11 | [11-ai-workflow.md](11-ai-workflow.md) | Cách làm việc với Claude, mẫu CLAUDE.md, checklist review | Trước P0 |

### Kế hoạch chi tiết theo phase

| Phase | File | Tuần | Thời gian |
|---|---|---|---|
| P0 | [phases/P0-spike.md](phases/P0-spike.md) | W1–W2 | 05/10 – 16/10/2026 |
| P1 | [phases/P1-engine-core.md](phases/P1-engine-core.md) | W3–W14 | 19/10/2026 – 08/01/2027 |
| P2 | [phases/P2-ui-bridge.md](phases/P2-ui-bridge.md) | W15–W22 | 11/01 – 05/03/2027 |
| P3 | [phases/P3-advanced-dsp.md](phases/P3-advanced-dsp.md) | W23–W28 | 08/03 – 16/04/2027 |
| P4 | [phases/P4-connectivity-polish.md](phases/P4-connectivity-polish.md) | W29–W35 | 19/04 – 04/06/2027 |

---

## Quy ước

- **Mã task:** `P<phase>-<số hai chữ số>`, ví dụ `P1-07`. Dùng mã này trong commit message, trong prompt gửi Claude và trong nhật ký tiến độ.
- **Trạng thái task:**
  - `[ ]` chưa làm
  - `[~]` đang làm
  - `[x]` xong
  - `[-]` bỏ hoặc dời (ghi lý do ngay cạnh)
- **DoD (Definition of Done):** mỗi task có một danh sách tiêu chí xong. Chỉ đánh `[x]` khi đạt **tất cả**.
- **Ước lượng:** tính bằng **ngày làm việc** (1d = khoảng 6 giờ tập trung). Mỗi task từ 0.5d tới 3d. Task nào lớn hơn thì phải tách ra.
- **Đánh dấu RT:** task nào đụng tới audio thread có nhãn **[RT]**. Loại task này bắt buộc chạy build RTSan trước khi đóng (xem 08 §4).
- **Tham chiếu:** `04 §4.2` nghĩa là file 04, mục 4.2.

---

## Thuật ngữ

| Thuật ngữ | Nghĩa |
|---|---|
| Sample / frame | 1 giá trị biên độ tại 1 thời điểm. Frame là 1 sample của **mọi** kênh cùng lúc |
| Block / buffer | Nhóm frame mà Core Audio yêu cầu mỗi lần callback, mục tiêu 128 frame (~2.7ms @ 48kHz) |
| Audio thread / RT thread | Thread real-time gọi callback xử lý âm thanh. Trễ hạn là mất tiếng |
| Xrun | Callback không kịp hạn, nghe thành tiếng click hoặc rè |
| Round-trip latency | Thời gian từ lúc âm vào mic tới lúc nghe thấy nó ở loa hoặc tai nghe |
| Transport | Đồng hồ nhạc: BPM, vị trí beat, trạng thái play/stop |
| Quantize | Làm tròn thời điểm launch hoặc stop clip tới ranh giới gần nhất (1 bar, 1/4…) |
| Clip / Scene / Track | Ô trên grid / một hàng ngang / một cột dọc |
| Voice | Một nốt đang kêu trong sampler. Pool có 64 voice |
| Zone | Một mẫu âm trong instrument, áp dụng cho một dải phím và dải velocity |
| Formant | Đặc trưng âm sắc (chất giọng), độc lập với cao độ |
| Re-Pitch | Đổi tốc độ phát nên cao độ đổi theo, gần như không tốn CPU |
| Warp / time-stretch | Đổi độ dài âm mà giữ nguyên cao độ, tốn CPU |
| Snapshot (`GraphSnapshot`) | Bản cấu hình bất biến mà audio thread đọc, được thay bằng atomic swap |
| SPSC queue | Hàng đợi 1 bên ghi, 1 bên đọc, không dùng lock |
| RTSan | RealtimeSanitizer của Clang: tự bắt `malloc`, lock, I/O trên audio thread |
| Golden test | Render offline một kịch bản rồi so với file WAV chuẩn đã duyệt |
| Harness | App CLI trên macOS để chạy engine mà không cần UI |

---

## Cập nhật bộ docs thế nào

1. Khi đổi một quyết định: sửa bảng trong 01, sau đó ghi một dòng vào **Nhật ký thay đổi** ở cuối [02-roadmap.md](02-roadmap.md).
2. Xong một task: đánh `[x]` trong file phase, rồi ghi một dòng vào **Nhật ký tiến độ** trong 02.
3. Có số đo thật (CPU, latency…): cập nhật bảng "Kết quả đo" trong 08.
4. Khi spec và code lệch nhau thì **sửa spec cho khớp với code đã chạy đúng**. Docs chỉ có ích khi phản ánh sự thật.
