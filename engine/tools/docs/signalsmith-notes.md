# Signalsmith Stretch: ghi chú dùng cho LoopCore (P0-09)

> Viết bởi agent `80` ngày 29/09/2026, sau khi đọc `third_party/signalsmith-stretch/signalsmith-stretch.h` và chạy thử.
> Code dùng các kết luận này: `src/spike/measure/StretchBench.cpp`. Test: `tests/unit/spike/StretchBenchTest.cpp`.
> `68` lấy mục 2, 3 và 5 để cập nhật `docs/04-engine-design.md` §8.

## 1. Phiên bản

| Thư viện | Pin | Ghi chú |
|---|---|---|
| signalsmith-stretch | tag `1.4.0` (commit `a670068`, 25/09/2026) | Mảng `version[3]` trong header vẫn ghi `{1, 3, 2}`, tác giả quên cập nhật. Tin tag git |
| signalsmith-linear | tag `0.6.4` | **Phụ thuộc mới** từ 1.4.0: FFT/STFT nằm ở đây. Define `SIGNALSMITH_USE_ACCELERATE` để chạy FFT bằng vDSP (Mac + iOS) |

Include: `#include "signalsmith-stretch/signalsmith-stretch.h"`, với include dir là `signalsmith-stretch/include` và `signalsmith-linear/include`.
README cảnh báo: Apple Clang **16.0.0** + `-ffast-math` sinh SIMD sai. Máy hiện dùng Apple clang 17 nên không bị.

## 2. API formant: kết luận (đã chốt)

**Giữ formant (chất giọng) khi dịch cao độ k nửa cung:**
```cpp
stretch.setTransposeSemitones(k - cents / 100.0f);   // cents từ Yin: sửa luôn cho đúng cao độ tuyệt đối
stretch.setFormantFactor(1.0f, /*compensatePitch=*/true);
stretch.setFormantBase(f0Hz / sampleRate);            // f0 từ Yin; 0 = thư viện tự đoán (kém chính xác)
```
`setFormantSemitones(0, true)` cho kết quả y hệt `setFormantFactor(1, true)`.
**Không giữ formant** (formant trượt theo cao độ, nghe "chipmunk" khi dịch lên): `setFormantFactor(1, false)`, cũng là mặc định.

Đọc code thấy các điểm sau (số dòng tính trong `signalsmith-stretch.h` bản 1.4.0):

| Hàm | Làm gì |
|---|---|
| `setTransposeSemitones(s, tonalityLimit = 0)` (dòng 116) | Đặt `freqMultiplier = 2^(s/12)`. `tonalityLimit` tính theo **tỉ lệ sample rate** (VD `8000/sr`): tần số trên ngưỡng chỉ được cộng thêm một khoảng cố định chứ không nhân, nên giữ được chút âm sắc. Mặc định tắt |
| `setFormantFactor(m, compensatePitch)` (dòng 124) | `m` = dịch formant thêm (1 = không dịch). `compensatePitch = true` = tính đường bao phổ **có bù phần dịch cao độ** |
| `setFormantBase(f)` (dòng 133) | `f` là f0 **chia cho sample rate** (hàm `freqToBin(f) = f·fftSize`, dòng 79 của `stft.h`). Chỉ dùng để chọn độ rộng làm mượt đường bao (`decay` ở dòng 985). `0` = tự đoán từ 3 đỉnh phổ mạnh nhất, tác giả tự ghi chú là "VERY rough" |
| Khi nào tính formant (dòng 310) | `processFormants = formantMultiplier != 1 \|\| (formantCompensation && đang dịch cao độ)` |
| Công thức (dòng 1018–1034) | Với mỗi band tần số f: `năng lượng mới = năng lượng · env(f_out) / env(f)`, trong đó `f_out = invMapFormant(compensate ? mapFreq(f) : f)`. Với m = 1 và compensate = true: hoạ âm dời lên hay xuống, nhưng biên độ của nó được chỉnh theo **đường bao gốc tại vị trí mới**, tức là formant đứng yên |

Các tham số được đọc ở mỗi block phổ mới nên thứ tự gọi setter không quan trọng. Chỉ cần gọi **trước** `seek()`.

**Bằng chứng** (test "formant on giữ đường bao phổ"): nguyên âm tổng hợp f0 = 150 Hz, formant ở 800 Hz và 2500 Hz, dịch +12:

