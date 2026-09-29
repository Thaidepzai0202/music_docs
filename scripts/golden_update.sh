#!/usr/bin/env bash
# Render lại golden của 1 scenario rồi IN ĐƯỜNG DẪN để người dùng TỰ NGHE (08 §3.2).
#   scripts/golden_update.sh <tên scenario>        (VD: scripts/golden_update.sh metronome_4bars)
# Chỉ người dùng chạy script này. AI không cập nhật golden (CLAUDE.md).
# Các kỳ vọng khác (firstNonSilentSample, maxSampleJumpDb...) phải đạt trước, thì mới ghi golden.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="${1:?cách dùng: scripts/golden_update.sh <tên scenario>}"
SC="$ROOT/engine/tests/scenarios/$NAME.json"
[ -f "$SC" ] || { echo "không có $SC"; exit 2; }

"$ROOT/scripts/build_engine_mac.sh" mac-debug >/dev/null
HARNESS="$ROOT/build/mac-debug/harness/le-harness"

GOLDEN_REL=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("expect",{}).get("golden",""))' "$SC")
if [ -z "$GOLDEN_REL" ]; then
  echo "Scenario chưa khai báo \"expect\": {\"golden\": \"golden/$NAME.wav\"} → thêm vào rồi chạy lại."; exit 2
fi
OUT="$ROOT/engine/tests/$GOLDEN_REL"

echo "== Kiểm kỳ vọng (bỏ qua golden)"
"$HARNESS" render "$SC" --no-golden || { echo "Kỳ vọng khác chưa đạt → KHÔNG ghi golden."; exit 1; }

echo "== Ghi golden"
"$HARNESS" render "$SC" -o "$OUT" --no-check
echo
echo "Golden mới: $OUT"
echo ">>> HÃY NGHE file trên (QuickTime / Audacity) trước khi commit."
echo "    Không đúng thì hoàn tác: git checkout -- \"$OUT\"  (hoặc xoá nếu là file mới)"
