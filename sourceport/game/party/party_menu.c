/* Melee Party: the main menu's Vs. entries. Tournament Melee becomes Melee Party (the party) and
 * Special Melee becomes Party Minigames, whose submenu lists the minigames instead of the special
 * modes: picking one plays it, after the CSS, and comes back to the CSS.
 *
 * The menu's option names are textures in MnMaAll (the frames of its material animations), and
 * nothing of the disc may ship with this build, so the new names are made at runtime from the
 * names already there: each letter is cut out of a label that has it (along the italic's slant
 * for the slanted styles), the letters are laid out, squeezed sideways when too wide as the
 * game's own long labels are, and written over the old label. The outlined labels get their black
 * outline drawn again around the new letters. Three letters are in no label: F is E without its
 * bottom bar, and z, w and the apostrophe are drawn.
 *
 * Tournament Melee becomes Melee Party, whose submenu lists the boards. Both submenus are Special
 * Melee's: its rows are written again with the boards' or the minigames' names each time one of
 * the two entries opens it (party_menu_show).
 *
 * Patched, each time the menu loads its archive (mu_party_menu_loaded):
 * - the outlined labels (MenMainCursor, IA4 176x30) of Tournament Melee, Special Melee and the
 *   first Special Melee rows;
 * - the slanted headers of the preview box (MenMainPanel, I4 168x28) of the two Vs. entries;
 * - the preview lists (MenMainConTop, I4 128x104): the Vs. Mode one's second and third lines, and
 *   the Special Melee one, which becomes a four-line list like the 1-P Mode one's. */
#include <math.h>
#include <string.h>

#include <dolphin/gx.h>
#include <melee/gm/forward.h>
#include <melee/mn/forward.h>
#include <melee/mn/mnmain.h>
#include <melee/mn/types.h>
#include <melee/sc/types.h>
#include <sysdolphin/baselib/mobj.h>
#include <sysdolphin/baselib/sislib.h>
#include <sysdolphin/baselib/tobj.h>

#include "party.h"

extern StaticModelSyms MenMainCursor_Top;
extern StaticModelSyms MenMainConTop_Top;
extern MenuKindData mn_803EB6B0[];

enum { TEX_CURSOR, TEX_PANEL1, TEX_PANEL2, TEX_LIST };

/* A letter of a label: rows y0..y1 of image k of a table, its baseline yb, and its columns u0..u1
 * along the slant from the baseline. scale (percent) widens a letter cut from a squeezed label.
 * mask: F from E, the rows from mask_rows above the baseline cleared right of mask_u2 / 2. */
typedef struct Glyph {
    char ch;
    u8 tex, k;
    s16 y0, y1, yb, u0, u1;
    u8 scale;
    u8 mask_rows, mask_u2;
} Glyph;

/* Measured from the labels of the NTSC 1.02 disc's MnMaAll.usd. */
static const Glyph cursor_glyphs[] = {
    { 'B', TEX_CURSOR, 54, 0, 30, 24, 4, 19, 115, 0, 0 },
    { 'D', TEX_CURSOR, 4, 0, 30, 24, 56, 75, 100, 0, 0 },
    { 'E', TEX_CURSOR, 6, 0, 30, 24, 6, 21, 100, 0, 0 },
    { 'F', TEX_CURSOR, 6, 0, 30, 24, 6, 21, 100, 3, 13 },
    { 'G', TEX_CURSOR, 15, 0, 30, 24, 39, 58, 100, 0, 0 },
    { 'L', TEX_CURSOR, 16, 0, 30, 23, 38, 53, 100, 0, 0 },
    { 'M', TEX_CURSOR, 10, 0, 30, 24, 48, 70, 100, 0, 0 },
    { 'P', TEX_CURSOR, 0, 0, 30, 24, 55, 74, 100, 0, 0 },
    { 'R', TEX_CURSOR, 19, 0, 30, 24, 36, 54, 100, 0, 0 },
    { 'S', TEX_CURSOR, 8, 0, 30, 24, 32, 50, 100, 0, 0 },
    { 'T', TEX_CURSOR, 9, 0, 30, 23, 32, 50, 100, 0, 0 },
    { 'V', TEX_CURSOR, 1, 0, 30, 24, 20, 41, 100, 0, 0 },
    { 'a', TEX_CURSOR, 4, 0, 30, 24, 77, 92, 100, 0, 0 },
    { 'b', TEX_CURSOR, 19, 0, 30, 24, 99, 114, 100, 0, 0 },
    { 'c', TEX_CURSOR, 6, 0, 30, 24, 138, 154, 100, 0, 0 },
    { 'd', TEX_CURSOR, 0, 0, 30, 24, 122, 138, 100, 0, 0 },
    { 'e', TEX_CURSOR, 10, 0, 30, 24, 72, 87, 100, 0, 0 },
    { 'g', TEX_CURSOR, 9, 0, 30, 23, 128, 143, 100, 0, 0 },
    { 'h', TEX_CURSOR, 2, 0, 30, 23, 92, 106, 100, 0, 0 },
    { 'i', TEX_CURSOR, 9, 0, 30, 23, 79, 84, 100, 0, 0 },
    { 'l', TEX_CURSOR, 10, 0, 30, 24, 90, 95, 100, 0, 0 },
    { 'm', TEX_CURSOR, 8, 0, 30, 24, 121, 143, 100, 0, 0 },
    { 'n', TEX_CURSOR, 9, 0, 30, 23, 87, 101, 100, 0, 0 },
    { 'o', TEX_CURSOR, 0, 0, 30, 24, 108, 122, 100, 0, 0 },
    { 'p', TEX_CURSOR, 2, 0, 30, 23, 75, 90, 100, 0, 0 },
    { 'r', TEX_CURSOR, 9, 0, 30, 23, 51, 61, 100, 0, 0 },
    { 's', TEX_CURSOR, 2, 0, 30, 23, 132, 146, 100, 0, 0 },
    { 't', TEX_CURSOR, 4, 0, 30, 24, 92, 104, 100, 0, 0 },
    { 'u', TEX_CURSOR, 8, 0, 30, 24, 104, 118, 100, 0, 0 },
    { 'v', TEX_CURSOR, 6, 0, 30, 24, 21, 36, 100, 0, 0 },
    { 'y', TEX_CURSOR, 16, 0, 30, 23, 121, 137, 100, 0, 0 },
    { 0 },
};

