"""Write sourceport/game/party/dungeon_m432.h: the layout of Mario Party 4's Dungeon Duos.

Reads data/m432.bin from a Mario Party 4 disc image (USA) and decodes three of its HSF models
(format: partyboard src/game/hsfload.c, include/game/hsfformat.h, include/port/byteswap.h):
- file 0x01, the locators: every "m432_00-*" point the minigame reads by name (main.c fn_1_113F4);
- file 0x00, the collision map: its floors (faces turned up) and its walls (the rest);
- files 0x10, 0x12 and 0x0C, the two turntables and a gate: their sizes.
Positions stay in MP4 units, around one team's dungeon (the game places team 0's at x = -800 and
team 1's at x = +800); mg_dungeon.c scales them by 0.1.

    python tools/extract_mp4_dungeon.py --iso "Mario Party 4 (USA) (v1.01).iso"
"""
import argparse
import math
import struct
from pathlib import Path

from extract_mp4_board import lz_decode, read_file

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "sourceport" / "game" / "party" / "dungeon_m432.h"
SECTIONS = ["scene", "color", "material", "attribute", "vertex", "normal", "st", "face", "object",
            "bitmap", "palette", "motion", "cenv", "skeleton", "part", "cluster", "shape", "mapAttr",
            "matrix", "symbol", "string"]
OBJECT_SIZE = 0x144   # HsfObject32b
FACE_SIZE = 0x30      # HsfFace32b
PIT = -800.0          # below this a player has fallen (main.c fn_1_ED0C): no floor down there


def archive_file(archive, index):
    offset = struct.unpack(">I", archive[4 + 4 * index:8 + 4 * index])[0]
    size, kind = struct.unpack(">II", archive[offset:offset + 8])
    if kind != 1:
        raise SystemExit("m432.bin file %d uses decode type %d, expected 1 (LZ)" % (index, kind))
    return lz_decode(archive[offset + 8:], size)


class Hsf:
    """An HSF model's bytes and section table (load builds one)."""

    def u32(self, p):
        return struct.unpack(">I", self.d[p:p + 4])[0]

    def i32(self, p):
        return struct.unpack(">i", self.d[p:p + 4])[0]

    def f3(self, p):
        return struct.unpack(">3f", self.d[p:p + 12])

    def string(self, offset):
        p = self.sec["string"][0] + offset
        return self.d[p:self.d.index(b"\0", p)].decode("latin1")

    def vertices(self, index):
        base, count = self.sec["vertex"]
        p = base + 12 * index
        n, offset = self.i32(p + 4), self.u32(p + 8)
        area = base + 12 * count + offset
        return [self.f3(area + 12 * k) for k in range(n)]

    def triangles(self, index):
        """Position indices of a face buffer's triangles (quads and strips split)."""
        base, count = self.sec["face"]
        p = base + 12 * index
        n, offset = self.i32(p + 4), self.u32(p + 8)
        start = base + 12 * count + offset
        strip = start + FACE_SIZE * n
        tris = []
        for k in range(n):
            q = start + FACE_SIZE * k
            kind = struct.unpack(">h", self.d[q:q + 2])[0] & 7
            corner = [struct.unpack(">h", self.d[q + 4 + 8 * j:q + 6 + 8 * j])[0] for j in range(4)]
            if kind == 2:
                tris.append(corner[:3])
            elif kind == 3:
                tris += [corner[:3], [corner[1], corner[3], corner[2]]]
            elif kind == 4:
                more, first = struct.unpack(">II", self.d[q + 28:q + 36])
                vs = corner[:3] + [struct.unpack(">h", self.d[strip + 8 * (first + j):
                                                              strip + 8 * (first + j) + 2])[0]
                                   for j in range(more)]
                tris += [vs[j:j + 3] for j in range(len(vs) - 2)]
            else:
                raise SystemExit("face type %d" % kind)
        return tris


IDENTITY = [[1.0 if r == c else 0.0 for c in range(4)] for r in range(4)]


