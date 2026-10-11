#!/bin/bash
# Wgrywa przepakowany WAD do partycji "wad" (offset zgodny z doom_c6/partitions.csv).
# Uzycie: tools/flash_wad.sh [plik.wad] [offset] [port]
set -e
cd "$(dirname "$0")/.."
WAD=${1:-wad/ep/doom1.wad}; OFFSET=${2:-0x150000}; PORT=${3:-/dev/ttyACM1}
ESPTOOL=$(ls ~/.arduino15/packages/esp32/tools/esptool_py/*/esptool | head -1)
$ESPTOOL --chip esp32c6 --port $PORT --baud 921600 write_flash $OFFSET "$WAD"
