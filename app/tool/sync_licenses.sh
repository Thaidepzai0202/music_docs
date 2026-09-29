#!/usr/bin/env bash
# Chép giấy phép vào app/assets/licenses cho màn Settings → Giấy phép (P4-13, P4-22).
#   content/LICENSES/*.md            → assets/licenses/content/     (nội dung âm thanh của app)
#   engine/third_party/<lib>/LICENSE → assets/licenses/third_party/ (thư viện C++ build vào app)
# Catch2 chỉ dùng cho test engine (không vào app) → không chép. LinkKit chưa vendored (P4-06).
# Dùng: app/tool/sync_licenses.sh   (chạy lại khi content/LICENSES hoặc submodule đổi)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TP="$ROOT/engine/third_party"
DST="$ROOT/app/assets/licenses"

rm -rf "$DST/content" "$DST/third_party"
mkdir -p "$DST/content" "$DST/third_party"
cp "$ROOT/content/LICENSES/"*.md "$DST/content/"
cp "$TP/JUCE/LICENSE.md" "$DST/third_party/juce.txt"
cp "$TP/signalsmith-stretch/LICENSE.txt" "$DST/third_party/signalsmith-stretch.txt"
cp "$TP/signalsmith-linear/LICENSE.txt" "$DST/third_party/signalsmith-linear.txt"
cp "$TP/SPSCQueue/LICENSE" "$DST/third_party/spscqueue.txt"
if [[ -f "$TP/LinkKit/LICENSE.md" ]]; then cp "$TP/LinkKit/LICENSE.md" "$DST/third_party/linkkit.txt"; fi
echo "Đã chép giấy phép: $(find "$DST" -type f | wc -l | tr -d ' ') file"
