/* Melee Party's Mario Party 4 runtime (party/mp4): the HSF model loader on a model made here, byte
 * by byte, in the file's big-endian layout. One mesh object with three vertices, three 8-bit
 * normals, one triangle and one material, and a motion with one linear track on the mesh. No disc
 * data. The loader is MP4's (hsfload.c), with the PC port's swap into 64-bit structures
 * (hsf_byteswap.c) and MP4's heaps (mp4_mem.c). */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "game/hsfload.h"
#include "game/memory.h"
#include "port/byteswap.h"

static int failures;

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
            failures++;                                                            \
        }                                                                          \
    } while (0)

/* What the loader needs from the rest of the game. */
const unsigned char __ctype_map[256] = {
    ['A'] = 0x80, ['B'] = 0x80, ['C'] = 0x80, ['D'] = 0x80, ['E'] = 0x80, ['F'] = 0x80, ['G'] = 0x80,
    ['H'] = 0x80, ['I'] = 0x80, ['J'] = 0x80, ['K'] = 0x80, ['L'] = 0x80, ['M'] = 0x80, ['N'] = 0x80,
    ['O'] = 0x80, ['P'] = 0x80, ['Q'] = 0x80, ['R'] = 0x80, ['S'] = 0x80, ['T'] = 0x80, ['U'] = 0x80,
    ['V'] = 0x80, ['W'] = 0x80, ['X'] = 0x80, ['Y'] = 0x80, ['Z'] = 0x80,
    ['a'] = 0x40, ['b'] = 0x40, ['c'] = 0x40, ['d'] = 0x40, ['e'] = 0x40, ['f'] = 0x40, ['g'] = 0x40,
    ['h'] = 0x40, ['i'] = 0x40, ['j'] = 0x40, ['k'] = 0x40, ['l'] = 0x40, ['m'] = 0x40, ['n'] = 0x40,
    ['o'] = 0x40, ['p'] = 0x40, ['q'] = 0x40, ['r'] = 0x40, ['s'] = 0x40, ['t'] = 0x40, ['u'] = 0x40,
    ['v'] = 0x40, ['w'] = 0x40, ['x'] = 0x40, ['y'] = 0x40, ['z'] = 0x40,
};
void OSReport(char* fmt, ...) { (void) fmt; }
void Hu3DMtxScaleGet(Mtx m, Vec* out)
{
    (void) m;
    out->x = out->y = out->z = 1.0f;
}

/* ---- writing the file ---- */

static u8 file[0x2000] __attribute__((aligned(32)));
static u32 used;

static void put8(u32 at, u32 v) { file[at] = (u8) v; }
static void put16(u32 at, u32 v)
{
    file[at] = (u8) (v >> 8);
    file[at + 1] = (u8) v;
}
static void put32(u32 at, u32 v)
{
    put16(at, v >> 16);
    put16(at + 2, v & 0xFFFF);
}
static void putf(u32 at, float f)
{
    u32 bits;
    memcpy(&bits, &f, 4);
    put32(at, bits);
}
static u32 take(u32 size)
{
    u32 at = used;
    used = (used + size + 3) & ~3u;
    return at;
}
/* A section's offset and count in the header (HSFHEADER: 8-byte magic, then the sections). */
static void section(size_t field, u32 ofs, u32 count)
{
    put32((u32) field, ofs);
    put32((u32) field + 4, count);
}

enum { STR_ROOT = 0, STR_MAT = 5, STR_VTX = 9, STR_NRM = 13, STR_FACE = 17, STR_MOT = 22 };
static const char strings[] = "root\0mat\0vtx\0nrm\0face\0mot";

static const float verts[3][3] = { { 0.0f, 1.5f, -2.0f }, { 10.25f, -3.0f, 4.0f }, { -7.0f, 0.125f, 9.5f } };
static const s8 normals[3][3] = { { 0, 127, 0 }, { -127, 0, 0 }, { 0, 0, 64 } };

