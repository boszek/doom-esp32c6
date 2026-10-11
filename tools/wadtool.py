#!/usr/bin/env python3
"""Narzedzie do okrajania WAD-a pod demo Dooma na ESP32-C6.

  wadtool.py info  doom1.wad
  wadtool.py pack  doom1.wad out.wad TRACE1 [TRACE2 ...] [--sprite-family] [--keep NAME,...]
  wadtool.py episode doom1.wad out.wad [--demos] [--keep NAME,...]   # pelny epizod do gry (bez sladu)

TRACE = stderr silnika zbudowanego z -DDOOM_LUMPTRACE (linie "LUMP <faza> <nr> <nazwa>").
Zachowujemy caly katalog lumpow (silnik polega na kolejnosci: S_START..S_END, F_START..F_END,
mapy itd.), a dane lumpow nieuzywanych w fazie gry (faza 1) zastepujemy jednym wspolnym,
malym patchem 1x1. Dane lumpow sa wyrownane do 4 bajtow (bezpieczny dostep ze flasha na RISC-V).
"""
import struct
import sys

ALWAYS_KEEP = {"PLAYPAL", "COLORMAP", "PNAMES", "TEXTURE1", "TEXTURE2", "DEMO1", "DEMO2", "DEMO3", "DEMO4"}


def read_wad(path):
    data = open(path, "rb").read()
    ident, n, off = struct.unpack_from("<4sii", data, 0)
    assert ident in (b"IWAD", b"PWAD"), "to nie jest WAD"
    lumps = []
    for i in range(n):
        pos, size, name = struct.unpack_from("<ii8s", data, off + 16 * i)
        lumps.append((name.split(b"\0")[0].decode("ascii"), pos, size))
    return data, ident, lumps


def classify(lumps):
    """Zwraca liste kategorii per lump (do statystyk)."""
    cats = []
    zone = None
    for name, pos, size in lumps:
        if name in ("S_START", "SS_START"):
            zone = "sprite"
        elif name in ("S_END", "SS_END"):
            zone = None
        elif name in ("F_START", "FF_START", "F1_START", "F2_START", "F3_START"):
            zone = "flat"
        elif name in ("F_END", "FF_END", "F1_END", "F2_END", "F3_END"):
            zone = None
        elif name in ("P_START", "PP_START", "P1_START", "P2_START", "P3_START"):
            zone = "patch"
        elif name in ("P_END", "PP_END", "P1_END", "P2_END", "P3_END"):
            zone = None
        if size == 0:
            cats.append("marker")
        elif zone:
            cats.append(zone)
        elif name.startswith("DS") or name.startswith("DP"):
            cats.append("sound")
        elif name.startswith("D_"):
            cats.append("music")
        elif name.startswith("DEMO"):
            cats.append("demo")
        else:
            cats.append("other")
    return cats


def cmd_info(path):
    data, ident, lumps = read_wad(path)
    cats = classify(lumps)
    tot = {}
    cnt = {}
    for (name, pos, size), c in zip(lumps, cats):
        tot[c] = tot.get(c, 0) + size
        cnt[c] = cnt.get(c, 0) + 1
    print(f"{path}: {len(data)} B, {len(lumps)} lumpow")
    for c in sorted(tot, key=lambda k: -tot[k]):
        print(f"  {c:8s} {cnt[c]:5d} lumpow {tot[c]:9d} B")


def parse_trace_names(files):
    names = set()
    for f in files:
        for line in open(f, errors="replace"):
            if line.startswith("LUMP "):
                names.add(line.split()[3])
    return names


def parse_trace(files):
    phase1, phase0 = set(), set()
    for f in files:
        for line in open(f, errors="replace"):
            if line.startswith("LUMP "):
                _, ph, num, name = line.split()[:4]
                (phase1 if ph == "1" else phase0).add(int(num))
    return phase0, phase1


def sprite_family(lumps, cats, keep):
    """Dla kazdego uzytego sprite'a zachowaj wszystkie lumpy z tym samym 4-literowym prefiksem."""
    prefixes = {lumps[i][0][:4] for i in keep if cats[i] == "sprite"}
    return {i for i, (n, _, _) in enumerate(lumps) if cats[i] == "sprite" and n[:4] in prefixes}


def pnames(data, lumps):
    for name, pos, size in lumps:
        if name == "PNAMES":
            n = struct.unpack_from("<i", data, pos)[0]
            return {data[pos + 4 + 8 * i: pos + 12 + 8 * i].split(b"\0")[0].decode("ascii").upper() for i in range(n)}
    return set()


