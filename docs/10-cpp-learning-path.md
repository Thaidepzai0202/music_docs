# 10 — Lộ trình học C++ & audio (chạy song song với dự án)

> **Mục tiêu:** đủ để **đọc hiểu, review và debug** code engine do AI viết, không phải để thành chuyên gia C++.
> **Nhịp học:** 1 giờ mỗi sáng trước khi code, cộng 1 bài tập (kata) mỗi tuần. Chỉ học **đúng thứ tuần đó cần dùng**.

---

## 1. Tập con C++ cần dùng (bỏ qua phần còn lại)

| Dùng | Tạm bỏ qua |
|---|---|
| Giá trị, tham chiếu `&`, `const`, con trỏ thô (chỉ để **mượn**) | Kế thừa nhiều tầng, đa kế thừa |
| `class` / `struct`, constructor và destructor, **RAII** | Template metaprogramming, SFINAE, concepts phức tạp |
| `std::unique_ptr`, `std::shared_ptr` (chỉ ở NRT) | Coroutine, module C++20 |
| `std::array`, `std::vector` (chỉ ở NRT), `std::span` | `std::variant` / `std::visit` phức tạp |
| `enum class`, `constexpr`, lambda đơn giản | Exception (engine không dùng) |
| `std::atomic`, `memory_order_acquire/release` | `memory_order_consume`, fence thủ công |
| Virtual function + interface (`Processor`) | CRTP |
| Header/source, namespace, CMake cơ bản | Build system nâng cao |

---

## 2. Lịch học theo phase

| Tuần | Chủ đề | Tài liệu | Kata (1–2 giờ) |
|---|---|---|---|
| **Trước W1** (cuối tuần này) | Cú pháp, kiểu dữ liệu, hàm, tham chiếu, `const` | learncpp.com chương 1–12 (đọc lướt) | Viết `gainToDb`/`dbToGain` + test |
| W1–W2 (P0) | Class, RAII, `unique_ptr`, header/source, CMake cơ bản | learncpp chương 13–15, 22. *An Introduction to Modern CMake* | Class `SineOsc` xuất 1 giây ra WAV bằng JUCE |
| W3–W4 | **Luật real-time**, atomic, acquire/release, SPSC | Ross Bencina *"Real-time audio programming 101: time waits for nothing"*. Talk *"Real-time 101"* (Fabian Renn-Giles & Dave Rowland, ADC 2019) | Tự viết SPSC ring buffer, chạy stress test dưới TSan, rồi so với rigtorp |
| W5 | Sample, buffer, dB, pan law, thời gian ↔ sample | *The Scientist and Engineer's Guide to DSP* (dspguide.com) ch. 1–3. JUCE tutorial "Audio basics" | Tính tay beat → sample cho 5 trường hợp, đối chiếu với test Transport |
| W6 | Nội suy (linear → Hermite), fade, click do gián đoạn | musicdsp.org (Hermite). JUCE `dsp` tutorial | Phát WAV ở tốc độ 0.5×/1.5×, so sánh linear với Hermite bằng tai |
| W7–W8 | Máy trạng thái, bù latency | 04 §3, §5 | Vẽ lại sơ đồ trạng thái clip trên giấy, sinh ra 10 test case |
| W9 | Ownership: `shared_ptr` + snapshot + ReleasePool | Talk *"C++ in the Audio Industry"* (Timur Doumler, CppCon 2015). Talk *"Using Locks in Real-Time Audio Processing, Safely"* (Timur Doumler, ADC 2020) | Giải thích bằng lời vì sao audio thread không được huỷ `shared_ptr` |
| W10 | Sampler: voice, ADSR, voice stealing | Will Pirkle *Designing Audio Effect Plugins in C++* (chương envelope) | Tự viết ADSR có test cho mỗi pha |
| W11 | MIDI: note on/off, velocity, CC, timestamp | Tóm tắt spec MIDI 1.0 (midi.org) | Parse chuỗi byte MIDI thành event |
| W12–W14 | Debug & profile: lldb, sanitizer, Instruments | Tài liệu Xcode Instruments, trang RealtimeSanitizer của Clang | Cố tình cài một `malloc` vào `process()` rồi để RTSan bắt |
| P2 (W15–W22) | FFI: ABI, layout struct, căn lề (alignment) | Tài liệu `dart:ffi`, 05 | Thêm 1 field vào `LeState` rồi cập nhật hai phía |
| P3 (W23–W28) | FFT cơ bản, pitch (YIN), time-stretch (khái niệm), biquad | Bài báo YIN (de Cheveigné & Kawahara, 2002). RBJ *Audio EQ Cookbook* (bản W3C). Blog của Signalsmith về time-stretch | Chạy YIN trên sine để kiểm tra sai số cent |
| P4 | Đồng bộ đồng hồ: host time, Link | Tài liệu và test plan của LinkKit | Giải thích `beatAtTime` bằng hình vẽ |

---

## 3. Tự kiểm tra trước khi sang phase mới

**Trước P1:**
- [ ] Giải thích được vì sao `std::vector::push_back` trong audio callback là nguy hiểm
- [ ] Viết được class có destructor giải phóng tài nguyên (RAII)
- [ ] Build được engine bằng CMake và chạy được test

**Trước P2:**
- [ ] Giải thích được `acquire/release` bằng ví dụ SPSC
- [ ] Đọc một hàm `process()` và chỉ ra được mọi chỗ có thể cấp phát bộ nhớ
- [ ] Tự sửa được một golden test hỏng (tìm ra sample nào lệch và vì sao)

**Trước P3:**
- [ ] Giải thích được Re-Pitch và time-stretch khác nhau thế nào, và vì sao stretch tốn CPU
- [ ] Hiểu được nguyên nhân gây click và 3 cách chống click (fade, crossfade, smoothing)

---

## 4. Cách đọc code AI viết

Mỗi PR hoặc mỗi task, tự hỏi:
1. Hàm này chạy trên **thread nào**? Có ghi rõ trong comment hay tên hàm không?
2. Nếu là RT: có cấp phát, lock, I/O hay log không (08 §4)? RTSan đã chạy chưa?
3. **Ai sở hữu** object này? Ai huỷ nó, và huỷ trên thread nào?
4. Có test cho **trường hợp biên** không: ranh giới rơi đúng frame 0, block 1 frame, BPM 300?
5. Nếu mình xoá dòng này thì test nào sẽ đỏ? Không có test nào đỏ thì dòng đó chưa được test.

Chỗ nào không trả lời được thì **yêu cầu Claude giải thích** đúng đoạn đó trước khi merge. Đây cũng là cách học nhanh nhất.
