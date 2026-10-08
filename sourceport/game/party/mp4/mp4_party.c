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

void mp4_camera_restore(void)
{
    HSD_CObj* cobj = HSD_CObjGetCurrent();
    GXRenderModeObj* rmode = HSD_VIGetRenderMode();
    f32 xs = (f32) rmode->fbWidth / (f32) rmode->viWidth;
    f32 ys = (f32) rmode->efbHeight / (f32) rmode->viHeight;
    f32 l, t, r, b;
    GXSetProjectionv(match_projection);
    if (cobj == NULL) {
        return;
    }
    l = cobj->viewport.xmin * xs;
    r = cobj->viewport.xmax * xs;
    t = cobj->viewport.ymin * ys;
    b = cobj->viewport.ymax * ys;
    GXSetViewport(l, t, r - l, b - t, 0.0f, 1.0f);
    l = cobj->scissor.left * xs;
    r = cobj->scissor.right * xs;
    t = cobj->scissor.top * ys;
    b = cobj->scissor.bottom * ys;
    GXSetScissor((u32) l, (u32) t, (u32) (r - l), (u32) (b - t));
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
    HSD_CObjGetViewingMtx(HSD_CObjGetCurrent(), view);
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
        cfg->group = 0;
        memset(&GWPlayer[i], 0, sizeof GWPlayer[i]);
    }
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
    {
        float eye[3], look[3], fov;
        if (mp4_camera(eye, look, &fov)) {
            party_log("mp4:   camera eye %.1f %.1f %.1f look %.1f %.1f %.1f fov %.1f near %.1f far %.1f",
                      eye[0], eye[1], eye[2], look[0], look[1], look[2], fov, Hu3DCamera[0].nnear,
                      Hu3DCamera[0].ffar);
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

/* MP4's camera (the first Hu3D camera) in Melee's world. */
int mp4_camera(float eye[3], float look[3], float* fov)
{
    HU3DCAMERA* cam = &Hu3DCamera[0];
    if (!match_running || cam->fov == -1.0f) {
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
