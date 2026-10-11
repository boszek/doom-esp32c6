#!/bin/bash
# Kopiuje pliki silnika (te same co w host/Makefile) + platforme na plytke do doom_c6/src/engine/.
# Uzycie: tools/sync_engine.sh   (po kazdej zmianie w third_party/doomgeneric lub doom_c6/platform)
set -e
cd "$(dirname "$0")/.."
E=third_party/doomgeneric/doomgeneric
D=doom_c6/src/engine
rm -rf "$D"; mkdir -p "$D"
SRC=$(grep -o "SRC_DOOM = .*" host/Makefile | sed 's/SRC_DOOM = //')
for s in $SRC; do cp "$E/$s.c" "$D/"; done
cp "$E"/*.h "$D/"
cp doom_c6/platform/* "$D/"
echo "skopiowano $(ls $D/*.c | wc -l) plikow .c do $D"
