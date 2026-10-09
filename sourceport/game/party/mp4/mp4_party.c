/* Melee Party, Mario Party 4 runtime: an MP4 minigame running inside a Melee match.
 *
 * MP4's frame (main.c there) is: read the pads, run every process (HuPrcCall: the object manager,
 * which runs the minigame's objects), the banners (MGSeqMain), then draw (Hu3DExec, with the
 * sprites) and the screen wipe. Here the first half runs from the match's on_frame_start hook and
 * the drawing from a GObj on the party's render link, after the stage, so the match camera draws
 * MP4's models with the scene. The cameras take the match camera's view (mp4_camera_view in
 * hsfman.c), scaled from MP4's units into Melee's, and the wrapper feeds MP4's camera back to the
 * match camera each frame, so the fighters and MP4's models agree.
 *
 * The runtime (processes, sprites, models, the object manager) comes up once, at the first
 * minigame; between minigames MP4's own overlay switch frees everything the last one loaded.
 *
 * MELEE_PARTY_MP4_MODEL=<archive>:<file>[:<motion file>] (a check of the model layer) draws one
 * model from the MP4 disc in every party match, at the stage's centre: the archive by its name in
 * the disc's data directory (m440, or data/m440.bin), the file by its number in it (decimal, or
 * 0x hex), and optionally a motion from the same archive played on a loop. MELEE_PARTY_MP4_SCALE
 * (default 0.1) and MELEE_PARTY_MP4_Y (default 0) place it. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <dolphin/gx.h>
#include <dolphin/os.h>
#include <melee/gm/types.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/gobjgxlink.h>
#include <sysdolphin/baselib/gobjproc.h>
#include <sysdolphin/baselib/state.h>
#include <sysdolphin/baselib/tev.h>
#include <sysdolphin/baselib/video.h>

#include "game/data.h"
#include "game/gamework_data.h"
#include "game/hu3d.h"
#include "game/init.h"
#include "game/memory.h"
#include "game/minigame_seq.h"
#include "game/object.h"
#include "game/pad.h"
#include "game/process.h"
#include "game/sprite.h"
#include "game/wipe.h"
#include "mp4.h"
#include "../party.h"

char* getenv(const char* name);
double atof(const char* s);
void HSD_ClearVtxDesc(void);
void mp4_gx_frame_begin(void);
void mp4_pad_reset(void);
extern MtxPtr mp4_camera_view;
extern float mp4_camera_scale;
extern s16 Hu3DAdvanceExternF;
extern u32 GlobalCounter;
void Hu3DAdvance(void);

static int runtime_up;
static int match_running;
static Vec world_offset;
static u16 pad_prev[4];

/* ---- the runtime ---- */

static void runtime_open(void)
{
    s32 i;
    if (runtime_up) {
        return;
    }
    runtime_up = 1;
    GWGameStat.language = 1;   /* English, as the USA disc's banners (MGSeqInit reads it) */
    HuPrcInit();
    HuSprInit();
    Hu3DInit();
    MGSeqInit();
    WipeInit(RenderMode);
    for (i = 0; i < 4; i++) {
        GWPlayerCfg[i].character = -1;
    }
    omMasterInit(0, NULL, DLL_MAX, DLL_bootDll);
    Hu3DAdvanceExternF = 1;   /* motions step in mp4_frame, drawn or not */
}

/* The match camera's projection, as set when MP4's draw starts, and its viewport and scissor from
 * the camera itself (as HSD sets them, setupNormalCamera in cobj.c). MP4's sprite layer
 * (HuSprDispInit) and each sprite replace them with their own, so every MP4 camera puts them back
 * (Hu3DCameraSet, through mp4_camera_restore), and so does the end of the draw. */
static f32 match_projection[7];

static void match_view_save(void)
{
    GXGetProjectionv(match_projection);
}

/* A split-screen match (mp4_views, mp4_view_begin): the whole screen's viewport, scissor and
 * aspect, saved when the first view of a frame is set up, for the HUD and for carving the
 * views out of it. */
static int views_active;
static int views_log;   /* MELEE_PARTY_MP4_LOG: frame_log asks for the next frame's views */
static HSD_RectF32 full_viewport;
static Scissor full_scissor;
static f32 full_aspect;
int mp4_hud_full;
extern s16 mp4_view_cameras;
extern int mp4_hud_pass;
extern int mp4_draw_rearm(void);

