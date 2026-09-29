#!/usr/bin/env bash
# Configure + build engine cho Mac. Cách dùng: scripts/build_engine_mac.sh [preset] (mặc định mac-debug)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="${1:-mac-debug}"
cd "$ROOT/engine"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
echo "Build xong: $ROOT/build/$PRESET"
