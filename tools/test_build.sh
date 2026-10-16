#!/usr/bin/env bash
# Builds the current checkout for Windows (Linux cross build), checks it plays a CPU party under
# Wine, packages a test zip and publishes it on the tailnet-only test build server.
# See docs/agent-testing.md, "Test builds for Santi".
#
#   tools/test_build.sh <name> "<what changed, one line>"
#
# Environment: TEST_BUILDS_DIR (default ~/services/test-builds/www), TEST_BUILDS_URL
# (default http://santi:8090), SKIP_SMOKE=1 to skip the headless CPU party.
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
name="${1:?usage: tools/test_build.sh <name> \"<what changed>\"}"
description="${2:-}"
www="${TEST_BUILDS_DIR:-$HOME/services/test-builds/www}"
url="${TEST_BUILDS_URL:-http://santi:8090}"
cd "$repo"

branch="$(git rev-parse --abbrev-ref HEAD)"
commit="$(git rev-parse --short HEAD)"
[ -z "$(git status --porcelain --untracked-files=no -- . ':!sourceport/extern/melee')" ] || commit="$commit+local"

echo "== sources"
python3 tools/prepare_native_sources.py
echo "== game library (build-sourceport-gcc)"
build_log="$repo/build-sourceport-gcc/test-build.log"
if ! cmake --build build-sourceport-gcc --target melee_game -j2 > "$build_log" 2>&1; then
  grep -E "error|Error" "$build_log" | head -30 >&2
  echo "game library build failed (see $build_log)" >&2
  exit 1
fi
echo "== host (build-clangcl)"
ninja -C build-clangcl -j2 melee_source
cp build-sourceport-gcc/melee_game.dll build-sourceport-gcc/melee_game.snapexcl build-clangcl/port/

if [ "${SKIP_SMOKE:-0}" != 1 ]; then
  echo "== smoke: one-turn CPU party, headless, under Wine (about 6 minutes)"
  log="$repo/build-clangcl/port/melee_port.log"
  MELEE_PARTY_BOOT=1 MELEE_PARTY_SKIP_CSS=1 MELEE_PARTY_ALL_CPU=1 MELEE_PARTY_TURNS=1 \
  MELEE_PARTY_SEED=1 MELEE_PARTY_AUTO_ROLL=1 MELEE_FRESH_CARD=1 \
    timeout 1200 tools/wine_party.sh --hidden --headless --fast --frames 9000 \
      --script "$repo/port/scripts/party_boot.txt" > /dev/null 2>&1 || true
  if ! grep -q "\[party\] party over" "$log"; then
    grep -E "\[party\]|crash|CRASH" "$log" | tail -20 >&2 || true
    echo "smoke test failed: the CPU party did not finish (see $log)" >&2
    exit 1
  fi
  grep "\[party\]" "$log" | grep -E "minigame .*: places|final:" | sed 's/^/   /'
fi

echo "== package"
mkdir -p "$www/builds"
zip="$(python3 -W ignore tools/package_test_build.py --name "$name" --branch "$branch" --commit "$commit" \
  --description "$description" --out "$www/builds" --index "$www/index.html" --keep 5)"
echo
echo "Test build: $url/builds/$(basename "$zip")"
echo "All builds: $url/"
