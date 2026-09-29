#!/usr/bin/env bash
# Chép "thư viện khởi đầu" vào app/assets/library (P2-22, bản tạm của P2-26).
# Nguồn: engine/tests/fixtures do agent 80 sinh bằng le-gen-fixtures — mọi âm thanh đều TỰ TỔNG HỢP,
# không có license bên thứ ba. manifest.json giữ tay trong app/assets/library (06 §4):
#   - "name" là object {"en": …, "vi": …} (thiếu ngôn ngữ nào thì app dùng "en"); chuỗi thường = tiếng Anh.
#   - "tags" là id tiếng Anh cố định (drums, bass, click…); app dịch nhãn qua ARB libraryTag. Gán loop → chép vào
#     clips[].tags của project.
#   - Loop có tag "drums" (và loop gõ như click) đặt "defaultWarp": "repitch", loop có cao độ để "stretch".
# Dùng: app/tool/sync_library.sh   (chạy lại khi fixture đổi)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$ROOT/engine/tests/fixtures"
DST="$ROOT/app/assets/library"

[[ -d "$SRC/kit_synth" && -d "$SRC/inst_synth" ]] || { echo "Thiếu $SRC/{kit_synth,inst_synth}" >&2; exit 1; }

rm -rf "$DST/kits" "$DST/instruments" "$DST/loops"
mkdir -p "$DST/kits/kit_synth/samples" "$DST/instruments/inst_synth/samples" "$DST/loops"
cp "$SRC/kit_synth/"*.sfz "$DST/kits/kit_synth/"
cp "$SRC/kit_synth/samples/"*.wav "$DST/kits/kit_synth/samples/"
cp "$SRC/inst_synth/"*.sfz "$DST/instruments/inst_synth/"
cp "$SRC/inst_synth/samples/"*.wav "$DST/instruments/inst_synth/samples/"
cp "$SRC/"clip_*.wav "$DST/loops/"
echo "Đã chép thư viện: $(find "$DST" -type f | wc -l | tr -d ' ') file, $(du -sh "$DST" | cut -f1)"
