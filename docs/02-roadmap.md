# 02 — Roadmap & lịch theo tuần

> Làm full-time, 1 người + AI. W1 = thứ Hai **05/10/2026**. TestFlight = thứ Sáu **04/06/2027** (W35).
> Tổng cộng 35 tuần, trong đó có khoảng 2.5 tuần buffer (Noel/Tết Dương lịch, Tết Nguyên đán, cuối P4).

---

## 1. Tổng quan

```
2026        Oct         Nov         Dec         Jan'27      Feb         Mar         Apr         May         Jun
W:  1  2 | 3  4  5  6  7  8  9 10 11 12 13 14 |15 16 17 18 19 20 21 22 |23 24 25 26 27 28 |29 30 31 32 33 34 35
    [P0 ] [P1 ─────── Engine core headless ───] [P2 ──── UI + Bridge ──] [P3 ── DSP nâng cao] [P4 ── Kết nối+polish]
       M0                                    M1                       M2                  M3                   M4
                                     (Noel/1-1)             (Tết 04–10/02)
```

| Phase | Tuần | Ngày | Mục tiêu một câu | Mốc |
|---|---|---|---|---|
| **P0** Spike | W1–W2 | 05/10 – 16/10 | Chứng minh JUCE + Flutter + iPad 8 đạt latency và CPU mục tiêu | **M0** go/no-go |
| **P1** Engine core | W3–W14 | 19/10 – 08/01 | Engine headless chạy được: grid, thu âm, sampler, MIDI clip, có test | **M1** |
| **P2** UI + Bridge | W15–W22 | 11/01 – 05/03 | Jam hoàn chỉnh trên iPad bằng UI Flutter, lưu và mở project | **M2** |
| **P3** DSP nâng cao | W23–W28 | 08/03 – 16/04 | Sampler từ tiếng tự thu, warp hybrid, FX, Export | **M3** |
| **P4** Kết nối + polish | W29–W35 | 19/04 – 04/06 | MIDI controller, Ableton Link, ổn định, TestFlight | **M4** |

---

## 2. Lịch theo tuần

| Tuần | Ngày bắt đầu | Phase | Trọng tâm | Task chính |
|---|---|---|---|---|
| W1 | 05/10/2026 | P0 | Môi trường, repo, JUCE static lib chạy trong Flutter trên iPad | P0-01 → P0-05 |
| W2 | 12/10 | P0 | Mic, đo latency, stress CPU, thử Signalsmith, RTSan, báo cáo | P0-06 → P0-12 |
| W3 | 19/10 | P1 | Khung engine, harness, render offline, CMake preset | P1-01 → P1-03 |
| W4 | 26/10 | P1 | Kênh lệnh RT, lệnh cấu trúc JSON, snapshot + ReleasePool, state publisher | P1-04 → P1-07 |
| W5 | 02/11 | P1 | Transport, clock, metronome, chia block chính xác từng sample | P1-08 → P1-10 |
| W6 | 09/11 | P1 | Phát audio clip, track, mixer, limiter master | P1-11 → P1-14 |
| W7 | 16/11 | P1 | ClipScheduler: máy trạng thái, quantize, scene | P1-15 → P1-17 |
| W8 | 23/11 | P1 | Recorder: count-in, độ dài cố định, bù latency, ghi đĩa | P1-18 → P1-21 |
| W9 | 30/11 | P1 | Overdub + undo, input monitoring, peak builder | P1-22 → P1-24 |
| W10 | 07/12 | P1 | Sampler core: voice pool, zone, ADSR, SFZ loader, drum kit | P1-25 → P1-28 |
| W11 | 14/12 | P1 | MIDI clip: phát, thu, quantize khi thu; kịch bản khôi phục project | P1-29 → P1-32 |
| W12 | 21/12 | P1 | Build engine cho iOS, chạy trên iPad 8 bằng shell spike, đo tải | P1-33 → P1-35 |
| W13 | 28/12 | P1 | Hardening, soak test 30 phút, buffer (nghỉ 01/01) | P1-36 → P1-37 |
| W14 | 04/01/2027 | P1 | **M1 review**, sửa lỗi, cập nhật docs, retro | P1-38 |
| W15 | 11/01 | P2 | Package FFI, EngineClient, ticker state, khung app, model Dart | P2-01 → P2-05 |
| W16 | 18/01 | P2 | Session grid: header track, ô clip, cột scene, launch | P2-06 → P2-09 |
| W17 | 25/01 | P2 | Transport bar, luồng thu âm, arm, chế độ Edit/Perform | P2-10 → P2-13 |
| W18 | 01/02 | P2 | Panel mixer, meter (tuần ngắn, nghỉ Tết từ 04/02) | P2-14 → P2-15 |
| W19 | 08/02 | — | **Tết (nghỉ đến 10/02) + buffer**, chỉ sửa lỗi nhẹ | — |
| W20 | 15/02 | P2 | Track instrument: pad, bàn phím, thu MIDI, xem MIDI clip | P2-16 → P2-19 |
| W21 | 22/02 | P2 | Waveform, sửa audio clip, browser thư viện | P2-20 → P2-22 |
| W22 | 01/03 | P2 | Danh sách project, lưu/mở, autosave, vòng đời app, **M2** | P2-23 → P2-27 |
| W23 | 08/03 | P3 | YIN, cắt khoảng lặng, luồng thu vào sampler | P3-01 → P3-03 |
| W24 | 15/03 | P3 | PitchRenderer, job system, chế độ Classic, cache zone | P3-04 → P3-07 |
| W25 | 22/03 | P3 | Warp hybrid: re-pitch tức thì, render nền, crossfade, debounce | P3-08 → P3-11 |
| W26 | 29/03 | P3 | FX chain: filter, delay, reverb, EQ3, compressor, UI FX | P3-12 → P3-16 |
| W27 | 05/04 | P3 | Export: ghi jam real-time, render scene offline, M4A, share | P3-17 → P3-20 |
| W28 | 12/04 | P3 | Đo hiệu năng toàn bộ trên iPad 8, **M3** | P3-21 → P3-22 |
| W29 | 19/04 | P4 | MIDI controller, BLE MIDI, MIDI learn, Launchpad | P4-01 → P4-05 |
| W30 | 26/04 | P4 | Tích hợp LinkKit | P4-06 → P4-09 |
| W31 | 03/05 | P4 | Qua toàn bộ test plan Link, Link đổi tempo sang warp | P4-10 → P4-11 |
| W32 | 10/05 | P4 | UI hiệu chỉnh latency, Settings, onboarding, chốt thư viện âm thanh | P4-12 → P4-15 |
| W33 | 17/05 | P4 | Ổn định: soak test, leak, bộ nhớ thấp, interruption | P4-16 → P4-19 |
| W34 | 24/05 | P4 | Bug bash, privacy manifest, màn license, asset App Store | P4-20 → P4-23 |
| W35 | 31/05 | P4 | Build TestFlight, tester ngoài, **M4** (04/06) | P4-24 → P4-25 |

