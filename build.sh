#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v docker >/dev/null 2>&1; then
  echo "error: docker not found in PATH" >&2
  echo "  install Docker Desktop: https://docs.docker.com/get-docker" >&2
  exit 1
fi

if ! docker info >/dev/null 2>&1; then
  echo "error: docker daemon is not running" >&2
  echo "  start Docker Desktop and try again" >&2
  exit 1
fi

if [ ! -f components/firefly-hollows/include/firefly-hollows.h ]; then
  echo "==> Initializing submodules"
  git submodule update --init --recursive
fi

# Workaround for a bug in the pinned firefly-hollows commit (2fbda9d):
# src/task-ble.c references TaskBleInit (defined in src/hollows.h) without
# including it, so the build fails. Upstream fixed this in commit 1f55b89,
# but that commit also reshuffles the FfxKey bit assignments, which is too
# risky to inherit without also updating downstream code. Patch in place;
# idempotent so re-runs are safe. Real fix: open PR against
# firefly/component-hollows to land just the include change.
hollows_ble="components/firefly-hollows/src/task-ble.c"
if [ -f "$hollows_ble" ] && ! grep -q '^#include "hollows.h"' "$hollows_ble"; then
  echo "==> patching $hollows_ble (add missing #include \"hollows.h\")"
  awk '
    /^#include "firefly-tx.h"/ && !patched {
      print
      print ""
      print "// Patched by build.sh: pinned commit is missing this include"
      print "#include \"hollows.h\""
      patched = 1
      next
    }
    { print }
  ' "$hollows_ble" > "$hollows_ble.tmp" && mv "$hollows_ble.tmp" "$hollows_ble"
fi

# Pin the IDF image - `espressif/idf:latest` (6.x) fails to bootstrap
# on this project. v5.5.x is the most recent line known to build cleanly.
# Override with IDF_IMAGE if you know better.
IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v5.5.4}"

# Switching IDF major.minor versions reuses incompatible cmake cache
# (different python_env path). Wipe build/ when the image changes.
if [ -d build ] && [ -f build/.idf-image ]; then
  cached=$(cat build/.idf-image)
  if [ "$cached" != "$IDF_IMAGE" ]; then
    echo "==> IDF image changed ($cached -> $IDF_IMAGE); cleaning build/"
    rm -rf build
  fi
fi

echo "==> Building Pixie firmware ($IDF_IMAGE)"
docker run --rm \
  -v "$PWD":/project \
  -w /project \
  -e HOME=/tmp \
  "$IDF_IMAGE" idf.py build

mkdir -p build && echo "$IDF_IMAGE" > build/.idf-image

echo
echo "==> Build complete"
if [ -f build/pixie.bin ]; then
  ls -lh build/pixie.bin
fi
echo
echo "Flash with:  ./flash.sh"
