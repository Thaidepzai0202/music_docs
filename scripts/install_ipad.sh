#!/usr/bin/env bash
# Build app và cài thẳng lên iPad đang cắm cáp, không qua phiên debug của Xcode.
# Dùng khi: lần đầu; mỗi 7 ngày (app ký bằng Apple ID miễn phí sẽ hết hạn); hoặc khi engine/app thay đổi.
#
#   scripts/install_ipad.sh                    # profile, mở màn spike (đo P0), có nút "Projects (dev)"
#   scripts/install_ipad.sh release            # bản release
#   scripts/install_ipad.sh profile --dart-define=LOOPCORE_START=projects   # mở thẳng app đầy đủ
#   SKIP_ENGINE=1 scripts/install_ipad.sh      # bỏ qua bước build lại XCFramework
#
# Ký bằng team 3NAKR5T93Y (Apple ID cá nhân). Lần đầu trên iPad:
#   Settings → General → VPN & Device Management → Apple Development: thethai2019@icloud.com → Trust
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODE="${1:-profile}"
[ $# -gt 0 ] && shift
BUNDLE_ID="com.thethai.musiclooper"

case "$MODE" in
  profile|release) ;;
  *) echo "Chế độ phải là profile hoặc release (debug của Flutter chạy chậm trên máy thật)"; exit 2 ;;
esac

# 1. Tìm iPad đang kết nối (CoreDevice identifier)
DEVICE="$(xcrun devicectl list devices 2>/dev/null \
  | awk '/connected/ && /iPad/ { for (i = 1; i <= NF; i++) if ($i ~ /^[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}$/) { print $i; exit } }')"
if [ -z "$DEVICE" ]; then
  echo "Không thấy iPad nào đang kết nối. Cắm cáp, mở khoá iPad, bấm 'Trust' nếu được hỏi, rồi chạy lại."
  exit 1
fi
echo "▶ iPad: $DEVICE"

# 2. Engine mới nhất
if [ "${SKIP_ENGINE:-0}" != "1" ]; then
  echo "▶ Build engine (XCFramework)…"
  "$ROOT/scripts/build_engine_xcframework.sh" >/dev/null
fi

# 3. Build app (có ký bằng team trong Xcode project)
echo "▶ Build app ($MODE)…"
cd "$ROOT/app"
flutter build ios --"$MODE" "$@"

APP=""
for p in build/ios/iphoneos/Runner.app "build/ios/$(tr '[:lower:]' '[:upper:]' <<< "${MODE:0:1}")${MODE:1}-iphoneos/Runner.app"; do
  [ -d "$p" ] && { APP="$p"; break; }
done
[ -n "$APP" ] || { echo "Không tìm thấy Runner.app sau khi build"; exit 1; }

# 4. Cài và mở
echo "▶ Cài $APP…"
xcrun devicectl device install app --device "$DEVICE" "$APP" >/dev/null
echo "▶ Mở app…"
if ! xcrun devicectl device process launch --device "$DEVICE" "$BUNDLE_ID" >/dev/null 2>&1; then
  echo "⚠ Đã cài nhưng chưa mở được. Nếu là lần đầu (hoặc sau khi cài lại chứng chỉ):"
  echo "  iPad → Settings → General → VPN & Device Management → Apple Development: thethai2019@icloud.com → Trust"
  exit 3
fi
echo "✓ Đã cài và mở Music Looper trên iPad."