def cmd_pack(src, dst, traces, family, extra_keep, compact=False):
    data, ident, lumps = read_wad(src)
    cats = classify(lumps)
    phase0, phase1 = parse_trace(traces)
    keep = set(phase1)
    keep |= {i for i, (n, _, _) in enumerate(lumps) if n in ALWAYS_KEEP or n in extra_keep}
    if family:
        keep |= sprite_family(lumps, cats, keep)

    # tryb --compact: wyrzuc z KATALOGU wpisy niepotrzebne w demach
    keep_entry = set(range(len(lumps)))
    if compact:
        init_names = parse_trace_names(traces)
        patch_names = pnames(data, lumps)
        keep_entry = set(keep)
        for i, (n, _, size) in enumerate(lumps):
            if size == 0:
                keep_entry.add(i)                       # markery stref/map
            elif cats[i] == "flat":
                keep_entry.add(i)                       # flaty: nazwy musza sie rozwiazywac
            elif n.upper() in patch_names and cats[i] != "sprite":
                keep_entry.add(i)                       # latki z PNAMES (TEXTURE1 sie do nich odwoluje)
            elif cats[i] != "sprite" and n in init_names:
                keep_entry.add(i)                       # lumpy uzywane (po nazwie) przy inicjalizacji

    # wspolny stub: patch 1x1 (width,height,left,top, columnofs[0]=12, koniec kolumny 0xFF) + padding
    stub = struct.pack("<hhhhi", 1, 1, 0, 0, 12) + b"\xff\xff\xff\xff"

    out = bytearray(b"\0" * 12)  # naglowek uzupelnimy na koncu

    def align():
        while len(out) % 4:
            out.append(0)

    stub_pos = len(out)
    out += stub
    new = []
    kept_bytes = 0
    for i, (name, pos, size) in enumerate(lumps):
        if i not in keep_entry:
            continue
        if size == 0:
            new.append((name, len(out) if False else 0, 0))
        elif i in keep:
            align()
            new.append((name, len(out), size))
            out += data[pos:pos + size]
            kept_bytes += size
        else:
            new.append((name, stub_pos, len(stub)))
    align()
    dir_off = len(out)
    for name, pos, size in new:
        out += struct.pack("<ii8s", pos, size, name.encode("ascii"))
    struct.pack_into("<4sii", out, 0, ident, len(new), dir_off)
    open(dst, "wb").write(out)

    nkeep = len(keep)
    print(f"{src} ({len(data)} B) -> {dst} ({len(out)} B)")
    print(f"  zachowane lumpy z danymi: {nkeep} (dane {kept_bytes} B), wpisow w katalogu: {len(new)} (oryginal {len(lumps)}), katalog {16 * len(new)} B")
    by = {}
    for i in keep:
        by[cats[i]] = by.get(cats[i], 0) + lumps[i][2]
    for c in sorted(by, key=lambda k: -by[k]):
        print(f"    {c:8s} {by[c]:9d} B")


MAP_LUMPS = {"THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS", "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP"}
GAME_KEEP = {"PLAYPAL", "COLORMAP", "PNAMES", "TEXTURE1", "TEXTURE2"}


def is_map_marker(name):
    return len(name) == 4 and name[0] == "E" and name[2] == "M" and name[1].isdigit() and name[3].isdigit()


def cmd_episode(src, dst, keep_demos, extra_keep):
    """WAD do grania: zostaja mapy, tekstury, flaty, sprite'y i tabele; dzwieki, muzyka, menu, HUD,
    intermisja itp. dostaja wspolny stub 1x1. Katalog zostaje caly (nazwy sie rozwiazuja)."""
    data, ident, lumps = read_wad(src)
    cats = classify(lumps)
    keep = set()
    inmap = False
    for i, (name, pos, size) in enumerate(lumps):
        if is_map_marker(name):
            inmap = True
            continue
        if inmap and name in MAP_LUMPS:
            keep.add(i)
            continue
        inmap = False
        if cats[i] in ("sprite", "flat", "patch"):
            keep.add(i)
        elif name in GAME_KEEP or name in extra_keep:
            keep.add(i)
        elif keep_demos and name.startswith("DEMO"):
            keep.add(i)
    stub = struct.pack("<hhhhi", 1, 1, 0, 0, 12) + b"\xff\xff\xff\xff"
    out = bytearray(b"\0" * 12)

    def align():
        while len(out) % 4:
            out.append(0)

    stub_pos = len(out)
    out += stub
    new = []
    by = {}
    for i, (name, pos, size) in enumerate(lumps):
        if size == 0:
            new.append((name, 0, 0))
        elif i in keep:
            align()
            new.append((name, len(out), size))
            out += data[pos:pos + size]
            by[cats[i]] = by.get(cats[i], 0) + size
        else:
            new.append((name, stub_pos, len(stub)))
    align()
    dir_off = len(out)
    for name, pos, size in new:
        out += struct.pack("<ii8s", pos, size, name.encode("ascii"))
    struct.pack_into("<4sii", out, 0, ident, len(new), dir_off)
    open(dst, "wb").write(out)
    print(f"{src} ({len(data)} B) -> {dst} ({len(out)} B), lumpow {len(new)}, z danymi {len(keep)}")
    for c in sorted(by, key=lambda k: -by[k]):
        print(f"    {c:8s} {by[c]:9d} B")


def main():
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == "info":
        cmd_info(a[1])
    elif len(a) >= 4 and a[0] == "pack":
        family = "--sprite-family" in a
        compact = "--compact" in a
        extra = set()
        rest = []
        i = 3
        while i < len(a):
            if a[i] == "--keep":
                extra |= set(a[i + 1].split(","))
                i += 2
            elif a[i].startswith("--"):
                i += 1
            else:
                rest.append(a[i])
                i += 1
        cmd_pack(a[1], a[2], rest, family, extra, compact)
    elif len(a) >= 3 and a[0] == "episode":
        extra = set()
        i = 3
        while i < len(a):
            if a[i] == "--keep":
                extra |= set(a[i + 1].split(","))
                i += 2
            else:
                i += 1
        cmd_episode(a[1], a[2], "--demos" in a, extra)
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
