"""Write sourceport/game/party/board_ggg.h: the space layout of Mario Party 4's Goomba's Greedy Gala.

Reads data/w02.bin from a Mario Party 4 disc image (USA) and decodes its file 0, the board's space
list, in the format BoardSpaceInit reads (partyboard: src/game/board/space.c; archive and LZ
decoding: src/game/data.c, src/game/decode.c). Every space keeps its MP4 type, flags and links;
board.c decides what each one is in Melee Party.

    python tools/extract_mp4_board.py --iso "Mario Party 4 (USA) (v1.01).iso"
"""
import argparse
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "sourceport" / "game" / "party" / "board_ggg.h"
TYPES = {0: "node", 1: "blue", 2: "red", 3: "bowser", 4: "mushroom", 5: "battle",
         6: "happening", 7: "fortune", 8: "star", 9: "warp"}


def read_file(iso, path):
    """A file from a GameCube disc's file system table."""
    def at(offset, size):
        iso.seek(offset)
        return iso.read(size)

    fst_offset, fst_size = struct.unpack(">II", at(0x424, 8))
    fst = at(fst_offset, fst_size)
    count = struct.unpack(">I", fst[8:12])[0]
    names = fst[count * 12:]
    dirs = [(count, "")]
    for i in range(1, count):
        while i >= dirs[-1][0]:
            dirs.pop()
        word, a, b = struct.unpack(">III", fst[i * 12:i * 12 + 12])
        start = word & 0xFFFFFF
        name = dirs[-1][1] + "/" + names[start:names.index(b"\0", start)].decode()
        if word >> 24:
            dirs.append((b, name))
        elif name == path:
            return at(a, b)
    raise SystemExit("%s is not on this disc" % path)


def lz_decode(src, size):
    """Decode type 1: an LZ with a 1 KB ring that starts writing at 958."""
    out = bytearray()
    ring = bytearray(1024)
    r, p, flag = 958, 0, 0
    while len(out) < size:
        flag >>= 1
        if not flag & 0x100:
            flag = src[p] | 0xFF00
            p += 1
        if flag & 1:
            c = src[p]
            p += 1
            out.append(c)
            ring[r] = c
            r = (r + 1) & 1023
        else:
            b1, b2 = src[p], src[p + 1]
            p += 2
            pos = b1 | ((b2 & 0xC0) << 2)
            for k in range((b2 & 0x3F) + 3):
                c = ring[(pos + k) & 1023]
                out.append(c)
                ring[r] = c
                r = (r + 1) & 1023
    return bytes(out[:size])


def board_spaces(archive):
    offset = struct.unpack(">I", archive[4:8])[0]   # file 0
    size, kind = struct.unpack(">II", archive[offset:offset + 8])
    if kind != 1:
        raise SystemExit("w02.bin file 0 uses decode type %d, expected 1 (LZ)" % kind)
    data = lz_decode(archive[offset + 8:], size)
    count = struct.unpack(">I", data[:4])[0]
    spaces, p = [], 4
    for _ in range(count):
        x, y, z = struct.unpack(">3f", data[p:p + 12])
        p += 36   # position, rotation, scale
        flag, kind, links = struct.unpack(">IHH", data[p:p + 8])
        p += 8
        link = list(struct.unpack(">%dH" % links, data[p:p + 2 * links]))
        p += 2 * links
        spaces.append((round(x), round(z), kind, flag, link))
    if p != len(data):
        raise SystemExit("w02 space data: %d bytes left over" % (len(data) - p))
    return spaces


def write_header(spaces, out):
    lines = [
        "/* Goomba's Greedy Gala (Mario Party 4), written by tools/extract_mp4_board.py from the",
        " * game's data/w02.bin: every space with its MP4 position (x, z), type, flags and links. Links",
        " * are 0-based indices into this table; MP4 numbers its spaces from 1 (index + 1). Do not edit. */",
        "#ifndef MU_BOARD_GGG_H",
        "#define MU_BOARD_GGG_H",
        "",
        "#define GGG_NODES %d" % len(spaces),
        "",
        "typedef struct GggNode {",
        "    s16 x, z;",
        "    u8 type;",
        "    u8 nlink;",
        "    u8 link[4];",
        "    u32 flag;",
        "} GggNode;",
        "",
        "static const GggNode ggg_nodes[GGG_NODES] = {",
    ]
    for i, (x, z, kind, flag, link) in enumerate(spaces):
        links = ", ".join(str(l) for l in link + [0] * (4 - len(link)))
        lines.append("    { %5d, %5d, %d, %d, { %s }, 0x%08Xu },   /* %3d %s */"
                     % (x, z, kind, len(link), links, flag, i, TYPES.get(kind, kind)))
    lines += ["};", "", "#endif", ""]
    out.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--iso", required=True, help="Mario Party 4 (USA) disc image")
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()
    with open(args.iso, "rb") as iso:
        if iso.read(6) != b"GMPE01":
            raise SystemExit("%s is not Mario Party 4 (USA)" % args.iso)
        spaces = board_spaces(read_file(iso, "/data/w02.bin"))
    write_header(spaces, args.out)
    print("%d spaces -> %s" % (len(spaces), args.out))


if __name__ == "__main__":
    main()