static void build(void)
{
    u32 mat, vtx, nrm, face, obj, mot, sym, str, at, i, j;
    memset(file, 0, sizeof file);
    used = sizeof(HSFHEADER);
    memcpy(file, "HSFV037", 8);

    mat = take(sizeof(HsfMaterial32b));
    put32(mat + offsetof(HsfMaterial32b, name), STR_MAT);
    put16(mat + offsetof(HsfMaterial32b, pass), 0x0012);
    put8(mat + offsetof(HsfMaterial32b, vtxMode), 1);
    put8(mat + offsetof(HsfMaterial32b, litColor), 200);
    put8(mat + offsetof(HsfMaterial32b, litColor) + 1, 100);
    put8(mat + offsetof(HsfMaterial32b, litColor) + 2, 50);
    put8(mat + offsetof(HsfMaterial32b, color), 255);
    putf(mat + offsetof(HsfMaterial32b, invAlpha), 0.25f);
    put32(mat + offsetof(HsfMaterial32b, flags), 0x100);
    put32(mat + offsetof(HsfMaterial32b, attrNum), 0);
    section(offsetof(HSFHEADER, material), mat, 1);

    /* buffers: their headers, then their data, which `data` counts from */
    vtx = take(sizeof(HsfBuffer32b) + sizeof verts);
    put32(vtx + offsetof(HsfBuffer32b, name), STR_VTX);
    put32(vtx + offsetof(HsfBuffer32b, count), 3);
    put32(vtx + offsetof(HsfBuffer32b, data), 0);
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            putf(vtx + sizeof(HsfBuffer32b) + (i * 3 + j) * 4, verts[i][j]);
        }
    }
    section(offsetof(HSFHEADER, vertex), vtx, 1);

    nrm = take(sizeof(HsfBuffer32b) + sizeof normals);
    put32(nrm + offsetof(HsfBuffer32b, name), STR_NRM);
    put32(nrm + offsetof(HsfBuffer32b, count), 3);
    put32(nrm + offsetof(HsfBuffer32b, data), 0);
    memcpy(file + nrm + sizeof(HsfBuffer32b), normals, sizeof normals);
    section(offsetof(HSFHEADER, normal), nrm, 1);

    face = take(sizeof(HsfBuffer32b) + sizeof(HsfFace32b));
    put32(face + offsetof(HsfBuffer32b, name), STR_FACE);
    put32(face + offsetof(HsfBuffer32b, count), 1);
    put32(face + offsetof(HsfBuffer32b, data), 0);
    at = face + sizeof(HsfBuffer32b);
    put16(at + offsetof(HsfFace32b, type), HSF_FACE_TRI);
    put16(at + offsetof(HsfFace32b, mat), 0);
    for (i = 0; i < 3; i++) {
        put16(at + offsetof(HsfFace32b, indices) + i * 8, i);        /* position */
        put16(at + offsetof(HsfFace32b, indices) + i * 8 + 2, 2 - i); /* normal */
        put16(at + offsetof(HsfFace32b, indices) + i * 8 + 4, 0xFFFF);
        put16(at + offsetof(HsfFace32b, indices) + i * 8 + 6, 0xFFFF);
    }
    putf(at + offsetof(HsfFace32b, nbt) + 4, 1.0f);
    section(offsetof(HSFHEADER, face), face, 1);

    obj = take(sizeof(HsfObject32b));
    put32(obj + offsetof(HsfObject32b, name), STR_ROOT);
    put32(obj + offsetof(HsfObject32b, type), HSF_OBJ_MESH);
    at = obj + offsetof(HsfObject32b, data);
    put32(at + offsetof(HsfObjectData32b, parent), 0xFFFFFFFF);
    put32(at + offsetof(HsfObjectData32b, childrenCount), 0);
    putf(at + offsetof(HsfObjectData32b, base) + offsetof(HSFTRANSFORM, pos) + 4, 30.0f);
    for (i = 0; i < 3; i++) {
        putf(at + offsetof(HsfObjectData32b, base) + offsetof(HSFTRANSFORM, scale) + i * 4, 2.0f);
    }
    putf(at + offsetof(HsfObjectData32b, mesh) + 12, 10.25f);   /* max.x */
    put32(at + offsetof(HsfObjectData32b, face), 0);
    put32(at + offsetof(HsfObjectData32b, vertex), 0);
    put32(at + offsetof(HsfObjectData32b, normal), 0);
    put32(at + offsetof(HsfObjectData32b, color), 0xFFFFFFFF);
    put32(at + offsetof(HsfObjectData32b, st), 0xFFFFFFFF);
    put32(at + offsetof(HsfObjectData32b, material), 0);
    put32(at + offsetof(HsfObjectData32b, attribute), 0xFFFFFFFF);
    put32(at + offsetof(HsfObjectData32b, cenv), 0xFFFFFFFF);
    section(offsetof(HSFHEADER, object), obj, 1);

    /* a motion: its header, its tracks, then the tracks' keyframes */
    mot = take(sizeof(HsfMotion32b) + sizeof(HsfTrack32b) + 4 * 4);
    put32(mot + offsetof(HsfMotion32b, name), STR_MOT);
    put32(mot + offsetof(HsfMotion32b, numTracks), 1);
    putf(mot + offsetof(HsfMotion32b, maxTime), 30.0f);
    at = mot + sizeof(HsfMotion32b);
    put8(at + offsetof(HsfTrack32b, type), HSF_TRACK_TRANSFORM);
    put16(at + offsetof(HsfTrack32b, target), STR_ROOT);
    put16(at + offsetof(HsfTrack32b, channel), HSF_CHANNEL_POSY);
    put16(at + offsetof(HsfTrack32b, curveType), HSF_CURVE_LINEAR);
    put16(at + offsetof(HsfTrack32b, numKeyframes), 2);
    put32(at + offsetof(HsfTrack32b, data), 0);
    at += sizeof(HsfTrack32b);
    putf(at, 0.0f);
    putf(at + 4, 5.0f);
    putf(at + 8, 30.0f);
    putf(at + 12, -5.0f);
    section(offsetof(HSFHEADER, motion), mot, 1);

    sym = take(4);
    put32(sym, 0);
    section(offsetof(HSFHEADER, symbol), sym, 1);

    str = take(sizeof strings);
    memcpy(file + str, strings, sizeof strings);
    section(offsetof(HSFHEADER, string), str, sizeof strings);
}

