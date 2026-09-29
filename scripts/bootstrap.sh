#!/usr/bin/env bash
# Chuẩn bị repo lần đầu (09 §6): submodule, LFS, kiểm tra công cụ, flutter pub get.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ok()   { printf '  \033[32m✓\033[0m %s\n' "$*"; }
warn() { printf '  \033[33m!\033[0m %s\n' "$*"; }
fail() { printf '  \033[31m✗\033[0m %s\n' "$*"; MISSING=1; }
MISSING=0

echo "== Công cụ"
for tool in git cmake ninja; do
  if command -v "$tool" >/dev/null 2>&1; then ok "$tool: $("$tool" --version | head -1)"; else fail "$tool (brew install $tool)"; fi
done
if command -v cmake >/dev/null 2>&1; then
  CMV="$(cmake --version | head -1 | awk '{print $3}')"
  if [ "$(printf '%s\n3.25\n' "$CMV" | sort -V | head -1)" != "3.25" ]; then fail "cmake $CMV < 3.25"; fi
fi
if git lfs version >/dev/null 2>&1; then ok "$(git lfs version)"; else fail "git-lfs (brew install git-lfs)"; fi
if command -v xcodebuild >/dev/null 2>&1; then ok "$(xcodebuild -version | head -1)"; else fail "Xcode"; fi
LLVM=/opt/homebrew/opt/llvm/bin/clang++
if [ -x "$LLVM" ]; then ok "Homebrew LLVM: $("$LLVM" --version | head -1) (cho mac-rtsan)"; else warn "chưa có Homebrew LLVM → preset mac-rtsan không chạy (brew install llvm)"; fi
if command -v ccache >/dev/null 2>&1; then ok "ccache"; else warn "ccache (tuỳ chọn, build nhanh hơn)"; fi
if command -v flutter >/dev/null 2>&1; then ok "$(flutter --version 2>/dev/null | head -1)"; else warn "flutter chưa có trong PATH (cần cho app/, không cần cho engine)"; fi

echo "== Submodule"
git submodule sync --recursive >/dev/null
git submodule update --init engine/third_party/JUCE engine/third_party/Catch2 engine/third_party/SPSCQueue \
  engine/third_party/signalsmith-stretch engine/third_party/signalsmith-linear
git submodule status | sed 's/^/  /'

echo "== Git LFS"
if git lfs version >/dev/null 2>&1; then
  git lfs install --local >/dev/null
  if [ -n "$(git remote)" ]; then
    git lfs pull && ok "lfs pull xong"
  else
    warn "repo chưa có remote → bỏ qua git lfs pull"
  fi
fi

if [ -f app/pubspec.yaml ] && command -v flutter >/dev/null 2>&1; then
  echo "== Flutter"
  (cd app && flutter pub get)
fi

if [ "$MISSING" -ne 0 ]; then
  echo "Thiếu công cụ bắt buộc (xem ✗ ở trên)."; exit 1
fi
echo "Bootstrap xong. Tiếp theo: scripts/test_engine.sh mac-debug"