static const Glyph panel_glyphs[] = {
    { 'D', TEX_PANEL1, 4, 0, 28, 23, 44, 64, 100, 0, 0 },
    { 'M', TEX_PANEL1, 0, 0, 28, 23, 70, 93, 100, 0, 0 },
    { 'P', TEX_PANEL1, 0, 0, 28, 23, 42, 59, 100, 0, 0 },
    { 'R', TEX_PANEL2, 10, 0, 28, 23, 30, 47, 100, 0, 0 },
    { 'S', TEX_PANEL2, 3, 0, 28, 24, 19, 36, 100, 0, 0 },
    { 'T', TEX_PANEL2, 4, 0, 28, 22, 16, 35, 100, 0, 0 },
    { 'a', TEX_PANEL1, 4, 0, 28, 23, 67, 84, 100, 0, 0 },
    { 'b', TEX_PANEL2, 10, 0, 28, 23, 92, 107, 100, 0, 0 },
    { 'd', TEX_PANEL1, 0, 0, 28, 23, 112, 129, 100, 0, 0 },
    { 'e', TEX_PANEL1, 0, 0, 28, 23, 130, 146, 100, 0, 0 },
    { 'g', TEX_PANEL2, 4, 0, 28, 22, 126, 142, 100, 0, 0 },
    { 'h', TEX_PANEL1, 2, 0, 28, 23, 83, 99, 100, 0, 0 },
    { 'i', TEX_PANEL2, 4, 0, 28, 22, 68, 76, 100, 0, 0 },
    { 'l', TEX_PANEL2, 10, 0, 28, 23, 110, 117, 100, 0, 0 },
    { 'm', TEX_PANEL2, 3, 0, 28, 24, 118, 142, 100, 0, 0 },
    { 'n', TEX_PANEL2, 4, 0, 28, 22, 78, 94, 100, 0, 0 },
    { 'o', TEX_PANEL1, 0, 0, 28, 23, 94, 110, 100, 0, 0 },
    { 'p', TEX_PANEL1, 2, 0, 28, 23, 66, 82, 100, 0, 0 },
    { 'r', TEX_PANEL2, 4, 0, 28, 22, 36, 49, 100, 0, 0 },
    { 's', TEX_PANEL1, 2, 0, 28, 23, 128, 142, 100, 0, 0 },
    { 't', TEX_PANEL1, 4, 0, 28, 23, 87, 99, 100, 0, 0 },
    { 'u', TEX_PANEL2, 3, 0, 28, 24, 100, 115, 100, 0, 0 },
    { 'y', TEX_PANEL2, 12, 0, 28, 23, 147, 160, 123, 0, 0 },
    { 0 },
};