int main(void)
{
    HSFDATA* hsf;
    HSFOBJECT* root;
    HSFFACE* f;
    HSFTRACK* track;
    HuVecF* v;
    float* key;
    s8* n;
    int i, ok;

    /* the file structures keep the console's sizes */
    CHECK(sizeof(HSFHEADER) == 0xB0);
    CHECK(sizeof(HsfObject32b) == 0x144);
    CHECK(sizeof(HsfMaterial32b) == 0x3C);
    CHECK(sizeof(HsfFace32b) == 0x30);
    CHECK(sizeof(HsfTrack32b) == 0x10);
    CHECK(sizeof(HsfBuffer32b) == 0x0C);

    HuMemInitAll();
    build();
    hsf = LoadHSF(file);
    CHECK(hsf != NULL);
    if (hsf == NULL) {
        return 1;
    }

    CHECK(hsf->objectNum == 1 && hsf->materialNum == 1 && hsf->vertexNum == 1 && hsf->faceNum == 1);
    root = hsf->root;
    CHECK(root == hsf->object);
    CHECK(root != NULL && root->type == HSF_OBJ_MESH && strcmp(root->name, "root") == 0);
    if (root == NULL) {
        return 1;
    }
    CHECK(root->mesh.parent == NULL && root->mesh.childrenCount == 0);
    CHECK(root->mesh.base.pos.y == 30.0f && root->mesh.base.scale.z == 2.0f);
    CHECK(root->mesh.mesh.max.x == 10.25f);
    CHECK(root->mesh.material == hsf->material && root->mesh.cenv == NULL && root->mesh.color == NULL);

    /* positions: in host order now, read in place in the file */
    CHECK(root->mesh.vertex == hsf->vertex && strcmp(hsf->vertex->name, "vtx") == 0);
    CHECK(hsf->vertex->count == 3);
    v = hsf->vertex->data;
    CHECK((u8*) v > file && (u8*) v < file + sizeof file);
    for (i = 0, ok = 1; i < 3; i++) {
        ok &= v[i].x == verts[i][0] && v[i].y == verts[i][1] && v[i].z == verts[i][2];
    }
    CHECK(ok);

    /* 8-bit normals stay as they are */
    n = root->mesh.normal->data;
    CHECK(memcmp(n, normals, sizeof normals) == 0);

    f = root->mesh.face->data;
    CHECK(root->mesh.face->count == 1 && (f->type & HSF_FACE_MASK) == HSF_FACE_TRI && f->mat == 0);
    CHECK(f->indices[1][0] == 1 && f->indices[2][1] == 0 && f->indices[0][2] == -1);
    CHECK(f->nbt.y == 1.0f);

    CHECK(strcmp(hsf->material->name, "mat") == 0);
    CHECK(hsf->material->pass == 0x12 && hsf->material->vtxMode == 1);
    CHECK(hsf->material->litColor[0] == 200 && hsf->material->litColor[2] == 50);
    CHECK(hsf->material->invAlpha == 0.25f && hsf->material->flags == 0x100);

    /* the track found its object by name; its keyframes are in host order */
    CHECK(hsf->motion != NULL && hsf->motion->numTracks == 1 && hsf->motion->maxTime == 30.0f);
    if (hsf->motion != NULL) {
        track = hsf->motion->track;
        CHECK(track->type == HSF_TRACK_TRANSFORM && track->target == 0);
        CHECK(track->channel == HSF_CHANNEL_POSY && track->curveType == HSF_CURVE_LINEAR);
        CHECK(track->numKeyframes == 2);
        key = track->data;
        CHECK(key[0] == 0.0f && key[1] == 5.0f && key[2] == 30.0f && key[3] == -5.0f);
    }

    if (failures != 0) {
        printf("mp4_hsf: %d failures\n", failures);
        return 1;
    }
    printf("mp4_hsf: ok\n");
    return 0;
}