void mp4_camera_restore(void)
{
    HSD_CObj* cobj = HSD_CObjGetCurrent();
    GXRenderModeObj* rmode = HSD_VIGetRenderMode();
    f32 xs = (f32) rmode->fbWidth / (f32) rmode->viWidth;
    f32 ys = (f32) rmode->efbHeight / (f32) rmode->viHeight;
    f32 l, t, r, b;
    HSD_RectF32 vp;
    Scissor sc;
    GXSetProjectionv(match_projection);
    if (cobj == NULL) {
        return;
    }
    vp = cobj->viewport;
    sc = cobj->scissor;
    if (views_active && mp4_hud_full) {
        vp = full_viewport;
        sc = full_scissor;
    }
    l = vp.xmin * xs;
    r = vp.xmax * xs;
    t = vp.ymin * ys;
    b = vp.ymax * ys;
    GXSetViewport(l, t, r - l, b - t, 0.0f, 1.0f);
    l = sc.left * xs;
    r = sc.right * xs;
    t = sc.top * ys;
    b = sc.bottom * ys;
    GXSetScissor((u32) l, (u32) t, (u32) (r - l), (u32) (b - t));
}

/* The views of a frame: the viewports the players' models are drawn in, one view each, in index
 * order (Team Treasure Trek draws everything four times, a quarter of the screen a camera; a
 * one-camera game has one). A camera that draws no player (Cheep Cheep Sweep's reflection
 * cameras, rendering into a corner for a screen copy, and its other full-screen passes) is not
 * a view: it draws inside the frame with its own perspective, viewport and eye (mp4_camera_set).
 * A view's primary camera is the first in it that draws a player: the match camera takes its
 * eye, so the fighters land in MP4's scene. */
typedef struct {
    s16 primary;      /* the camera the match camera follows */
    s16 cameras;      /* bits: the cameras drawn in this view */
    f32 x, y, w, h;   /* its viewport, on MP4's 640 x 480 */
} MP4View;

extern s16 mp4_char_model(int charNo);

static int view_list(MP4View out[HU3D_CAM_MAX])
{
    s16 players = 0;
    s16 all = 0;
    int n = 0;
    int p;
    s16 i, v;
    for (p = 0; p < 4; p++) {
        s16 model = mp4_char_model(p);
        if (model >= 0 && Hu3DData[model].hsf != NULL) {
            players |= Hu3DData[model].cameraBit;
        }
    }
    for (i = 0; i < HU3D_CAM_MAX; i++) {
        HU3DCAMERA* cam = &Hu3DCamera[i];
        if (cam->fov == -1.0f) {
            continue;
        }
        all |= (s16) (1 << i);
        if ((players & (1 << i)) == 0) {
            continue;
        }
        for (v = 0; v < n; v++) {
            if (out[v].x == cam->viewportX && out[v].y == cam->viewportY && out[v].w == cam->viewportW &&
                out[v].h == cam->viewportH) {
                break;
            }
        }
        if (v == n) {
            out[n].primary = i;
            out[n].cameras = 0;
            out[n].x = cam->viewportX;
            out[n].y = cam->viewportY;
            out[n].w = cam->viewportW;
            out[n].h = cam->viewportH;
            n++;
        }
        out[v].cameras |= (s16) (1 << i);
    }
    if (n == 0) {
        /* no player yet (the overlay is loading): one view, the first camera, everything in it */
        for (i = 0; i < HU3D_CAM_MAX && (all & (1 << i)) == 0; i++) {
        }
        out[0].primary = i < HU3D_CAM_MAX ? i : 0;
        out[0].cameras = -1;
        out[0].x = out[0].y = 0.0f;
        out[0].w = 640.0f;
        out[0].h = 480.0f;
        return 1;
    }
    if (n == 1) {
        out[0].cameras = -1;   /* the passes without players draw in the one view too */
    }
    return n;
}

float mp4_fog_near = 1.0f;     /* the match camera's projection depths, for the fog and the passes */
float mp4_fog_far = 16384.0f;
static f32 clip_max(f32 v, f32 limit)
{
    return v < limit ? v : limit;
}

static s16 view_primary;   /* this draw's: Hu3DCameraSet follows it (mp4_camera_set) */

