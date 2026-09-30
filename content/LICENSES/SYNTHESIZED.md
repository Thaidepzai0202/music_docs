# Nội dung tự tổng hợp — không có sample của bên thứ ba

Toàn bộ âm thanh trong **thư viện khởi đầu** của app (`app/assets/library/`) đều được **tổng hợp
bằng code** bởi công cụ của dự án: `le-gen-fixtures` (agent 80, P1-28), `le-gen-kits` (agent 80, P2-30 / P2-35) và `le-gen-instruments` (agent 80, P2-35). Không có bản ghi, sample hay
preset nào lấy từ nguồn bên ngoài, nên không có điều khoản license của bên thứ ba cần tuân theo.

| Thư mục trong app | Nội dung | Cách tạo |
|---|---|---|
| `assets/library/kits/kit_808/` | Kit 808: 16 pad map GM 36–51 (FLAC) + `kit_808.sfz` (có `region_label`) | Tổng hợp bằng code trong `le-gen-kits`, không sample bên thứ ba |
| `assets/library/kits/kit_909/` | Kit 909: 16 pad map GM 36–51 (FLAC) + `kit_909.sfz` | Tổng hợp bằng code trong `le-gen-kits`, không sample bên thứ ba |
| `assets/library/kits/kit_perc/` | Bộ gõ (conga, bongo, cowbell…): 16 pad GM 36–51 (FLAC) + `kit_perc.sfz` | Tổng hợp bằng code trong `le-gen-kits`, không sample bên thứ ba |
| `assets/library/kits/{kit_trap,kit_lofi,kit_606,kit_707,kit_linn}/` | Kit Trap, Lo-fi, 606, 707, Linn: mỗi kit 16 pad GM 36–51 (FLAC) + `.sfz` có `region_label` | Tổng hợp bằng code trong `le-gen-kits`, không sample bên thứ ba |
| `assets/library/instruments/inst_epiano/` | Piano điện kiểu Rhodes (tổng hợp FM), nhiều zone × 2 lớp velocity (FLAC) + `inst_epiano.sfz` | Tổng hợp bằng code trong `le-gen-instruments`, không sample bên thứ ba |
| `assets/library/instruments/inst_organ/` | Organ (tổng hợp drawbar), nhiều zone, có loop (FLAC) + `inst_organ.sfz` | Tổng hợp bằng code trong `le-gen-instruments`, không sample bên thứ ba |
| `assets/library/kits/kit_synth/` | Bộ trống cũ: kick, snare, hi-hat đóng/mở (WAV) + `kit_synth.sfz`. Ẩn khỏi Browser, giữ để project cũ còn mở được | Tổng hợp bằng dao động + envelope trong `le-gen-fixtures` |
| `assets/library/instruments/inst_synth/` | Tone tổng hợp: zone C0–C8 × 2 lớp velocity (FLAC), phủ phím 0–127 + `inst_synth.sfz` | Tổng hợp bằng code trong `le-gen-instruments` (P2-35 B2), không sample bên thứ ba |
| `assets/library/loops/` | `clip_click_4beats_100/120.wav`, `clip_sine_4beats_120.wav` | Tổng hợp (click / sine 4 beat) |

## Tái tạo

```bash
# 1) Sinh lại fixture (engine) — xem engine/tools của agent 80
#    (đầu ra: engine/tests/fixtures/{kit_synth,inst_synth,clip_*.wav})
#    Drum kit (P2-30 / P2-35): le-gen-kits content/kits <thư mục nghe thử>   (ra đúng từng bit)
#    (đầu ra: content/kits/<id>/ — kit_808, kit_909, kit_perc, kit_trap, kit_lofi, kit_606, kit_707, kit_linn)
#    Nhạc cụ (P2-35): le-gen-instruments content/instruments <thư mục nghe thử>
#    (đầu ra: content/instruments/<id>/ — inst_synth, inst_epiano, inst_organ)
# 2) Chép vào thư viện của app
app/tool/sync_library.sh
```

`app/assets/library/manifest.json` ghi license cho từng mục ("Tự tổng hợp (le-gen-fixtures), không
license bên thứ ba") và được màn "Giấy phép" hiển thị (P4). Khi thêm nội dung có license bên ngoài
(VSCO-2, Freesound CC0…), thêm một file license riêng vào thư mục này (06 §4).