---

## 3. Các mốc và tiêu chí go/no-go

### M0: 16/10/2026, cuối spike
Quyết định có giữ **JUCE bên trong Flutter** hay không.
- [ ] Engine JUCE (static lib) phát sine và thu mic được bên trong app Flutter trên iPad 8, không xung đột AVAudioSession
- [ ] Buffer 128 (hoặc 256) chạy 10 phút **0 xrun** với tải giả lập 64 voice
- [ ] Round-trip latency đo bằng loopback ≤ 20ms (mic trong máy và loa)
- [ ] Signalsmith Stretch render 13 zone cho mẫu 4 giây trong < 3 giây trên iPad 8, chất lượng nghe chấp nhận được
- [ ] Build RTSan trên macOS chạy được với JUCE

**Nếu không đạt:**
- Lỗi phần JUCE I/O → **Plan B** (03 §8): tự viết lớp I/O Core Audio, chỉ giữ các module DSP và format của JUCE.
- Signalsmith kém → đánh giá Rubber Band R3.

### M1: 08/01/2027, engine core
- [ ] Harness macOS: jam 8 track bằng bàn phím (launch, scene, record, overdub, sampler, MIDI clip)
- [ ] Toàn bộ golden test xanh. Build RTSan, TSan, ASan sạch
- [ ] Engine chạy trên iPad 8 (qua shell spike): 8 track + 64 voice, CPU < 50%, soak 30 phút 0 xrun

### M2: 05/03/2027, jam được trên iPad
- [ ] Tạo project → thu 4 track → launch scene → lưu, đóng, mở lại vẫn nguyên
- [ ] UI 60fps ổn định trên iPad 8 khi 8 track đang phát. Pointer-down tới lúc launch được xếp hàng < 1 frame
- [ ] Không crash sau 1 giờ dùng thử liên tục

### M3: 16/04/2027, đủ tính năng DSP
- [ ] Hát "la" → sampler chơi được 3 quãng tám, có chế độ Classic
- [ ] Đổi BPM trong lúc đang phát: không glitch, chuyển sang bản stretch trong < 1 giây
- [ ] Export WAV/M4A đúng độ dài, share được
- [ ] Mọi tính năng bật cùng lúc: CPU < 50% trên iPad 8