static const Glyph list_glyphs[] = {
    { 'B', TEX_LIST, 11, 44, 59, 58, 0, 11, 100, 0, 0 },
    { 'C', TEX_LIST, 2, 74, 89, 88, 0, 12, 100, 0, 0 },
    { 'D', TEX_LIST, 3, 85, 100, 99, 57, 68, 100, 0, 0 },
    { 'E', TEX_LIST, 0, 31, 45, 44, 0, 11, 100, 0, 0 },
    { 'F', TEX_LIST, 0, 31, 45, 44, 0, 11, 100, 5, 13 },
    { 'G', TEX_LIST, 2, 14, 33, 28, 0, 12, 100, 0, 0 },
    { 'H', TEX_LIST, 10, 66, 84, 79, -1, 12, 100, 0, 0 },
    { 'L', TEX_LIST, 2, 45, 63, 58, 1, 11, 100, 0, 0 },
    { 'M', TEX_LIST, 0, 3, 22, 17, 67, 79, 100, 0, 0 },
    { 'N', TEX_LIST, 1, 86, 104, 99, 0, 13, 100, 0, 0 },
    { 'P', TEX_LIST, 10, 66, 84, 79, 69, 81, 100, 0, 0 },
    { 'R', TEX_LIST, 0, 3, 22, 17, 0, 10, 100, 0, 0 },
    { 'S', TEX_LIST, 0, 57, 72, 71, -1, 11, 100, 0, 0 },
    { 'T', TEX_LIST, 0, 84, 102, 97, -1, 10, 100, 0, 0 },
    { 'V', TEX_LIST, 11, 15, 30, 29, 2, 12, 100, 0, 0 },
    { 'a', TEX_LIST, 0, 3, 22, 17, 44, 54, 100, 0, 0 },
    { 'b', TEX_LIST, 3, 1, 16, 15, 42, 52, 100, 0, 0 },
    { 'c', TEX_LIST, 0, 3, 22, 17, 95, 105, 100, 0, 0 },
    { 'd', TEX_LIST, 0, 57, 72, 71, 34, 45, 100, 0, 0 },
    { 'e', TEX_LIST, 0, 3, 22, 17, 10, 19, 100, 0, 0 },
    { 'g', TEX_LIST, 0, 3, 22, 17, 19, 29, 100, 0, 0 },
    { 'h', TEX_LIST, 0, 3, 22, 17, 105, 114, 100, 0, 0 },
    { 'i', TEX_LIST, 0, 57, 72, 71, 47, 52, 100, 0, 0 },
    { 'l', TEX_LIST, 0, 3, 22, 17, 40, 44, 100, 0, 0 },
    { 'm', TEX_LIST, 0, 57, 72, 71, 67, 82, 100, 0, 0 },
    { 'n', TEX_LIST, 0, 31, 45, 44, 32, 41, 100, 0, 0 },
    { 'o', TEX_LIST, 2, 45, 63, 58, 13, 23, 100, 0, 0 },
    { 'r', TEX_LIST, 0, 3, 22, 17, 55, 62, 100, 0, 0 },
    { 's', TEX_LIST, 11, 15, 30, 29, 98, 107, 100, 0, 0 },
    { 't', TEX_LIST, 0, 3, 22, 17, 89, 95, 100, 0, 0 },
    { 'u', TEX_LIST, 0, 3, 22, 17, 30, 39, 100, 0, 0 },
    { 'v', TEX_LIST, 0, 31, 45, 44, 12, 20, 100, 0, 0 },
    { 'w', TEX_LIST, 10, 66, 84, 79, 24, 38, 100, 0, 0 },
    { 'y', TEX_LIST, 2, 14, 33, 28, 60, 71, 100, 0, 0 },
    { 0 },
};

/* slope: the italic's lean, x per row above the baseline. outline: the IA4 labels. xh: the
 * x-height, for the drawn z. */
typedef struct Style {
    const Glyph* glyphs;
    float slope;
    u8 outline, gap, space, xh;
} Style;

static const Style cursor_style = { cursor_glyphs, 0.0f, 1, 2, 8, 15 };
static const Style panel_style = { panel_glyphs, 0.31f, 0, 2, 9, 17 };
static const Style list_style = { list_glyphs, 0.31f, 0, 1, 6, 11 };

/* ---- the textures ---- */

typedef struct Tex {
    u8* data;
    int w, h, fmt;
} Tex;

static const struct {
    StaticModelSyms* model;
    const char* path;   /* from the material animation's root: c child, n next */
} tables[] = {
    { &MenMainCursor_Top, "ccn" },
    { &MenMainPanel_Top, "cnncnnn" },
    { &MenMainPanel_Top, "cnncnnnn" },
    { &MenMainConTop_Top, "ccncnc" },
};

/* Image k of a table, if it is the size and format these tables were measured on. */
static int tex_get(int table, int k, Tex* t)
{
    static const u16 size[4][3] = {
        { 176, 30, GX_TF_IA4 }, { 168, 28, GX_TF_I4 }, { 168, 28, GX_TF_I4 }, { 128, 104, GX_TF_I4 },
    };
    HSD_MatAnimJoint* j = tables[table].model->matanim_joint;
    const char* p;
    HSD_MatAnim* m;
    HSD_TexAnim* ta;
    struct HSD_ImageDesc* d;
    for (p = tables[table].path; j != NULL && *p != '\0'; p++) {
        j = *p == 'c' ? DP(j->child) : DP(j->next);
    }
    if (j == NULL || (m = DP(j->matanim)) == NULL || (ta = DP(m->texanim)) == NULL ||
        k >= ta->n_imagetbl || (d = DP(DP(ta->imagetbl)[k])) == NULL) {
        return 0;
    }
    t->data = DP(d->image_ptr);
    t->w = d->width;
    t->h = d->height;
    t->fmt = d->format;
    return t->data != NULL && t->w == size[table][0] && t->h == size[table][1] &&
           t->fmt == size[table][2];
}

