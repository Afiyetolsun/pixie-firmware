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

# Pin the IDF image - the project's sdkconfig was generated against
# ESP-IDF 5.4.1 (see commit 547c1e1) and `espressif/idf:latest` (6.x)
# fails to bootstrap on it. Override with IDF_IMAGE if you know better.
IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v5.4.1}"

echo "==> Building Pixie firmware ($IDF_IMAGE)"
docker run --rm \
  -v "$PWD":/project \
  -w /project \
  -e HOME=/tmp \
  "$IDF_IMAGE" idf.py build

echo
echo "==> Build complete"
if [ -f build/pixie.bin ]; then
  ls -lh build/pixie.bin
fi
echo
echo "Flash with:  ./flash.sh"
