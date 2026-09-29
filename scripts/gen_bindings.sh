#!/usr/bin/env bash
# Sinh bindings Dart từ engine/include/le/engine_api.h (ffigen) + file C chống strip symbol,
# rồi chạy test hợp đồng Dart.
# Dùng: scripts/gen_bindings.sh
# Chạy lại mỗi khi header đổi (05 §5). Header đổi kích thước struct → test hợp đồng sẽ đỏ.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKG="$ROOT/app/packages/engine_ffi"
HEADER="$ROOT/engine/include/le/engine_api.h"
KEEP_ALIVE="$PKG/ios/Classes/LoopCoreKeepAlive.c"

[[ -f "$HEADER" ]] || { echo "Không thấy $HEADER" >&2; exit 1; }
export LANG="${LANG:-en_US.UTF-8}"

cd "$PKG"
flutter pub get >/dev/null
echo "▶ ffigen: $HEADER"
dart run ffigen --config ffigen.yaml
dart format lib/src/loopcore_bindings.g.dart >/dev/null

# Dart tìm le_* lúc chạy (DynamicLibrary.process()) nên linker không thấy ai gọi và sẽ bỏ
# các object đó khỏi libloopcore.a. File này tham chiếu từng hàm LE_EXPORT → linker phải giữ (09 §5).
echo "▶ keep-alive: $KEEP_ALIVE"
FUNCS=$(sed -nE 's/^LE_EXPORT[^(]*[ *](le_[A-Za-z0-9_]+)[[:space:]]*\(.*/\1/p' "$HEADER")
[[ -n "$FUNCS" ]] || { echo "Không tìm thấy hàm LE_EXPORT nào trong header" >&2; exit 1; }
{
  echo "// SINH TỰ ĐỘNG bởi scripts/gen_bindings.sh từ engine/include/le/engine_api.h — KHÔNG sửa tay."
  echo "//"
  echo "// Tham chiếu mọi hàm le_* để linker không bỏ chúng khỏi libloopcore.a (09 §5)."
  echo "// Khai báo chỉ để lấy địa chỉ (không include header, tránh phụ thuộc đường dẫn header trong pod)."
  echo "// Chỉ được biên dịch khi có LoopCore.xcframework (xem engine_ffi.podspec)."
  echo
  for f in $FUNCS; do echo "extern void $f(void);"; done
  echo
  echo "__attribute__((used, visibility(\"hidden\")))"
  echo "void *const loopcore_keep_alive[] = {"
  for f in $FUNCS; do echo "    (void *)&$f,"; done
  echo "};"
} > "$KEEP_ALIVE"
echo "  $(echo "$FUNCS" | wc -l | tr -d ' ') hàm: $(echo $FUNCS)"

echo "▶ test hợp đồng (engine_ffi): layout struct + engine thật trên Mac (tự skip nếu chưa có dylib)"
flutter test test/contract_test.dart test/real_engine_contract_test.dart
