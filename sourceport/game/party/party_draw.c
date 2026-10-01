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

/* A flat disc lying on the floor (a board space), seen from the match camera's slight tilt. */
void party_draw_disc(GXColor color, float x, float y, float rx, float rz)
{
    enum { N = 20 };
    int i;
    mpLib_SetupDraw(color);
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, N + 2);
    GXPosition3f32(x, y, 0.0f);
    for (i = 0; i <= N; i++) {
        float a = (float) i * (2.0f * (float) M_PI / N);
        GXPosition3f32(x + cosf(a) * rx, y, sinf(a) * rz);
    }
    GXEnd();
}
