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

# Pin to the latest ESP-IDF v6 line. The submodule pins on this branch
# include the upstream v6 fixes from firefly-display, firefly-scene and
# firefly-hollows ("Update for ESP-IDF v6", "Update BLE API for ESP-IDF
# v6", "Update for LED strip IRAM config for ESP-IDF v6"), so the v5
# build-time patches that earlier branches needed are no longer required.
# Override with IDF_IMAGE if you want a different version.
IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v6.0.1}"

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