/* The byte of texel (x, y): IA4 is 8x4 blocks of a byte each, I4 8x8 blocks of a nibble each. */
static u8* texel(const Tex* t, int x, int y, int* low)
{
    int bw = (t->w + 7) / 8;
    if (t->fmt == GX_TF_IA4) {
        *low = 0;
        return t->data + ((y / 4) * bw + x / 8) * 32 + (y % 4) * 8 + x % 8;
    }
    *low = x & 1;
    return t->data + ((y / 8) * bw + x / 8) * 32 + ((y % 8) * 8 + x % 8) / 2;
}

/* A letter's white, 0..255: an IA4 label's opaque part (its outline is black), an I4 one's
 * intensity. */
static int core(const Tex* t, int x, int y)
{
    int low;
    u8 v;
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) {
        return 0;
    }
    v = *texel(t, x, y, &low);
    if (t->fmt == GX_TF_IA4) {
        return (v >> 4) >= 8 ? (v & 15) * 17 : 0;
    }
    return (low ? v & 15 : v >> 4) * 17;
}

static void put(const Tex* t, int x, int y, int i, int a)
{
    int low;
    u8* p = texel(t, x, y, &low);
    int ni = (i + 8) / 17, na = (a + 8) / 17;
    ni = ni > 15 ? 15 : ni;
    na = na > 15 ? 15 : na;
    if (t->fmt == GX_TF_IA4) {
        *p = (u8) (na << 4 | ni);
    } else if (low) {
        *p = (u8) ((*p & 0xF0) | ni);
    } else {
        *p = (u8) ((*p & 0x0F) | ni << 4);
    }
}

/* ---- the letters ---- */

#define DY_MIN (-32)
#define DY_ROWS 48
#define STRIP_W 640

static u8 strip[DY_ROWS][STRIP_W];   /* a line of letters, upright, rows from DY_MIN */
static u8 work_i[104][176], work_a[104][176];

/* The game's math library has no floorf. */
static float floor_f(float v)
{
    int i = (int) v;
    return (float) (v < (float) i ? i - 1 : i);
}

static float lean(const Style* st, int dy)
{
    return floor_f(st->slope * (float) -dy + 0.5f);
}

static float sample(const Tex* t, float x, int y)
{
    int a = (int) floor_f(x);
    float f = x - (float) a;
    return (float) core(t, a, y) * (1.0f - f) + (float) core(t, a + 1, y) * f;
}

static const Glyph* find(const Style* st, char ch)
{
    const Glyph* g;
    for (g = st->glyphs; g->ch != 0; g++) {
        if (g->ch == ch) {
            return g;
        }
    }
    return NULL;
}

static int round_i(float v)
{
    return (int) floor_f(v + 0.5f);
}

/* z: bars at the top and bottom of the x-height and a diagonal between them. */
static int draw_z(const Style* st, int x)
{
    int xh = st->xh, w = round_i((float) xh * 0.85f), bar = round_i((float) xh * 0.25f);
    int dy, u;
    bar = bar < 2 ? 2 : bar;
    for (dy = -xh + 1; dy <= 0; dy++) {
        for (u = 0; u < w && x + u < STRIP_W; u++) {
            int v;
            if (dy <= -xh + bar || dy > -bar) {
                v = 255;
            } else {
                float t = (float) (dy + xh - 1 - bar) / (xh - 2 * bar - 1 > 1 ? (float) (xh - 2 * bar - 1) : 1.0f);
                float d = fabsf((float) u - (float) (w - 1) * (1.0f - t));
                float r = (float) bar * 0.6f;
                v = d < r ? 255 : d < r + 0.7f ? 128 : 0;
            }
            if (v > strip[dy - DY_MIN][x + u]) {
                strip[dy - DY_MIN][x + u] = (u8) v;
            }
        }
    }
    return w;
}

/* w: four strokes over the x-height, from its top down to two feet and back up, as wide as z
 * and a half. */
static int draw_w(const Style* st, int x)
{
    int xh = st->xh, w = round_i((float) xh * 1.3f);
    float q = (float) (w - 1) / 4.0f, r = (float) xh * 0.15f;
    int dy, u;
    r = r < 1.0f ? 1.0f : r;
    for (dy = -xh + 1; dy <= 0; dy++) {
        float t = (float) (dy + xh - 1) / (float) (xh > 1 ? xh - 1 : 1);   /* 0 at the top, 1 at the baseline */
        for (u = 0; u < w && x + u < STRIP_W; u++) {
            /* the strokes' centres on this row: the outer pair lean inwards going down to the
             * feet at a quarter and three quarters of the width, the inner pair outwards */
            float cx[4], d = 1e9f;
            int i, v;
            cx[0] = q * t;
            cx[1] = 2.0f * q - q * t;
            cx[2] = 2.0f * q + q * t;
            cx[3] = 4.0f * q - q * t;
            for (i = 0; i < 4; i++) {
                float e = fabsf((float) u - cx[i]);
                d = e < d ? e : d;
            }
            v = d < r ? 255 : d < r + 0.7f ? 128 : 0;
            if (v > strip[dy - DY_MIN][x + u]) {
                strip[dy - DY_MIN][x + u] = (u8) v;
            }
        }
    }
    return w;
}

