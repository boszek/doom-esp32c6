#!/bin/bash
# Szuka minimalnej strefy (KB), przy ktorej wszystkie dema daja trace identyczny z referencja.
# Uzycie: tools/zonesweep.sh BINARKA WAD KB1 KB2 ...
cd "$(dirname "$0")/../host"
BIN=$1; WAD=$2; shift 2
for kb in "$@"; do
  ok=1; info=""
  for d in demo1 demo2 demo3; do
    out=$(timeout 300 ./$BIN -nogui -iwad $WAD -kb $kb -timedemo $d 2>&1 >/dev/null | tr '\r' '\n')
    r=$(echo "$out" | grep -o "TRACE.*" | head -1)
    ref=$(grep -A1 "== $d" reference_trace.txt | grep -o "TRACE.*")
    if [ "$r" != "$ref" ]; then ok=0; info="$info [$d: $(echo "$out" | grep -o 'Z_Malloc.*\|I_Error.*\|R_.*no more.*' | head -1 | cut -c1-60)]"; fi
  done
  echo "$kb KB -> $([ $ok = 1 ] && echo OK || echo FAIL)$info"
done