int mp4_views(void)
{
    MP4View views[HU3D_CAM_MAX];
    int n;
    if (!match_running) {
        views_active = 0;
        mp4_view_cameras = -1;
        mp4_hud_pass = 1;
        return 1;
    }
    n = view_list(views);
    if (n <= 1) {
        views_active = 0;
        mp4_view_cameras = -1;
        mp4_hud_pass = 1;
        return 1;
    }
    return n;
}

/* One view of a split screen, before the match camera draws it: the match camera takes the
 * primary camera's eye, target and fov, and the view's viewport and scissor carved out of the
 * whole screen's; the MP4 draw renders the view's cameras alone, once per view, and the HUD once a
 * frame, on the view drawn last. View 0 is drawn first: its camera 0 renders MP4's shadow maps in
 * the top-left corner and then repaints the whole screen (Hu3DShadowExec, Paths of Peril's layer
 * hook), which on a GameCube happens before any camera draws. */
void mp4_view_begin(int call, HSD_CObj* cobj)
{
    MP4View views[HU3D_CAM_MAX];
    int n = view_list(views);
    int view;
    MP4View* v;
    HU3DCAMERA* cam;
    HSD_RectF32 vp;
    Vec3 eye, look;
    f32 sx, sy;
    if (n <= 1 || call < 0 || call >= n) {
        return;
    }
    view = n - 1 - call;   /* the camera loop counts down: call n - 1 is drawn first, call 0 last */
    if (call == n - 1) {
        views_active = 1;
        full_viewport = cobj->viewport;
        full_scissor = cobj->scissor;
        full_aspect = HSD_CObjGetAspect(cobj);
    }
    v = &views[view];
    cam = &Hu3DCamera[v->primary];
    view_primary = v->primary;
    mp4_view_cameras = v->cameras;
    mp4_hud_pass = call == 0;
    mp4_draw_rearm();
    /* MP4 lays its cameras out on a 640 x 480 screen */
    sx = (full_viewport.xmax - full_viewport.xmin) / 640.0f;
    sy = (full_viewport.ymax - full_viewport.ymin) / 480.0f;
    vp.xmin = full_viewport.xmin + v->x * sx;
    vp.xmax = vp.xmin + v->w * sx;
    vp.ymin = full_viewport.ymin + v->y * sy;
    vp.ymax = vp.ymin + v->h * sy;
    HSD_CObjSetViewportfx4(cobj, vp.xmin, vp.xmax, vp.ymin, vp.ymax);
    /* clamped to the screen: Paths of Peril sets quarter scissors 640 wide (the hardware clamps) */
    HSD_CObjSetScissorx4(cobj, (u16) (full_viewport.xmin + cam->scissorX * sx),
                         (u16) clip_max(full_viewport.xmin + (cam->scissorX + cam->scissorW) * sx, full_viewport.xmax),
                         (u16) (full_viewport.ymin + cam->scissorY * sy),
                         (u16) clip_max(full_viewport.ymin + (cam->scissorY + cam->scissorH) * sy, full_viewport.ymax));
    HSD_CObjSetAspect(cobj, full_aspect * (v->w / 640.0f) / (v->h / 480.0f));
    eye.x = cam->pos.x * MP4_SCALE + world_offset.x;
    eye.y = cam->pos.y * MP4_SCALE + world_offset.y;
    eye.z = cam->pos.z * MP4_SCALE + world_offset.z;
    look.x = cam->target.x * MP4_SCALE + world_offset.x;
    look.y = cam->target.y * MP4_SCALE + world_offset.y;
    look.z = cam->target.z * MP4_SCALE + world_offset.z;
    HSD_CObjSetEyePosition(cobj, &eye);
    HSD_CObjSetInterest(cobj, &look);
    HSD_CObjSetFov(cobj, cam->fov);
    if (views_log) {
        party_log("mp4:   view %d of %d: camera %d (cameras %04X) at %.0f %.0f %.0f x %.0f, viewport %.0f-%.0f %.0f-%.0f "
                  "scissor %d-%d %d-%d, eye %.1f %.1f %.1f look %.1f %.1f %.1f fov %.1f", view, n, (int) v->primary,
                  (int) (u16) v->cameras, v->x, v->y, v->w, v->h, vp.xmin, vp.xmax, vp.ymin, vp.ymax,
                  (int) cobj->scissor.left, (int) cobj->scissor.right, (int) cobj->scissor.top,
                  (int) cobj->scissor.bottom, eye.x, eye.y, eye.z, look.x, look.y, look.z, cam->fov);
        if (call == 0) {
            views_log = 0;
        }
    }
}