/* ': a short stroke at the top of the capitals, narrowing at its foot. */
static int draw_apostrophe(const Style* st, int x)
{
    int cap = round_i((float) st->xh * 1.3f), len = round_i((float) st->xh * 0.45f);
    int w = 3, dy, u;
    len = len < 5 ? 5 : len;   /* small labels: big enough to survive clean() */
    for (dy = -cap; dy < -cap + len; dy++) {
        int ww = dy < -cap + len * 2 / 3 || st->xh < 12 ? w : w - 1;
        for (u = 0; u < ww && x + u < STRIP_W; u++) {
            if (dy >= DY_MIN) {
                strip[dy - DY_MIN][x + u] = 255;
            }
        }
    }
    return w;
}

/* Rows dy_lo..dy_hi (from the baseline) of a letter into the strip at x; returns its width, -1 if
 * its label is not there. */
static int draw_rows(const Style* st, const Glyph* g, int x, int dy_lo, int dy_hi)
{
    Tex src;
    float scale;
    int w, y, u;
    if (!tex_get(g->tex, g->k, &src)) {
        return -1;
    }
    scale = (float) g->scale / 100.0f;
    w = round_i((float) (g->u1 - g->u0) * scale);
    for (y = g->y0; y < g->y1; y++) {
        int dy = y - g->yb;
        if (dy < DY_MIN || dy >= DY_MIN + DY_ROWS || dy < dy_lo || dy > dy_hi) {
            continue;
        }
        for (u = 0; u < w && x + u < STRIP_W; u++) {
            float su = (float) g->u0 + (float) u / scale;
            int v = (int) sample(&src, su + lean(st, dy), y);
            if (g->mask_rows != 0 && dy > -g->mask_rows &&
                (float) u >= (float) g->mask_u2 / 2.0f * scale) {
                v = 0;
            }
            if (v > strip[dy - DY_MIN][x + u]) {
                strip[dy - DY_MIN][x + u] = (u8) v;
            }
        }
    }
    return w;
}

/* A letter into the strip at x; returns its width, -1 if the style has no such letter. */
static int draw_glyph(const Style* st, char ch, int x)
{
    const Glyph *g, *top, *bottom;
    if (ch == 'z') {
        return draw_z(st, x);
    }
    if (ch == '\'') {
        return draw_apostrophe(st, x);
    }
    if (ch == 'w') {
        return draw_w(st, x);
    }
    if ((g = find(st, ch)) != NULL) {
        return draw_rows(st, g, x, DY_MIN, DY_MIN + DY_ROWS);
    }
    /* B, where no label has one: P's bowl over D's lower half. */
    if (ch == 'B' && (top = find(st, 'P')) != NULL && (bottom = find(st, 'D')) != NULL) {
        int half = -round_i((float) st->xh * 0.7f), w0, w1;
        w0 = draw_rows(st, top, x, DY_MIN, half);
        w1 = draw_rows(st, bottom, x, half + 1, DY_MIN + DY_ROWS);
        return w0 > w1 ? w0 : w1;
    }
    return -1;
}

/* Drops specks: 8-connected pieces of fewer than 6 pixels, then the faintest pixels. */
static void clean(int w, int y0, int y1)
{
    static u8 seen[104][176];
    static s16 stack[104 * 176][2];
    int x, y;
    memset(seen, 0, sizeof seen);
    for (y = y0; y < y1; y++) {
        for (x = 0; x < w; x++) {
            int n = 0, top = 0, i;
            if (work_i[y][x] <= 24 || seen[y][x]) {
                continue;
            }
            seen[y][x] = 1;
            stack[top][0] = (s16) x;
            stack[top++][1] = (s16) y;
            /* The piece is the stack's first n entries once it is walked. */
            while (n < top) {
                int cx = stack[n][0], cy = stack[n][1], dx, dy;
                n++;
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        int nx = cx + dx, ny = cy + dy;
                        if (ny >= y0 && ny < y1 && nx >= 0 && nx < w && !seen[ny][nx] &&
                            work_i[ny][nx] > 24) {
                            seen[ny][nx] = 1;
                            stack[top][0] = (s16) nx;
                            stack[top++][1] = (s16) ny;
                        }
                    }
                }
            }
            if (top < 6) {
                for (i = 0; i < top; i++) {
                    work_i[stack[i][1]][stack[i][0]] = 0;
                }
            }
        }
    }
    for (y = y0; y < y1; y++) {
        for (x = 0; x < w; x++) {
            if (work_i[y][x] <= 24) {
                work_i[y][x] = 0;
            }
        }
    }
}

