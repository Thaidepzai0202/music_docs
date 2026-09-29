#!/usr/bin/env bash
# Build engine cho iOS (device + simulator) → LoopCore.xcframework trong plugin FFI (09 §5).
#   scripts/build_engine_xcframework.sh
# Đầu ra: app/packages/engine_ffi/ios/Frameworks/LoopCore.xcframework
#   ios-arm64/libloopcore.a                    (device)
#   ios-arm64-simulator/libloopcore.a          (simulator)
#   Headers/le/engine_api.h
# Mỗi libloopcore.a đã GỘP sẵn object JUCE (libtool -static), nên app chỉ cần link 1 file.
#
# Chống strip symbol: hàm le_* có __attribute__((visibility("default"), used)) (LE_EXPORT) và `used` đặt cờ
# no_dead_strip trong Mach-O. Script này KIỂM TRA mọi hàm LE_EXPORT trong header đều có mặt và là
# "external" (không phải "private external") trong cả hai slice; thiếu thì dừng với lỗi.
# Phía app (agent 77): LoopCoreKeepAlive.c tham chiếu từng hàm + STRIP_STYLE=non-global (09 §5).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HEADER="$ROOT/engine/include/le/engine_api.h"
OUT="$ROOT/app/packages/engine_ffi/ios/Frameworks/LoopCore.xcframework"
STAGE="$ROOT/build/xcframework"
CONFIG=Release

cd "$ROOT/engine"
for preset in ios-device ios-sim; do
  echo "════════ build $preset ($CONFIG) ════════"
  cmake --preset "$preset" >/dev/null
  cmake --build --preset "$preset" -- -quiet
done

rm -rf "$STAGE"
mkdir -p "$STAGE/ios-device" "$STAGE/ios-sim"
libtool -static -o "$STAGE/ios-device/libloopcore.a" \
  "$ROOT/build/ios-device/$CONFIG-iphoneos/libloopcore.a" "$ROOT/build/ios-device/$CONFIG-iphoneos/lible_juce.a"
libtool -static -o "$STAGE/ios-sim/libloopcore.a" \
  "$ROOT/build/ios-sim/$CONFIG-iphonesimulator/libloopcore.a" "$ROOT/build/ios-sim/$CONFIG-iphonesimulator/lible_juce.a"

# ── Kiểm tra symbol ──
EXPECTED=$(grep -oE 'LE_EXPORT[^;]*\ble_[a-z_]+\(' "$HEADER" | grep -oE 'le_[a-z_]+' | sort -u)
FAILED=0
for slice in ios-device ios-sim; do
  LIB="$STAGE/$slice/libloopcore.a"
  # "private external" = bị ẩn (visibility hidden) → Dart không tìm thấy.
  SYMS=$(nm -m "$LIB" 2>/dev/null | grep -E '\(__TEXT,__text\) external ' | grep -E '_le_[a-z_]+$' || true)
  for fn in $EXPECTED; do
    if ! grep -qE "external (\[no dead strip\] )?_${fn}\$" <<<"$SYMS"; then
      echo "✗ $slice: thiếu hoặc bị ẩn symbol _$fn"; FAILED=1
    fi
  done
  echo "✓ $slice: $(grep -c . <<<"$SYMS") symbol le_* external  ($(du -h "$LIB" | cut -f1))"
done
[ "$FAILED" -eq 0 ] || { echo "Symbol le_* thiếu → dừng."; exit 1; }

rm -rf "$OUT"
mkdir -p "$(dirname "$OUT")"
xcodebuild -create-xcframework \
  -library "$STAGE/ios-device/libloopcore.a" -headers "$ROOT/engine/include" \
  -library "$STAGE/ios-sim/libloopcore.a"    -headers "$ROOT/engine/include" \
  -output "$OUT" >/dev/null

echo
echo "XCFramework: $OUT"
nm -gU "$OUT/ios-arm64/libloopcore.a" 2>/dev/null | grep ' _le_' | sed 's/^/  /'
cat <<'EOF'

Framework hệ thống app phải link (podspec s.frameworks):
  AVFoundation AudioToolbox CoreAudio CoreMIDI Accelerate QuartzCore Foundation UIKit
  (CoreAudioKit chưa cần ở P0; thêm khi có màn Bluetooth MIDI ở P4)
Thư viện: s.libraries = 'c++'
EOF