/* hsfman.c, Hu3DCameraSet, while MP4 draws in the match: a camera's GX setup and view matrix.
 * The view's primary camera takes the match camera's projection, viewport, scissor and view
 * (the fighters are drawn with them). Any other camera in the draw is a render pass of its own
 * (Cheep Cheep Sweep's reflection cameras render into a corner for a screen copy): it keeps its
 * own perspective, viewport, scissor and eye, in Melee's units. */
void mp4_camera_set(s32 camNo, Mtx out)
{
    Mtx scale, shift, view;
    HU3DCAMERA* cam;
    Vec3 eye, up, look;
    Mtx44 proj;
    GXRenderModeObj* rmode;
    f32 xs, ys, sx, sy, l, t, r, b;
    MTXScale(scale, mp4_camera_scale, mp4_camera_scale, mp4_camera_scale);
    if (camNo < 0 || camNo >= HU3D_CAM_MAX || camNo == view_primary) {
        mp4_camera_restore();
        MTXConcat(mp4_camera_view, scale, out);
        return;
    }
    cam = &Hu3DCamera[camNo];
    /* the match camera's near and far, not the pass's own: the depth buffer must compare across
     * the passes and with Melee's fighters (MP4's cameras all share one pair, so it does there) */
    C_MTXPerspective(proj, cam->fov, full_aspect * (cam->viewportW / 640.0f) / (cam->viewportH / 480.0f),
                     mp4_fog_near, mp4_fog_far);
    GXSetProjection(proj, GX_PERSPECTIVE);
    rmode = HSD_VIGetRenderMode();
    xs = (f32) rmode->fbWidth / (f32) rmode->viWidth;
    ys = (f32) rmode->efbHeight / (f32) rmode->viHeight;
    sx = (full_viewport.xmax - full_viewport.xmin) / 640.0f;
    sy = (full_viewport.ymax - full_viewport.ymin) / 480.0f;
    l = (full_viewport.xmin + cam->viewportX * sx) * xs;
    r = (full_viewport.xmin + (cam->viewportX + cam->viewportW) * sx) * xs;
    t = (full_viewport.ymin + cam->viewportY * sy) * ys;
    b = (full_viewport.ymin + (cam->viewportY + cam->viewportH) * sy) * ys;
    GXSetViewport(l, t, r - l, b - t, cam->viewportNear, cam->viewportFar);
    l = (full_viewport.xmin + cam->scissorX * sx) * xs;
    r = clip_max(full_viewport.xmin + (cam->scissorX + cam->scissorW) * sx, full_viewport.xmax) * xs;
    t = (full_viewport.ymin + cam->scissorY * sy) * ys;
    b = clip_max(full_viewport.ymin + (cam->scissorY + cam->scissorH) * sy, full_viewport.ymax) * ys;
    GXSetScissor((u32) l, (u32) t, (u32) (r - l), (u32) (b - t));
    eye.x = cam->pos.x * MP4_SCALE + world_offset.x;
    eye.y = cam->pos.y * MP4_SCALE + world_offset.y;
    eye.z = cam->pos.z * MP4_SCALE + world_offset.z;
    look.x = cam->target.x * MP4_SCALE + world_offset.x;
    look.y = cam->target.y * MP4_SCALE + world_offset.y;
    look.z = cam->target.z * MP4_SCALE + world_offset.z;
    up.x = cam->up.x;
    up.y = cam->up.y;
    up.z = cam->up.z;
    C_MTXLookAt(view, &eye, &up, &look);
    MTXTrans(shift, world_offset.x, world_offset.y, world_offset.z);
    MTXConcat(view, shift, view);
    MTXConcat(view, scale, out);
}

/* Melee's cached GX state no longer holds after MP4's draws. */
static void gx_restore(void)
{
    mp4_camera_restore();
    GXSetCurrentMtx(0);
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
    HSD_StateInvalidate(-1);
    HSD_StateInitTev();
    HSD_ClearVtxDesc();
}

