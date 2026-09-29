# Nội dung tự tổng hợp — không có sample của bên thứ ba

Toàn bộ âm thanh trong **thư viện khởi đầu** của app (`app/assets/library/`) đều được **tổng hợp
bằng code** bởi công cụ `le-gen-fixtures` của dự án (agent 80, P1-28). Không có bản ghi, sample hay
preset nào lấy từ nguồn bên ngoài, nên không có điều khoản license của bên thứ ba cần tuân theo.

| Thư mục trong app | Nội dung | Cách tạo |
|---|---|---|
| `assets/library/kits/kit_synth/` | Bộ trống: kick, snare, hi-hat đóng/mở (WAV) + `kit_synth.sfz` | Tổng hợp bằng dao động + envelope trong `le-gen-fixtures` |
| `assets/library/instruments/inst_synth/` | Tone C3/C4/C5 × 2 lớp velocity (WAV) + `inst_synth.sfz` | Tổng hợp, tần số nguyên Hz, `tune` bù về nốt chuẩn |
| `assets/library/loops/` | `clip_click_4beats_100/120.wav`, `clip_sine_4beats_120.wav` | Tổng hợp (click / sine 4 beat) |

## Tái tạo

```bash
# 1) Sinh lại fixture (engine) — xem engine/tools của agent 80
#    (đầu ra: engine/tests/fixtures/{kit_synth,inst_synth,clip_*.wav})
# 2) Chép vào thư viện của app
app/tool/sync_library.sh
```

`app/assets/library/manifest.json` ghi license cho từng mục ("Tự tổng hợp (le-gen-fixtures), không
license bên thứ ba") và được màn "Giấy phép" hiển thị (P4). Khi thêm nội dung có license bên ngoài
(VSCO-2, Freesound CC0…), thêm một file license riêng vào thư mục này (06 §4).
