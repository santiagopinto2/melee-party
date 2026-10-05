/* Melee Party: text over a match (scores, coins, prompts). A HUD camera and a text canvas created
 * at match start, as the Slippi online HUD does (shim/mu_slippi_splash.c). Coordinates are the
 * HUD's: x -320..320 left to right, y -240..240 top to bottom. */
#include <stdarg.h>

#include <melee/if/ifall.h>
#include <melee/lb/lbarchive.h>
#include <melee/sc/types.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/gobjgxlink.h>
#include <sysdolphin/baselib/gobjobject.h>
#include <sysdolphin/baselib/sislib.h>

#include "party.h"
#include "party_hud.h"

int vsnprintf(char* buffer, __SIZE_TYPE__ size, const char* format, __builtin_va_list args);

static int canvas = -1;

static void hud_cobj_cb(HSD_GObj* gobj, intptr_t pass)
{
    HSD_GObj_803910D8(gobj, pass);
}

void party_hud_init(void)
{
    SceneDesc* sd = NULL;
    HSD_CObj* cobj;
    HSD_GObj* cam;

    canvas = -1;
    lbArchive_LoadSections(*ifAll_GetArchive(), (void**) &sd, "ScInfDmg_scene_data", NULL);
    if (sd == NULL) {
        party_log("hud: no ScInfDmg camera");
        return;
    }
    cobj = HSD_CObjLoadDesc(DP(DP(sd->cameras)[0].desc));
    cam = GObj_Create(19, 20, 0);
    HSD_GObjObject_80390A70(cam, HSD_GObj_CameraKind, cobj);
    GObj_SetupGXLinkMax(cam, hud_cobj_cb, 8);
    cam->gxlink_prios = 1ULL << 12;
    canvas = HSD_SisLib_803A611C(2, cam, 9, 13, 0, 12, 80, 8);
}

HSD_Text* party_hud_text(void)
{
    HSD_Text* t;
    if (canvas < 0) {
        return NULL;
    }
    t = HSD_SisLib_803A6754(2, canvas);
    t->default_kerning = 1;
    t->default_alignment = 1;   /* centred */
    t->pos_z = 0.0f;
    t->font_size.x = 0.1f;
    t->font_size.y = 0.1f;
    return t;
}

int party_hud_line(HSD_Text* t, float x, float y, float scale, GXColor color)
{
    int idx;
    if (t == NULL) {
        return -1;
    }
    idx = HSD_SisLib_803A6B98(t, x, y, "");
    HSD_SisLib_803A7548(t, idx, scale, scale);
    HSD_SisLib_803A74F0(t, idx, &color);
    return idx;
}

/* The Shift-JIS pair of a symbol the text encoder has no ASCII case for, or 0. The encoder reads
 * any other byte as the first half of a Shift-JIS pair: a lone "!" swallowed the end of the
 * string and the encoder went on into whatever followed it. */
static unsigned sjis_symbol(char c)
{
    switch (c) {
    case '!': return 0x8149; case '?': return 0x8148; case '/': return 0x815E;
    case '+': return 0x817B; case '=': return 0x8181; case '%': return 0x8193;
    case '#': return 0x8194; case '&': return 0x8195; case '*': return 0x8196;
    case '(': return 0x8169; case ')': return 0x816A; case '<': return 0x8183;
    case '>': return 0x8184;
    }
    return 0;
}

void party_hud_set(HSD_Text* t, int idx, const char* fmt, ...)
{
    char buf[128], out[128];
    int i, n = 0;
    va_list ap;
    if (t == NULL || idx < 0) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    for (i = 0; buf[i] != '\0' && n + 2 < (int) sizeof out; i++) {
        char c = buf[i];
        unsigned sym = sjis_symbol(c);
        if (sym != 0) {
            out[n++] = (char) (sym >> 8);
            out[n++] = (char) sym;
        } else if (c == ' ' || c == '"' || c == '\'' || c == ',' || c == '-' || c == '.' ||
                   c == ':' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z')) {
            out[n++] = c;
        }
        /* anything else has no glyph: left out */
    }
    out[n] = '\0';
    HSD_SisLib_803A70A0(t, idx, (char*) "%s", out);
}

void party_hud_color(HSD_Text* t, int idx, GXColor color)
{
    if (t != NULL && idx >= 0) {
        HSD_SisLib_803A74F0(t, idx, &color);
    }
}

void party_hud_move(HSD_Text* t, int idx, float x, float y)
{
    if (t != NULL && idx >= 0) {
        HSD_SisLib_803A746C(t, idx, x, y);
    }
}
