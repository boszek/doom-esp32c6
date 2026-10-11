#!/bin/bash
# Kompiluje silnik kompilatorem RISC-V (jak na ESP32-C6) i wypisuje rozmiary statyczne.
# Uzycie: tools/rvsize.sh [dodatkowe_cflagi]
set -e
cd "$(dirname "$0")/.."
T=$(ls -d ~/.arduino15/packages/esp32/tools/esp-rv32/*/bin | head -1)
CC=$T/riscv32-esp-elf-gcc; SZ=$T/riscv32-esp-elf-size
E=third_party/doomgeneric/doomgeneric
OUT=${RV_OUT:-/tmp/rvsize_out}; rm -rf "$OUT"; mkdir -p "$OUT"
SRC=$(grep -o "SRC_DOOM = .*" host/Makefile | sed 's/SRC_DOOM = //')
for s in $SRC; do
  $CC -march=rv32imac -mabi=ilp32 -Os -w -I$E -DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE "$@" -c $E/$s.c -o $OUT/$s.o
done
echo "RAZEM (text=kod+rodata, data, bss):"; $SZ -t $OUT/*.o | tail -1
echo; echo "Najwieksze obiekty wg data+bss:"
$SZ $OUT/*.o | awk 'NR>1 && $1 != "(TOTALS)" {printf "%7d  data=%d bss=%d  %s\n", $2+$3, $2, $3, $6}' | sort -rn | head -12