| | Trọng tâm phổ (50–8000 Hz) |
|---|---|
| Gốc | 1437 Hz |
| +12, formant off | 2887 Hz (×2.0, đường bao bị kéo lên theo) |
| +12, formant on | 1376 Hz (×0.96, đường bao giữ nguyên) |

⚠️ **Bù formant trên sine trần là vô nghĩa**: sine không có đường bao nên năng lượng bị dồn sang chỗ khác (đo được cao độ lệch 11% ở −18). Chỉ đánh giá formant với giọng hoặc nhạc cụ thật.

## 3. Render offline cho đúng độ dài và thẳng hàng (đã chốt)

```
n = số sample input, Li = inputLatency(), Lo = outputLatency()      // presetDefault @48k: Li = Lo = 2880
padded = input + Li sample 0 ở cuối
reset()
seek(padded, Li, playbackRate = 1.0)          // "thời điểm xử lý" = sample 0 của input
process(padded + Li, n, out, n)               // n sample in → n sample out
flush(out + n, Lo, playbackRate = 1.0)        // ⚠️ truyền 1.0, KHÔNG dùng mặc định 0
kết quả = out[Lo .. Lo + n)                   // bỏ Lo sample pre-roll → dài đúng n, thẳng hàng input
```

Đo với dịch 0 nửa cung (lý tưởng thì output = input), null test (output − input):

| Đoạn | `flush(…, rate = 1)` | `flush(…)` mặc định rate = 0 |
|---|---|---|
| 2880 sample đầu | −48 dB | −48 dB |
| Giữa | −132 dB | −132 dB |
| 2880 sample cuối | **−54 dB** | **−10 dB** (đuôi bị nhoè: rate 0 = "đóng băng thời gian") |

Hàm `exact(in, n, out, n)` (có từ 1.4.0) cho kết quả đầu và giữa giống hệt. Ta vẫn dùng seek/process/flush như spec 04 §8 để kiểm soát từng bước và đo thời gian.

## 4. ⚠️ Giới hạn đã biết: cao độ lệch với âm gần như sine

**(a) Sine trần** (đo bằng đếm cắt 0 trên 3.2 giây, rất chính xác; input 196, 262 và 440 Hz, 13 zone):

| Cấu hình STFT | Lệch tệ nhất | Lệch trung bình | 13 zone × 4 s trên Mac |
|---|---|---|---|
| `presetDefault` (block 120 ms, interval 30 ms) | **22 cent** | 4.8 cent | 245 ms |
| `presetCheaper` (100/40 ms) | 26 cent | 9.0 cent | 158 ms |
| `configure` 200/50 ms | 7.9 cent | 2.6 cent | 235 ms |
| `configure` 300/75 ms | 7.8 cent | 1.7 cent | 249 ms |

**(b) Âm có hoạ âm** (đo bằng Yin P3-01 trên từng zone, so với cao độ gốc × 2^(k/12), preset mặc định):

| Input | formant off, lệch tệ nhất | formant on, lệch tệ nhất |
|---|---|---|
| Sine 196 Hz (để so) | 22.5 cent | 27.1 cent |
| Nguyên âm tổng hợp 196 Hz (hoạ âm + 2 formant, cao độ đứng yên) | **1.1 cent** | **1.7 cent** |
| Giọng nói `say` của macOS (cao độ trượt, Yin confidence chỉ 0.32) | 9.5 cent | 77 cent ở −18 (mọi zone khác ≤ 39 cent) |

