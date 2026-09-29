#!/usr/bin/env bash
# Build (mac-debug) rồi chạy harness. VD: scripts/run_harness.sh play
#                                          scripts/run_harness.sh spike --seconds 10 --voices 64
#                                          scripts/run_harness.sh render engine/tests/scenarios/x.json -o /tmp/x.wav
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="${LE_PRESET:-mac-debug}"
"$ROOT/scripts/build_engine_mac.sh" "$PRESET" >/dev/null
exec "$ROOT/build/$PRESET/harness/le-harness" "$@"
