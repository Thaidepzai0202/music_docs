# P3 — DSP nâng cao (W23–W28: 08/03 – 16/04/2027)

**Mục tiêu:** đủ 3 tính năng tạo nên bản sắc của app:
1. Sampler làm từ tiếng tự thu (hát "la" rồi chơi thành giai điệu)
2. Đổi BPM mà không glitch (warp hybrid)
3. FX và Export

**Spec chính:** 04 §8 (pitch), §9 (FX), §10 (warp), §12 (export). Số liệu của Signalsmith lấy từ `spike-report.md`.

---

## Tổng quan task

| TT | Mã | Việc | Tuần | Ước lượng |
|---|---|---|---|---|
| [~] | P3-01 | `Yin` + unit test độ chính xác (sai số < 5 cent) | W23 | 1.5d |
| [x] | P3-02 | `SilenceTrimmer` + op `capture.start/stop` (thu một mẫu, không phải clip) | W23 | 1d |
| [~] | P3-03 | Sheet Record-to-Sampler (07 §4.2): thu, trim, nốt gốc, chọn chế độ | W23 | 2d |
| [~] | P3-04 | `PitchRenderer`: 13 zone, formant, job có progress và cancel | W24 | 2d |
| [x] | P3-05 | `instrument.createFromRecording` + swap snapshot + cache zone trên đĩa | W24 | 1d |
| [x] | P3-06 | Chế độ Classic + `instrument.setMode` + nút chuyển trong UI | W24 | 0.5d |
| [~] | P3-07 | Golden cho instrument đã render + chỉnh ADSR trong UI | W24 | 1d |
| [x] | P3-08 | Hook đổi tempo phía NRT + debounce 300ms | W25 | 1d |
| [x] | P3-09 | `WarpRenderer` (job, cancel, cache `stretched/`) | W25 | 1.5d |
| [x] | P3-10 | [RT] Chuyển Re-Pitch → Stretched ở ranh giới bar (crossfade 10ms) | W25 | 1.5d |
| [x] | P3-11 | Loop thư viện đi qua warp + scenario "đổi BPM không glitch" | W25 | 1d |
| [x] | P3-12 | `Processor` + `FxChain` (3 slot) + `fx.set/remove` + smoothing | W26 | 1.5d |
| [x] | P3-13 | Filter + Delay (đồng bộ tempo, ping-pong) | W26 | 1d |
| [x] | P3-14 | Reverb + EQ3 (tự tính biquad) + Compressor | W26 | 1.5d |
| [x] | P3-15 | Master EQ3 + tham số limiter | W26 | 0.5d |
| [~] | P3-16 | Panel FX (3 slot, knob, bypass, chọn loại) | W26 | 1.5d |
| [x] | P3-17 | Ghi lại buổi jam (master bus → WAV, real-time) | W27 | 1d |
| [x] | P3-18 | Export scene offline (+ stems) bằng `RtEngine` offline | W27 | 1.5d |
| [x] | P3-19 | Chuyển M4A/AAC (AVFoundation, Obj-C++) | W27 | 1d |
| [~] | P3-20 | Sheet Export + share sheet | W27 | 1d |
| [ ] | P3-21 | Đo hiệu năng full tính năng trên iPad 8 (tải chuẩn 08 §1) + tối ưu | W28 | 2d |
| [ ] | P3-22 | **Review M3** | W28 | 1d |

> Bổ sung vào 05 §3 khi làm P3-02: `capture.start {path, maxSeconds}` → `{}`, `capture.stop` → `{file, seconds}`.

---

## DoD từng task

- **P3-01:**
  - [ ] Sine 110/220/440/880 Hz: sai số < 5 cent
  - [ ] Giọng hát mẫu (fixture) ra đúng nốt
  - [ ] Nhiễu: confidence < 0.3
- **P3-02:**
  - [ ] Trim giữ 5ms pre-roll
  - [ ] Capture dừng tự động ở `maxSeconds`
  - [ ] RTSan sạch
- **P3-03:**
  - [ ] Confidence thấp → bắt buộc chọn nốt
  - [ ] Hiển thị "A3 +12 cent"
  - [ ] Huỷ giữa chừng không làm rò file
- **P3-04:**
  - [ ] Mẫu 4 giây → 13 zone < 2 giây trên iPad 8
  - [ ] Cancel dừng trong < 100ms
  - [ ] Output deterministic (golden được)
- **P3-05:**
  - [ ] Mở lại project dùng zone đã cache (không render lại)
  - [ ] Xoá cache → tự render lại
- **P3-06 → P3-07:**
  - [ ] Classic: mọi phím dùng zone gốc
  - [ ] Bạn nghe duyệt golden
- **P3-08 → P3-10:**
  - [ ] Scenario đổi BPM 120 → 100 trong lúc phát:
    - Không có bước nhảy sample > ngưỡng (`maxSampleJumpDb`)
    - Chuyển sang bản stretched đúng ở ranh giới bar kế tiếp sau khi render xong
    - Độ dài vòng đúng theo BPM mới
  - [ ] Kéo BPM liên tục → chỉ còn 1 job cuối được giữ lại (các job trước bị cancel)
- **P3-11:**
  - [ ] Loop 100 BPM gán vào project 120 BPM → khớp nhịp trong < 1 giây
- **P3-12:**
  - [ ] Thêm hoặc xoá FX trong lúc đang phát: không click, không vi phạm RTSan
  - [ ] Object cũ được thu hồi qua ReleasePool
- **P3-13 → P3-15:**
  - [ ] Unit test đáp ứng tần số (filter, EQ: đo gain tại tần số cắt ±0.5 dB)
  - [ ] Delay đúng thời gian ở BPM đã đặt
- **P3-16:**
  - [ ] Kéo knob gửi tối đa 1 lệnh mỗi frame
  - [ ] Bypass không gây click
- **P3-17 → P3-18:**
  - [ ] File export dài đúng N bar ±0 sample (WAV). M4A lệch ≤ 1024 frame do priming của AAC
  - [ ] WAV 24-bit có dither TPDF: sai lệch ≤ **1.5 LSB** (giới hạn toán học của TPDF ±1 LSB cộng làm tròn)
  - [ ] Export offline không làm xrun phần đang phát live
- **P3-19 → P3-20:**
  - [ ] M4A mở được trong app Files và Music
  - [ ] Share sheet hoạt động
- **P3-21 → P3-22 (M3):**
  - [ ] CPU đỉnh < 50% với tải chuẩn đầy đủ
  - [ ] Đạt checklist M3 (02 §3)

---

## Rủi ro riêng của P3
| Rủi ro | Giảm thiểu |
|---|---|
| Stretch cho ra đầu và cuối lệch độ dài | Dùng `seek` + `flush` theo tài liệu. Test độ dài chính xác từng sample |
| Formant nghe kỳ khi dịch lên cao | Giới hạn dịch lên cao ở +12 nửa cung, phần trên đó dùng Classic, hoặc nâng lên Rubber Band R3 (quyết định từ spike) |
| Nhiều clip cùng cần warp một lúc | Ưu tiên clip **đang phát**. Tối đa 2 job song song |