/* Melee's match camera renders GX link 6 five times a frame (fn_800301D0 in cm/camera.c: passes
 * 0 and 1, then 0, 1 and 2). MP4 draws its whole scene in one go and its draw-time hooks step
 * the minigame's effects (m440's debris fades in one), so it draws once a frame: on the first
 * pass 0, with the pass 2 call marking the end of a frame. */
static int draw_armed = 1;


/* mp4_view_begin: the next pass 0 draws again (a split screen draws once per view). */
int mp4_draw_rearm(void)
{
    draw_armed = 1;
    return 1;
}

static void draw(HSD_GObj* gobj, intptr_t pass)
{
    HU3DMODEL* model;
    Mtx view, shift;
    s16 i;
    (void) gobj;
    if (pass == 2) {
        draw_armed = 1;
        return;
    }
    if (pass != 0 || !draw_armed) {
        return;
    }
    draw_armed = 0;
    if (!views_active) {
        /* one view: the whole screen, following the camera the players are drawn with */
        MP4View views[HU3D_CAM_MAX];
        HSD_CObj* cobj = HSD_CObjGetCurrent();
        view_list(views);
        view_primary = views[0].primary;
        mp4_view_cameras = -1;
        full_viewport = cobj->viewport;
        full_scissor = cobj->scissor;
        full_aspect = HSD_CObjGetAspect(cobj);
    }
    HSD_CObjGetViewingMtx(HSD_CObjGetCurrent(), view);
    /* GX fog maps the depth buffer back to eye space with the projection's near and far: the
     * match camera's, not the Hu3D camera's (hsfman.c scales MP4's fog distances only) */
    mp4_fog_near = HSD_CObjGetNear(HSD_CObjGetCurrent());
    mp4_fog_far = HSD_CObjGetFar(HSD_CObjGetCurrent());
    /* MP4's origin sits at the offset in Melee's world */
    MTXTrans(shift, world_offset.x, world_offset.y, world_offset.z);
    MTXConcat(view, shift, view);
    mp4_camera_view = view;
    mp4_camera_scale = MP4_SCALE;
    match_view_save();
    mp4_gx_frame_begin();
    /* what Hu3DPreProc does at the start of MP4's frame, without its EFB clear colour */
    for (i = 0, model = Hu3DData; i < HU3D_MODEL_MAX; i++, model++) {
        if (model->hsf != NULL) {
            model->attr &= ~HU3D_ATTR_MOT_EXEC;
        }
    }
    Hu3DExec();
    WipeExecAlways();
    mp4_camera_view = NULL;
    mp4_camera_scale = 1.0f;
    gx_restore();
}

static void draw_gobj_create(void)
{
    /* Not in the item list: item loops read every entry there as an Item (party_draw.c). */
    HSD_GObj* gobj = GObj_Create(14, 15, 0);
    if (gobj != NULL) {
        GObj_SetupGXLink(gobj, draw, 6, 0);
    }
}

/* ---- a minigame ---- */

static s8 match_groups[PARTY_PLAYERS];

void mp4_match_groups(const s8 groups[4])
{
    memcpy(match_groups, groups, sizeof match_groups);
}

void mp4_match_begin(int overlay, float offset_x, float offset_y, float offset_z)
{
    int i;
    if (!mp4_available()) {
        return;
    }
    runtime_open();
    world_offset.x = offset_x;
    world_offset.y = offset_y;
    world_offset.z = offset_z;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        PlayerConfig* cfg = &GWPlayerCfg[i];
        int level = party.p[i].cpu_level;
        cfg->character = (s16) i;   /* its MP4 character only names its hidden model */
        cfg->pad_idx = (s16) i;
        cfg->iscom = party.p[i].slot_type == Gm_PKind_Cpu;
        /* MP4's easy, normal, hard, very hard from Melee's levels 1-9 */
        cfg->diff = (s16) (level >= 9 ? 3 : level >= 6 ? 2 : level >= 3 ? 1 : 0);
        cfg->group = match_groups[i];   /* mp4_match_groups, 0 unless the wrapper set them */
        memset(&GWPlayer[i], 0, sizeof GWPlayer[i]);
    }
    memset(match_groups, 0, sizeof match_groups);   /* the next match is four-player unless told */
    memset(&GWSystem, 0, sizeof GWSystem);
    GWSystem.player_curr = (s8) (party.mover >= 0 && party.mover < PARTY_PLAYERS ? party.mover : 0);
    memset(pad_prev, 0, sizeof pad_prev);
    mp4_pad_reset();
    WipeInit(RenderMode);   /* a match that ended mid-wipe would block the next one's fade-in */
    mp4_boot_reset();
    /* a match that ended by a quit left its overlay in the history: the new one returns to boot */
    omovlhisidx = 0;
    omOvlCallEx((OMOVL) overlay, 1, 0, 0);
    match_running = 1;
    draw_gobj_create();
    party_log("mp4: overlay %d starts", overlay);
}