/* The black outline of the IA4 labels: opaque 2 px around the letters, fading over the third. */
static void outline(int w, int y0, int y1)
{
    int x, y;
    for (y = y0; y < y1; y++) {
        for (x = 0; x < w; x++) {
            float best = 9.0f;
            int dx, dy;
            if (work_i[y][x] > 0) {
                work_a[y][x] = 255;
                continue;
            }
            for (dy = -3; dy <= 3; dy++) {
                for (dx = -3; dx <= 3; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (yy >= y0 && yy < y1 && xx >= 0 && xx < w && work_i[yy][xx] > 0) {
                        float d = sqrtf((float) (dx * dx + dy * dy));
                        best = d < best ? d : best;
                    }
                }
            }
            work_a[y][x] = best <= 2.0f ? 255 : best < 3.0f ? (u8) (255.0f * (3.0f - best)) : 0;
        }
    }
}

/* One line of text into rows clear_y0..clear_y1 of a label, on baseline yb: centred, or from the
 * left edge, squeezed sideways into maxw. */
static void draw_line(const Style* st, int table, int k, const char* text, int yb, int clear_y0,
                      int clear_y1, int maxw, int centred)
{
    Tex t;
    int x = 0, total, x0, X, y, dy, n = 0;
    float scale, mw;
    if (!tex_get(table, k, &t)) {
        party_log("menu: label %d/%d is not the one expected, left as it is", table, k);
        return;
    }
    clear_y1 = clear_y1 > t.h ? t.h : clear_y1;
    memset(strip, 0, sizeof strip);
    for (; *text != '\0'; text++, n++) {
        int w;
        if (n > 0) {
            x += st->gap;
        }
        if (*text == ' ') {
            x += st->space;
            continue;
        }
        if ((w = draw_glyph(st, *text, x)) < 0) {
            party_log("menu: no letter '%c' to make a label from", *text);
            continue;
        }
        x += w;
    }
    total = x < STRIP_W - 1 ? x : STRIP_W - 1;
    if (total <= 0) {
        return;
    }
    mw = (float) maxw - st->slope * 16.0f;   /* room for the lean */
    scale = mw / (float) total < 1.0f ? mw / (float) total : 1.0f;
    x0 = centred ? (int) (((float) t.w - (float) total * scale) / 2.0f) : 1;

    for (y = clear_y0; y < clear_y1; y++) {
        memset(work_i[y], 0, sizeof work_i[y]);
        memset(work_a[y], 0, sizeof work_a[y]);
    }
    for (dy = DY_MIN; dy < DY_MIN + DY_ROWS; dy++) {
        const u8* s = strip[dy - DY_MIN];
        float shear = lean(st, dy);
        y = yb + dy;
        if (y < clear_y0 || y >= clear_y1) {
            continue;
        }
        for (X = 0; X < t.w; X++) {
            float su = ((float) X - (float) x0 - shear) / scale;
            if (su >= 0.0f && su < (float) total) {
                int a = (int) su;
                float f = su - (float) a;
                int v = (int) ((float) s[a] * (1.0f - f) + (float) s[a + 1] * f);
                if (v > work_i[y][X]) {
                    work_i[y][X] = (u8) v;
                }
            }
        }
    }
    clean(t.w, clear_y0, clear_y1);
    if (st->outline) {
        outline(t.w, clear_y0, clear_y1);
    }
    for (y = clear_y0; y < clear_y1; y++) {
        for (X = 0; X < t.w; X++) {
            put(&t, X, y, work_i[y][X], st->outline ? work_a[y][X] : work_i[y][X]);
        }
    }
}

/* ---- the menu ---- */

#define CURSOR_TOURNAMENT 11
#define CURSOR_SPECIAL 12
#define CURSOR_RULES 13
#define CURSOR_SPECIAL_ROW0 41   /* Camera Mode, then the other Special Melee rows */
#define PANEL_TOURNAMENT 6
#define PANEL_SPECIAL 7
#define PANEL_RULES 8
#define LIST_VS 1                /* Melee, Tournament Melee, Special Melee, Custom Rules, Name Entry */
#define LIST_SPECIAL 7
#define LIST_RULES 8

static const char* const NAME_PARTY = "Melee Party";
static const char* const NAME_MINIGAMES = "Party Minigames";
static const char* const NAME_DEBUG = "Debug Boards";   /* in place of Custom Rules */

static int showing = PARTY_MENU_MINIGAMES;   /* what the Special Melee submenu lists */

static int special_rows(void)
{
    int n = showing != PARTY_MENU_MINIGAMES ? board_count() : minigame_offered_count(0);
    return n > 10 ? 10 : n;
}

static const char* row_name(int i)
{
    return showing != PARTY_MENU_MINIGAMES ? board_name(i)
                                           : minigame_get(minigame_offered_at(i, 0))->name;
}