### M4: 04/06/2027, TestFlight
- [ ] Qua **100%** test plan của LinkKit
- [ ] Soak 30 phút 0 xrun, 0 leak, 0 crash trong bug bash
- [ ] Privacy manifest, màn license bên thứ ba, entitlement multicast đã được duyệt

---

## 4. Phụ thuộc chính

```
P0 spike ──► P1-01..07 (khung + các kênh lệnh) ──► mọi task engine khác
P1-08 Transport ──► P1-15 ClipScheduler ──► P1-18 Recorder ──► P1-22 Overdub
P1-25 Sampler ──► P1-29 MIDI clip ──► P3-04 PitchRenderer (sampler từ tiếng tự thu)
P1-11 AudioClip ──► P3-08 Warp hybrid ──► P4-11 Link tempo → warp
P1-04..07 kênh lệnh ──► P2-01 FFI package ──► mọi task UI
P1-32 kịch bản khôi phục ──► P2-24 lưu/mở project
Việc hành chính: xin entitlement multicast (P0-01) ──► P4-06 LinkKit   ← nộp đơn NGAY TUẦN 1
```

---

## 5. Nhịp làm việc hằng tuần

| Ngày | Việc |
|---|---|
| Thứ Hai sáng | Đọc lại file phase, chọn task của tuần, ghi mục tiêu tuần vào Nhật ký tiến độ |
| Hằng ngày | 1 task = 1 nhánh git = 1 hoặc vài phiên Claude. Đóng task bằng DoD + test (+ RTSan nếu là task [RT]) |
| Thứ Sáu chiều | Tự demo: quay màn hình hoặc thu âm kết quả tuần. Retro 15 phút: cái gì chậm, vì sao. Cập nhật docs |
| Cuối mỗi phase | Review mốc, cập nhật 08 (kết quả đo), quyết định có cần cắt phạm vi không |

**Luật trễ hạn:** nếu một phase trễ quá **1 tuần** so với lịch thì áp dụng ngay thứ tự cắt phạm vi ở §6. Không kéo dài phase.

---

## 6. Thứ tự cắt phạm vi khi bị trễ

Cắt từ trên xuống. Mỗi mục đã cắt được chuyển sang backlog phase 2.

| # | Cắt | Tiết kiệm | Ảnh hưởng |
|---|---|---|---|
| 1 | Export M4A (chỉ giữ WAV) | ~1.5d | Thấp |
| 2 | Đèn LED phản hồi trên Launchpad | ~2d | Thấp |
| 3 | MIDI learn cho tham số FX (chỉ giữ launch clip và transport) | ~2d | Thấp |
| 4 | Compressor mỗi track (vẫn giữ limiter master) | ~1d | Thấp |
| 5 | Undo overdub | ~1.5d | Trung bình |
| 6 | Xuất stems theo từng track | ~1.5d | Trung bình |
| 7 | Sửa nốt MIDI (chỉ giữ thu, xóa, quantize) | ~3d | Trung bình |
| 8 | Warp hybrid → chỉ Re-Pitch khi đổi BPM | ~5d | **Cao**, phải ghi lại vào 01 |

**Không bao giờ cắt:** bù latency khi thu, 0 xrun, lưu project an toàn, test plan Link (nếu vẫn giữ Link).

---

## 7. Rủi ro theo dõi hằng tuần

| Rủi ro | Dấu hiệu sớm | Hành động |
|---|---|---|
| Học C++ chậm hơn dự kiến | Task [RT] tốn gấp 2 lần ước lượng hai tuần liền | Giảm độ khó: dùng lại code mẫu JUCE, dành 1 ngày học có chủ đích (10) |
| JUCE và Flutter xung đột | Crash lúc khởi động, mất tiếng sau interruption | Kích hoạt Plan B (03 §8) |
| Formant nghe tệ | Test nghe ở P0-09 không đạt | Đánh giá Rubber Band R3 (license trả phí), hoặc giảm khoảng zone xuống 2 nửa cung |
| Entitlement multicast bị chậm | Chưa được duyệt trước W28 | Liên hệ lại Apple. Nếu vẫn chưa có thì tắt Link ở build TestFlight đầu |
| Phạm vi phình ra | Có task mới không nằm trong file phase | Đưa vào backlog phase 2, không làm trong MVP |

---

## 8. Nhật ký tiến độ

| Tuần | Mục tiêu | Kết quả | Task xong | Ghi chú |
|---|---|---|---|---|
| W1 | | | | |

---

## 9. Nhật ký thay đổi kế hoạch

| Ngày | Thay đổi | Lý do |
|---|---|---|
| 2026-09-29 | Tạo kế hoạch v1 | Kết quả phiên grill-me |
