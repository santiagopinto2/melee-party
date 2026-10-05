/* Melee Party: flat shapes in the match world (board spaces, the volleyball net). No assets: the
 * GX setup the collision display uses (mp/mplib.c mpLib_SetupDraw and its viewing matrix), drawn
 * from a GObj on the items' render link so the match camera draws it with the scene. */
#include <math.h>

#include <dolphin/gx.h>
#include <melee/mp/mplib.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/gobjgxlink.h>
#include <sysdolphin/baselib/pobj.h>
#include <sysdolphin/baselib/state.h>
#include <sysdolphin/baselib/tev.h>

#include "party.h"
#include "party_draw.h"

static void (*draw_cb)(void);

static void world_begin(void)
{
    Mtx view;
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    HSD_CObjGetViewingMtx(HSD_CObjGetCurrent(), view);
    GXSetCurrentMtx(0);
    GXLoadPosMtxImm(view, 0);
    GXSetCullMode(GX_CULL_NONE);   /* floor shapes are seen from above, upright ones from the front */
}

static void render(HSD_GObj* gobj, intptr_t pass)
{
    (void) gobj;
    (void) pass;
    if (draw_cb == NULL) {
        return;
    }
    world_begin();
    draw_cb();
    HSD_StateInvalidate(-1);
    HSD_StateInitTev();
    HSD_ClearVtxDesc();
}

void party_draw_init(void (*cb)(void))
{
    /* Not in the item list: item loops read every entry there as an Item. */
    HSD_GObj* gobj = GObj_Create(14, 15, 0);
    draw_cb = cb;
    if (gobj != NULL) {
        GObj_SetupGXLink(gobj, render, 6, 0);
    }
}

void party_draw_quad(GXColor color, float x0, float y0, float x1, float y1, float z)
{
    mpLib_SetupDraw(color);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, z);
    GXPosition3f32(x1, y0, z);
    GXPosition3f32(x1, y1, z);
    GXPosition3f32(x0, y1, z);
    GXEnd();
}

/* A flat disc lying on the floor (a board space). */
void party_draw_disc(GXColor color, float x, float y, float z, float rx, float rz)
{
    enum { N = 24 };
    int i;
    mpLib_SetupDraw(color);
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, N + 2);
    GXPosition3f32(x, y, z);
    for (i = 0; i <= N; i++) {
        float a = (float) i * (2.0f * (float) M_PI / N);
        GXPosition3f32(x + cosf(a) * rx, y, z + sinf(a) * rz);
    }
    GXEnd();
}

void party_draw_disc_n(GXColor color, float x, float y, float z, float r, int segments)
{
    int i;
    mpLib_SetupDraw(color);
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, (u16) (segments + 2));
    GXPosition3f32(x, y, z);
    for (i = 0; i <= segments; i++) {
        float a = (float) i * (2.0f * (float) M_PI / (float) segments);
        GXPosition3f32(x + cosf(a) * r, y, z + sinf(a) * r);
    }
    GXEnd();
}

void party_draw_star(GXColor color, float x, float y, float z, float r)
{
    int i;
    mpLib_SetupDraw(color);
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, 12);
    GXPosition3f32(x, y, z);
    for (i = 0; i <= 10; i++) {
        /* Points and notches in turn, the first point straight up. */
        float a = (float) M_PI_2 + (float) i * ((float) M_PI / 5.0f);
        float d = (i & 1) ? r * 0.42f : r;
        GXPosition3f32(x + cosf(a) * d, y + sinf(a) * d, z);
    }
    GXEnd();
}

void party_draw_floor_quad(GXColor color, float y, const float corners[8])
{
    int i;
    mpLib_SetupDraw(color);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    for (i = 0; i < 4; i++) {
        GXPosition3f32(corners[2 * i], y, corners[2 * i + 1]);
    }
    GXEnd();
}

void party_draw_floor_arrow(GXColor color, float y, float x, float z, float dx, float dz, float len,
                            float width)
{
    float head = len * 0.45f, hw = width * 1.6f;
    float sx = x + dx * (len - head), sz = z + dz * (len - head);   /* where the head starts */
    float nx = -dz * width * 0.5f, nz = dx * width * 0.5f;
    float shaft[8];
    shaft[0] = x - nx; shaft[1] = z - nz;
    shaft[2] = sx - nx; shaft[3] = sz - nz;
    shaft[4] = sx + nx; shaft[5] = sz + nz;
    shaft[6] = x + nx; shaft[7] = z + nz;
    party_draw_floor_quad(color, y, shaft);
    mpLib_SetupDraw(color);
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(sx - dz * hw, y, sz + dx * hw);
    GXPosition3f32(x + dx * len, y, z + dz * len);
    GXPosition3f32(sx + dz * hw, y, sz - dx * hw);
    GXEnd();
}

void party_draw_floor_sector(GXColor color, float x, float y, float z, float r, float a0, float a1)
{
    enum { N = 12 };
    int i;
    mpLib_SetupDraw(color);
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, N + 2);
    GXPosition3f32(x, y, z);
    for (i = 0; i <= N; i++) {
        float a = a0 + (a1 - a0) * (float) i / N;
        GXPosition3f32(x + cosf(a) * r, y, z + sinf(a) * r);
    }
    GXEnd();
}

void party_draw_tris(GXColor color, const float* xyz, int count)
{
    int i;
    if (count <= 0) {
        return;
    }
    mpLib_SetupDraw(color);
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, (u16) (3 * count));
    for (i = 0; i < 3 * count; i++) {
        GXPosition3f32(xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]);
    }
    GXEnd();
}

void party_draw_box(GXColor top, GXColor side, float x0, float y0, float z0, float x1, float y1,
                    float z1)
{
    mpLib_SetupDraw(side);
    GXBegin(GX_QUADS, GX_VTXFMT0, 16);
    GXPosition3f32(x0, y0, z1);   /* front */
    GXPosition3f32(x1, y0, z1);
    GXPosition3f32(x1, y1, z1);
    GXPosition3f32(x0, y1, z1);
    GXPosition3f32(x0, y0, z0);   /* back */
    GXPosition3f32(x0, y1, z0);
    GXPosition3f32(x1, y1, z0);
    GXPosition3f32(x1, y0, z0);
    GXPosition3f32(x0, y0, z0);   /* left */
    GXPosition3f32(x0, y0, z1);
    GXPosition3f32(x0, y1, z1);
    GXPosition3f32(x0, y1, z0);
    GXPosition3f32(x1, y0, z0);   /* right */
    GXPosition3f32(x1, y1, z0);
    GXPosition3f32(x1, y1, z1);
    GXPosition3f32(x1, y0, z1);
    GXEnd();
    mpLib_SetupDraw(top);
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y1, z0);
    GXPosition3f32(x0, y1, z1);
    GXPosition3f32(x1, y1, z1);
    GXPosition3f32(x1, y1, z0);
    GXEnd();
}
