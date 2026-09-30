#!/usr/bin/env bash
# Build rồi chạy toàn bộ test (unit + scenario) cho từng preset.
# Cách dùng: scripts/test_engine.sh [preset…]   (mặc định: mac-debug mac-rtsan)
# Watchdog: mỗi test CTest tối đa 1800 s (CMakePresets test-base); LE_TEST_TIMEOUT=giây để đổi. Quá giờ → ✗ kèm
# output (test case cuối đã xong ở dòng "x.xxx s: …" cuối; test treo là test kế tiếp), không treo âm thầm.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESETS=("$@")
[ ${#PRESETS[@]} -eq 0 ] && PRESETS=(mac-debug mac-rtsan)
cd "$ROOT/engine"

declare -a RESULTS=()
STATUS=0
for p in "${PRESETS[@]}"; do
  echo "════════ $p ════════"
  if cmake --preset "$p" >/dev/null && cmake --build --preset "$p" && ctest --preset "$p" ${LE_TEST_TIMEOUT:+--timeout "$LE_TEST_TIMEOUT"}; then
    RESULTS+=("✓ $p")
  else
    RESULTS+=("✗ $p"); STATUS=1
  fi
done
echo "════════ Tổng kết ════════"
printf '  %s\n' "${RESULTS[@]}"
exit $STATUS