/* MELEE_PARTY_MP4_LOG=1: a line a second on where the minigame is (the players' models, the
 * banner sequence, the overlay), for runs nobody watches. */
static void frame_log(void)
{
    static int enabled = -1;
    static u32 frames;
    HU3DMODEL* m;
    s16 i, models = 0;
    int p;
    if (enabled < 0) {
        const char* v = getenv("MELEE_PARTY_MP4_LOG");
        enabled = v != NULL && *v && *v != '0';
    }
    if (!enabled || (frames++ % 60) != 0) {
        return;
    }
    for (i = 0, m = Hu3DData; i < HU3D_MODEL_MAX; i++, m++) {
        models += m->hsf != NULL;
    }
    party_log("mp4: frame %u ovl %d models %d seq %d exit %d", frames - 1, (int) omcurovl, models,
              (int) MGSeqDoneCheck(), (int) omSysExitReq);
    if (((frames - 1) % 300) == 0) {
        HuMemHeapDump(HuMemHeapPtrGet(HEAP_SYSTEM), 0);   /* the system heap, then the data heap */
        HuMemHeapDump(HuMemHeapPtrGet(HEAP_DATA), 0);
    }
    {
        float eye[3], look[3], fov;
        if (mp4_camera(eye, look, &fov)) {
            party_log("mp4:   camera eye %.1f %.1f %.1f look %.1f %.1f %.1f fov %.1f near %.1f far %.1f",
                      eye[0], eye[1], eye[2], look[0], look[1], look[2], fov, Hu3DCamera[0].nnear,
                      Hu3DCamera[0].ffar);
        }
    }
    if (((frames - 1) % 300) == 0) {
        views_log = 1;
        for (p = 0; p < 4; p++) {
            s16 model = mp4_char_model(p);
            if (model >= 0 && Hu3DData[model].hsf != NULL) {
                party_log("mp4:   P%d model %d cameras %04X", p + 1, (int) model, (int) (u16) Hu3DData[model].cameraBit);
            }
        }
    }
    for (p = 0; p < 4; p++) {
        float x, y, z, yaw;
        if (mp4_player_pose(p, &x, &y, &z, &yaw)) {
            party_log("mp4:   P%d at %.1f %.1f %.1f yaw %.0f motion %d coins %d", p + 1, x, y, z,
                      yaw * (float) (180.0 / M_PI), (int) mp4_player_motion(p), mp4_player_coins(p));
        }
    }
}

void mp4_frame(void)
{
    int i;
    if (!match_running) {
        return;
    }
    /* what HuPadRead leaves of the last frame's pads: a press is new for one frame */
    for (i = 0; i < 4; i++) {
        HuPadBtnDown[i] = HuPadBtn[i] & ~pad_prev[i];
        pad_prev[i] = HuPadBtn[i];
    }
    GlobalCounter++;
    HuPrcCall(1);
    MGSeqMain();
    Hu3DAdvance();
    frame_log();
}

int mp4_match_over(void)
{
    return match_running && mp4_boot_reached();
}

void mp4_pad(int pad, u32 held, float stick_x, float stick_y, float substick_x, float substick_y,
             float trigger)
{
    if (pad < 0 || pad >= 4) {
        return;
    }
    HuPadBtn[pad] = (u16) (held & 0x0FFF & ~PAD_BUTTON_DIR);
    if (trigger >= 0.75f) {
        HuPadBtn[pad] |= PAD_BUTTON_TRIGGER_L;
    }
    HuPadStkX[pad] = (s8) (stick_x * 72.0f);
    HuPadStkY[pad] = (s8) (stick_y * 72.0f);
    HuPadSubStkX[pad] = (s8) (substick_x * 72.0f);
    HuPadSubStkY[pad] = (s8) (substick_y * 72.0f);
    HuPadTrigL[pad] = HuPadTrigR[pad] = (u8) (trigger * 255.0f);
}

