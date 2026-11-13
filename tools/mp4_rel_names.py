#!/usr/bin/env python3
"""Prefix a ported Mario Party 4 minigame's own symbols with its name.

Every MP4 minigame (REL) in the decompilation names its functions fn_1_<address> and its data
lbl_1_<section>_<address>, and defines ObjectSetup; linked into one library, two minigames would
clash. This writes a header of #defines (fn_1_3C4 -> m440_fn_1_3C4) that the build includes
ahead of that minigame's sources.

Usage: mp4_rel_names.py <minigame dir> <output header>
"""
import re
import sys
from pathlib import Path

NAMES = re.compile(r"\b(fn_1_[0-9A-Fa-f]+|lbl_1_[A-Za-z0-9_]+|ObjectSetup)\b")


def main():
    rel = Path(sys.argv[1])
    out = Path(sys.argv[2])
    prefix = rel.name
    names = set()
    for src in list(rel.glob("*.c")) + list(rel.glob("*.h")):
        names.update(NAMES.findall(src.read_text(errors="replace")))
    header = rel.parent / "include" / "REL" / f"{prefix}Dll.h"
    if header.exists():
        names.update(NAMES.findall(header.read_text(errors="replace")))
    lines = [f"/* {prefix}'s own symbols, prefixed (tools/mp4_rel_names.py). Generated. */"]
    for name in sorted(names):
        lines.append(f"#define {name} {prefix}_{name}")
    out.parent.mkdir(parents=True, exist_ok=True)
    text = "\n".join(lines) + "\n"
    if not out.exists() or out.read_text() != text:
        out.write_text(text)


if __name__ == "__main__":
    main()