- **Vì sao sine bị lệch:** phase vocoder xoay pha mỗi bin output theo tần số tâm bin, rồi cộng thêm độ lệch tần số của input so với tâm bin input mà **không nhân với tỉ lệ dịch** (dòng 647–716). Một sine đứng riêng lệch tới ~1 Hz tuyệt đối, ở nốt trầm thành nhiều cent. Với âm nhiều hoạ âm, bước "khoá pha dọc" (dòng 722–802) giữ các hoạ âm đúng tỉ lệ với nhau nên **chu kỳ lặp (cao độ nghe thấy) vẫn đúng**.
- **Ảnh hưởng thực tế:** giọng hát và phần lớn nhạc cụ có nhiều hoạ âm, nên dự kiến lệch ≤ 2 cent. Âm gần như sine (huýt sáo, sáo trúc thổi nhẹ, "uuu") có thể lệch 10–25 cent ở zone thấp.
- **Chưa kết luận được:** giọng nói TTS không phải nốt hát nên số ở dòng cuối không đáng tin. Riêng formant on ở −18 lệch lớn cần kiểm lại bằng **giọng hát thật** (bước người dùng, §7).
- **Hướng xử lý cho P3-04** (rẻ, nên làm bất kể thế nào): sau khi render, chạy Yin trên ~1 giây giữa của mỗi zone rồi ghi độ lệch vào `Zone.tuneCents` để sampler bù lúc phát (chỉnh `inc`). Yin mất ~26 ms cho 4 giây trên Mac, nên 13 zone × 1 giây ≈ 85 ms trên Mac. iPad cần đo. Nếu âm gần sine là trường hợp quan trọng thì thử thêm block 200/50 ms (CPU không đổi, transient có thể nhoè hơn): `le-stretch-bench in.wav out --block 200 --interval 50`.
- Test khoá mức hiện tại để phát hiện thoái lui khi nâng thư viện: sine 440 Hz preset mặc định < 15 cent, 200/50 < 8 cent (`StretchBenchTest` "cao độ từng zone"). Nguyên âm qua Yin phải đúng nốt, < 30 cent ("StretchBench + Yin").

## 5. Âm lượng thay đổi khi giữ formant

RMS từng zone so với gốc, mẫu giọng nói "la la" (`say` của macOS, 2.1 s):

| Nửa cung | −18 | −12 | −6 | 0 | +6 | +12 | +18 |
|---|---|---|---|---|---|---|---|
| formant off | −0.6 | −0.4 | −0.3 | 0 | −0.4 | −0.7 | −1.3 dB |
| formant on | +1.5 | +0.3 | −0.6 | 0 | −5.2 | −5.9 | **−12.3 dB** |

- Dịch lên mà giữ formant thì hoạ âm thưa ra dưới cùng một đường bao nên năng lượng giảm. Dịch xuống thì ngược lại, peak có thể > 1.0.
- **P3-04 cần:** chuẩn hoá RMS từng zone về bằng zone gốc (giới hạn ±12 dB), và lưu zone dạng float để không clip.

## 6. Hiệu năng & quy tắc thread

- Mẫu 4 giây, 13 zone, Mac (M-series), Release-level `-O2`: khoảng 240 ms (formant off) và 380 ms (formant on). **Số của iPad 8 phải đo** (DoD P0-09 < 3 s, P3-04 < 2 s).
- Debug `-O0` chậm tới ~10×, nên top-level CMake đặt `-O2` cho `StretchBench.cpp` và `*Renderer.cpp` ở mọi config.
- `presetCheaper` nhanh hơn khoảng 35% nhưng cao độ kém chính xác hơn (mục 4).
- **Chỉ chạy trên worker:** `process`, `seek`, `flush` đều có `std::vector::resize` bên trong, `setFreqMap` dùng `std::function`, `configure` cấp phát. Không bao giờ gọi trên audio thread. WarpRenderer (P3-09) cũng chỉ được chạy ở worker.
- **Deterministic:** constructor mặc định seed bằng `std::random_device`. Luôn dùng `SignalsmithStretch<float>(seed)`. Số ngẫu nhiên chỉ dùng khi time-stretch > 2× (`maxCleanStretch`), nhưng cứ cố định seed để golden test ổn định. Test "deterministic" đã khoá điều này.

## 7. Việc người dùng cần làm (không AI nào làm thay được)

1. Trên Mac: `build/mac-debug/tools/le-stretch-bench <giọng-hát.wav> <outdir>` → nghe `z04 z02 z10 z08` (±6, ±12) giữa `_plain` và `_formant`. Nghe thêm `z00_-18st_formant` (nghi bị lệch cao độ, §4b): so với piano hoặc tuner.
   Muốn Yin tự kiểm luôn: chép file hát vào `engine/tests/fixtures/voice_la_A3.wav` (đổi `A3` thành nốt đã hát, thăng thì ghi `Cs4`) rồi chạy `le-tests "[fixture]"`.
2. So `--block 200 --interval 50` với mặc định: nghe phụ âm đầu có nhoè không, nốt trầm có đúng cao độ hơn không (so với piano/tuner).
3. Trên iPad 8 (qua `spike.stretchBench` trong app): ghi tổng thời gian 13 zone cho mẫu 4 giây, rồi nghe trong app Files.
4. Ghi nhận xét vào `spike-report.md` (P0-12): chọn formant on/off mặc định, chọn block STFT, có cần Rubber Band không.
