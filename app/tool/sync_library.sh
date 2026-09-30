#!/usr/bin/env bash
# Chép "thư viện khởi đầu" vào app/assets/library (P2-22, bản tạm của P2-26; P2-30 drum kit; P2-35 FLAC + danh mục).
# Nguồn (agent 80, mọi âm thanh đều TỰ TỔNG HỢP, không có license bên thứ ba):
#   - engine/tests/fixtures: kit_synth (kit cũ, "hidden" trong manifest — project cũ còn mở được) và loop (WAV).
#   - content/kits/<id>: 16 pad GM 36–51, có region_label, FLAC 24-bit — tổng hợp (le-gen-kits) hoặc mẫu thật
#     (kit_acoustic: Big Rusty Drums của Karoryfer, CC0, 2 lớp velocity).
#   - content/instruments/<id>: nhạc cụ nhiều zone phủ phím 0–127, FLAC 24-bit — tổng hợp (le-gen-instruments: synth,
#     epiano, organ) hoặc mẫu
#     thật (P2-35 đợt B: Salamander Grand Piano CC BY 3.0, VSCO-2 CE CC0 — license + ghi công ở content/LICENSES).
#   Mỗi mục trong content có <id>.json gợi ý mục manifest (id, name {en, vi}, category, tags…) — gộp tay vào manifest.
# manifest.json giữ tay trong app/assets/library (06 §4):
#   - "name" là object {"en": …, "vi": …} (thiếu ngôn ngữ nào thì app dùng "en"); chuỗi thường = tiếng Anh.
#   - "tags" là id tiếng Anh cố định (drums, bass, click…); app dịch nhãn qua ARB libraryTag. Gán loop → chép vào
#     clips[].tags của project.
#   - Loop có tag "drums" (và loop gõ như click) đặt "defaultWarp": "repitch", loop có cao độ để "stretch".
#   - "hidden": true → Browser không liệt kê nhưng file vẫn trong bundle.
#   - "category" là đường thư mục của Browser (06 §4: "Drums", "Instruments/Keys"…); tên thư mục dịch qua ARB libraryFolder.
#   - Thêm kit / nhạc cụ mới: thêm vào KITS / INSTRUMENTS dưới đây + 2 dòng assets trong pubspec.yaml + mục manifest.
# Dùng: app/tool/sync_library.sh   (chạy lại khi fixture đổi)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$ROOT/engine/tests/fixtures"
CONTENT="$ROOT/content"
DST="$ROOT/app/assets/library"
KITS=(kit_808 kit_909 kit_perc kit_trap kit_lofi kit_606 kit_707 kit_linn kit_acoustic)
INSTRUMENTS=(inst_synth inst_epiano inst_organ \
  inst_piano inst_violin inst_viola inst_cello inst_contrabass inst_strings inst_pizzicato inst_harp \
  inst_flute inst_clarinet inst_oboe inst_trumpet inst_horn inst_trombone)

[[ -d "$SRC/kit_synth" ]] || { echo "Thiếu $SRC/kit_synth" >&2; exit 1; }
for k in "${KITS[@]}"; do
  [[ -f "$CONTENT/kits/$k/$k.sfz" ]] || { echo "Thiếu $CONTENT/kits/$k/$k.sfz" >&2; exit 1; }
done
for i in "${INSTRUMENTS[@]}"; do
  [[ -f "$CONTENT/instruments/$i/$i.sfz" ]] || { echo "Thiếu $CONTENT/instruments/$i/$i.sfz" >&2; exit 1; }
done

rm -rf "$DST/kits" "$DST/instruments" "$DST/loops"
mkdir -p "$DST/kits/kit_synth/samples" "$DST/loops"
cp "$SRC/kit_synth/"*.sfz "$DST/kits/kit_synth/"
cp "$SRC/kit_synth/samples/"*.wav "$DST/kits/kit_synth/samples/"
cp "$SRC/"clip_*.wav "$DST/loops/"
for k in "${KITS[@]}"; do
  mkdir -p "$DST/kits/$k/samples"
  cp "$CONTENT/kits/$k/$k.sfz" "$DST/kits/$k/"
  cp "$CONTENT/kits/$k/samples/"*.flac "$DST/kits/$k/samples/"
done
for i in "${INSTRUMENTS[@]}"; do
  mkdir -p "$DST/instruments/$i/samples"
  cp "$CONTENT/instruments/$i/$i.sfz" "$DST/instruments/$i/"
  cp "$CONTENT/instruments/$i/samples/"*.flac "$DST/instruments/$i/samples/"
done
echo "Đã chép thư viện: $(find "$DST" -type f | wc -l | tr -d ' ') file, $(du -sh "$DST" | cut -f1)"