/* The submenu's header, rows and number of rows, for the list it shows. */
static void label_rows(void)
{
    int i;
    draw_line(&panel_style, TEX_PANEL2, PANEL_SPECIAL,
              showing == PARTY_MENU_BOARDS ? NAME_PARTY
              : showing == PARTY_MENU_DEBUG_BOARDS ? NAME_DEBUG : NAME_MINIGAMES,
              23, 0, 28, 166, 1);
    mn_803EB6B0[MENU_KIND_SPECIAL].selection_count = (u8) special_rows();
    for (i = 0; i < special_rows(); i++) {
        draw_line(&cursor_style, TEX_CURSOR, CURSOR_SPECIAL_ROW0 + i, row_name(i), 24, 0, 30, 172, 1);
    }
}

void party_menu_show(int list)
{
    showing = list;
}

/* mn/mnmain.c, the Vs. menu: Melee Party, Party Minigames and Debug Boards all open the Special
 * Melee submenu, with the boards or the minigames. 1: open it (the menu's own way for Special
 * Melee). */
int mu_party_vs_submenu(int selection)
{
    if (!mu_party_menu_on()) {
        return 0;
    }
    switch (selection) {
    case SEL_VS_TOURNAMENT: party_menu_show(PARTY_MENU_BOARDS); break;
    case SEL_VS_SPECIAL: party_menu_show(PARTY_MENU_MINIGAMES); break;
    case SEL_VS_RULES: party_menu_show(PARTY_MENU_DEBUG_BOARDS); break;
    default: return 0;
    }
    label_rows();
    return 1;
}

/* mn/mnmain.c, back out of the submenu: onto the Vs. entry that opened it. */
int mu_party_special_back(int retail_selection)
{
    if (!mu_party_menu_on()) {
        return retail_selection;
    }
    return showing == PARTY_MENU_BOARDS ? SEL_VS_TOURNAMENT
         : showing == PARTY_MENU_DEBUG_BOARDS ? SEL_VS_RULES : SEL_VS_SPECIAL;
}

/* mn/mnmain.c mnMain_Scene_OnEnter, once MnMaAll's symbols are loaded. */
void mu_party_menu_loaded(void)
{
    /* The 1-P Mode list's four lines: their baselines and rows. */
    static const s16 four_lines[4][3] = { { 17, 3, 26 }, { 44, 31, 49 }, { 71, 57, 76 }, { 97, 84, 104 } };
    int i;
    if (!mu_party_menu_on()) {
        return;
    }
    draw_line(&cursor_style, TEX_CURSOR, CURSOR_TOURNAMENT, NAME_PARTY, 24, 0, 30, 172, 1);
    draw_line(&cursor_style, TEX_CURSOR, CURSOR_SPECIAL, NAME_MINIGAMES, 24, 0, 30, 172, 1);
    label_rows();
    draw_line(&panel_style, TEX_PANEL2, PANEL_TOURNAMENT, NAME_PARTY, 23, 0, 28, 166, 1);
    draw_line(&list_style, TEX_LIST, LIST_VS, NAME_PARTY, 35, 21, 38, 124, 0);
    draw_line(&list_style, TEX_LIST, LIST_VS, NAME_MINIGAMES, 57, 43, 63, 124, 0);
    draw_line(&cursor_style, TEX_CURSOR, CURSOR_RULES, NAME_DEBUG, 24, 0, 30, 172, 1);
    draw_line(&panel_style, TEX_PANEL2, PANEL_RULES, NAME_DEBUG, 23, 0, 28, 166, 1);
    draw_line(&list_style, TEX_LIST, LIST_VS, NAME_DEBUG, 79, 64, 86, 124, 0);
    {
        /* The Special Melee list: cleared, then the minigames on the 1-P list's lines. */
        Tex t;
        if (tex_get(TEX_LIST, LIST_SPECIAL, &t)) {
            memset(t.data, 0, (size_t) (t.w * t.h / 2));
        }
        for (i = 0; i < minigame_offered_count(0) && i < 4; i++) {
            draw_line(&list_style, TEX_LIST, LIST_SPECIAL, minigame_get(minigame_offered_at(i, 0))->name,
                      four_lines[i][0], four_lines[i][1], four_lines[i][2], 124, 0);
        }
        /* The Custom Rules list (now Debug Boards'): the boards, the same way. */
        if (tex_get(TEX_LIST, LIST_RULES, &t)) {
            memset(t.data, 0, (size_t) (t.w * t.h / 2));
        }
        for (i = 0; i < board_count() && i < 4; i++) {
            draw_line(&list_style, TEX_LIST, LIST_RULES, board_name(i), four_lines[i][0],
                      four_lines[i][1], four_lines[i][2], 124, 0);
        }
    }
    party_log("menu: Vs. entries renamed, %d minigames, %d boards", minigame_offered_count(0),
              board_count());
}