int mp4_player_pose(int player, float* x, float* y, float* z, float* yaw)
{
    s16 model = mp4_char_model(player);
    HU3DMODEL* m;
    if (model < 0 || !match_running) {
        return 0;
    }
    m = &Hu3DData[model];
    if (m->hsf == NULL) {
        return 0;
    }
    *x = m->pos.x * MP4_SCALE + world_offset.x;
    *y = m->pos.y * MP4_SCALE + world_offset.y;
    *z = m->pos.z * MP4_SCALE + world_offset.z;
    *yaw = m->rot.y * (float) (M_PI / 180.0);
    return 1;
}

s32 mp4_player_motion(int player)
{
    s16 model = mp4_char_model(player);
    HU3DMODEL* m;
    if (model < 0 || Hu3DData[model].hsf == NULL) {
        return -1;
    }
    /* a motion being blended in (Hu3DMotionShiftSet) is the one the player is going into */
    m = &Hu3DData[model];
    return mp4_char_motion_data(m->motIdShift != HU3D_MOTID_NONE ? m->motIdShift : m->motId);
}

int mp4_player_shown(int player)
{
    s16 model = mp4_char_model(player);
    return model >= 0 && Hu3DData[model].hsf != NULL && (Hu3DData[model].attr & HU3D_ATTR_DISPOFF) == 0;
}

int mp4_player_coins(int player)
{
    return player >= 0 && player < 4 ? GWPlayer[player].coin_win : 0;
}

/* MP4's camera in Melee's world: the first that draws a player (the first camera before any
 * player exists). */
int mp4_camera(float eye[3], float look[3], float* fov)
{
    MP4View views[HU3D_CAM_MAX];
    HU3DCAMERA* cam;
    if (!match_running) {
        return 0;
    }
    view_list(views);
    cam = &Hu3DCamera[views[0].primary];
    if (cam->fov == -1.0f) {
        return 0;
    }
    eye[0] = cam->pos.x * MP4_SCALE + world_offset.x;
    eye[1] = cam->pos.y * MP4_SCALE + world_offset.y;
    eye[2] = cam->pos.z * MP4_SCALE + world_offset.z;
    look[0] = cam->target.x * MP4_SCALE + world_offset.x;
    look[1] = cam->target.y * MP4_SCALE + world_offset.y;
    look[2] = cam->target.z * MP4_SCALE + world_offset.z;
    *fov = cam->fov;
    return 1;
}

/* ---- the model check ---- */

static int hu3d_ready;
static void (*chained_start)(void);
static s16 debug_model = -1;

static void hu3d_open(void)
{
    if (!hu3d_ready) {
        hu3d_ready = 1;
        Hu3DInit();
    } else {
        Hu3DAllKill();
    }
    Hu3DCameraCreate(HU3D_CAM0);
    Hu3DCameraPerspectiveSet(HU3D_CAM0, 30.0f, 1.0f, 10000.0f, 1.2f);
    Hu3DGLightCreate(-1000.0f, 1000.0f, 1000.0f, 1.0f, -1.0f, -1.0f, 0xFF, 0xFF, 0xFF);
    Hu3DGLightInfinitytSet(0);
}

static void debug_draw(HSD_GObj* gobj, intptr_t pass)
{
    HU3DMODEL* model;
    Mtx view;
    s16 i;
    (void) gobj;
    (void) pass;
    HSD_CObjGetViewingMtx(HSD_CObjGetCurrent(), view);
    mp4_camera_view = view;
    match_view_save();
    mp4_gx_frame_begin();
    for (i = 0, model = Hu3DData; i < HU3D_MODEL_MAX; i++, model++) {
        if (model->hsf != NULL) {
            model->attr &= ~HU3D_ATTR_MOT_EXEC;
        }
    }
    Hu3DExec();
    mp4_camera_view = NULL;
    gx_restore();
}

static s32 knob_num(const char* text)
{
    char* end;
    long v = strtol(text, &end, 0);
    return end == text ? -1 : (s32) v;
}

