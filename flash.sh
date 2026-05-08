#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

usage() {
  cat <<EOF
Usage: $0 [-p PORT] [-b BAUD] [--monitor]

Flashes build/pixie.bin to a connected Firefly Pixie. Auto-detects the
serial port if -p is omitted; first \$ESPPORT then \$1 then a /dev/tty
scan are consulted.

Options:
  -p PORT       serial port (e.g. /dev/tty.usbmodem1101)
  -b BAUD       baud rate (default: 460800)
  --monitor     open serial monitor after flashing
  -h, --help    show this help
EOF
}

PORT="${ESPPORT:-}"
BAUD="${ESPBAUD:-460800}"
MONITOR=0

while [ $# -gt 0 ]; do
  case "$1" in
    -p) PORT="$2"; shift 2 ;;
    -b) BAUD="$2"; shift 2 ;;
    --monitor) MONITOR=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "error: unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [ -z "$PORT" ]; then
  for p in /dev/tty.usbmodem* /dev/cu.usbmodem* /dev/ttyACM* /dev/ttyUSB*; do
    if [ -e "$p" ]; then PORT="$p"; break; fi
  done
fi

if [ -z "$PORT" ]; then
  echo "error: no serial port detected" >&2
  echo "  pass -p /dev/your-port or export ESPPORT=/dev/your-port" >&2
  echo "  on macOS try: ls /dev/tty.usbmodem*" >&2
  echo "  on linux try: ls /dev/ttyACM* /dev/ttyUSB*" >&2
  exit 1
fi

if [ ! -f build/pixie.bin ]; then
  echo "error: build/pixie.bin not found - run ./build.sh first" >&2
  exit 1
fi

echo "==> Flashing $PORT @ ${BAUD}"

if command -v esptool.py >/dev/null 2>&1; then
  esptool.py --chip esp32c3 -p "$PORT" -b "$BAUD" \
    --before default_reset --after hard_reset \
    write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
    0x0     build/bootloader/bootloader.bin \
    0x8000  build/partition_table/partition-table.bin \
    0x10000 build/pixie.bin
elif command -v idf.py >/dev/null 2>&1; then
  idf.py -p "$PORT" -b "$BAUD" flash
else
  echo "error: neither esptool.py nor idf.py found in PATH" >&2
  echo "  install one of:" >&2
  echo "    pip install esptool" >&2
  echo "    or set up ESP-IDF: https://docs.espressif.com/projects/esp-idf/" >&2
  exit 1
fi

echo
echo "==> Flash complete"

if [ "$MONITOR" = "1" ]; then
  if command -v idf.py >/dev/null 2>&1; then
    exec idf.py -p "$PORT" monitor
  elif command -v screen >/dev/null 2>&1; then
    exec screen "$PORT" 115200
  else
    echo "warning: no monitor tool available (idf.py or screen)" >&2
  fi
else
  echo "Monitor with: ./flash.sh --monitor   (or)  screen $PORT 115200"
fi
