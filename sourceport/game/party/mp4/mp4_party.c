/* Melee Party, Mario Party 4 runtime: MP4's models in a Melee match.
 *
 * MP4's own Hu3DExec draws every model it holds, from a GObj on the same render link the party's
 * flat shapes use (party_draw.c), so the match camera draws them with the scene, after the stage.
 * Its camera takes the match camera's view (mp4_camera_view in hsfman.c) and keeps Melee's
 * projection and viewport.
 *
 * MELEE_PARTY_MP4_MODEL=<archive>:<file>[:<motion file>] draws one model from the MP4 disc in
 * every party match, at the stage's centre: the archive by its name in the disc's data directory
 * (m440, or data/m440.bin), the file by its number in it (decimal, or 0x hex), and optionally a
 * motion from the same archive played on a loop. MELEE_PARTY_MP4_SCALE (default 0.1, MP4's units
 * against Melee's) and MELEE_PARTY_MP4_Y (default 0) place it. Without an MP4 disc, or with the
 * knob unset, nothing here runs. */
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

#include "game/data.h"
#include "game/hu3d.h"
#include "mp4.h"

char* getenv(const char* name);
double atof(const char* s);
void HSD_ClearVtxDesc(void);
void mp4_gx_frame_begin(void);
extern MtxPtr mp4_camera_view;

static int hu3d_ready;
static void (*chained_start)(void);
static s16 debug_model = -1;

/* Hu3DInit once, the first time MP4 models are wanted; between matches the models go. */
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

static void draw(HSD_GObj* gobj, intptr_t pass)
{
    HU3DMODEL* model;
    Mtx view;
    s16 i;
    (void) gobj;
    (void) pass;
    HSD_CObjGetViewingMtx(HSD_CObjGetCurrent(), view);
    mp4_camera_view = view;
    mp4_gx_frame_begin();
    /* what Hu3DPreProc does at the start of MP4's frame, without its EFB clear colour */
    for (i = 0, model = Hu3DData; i < HU3D_MODEL_MAX; i++, model++) {
        if (model->hsf != NULL) {
            model->attr &= ~HU3D_ATTR_MOT_EXEC;
        }
    }
    Hu3DExec();
    mp4_camera_view = NULL;
    /* Melee's cached GX state no longer holds after MP4's draws */
    GXSetCurrentMtx(0);
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
    HSD_StateInvalidate(-1);
    HSD_StateInitTev();
    HSD_ClearVtxDesc();
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

    /* Not in the item list: item loops read every entry there as an Item (party_draw.c). */
    gobj = GObj_Create(14, 15, 0);
    if (gobj != NULL) {
        GObj_SetupGXLink(gobj, draw, 6, 0);
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
