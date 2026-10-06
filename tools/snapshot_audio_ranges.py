#!/usr/bin/env python3
"""Rollback snapshot exclusions for the audio driver, from the game library's linker map.

A console savestate leaves out the whole sound driver: every variable of the AX driver files and of
the synth and axdriver sound managers (their voice tables and sound-effect node pools follow the
voices the DSP keeps playing through a load). Restoring those tables on a rollback while the voices
play on gives cut-off and restarted sounds, heard as crackle. The native build names only a few of
their variables, so this lists each file's whole .bss and .data range instead, as image offsets.

    python tools/snapshot_audio_ranges.py <melee_game.map> <image base hex> <output file> [melee_game.dll]

Output: one line per range, "<offset hex> <size hex> <section> <file>". With the library given, a first
line "# dll_size <bytes>" ties the ranges to that exact build: the offsets are only right for the
library they were read from, and the host ignores them for any other (a copied library with an old
file beside it restored the wrong memory on every rollback, 09-29).
"""
import os
import re
import sys

FILES = ("synth.c.obj", "axdriver.c.obj", "AXVPB.c.obj", "AXAux.c.obj", "AXCL.c.obj", "AXOut.c.obj",
         "AXAlloc.c.obj", "AXSPB.c.obj", "AXProf.c.obj")
# PE maps name .bss and .data; ELF ones (the Linux and Android builds, objects named .c.o) also have
# .data.rel.local and friends for data holding addresses, and put a long section name on a line of
# its own with the addresses on the next.
LINE = re.compile(r"^ \.(bss|data)(?:\.[\w.]+)?\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+) (\S+)$")
WRAPPED = re.compile(r"^ \.(?:bss|data)(?:\.[\w.]+)?$")


def object_name(path):
    name = path.replace("\\", "/").rsplit("/", 1)[-1]
    # Makefile builds on Linux link through an archive: "objects.a(AXAlloc.c.obj)"
    m = re.match(r"^[^()]+\.a\((.+)\)$", name)
    if m:
        name = m.group(1)
    return name + "bj" if name.endswith(".c.o") else name


def main():
    map_path, base, out_path = sys.argv[1], int(sys.argv[2], 16), sys.argv[3]
    rows = []
    pending = None
    for line in open(map_path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        if pending is not None:
            line, pending = pending + line, None
        elif WRAPPED.match(line):
            pending = line
            continue
        m = LINE.match(line)
        if not m:
            continue
        section, address, size, obj = m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)
        name = object_name(obj)
        if name in FILES and size:
            rows.append((address - base, size, section, name))
    missing = sorted(set(FILES) - {r[3] for r in rows})
    if missing:
        sys.exit("snapshot_audio_ranges: no sections found for " + ", ".join(missing))
    with open(out_path, "w", encoding="utf-8") as f:
        if len(sys.argv) > 4:
            f.write(f"# dll_size {os.path.getsize(sys.argv[4])}\n")
        for offset, size, section, name in rows:
            f.write(f"{offset:x} {size:x} {section} {name}\n")
    print(f"snapshot_audio_ranges: {len(rows)} ranges, {sum(r[1] for r in rows)} bytes")


if __name__ == "__main__":
    main()