def mul(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def local(o):
    rx, ry, rz = (math.radians(a) for a in o["rot"])
    sx, sy, sz = o["scale"]
    x = [[1, 0, 0, 0], [0, math.cos(rx), -math.sin(rx), 0], [0, math.sin(rx), math.cos(rx), 0],
         [0, 0, 0, 1]]
    y = [[math.cos(ry), 0, math.sin(ry), 0], [0, 1, 0, 0], [-math.sin(ry), 0, math.cos(ry), 0],
         [0, 0, 0, 1]]
    z = [[math.cos(rz), -math.sin(rz), 0, 0], [math.sin(rz), math.cos(rz), 0, 0], [0, 0, 1, 0],
         [0, 0, 0, 1]]
    m = mul(z, mul(y, mul(x, [[sx, 0, 0, 0], [0, sy, 0, 0], [0, 0, sz, 0], [0, 0, 0, 1]])))
    m[0][3], m[1][3], m[2][3] = o["pos"]
    return m


def apply(m, v):
    return tuple(m[r][0] * v[0] + m[r][1] * v[1] + m[r][2] * v[2] + m[r][3] for r in range(3))


def load(archive, index):
    """The model's objects, each with its world matrix."""
    h = Hsf.__new__(Hsf)
    data = archive_file(archive, index)
    if data[:4] != b"HSFV":
        raise SystemExit("m432.bin file %d is not an HSF model" % index)
    h.d = data
    h.sec = {name: struct.unpack(">ii", data[8 + 8 * k:16 + 8 * k]) for k, name in enumerate(SECTIONS)}
    base, count = h.sec["object"]
    symbols = h.sec["symbol"][0]
    objs = []
    for i in range(count):
        p = base + OBJECT_SIZE * i
        kind = h.u32(p + 4)
        o = {"name": h.string(h.u32(p)), "type": kind, "parent": None, "pos": h.f3(p + 28),
             "rot": h.f3(p + 40), "scale": h.f3(p + 52), "face": h.i32(p + 260),
             "vertex": h.i32(p + 264), "children": []}
        if kind not in (7, 8):   # cameras and lights have no children
            first, n = h.u32(p + 24), h.u32(p + 20)
            o["children"] = [h.u32(symbols + 4 * (first + k)) for k in range(n)]
        objs.append(o)
    for i, o in enumerate(objs):
        for c in o["children"]:
            objs[c]["parent"] = i

    def place(o, parent):
        o["world"] = mul(parent, local(o))
        for c in o["children"]:
            place(objs[c], o["world"])
    for o in objs:
        if o["parent"] is None:
            place(o, IDENTITY)
    h.objects = objs
    return h


def mesh_triangles(h):
    """Every mesh's triangles, in the world."""
    out = []
    for o in h.objects:
        if o["type"] != 2 or o["vertex"] < 0 or o["face"] < 0:
            continue
        vs = [apply(o["world"], v) for v in h.vertices(o["vertex"])]
        out += [tuple(vs[i] for i in t) for t in h.triangles(o["face"])]
    return out


def up(t):
    """The y of the triangle's unit normal."""
    a, b, c = t
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    length = math.sqrt(sum(x * x for x in n))
    return n[1] / length if length > 0 else 0.0


def extents(h):
    """x, y and z ranges of a model's meshes."""
    pts = [p for t in mesh_triangles(h) for p in t]
    return [(min(p[i] for p in pts), max(p[i] for p in pts)) for i in range(3)]


def write_header(nodes, floors, walls, tt0, tt1, gate, out):
    def tri(t):
        return "{ { %s } }" % ", ".join("{ %d, %d, %d }" % tuple(round(c) for c in p) for p in t)
    lines = [
        "/* Dungeon Duos (Mario Party 4, m432), written by tools/extract_mp4_dungeon.py from the game's",
        " * data/m432.bin: its locators (file 0x01), its collision map's floors and walls (file 0x00), and",
        " * the sizes of its turntables (files 0x10, 0x12) and gates (0x0C). MP4 units, around one team's",
        " * dungeon: the game puts team 0's at x = -800 and team 1's at x = +800. Do not edit. */",
        "#ifndef MU_DUNGEON_M432_H",
        "#define MU_DUNGEON_M432_H",
        "",
        "typedef struct DgNode {",
        "    const char* name;   /* \"m432_00-<name>\" in MP4 */",
        "    s16 x, y, z;",
        "    s16 rot_y;          /* degrees */",
        "} DgNode;",
        "",
        "typedef struct DgTri {",
        "    s16 v[3][3];        /* x, y, z */",
        "} DgTri;",
        "",
        "/* The turntables (files 0x10 and 0x12): bars turning about their node, their length along",
        " * their own z from ALONG0 to ALONG1, HALF_W either side of it, their top at TOP. */",
        "#define DG_TT0_ALONG0 %d" % round(tt0[2][0]),
        "#define DG_TT0_ALONG1 %d" % round(tt0[2][1]),
        "#define DG_TT0_HALF_W %d" % round(tt0[0][1]),
        "#define DG_TT1_ALONG0 %d" % round(tt1[2][0]),
        "#define DG_TT1_ALONG1 %d" % round(tt1[2][1]),
        "#define DG_TT1_HALF_W %d" % round(tt1[0][1]),
        "#define DG_TT_TOP %d" % round(tt0[1][1]),
        "/* A gate (file 0x0C): across its lane, HALF_W either side of its node, HEIGHT tall. */",
        "#define DG_GATE_HALF_W %d" % round(gate[0][1]),
        "#define DG_GATE_HEIGHT %d" % round(gate[1][1]),
        "#define DG_GATE_HALF_D %d" % round(gate[2][1]),
        "",
        "#define DG_NODES %d" % len(nodes),
        "static const DgNode dg_nodes[DG_NODES] = {",
    ]
    for name, (x, y, z), rot in nodes:
        lines.append("    { \"%s\", %d, %d, %d, %d }," % (name, round(x), round(y), round(z), round(rot)))
    lines += ["};", "", "/* Floors: the map's faces turned up, but none of the pits' bottoms. */",
              "#define DG_FLOORS %d" % len(floors), "static const DgTri dg_floors[DG_FLOORS] = {"]
    lines += ["    %s," % tri(t) for t in floors]
    lines += ["};", "", "/* Walls: the map's other faces. */",
              "#define DG_WALLS %d" % len(walls), "static const DgTri dg_walls[DG_WALLS] = {"]
    lines += ["    %s," % tri(t) for t in walls]
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
        archive = read_file(iso, "/data/m432.bin")

    locators = load(archive, 0x01)
    nodes = [(o["name"], apply(o["world"], (0, 0, 0)), o["rot"][1]) for o in locators.objects
             if o["type"] == 0 and len(o["name"]) == 2]
    nodes.sort(key=lambda n: n[0])

    floors, walls = [], []
    for t in mesh_triangles(load(archive, 0x00)):
        if up(t) > 0.7:
            if max(p[1] for p in t) > PIT:
                floors.append(t)
        elif max(p[1] for p in t) > PIT:
            walls.append(t)

    tt0 = extents(load(archive, 0x10))
    tt1 = extents(load(archive, 0x12))
    gate = extents(load(archive, 0x0C))
    write_header(nodes, floors, walls, tt0, tt1, gate, args.out)
    print("%d nodes, %d floors, %d walls -> %s" % (len(nodes), len(floors), len(walls), args.out))


if __name__ == "__main__":
    main()
