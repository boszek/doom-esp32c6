#!/bin/bash
# Wgrywa przepakowany WAD do partycji "wad" (offset zgodny z doom_c6/partitions.csv).
# Uzycie: tools/flash_wad.sh [plik.wad] [port]
set -e
cd "$(dirname "$0")/.."
WAD=${1:-wad/c1/doom1.wad}; PORT=${2:-/dev/ttyACM1}
ESPTOOL=$(ls ~/.arduino15/packages/esp32/tools/esptool_py/*/esptool | head -1)
$ESPTOOL --chip esp32c6 --port $PORT --baud 921600 write_flash 0x110000 "$WAD"
