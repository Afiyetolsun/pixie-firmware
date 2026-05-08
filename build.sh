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

# Same root cause - hollows.c:132 has a malformed initializer; the
# `.version = version` line is missing a trailing comma, so the next
# `.ready` field is parsed as a member access on `version`.
hollows_c="components/firefly-hollows/src/hollows.c"
if [ -f "$hollows_c" ] && grep -q '^[[:space:]]*\.version = version$' "$hollows_c"; then
  echo "==> patching $hollows_c (add missing comma in TaskBleInit init)"
  sed -i.bak 's/^\([[:space:]]*\)\.version = version$/\1.version = version,/' \
    "$hollows_c" && rm -f "$hollows_c.bak"
fi

# IDF v5.5's NimBLE asserts that the host lock is held when calling the
# id helpers (ble_hs_id_addr, called internally by both ble_hs_id_infer_auto
# and ble_hs_id_copy_addr). Pinned hollows code calls both bare from
# onSync, so the device panic-reboots right after BLE init. Wrap the
# whole sequence in one lock/unlock pair. ble_hs_lock/unlock exist in
# the NimBLE lib but aren't in the public include path, so forward-declare.
if [ -f "$hollows_ble" ] && \
   ! grep -q 'extern void ble_hs_lock' "$hollows_ble" && \
   grep -q '^    rc = ble_hs_id_infer_auto(0, &conn.own_addr_type);$' "$hollows_ble"; then
  echo "==> patching $hollows_ble (wrap onSync ble_hs_id_* calls with lock)"
  awk '
    /^    rc = ble_hs_id_infer_auto\(0, &conn\.own_addr_type\);$/ {
      print "    extern void ble_hs_lock(void);"
      print "    extern void ble_hs_unlock(void);"
      print "    ble_hs_lock();"
      print $0
      in_block = 1
      next
    }
    in_block && /^    print_addr/ {
      print "    ble_hs_unlock();"
      print ""
      print $0
      in_block = 0
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
