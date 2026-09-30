#!/usr/bin/env bash
# Tải đúng các file nguồn mẫu thật của P2-35 (tools/library_sources.tsv) vào <thư mục> — KHÔNG vào repo.
# License: Salamander Grand Piano V3 = CC BY 3.0 (Alexander Holm), VSCO-2 CE = CC0 (Versilian Studios); xem content/LICENSES/.
# Dùng: engine/tools/fetch_library_sources.sh <thư mục nguồn>   rồi   le-import-library <thư mục nguồn>
# File đã có (khác rỗng) thì bỏ qua → chạy lại được. ~740 MB.
set -euo pipefail
[[ $# -eq 1 ]] || { echo "usage: $0 <thư mục nguồn>" >&2; exit 1; }
DEST="$1"
LIST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/library_sources.tsv"
mkdir -p "$DEST"
grep -v '^#' "$LIST" | while IFS=$'\t' read -r url rel; do printf '%s\n%s\n' "$url" "$DEST/$rel"; done |
  xargs -n 2 -P 8 sh -c 'mkdir -p "$(dirname "$1")"; [ -s "$1" ] || curl -sSfL --retry 3 -o "$1" "$0" || { echo "LỖI tải $0" >&2; exit 255; }'
echo "Xong: $(find "$DEST" -type f \( -name '*.wav' -o -name '*.flac' \) | wc -l | tr -d ' ') file trong $DEST"