static void debug_start(void)
{
    const char* spec = getenv("MELEE_PARTY_MP4_MODEL");
    const char* scale_text = getenv("MELEE_PARTY_MP4_SCALE");
    const char* y_text = getenv("MELEE_PARTY_MP4_Y");
    char archive[64];
    const char* colon;
    const char* second;
    s32 file, motion_file = -1, dir;
    float scale = scale_text != NULL && *scale_text ? (float) atof(scale_text) : 0.1f;
    void* data;
    HSD_GObj* gobj;

    if (chained_start != NULL) {
        chained_start();
    }
    debug_model = -1;
    colon = strchr(spec, ':');
    if (colon == NULL || colon - spec >= (long) sizeof archive) {
        OSReport("[party] mp4: MELEE_PARTY_MP4_MODEL is <archive>:<file>[:<motion file>]\n");
        return;
    }
    memcpy(archive, spec, colon - spec);
    archive[colon - spec] = 0;
    file = knob_num(colon + 1);
    second = strchr(colon + 1, ':');
    if (second != NULL) {
        motion_file = knob_num(second + 1);
    }
    if (!mp4_available()) {
        return;
    }
    dir = mp4_data_dir(archive);
    if (dir < 0 || file < 0) {
        OSReport("[party] mp4: no archive %s in MP4's data directory\n", archive);
        return;
    }
    hu3d_open();
    data = HuDataSelHeapReadNum(dir | file, MEMORY_DEFAULT_NUM, HEAP_DATA);
    if (data == NULL) {
        OSReport("[party] mp4: no file %d in %s\n", file, archive);
        return;
    }
    debug_model = Hu3DModelCreate(data);
    if (debug_model < 0) {
        return;
    }
    Hu3DModelScaleSet(debug_model, scale, scale, scale);
    Hu3DModelPosSet(debug_model, 0.0f, y_text != NULL ? (float) atof(y_text) : 0.0f, 0.0f);
    Hu3DModelAttrSet(debug_model, HU3D_MOTATTR_LOOP);
    Hu3DFogClear();   /* a model's own fog is in MP4's distances, not the match camera's */
    if (motion_file >= 0) {
        void* motion = HuDataSelHeapReadNum(dir | motion_file, MEMORY_DEFAULT_NUM, HEAP_DATA);
        s16 motion_id = motion != NULL ? Hu3DMotionCreate(motion) : -1;
        if (motion_id >= 0) {
            Hu3DMotionSet(debug_model, motion_id);
        }
    }
    OSReport("[party] mp4: model %s:%d (%d objects, %d motions)%s\n", archive, file,
             Hu3DData[debug_model].hsf->objectNum, Hu3DData[debug_model].hsf->motionNum,
             motion_file >= 0 ? ", with a motion" : "");
    {
        /* its vertices' extent, a check of the loaded data (MP4's units) */
        HSFDATA* hsf = Hu3DData[debug_model].hsf;
        HSFOBJECT* obj = hsf->object;
        float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
        u32 total = 0;
        s32 i, j;
        for (i = 0; i < hsf->objectNum; i++, obj++) {
            HuVecF* v;
            if (obj->type != HSF_OBJ_MESH || obj->mesh.vertex == NULL) {
                continue;
            }
            v = obj->mesh.vertex->data;
            for (j = 0; j < (s32) obj->mesh.vertex->count; j++, v++, total++) {
                float c[3] = { v->x, v->y, v->z };
                int k;
                for (k = 0; k < 3; k++) {
                    lo[k] = c[k] < lo[k] ? c[k] : lo[k];
                    hi[k] = c[k] > hi[k] ? c[k] : hi[k];
                }
            }
            if (obj->mesh.vertex->count > 0 && i < 3) {
                v = obj->mesh.vertex->data;
                OSReport("[party] mp4: object %d \"%s\" %u vertices, first %g %g %g\n", i,
                         obj->name != NULL ? obj->name : "", obj->mesh.vertex->count, v->x, v->y, v->z);
            }
        }
        OSReport("[party] mp4: %u vertices in [%g %g %g]..[%g %g %g]\n", total, lo[0], lo[1], lo[2],
                 hi[0], hi[1], hi[2]);
    }

    gobj = GObj_Create(14, 15, 0);
    if (gobj != NULL) {
        GObj_SetupGXLink(gobj, debug_draw, 6, 0);
    }
}

void mp4_debug_setup(StartMeleeData* start)
{
    const char* spec = getenv("MELEE_PARTY_MP4_MODEL");
    if (spec == NULL || *spec == 0) {
        return;
    }
    chained_start = start->rules.on_match_start;
    start->rules.on_match_start = debug_start;
}