/* The descriptions under the menu, by minigame id. */
static const struct {
    const char* id;
    const char* text;
} descriptions[] = {
    { "volley", "Two against two. Hit the\nball over the net." },
    { "sandbag", "Damage the Sandbag, and\nknock it out for 40." },
    { "food", "Eat the most of the food\nthat rains down." },
    { "domination", "Mash A to swing. Make the\nmost Snorlaxes." },
    { "dungeon", "Two against two. Mash B,\nA, then L and R: get out!" },
    { "bigger-blast", "Push the plungers. One of\nthem sets Bowser off." },
    { "chomp-fever", "Dodge the Chain Chomps\nfor a minute. Stay on!" },
    { "blizzard-brigade", "Dodge the snowballs on\nthe ice for a minute." },
    { "booksquirm", "Find a cutout before the\npage lands on you." },
    { "butterfly-blitz", "Net the butterflies.\nA swings high, B low." },
    { "trace-race", "Steer your brush along\nthe line. Closest wins." },
    { "candlelight-flight", "One carries the candle;\nthree blow at it with A." },
};

static const char* description_of(int menu_kind, int selection)
{
    int i;
    if (menu_kind == MENU_KIND_VS && selection == SEL_VS_TOURNAMENT) {
        return "A board game for 1 to 4\nplayers, with minigames.";
    }
    if (menu_kind == MENU_KIND_VS && selection == SEL_VS_SPECIAL) {
        return "Play any of the party's\nminigames.";
    }
    if (menu_kind == MENU_KIND_VS && selection == SEL_VS_RULES) {
        return "Play the boards with no\nminigames, for testing.";
    }
    if (menu_kind == MENU_KIND_SPECIAL && showing != PARTY_MENU_MINIGAMES &&
        selection < special_rows()) {
        return board_description(selection);
    }
    if (menu_kind == MENU_KIND_SPECIAL && selection < special_rows()) {
        const PartyMinigame* mg = minigame_get(minigame_offered_at(selection, 0));
        for (i = 0; i < (int) (sizeof descriptions / sizeof descriptions[0]); i++) {
            if (strcmp(descriptions[i].id, mg->id) == 0) {
                return descriptions[i].text;
            }
        }
        return mg->name;
    }
    return NULL;
}

#define DESC_SCALE 0.52f   /* the size of the menu's own description text */
#define DESC_TOP (-19.0f)  /* its first line */
#define DESC_LINE 18.0f    /* and its line spacing */

/* mn/mnmain.c mn_80229A7C: the description of the hovered entry, made from text rather than the
 * menu's own strings for the entries the party changed. NULL: the menu's own. */
HSD_Text* mu_party_menu_description(int menu_kind, int selection)
{
    const char* desc;
    HSD_Text* text;
    SisBuffer* alloc;
    if (!mu_party_menu_on() || (desc = description_of(menu_kind, selection)) == NULL) {
        return NULL;
    }
    /* mn_80229A7C's box, with a buffer of its own as HSD_SisLib_803A6754 gives one. */
    text = HSD_SisLib_803A5ACC(0, mn_804D6BB4, -9.5f, 9.1f, 17.0f, 364.68332f, 38.38772f);
    text->font_size.x = 0.0521f;
    text->font_size.y = 0.0521f;
    alloc = HSD_SisLib_Alloc(sizeof(SisBuffer));
    alloc->data = HSD_SisLib_Alloc(0x80);
    alloc->end = alloc->data;
    alloc->size = 0x80;
    *alloc->end = 0;
    alloc->count = 0;
    text->alloc_data = alloc;
    HSD_SisLib_803A6368(text, 0);
    text->sis_buffer = alloc->data;
    text->default_kerning = 1;
    /* Two lines, as the menu's own descriptions are: up to the newline, then the rest. */
    {
        char line[64];
        const char* nl = strchr(desc, '\n');
        int n = nl != NULL ? (int) (nl - desc) : (int) strlen(desc), idx;
        n = n < (int) sizeof line - 1 ? n : (int) sizeof line - 1;
        memcpy(line, desc, (size_t) n);
        line[n] = '\0';
        idx = HSD_SisLib_803A6B98(text, 0.0f, DESC_TOP, "%s", line);
        HSD_SisLib_803A7548(text, idx, DESC_SCALE, DESC_SCALE);
        if (nl != NULL) {
            idx = HSD_SisLib_803A6B98(text, 0.0f, DESC_TOP + DESC_LINE, "%s", nl + 1);
            HSD_SisLib_803A7548(text, idx, DESC_SCALE, DESC_SCALE);
        }
    }
    return text;
}

/* mn/mnmain.c, the Special Melee submenu's think: a row picks its board or minigame. -1: the
 * menu's own. */
int mu_party_special_menu_mode(int selection)
{
    if (!mu_party_menu_on() || selection >= special_rows()) {
        return -1;
    }
    if (showing != PARTY_MENU_MINIGAMES) {
        party_board_pick(selection, showing == PARTY_MENU_DEBUG_BOARDS);
    } else {
        party_menu_pick(minigame_offered_at(selection, 0));
    }
    return PARTY_MODE;
}
