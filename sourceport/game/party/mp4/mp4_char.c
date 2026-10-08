/* Melee Party, Mario Party 4 runtime: the players' models (chrman.c in MP4). A minigame creates
 * each player's character model and its motions and drives them every frame: where it stands,
 * which way it faces, what it is doing. Here the model is the real one from the disc, loaded so
 * that every motion call works as in MP4, but never drawn: the Melee fighter stands where it
 * stands (mp4_char_model, read by the minigame's wrapper). Nothing else of chrman.c (eyes, voices,
 * effects, the per-character process) is here. */
#include <dolphin/os.h>

#include "game/chrman.h"
#include "game/data.h"
#include "game/hu3d.h"
#include "game/memory.h"
#include "mp4.h"

static const s32 charDirTbl[CHARNO_MAX][3] = {
    { DATADIR_MARIOMDL0, DATADIR_MARIOMDL1, DATADIR_MARIOMOT },
    { DATADIR_LUIGIMDL0, DATADIR_LUIGIMDL1, DATADIR_LUIGIMOT },
    { DATADIR_PEACHMDL0, DATADIR_PEACHMDL1, DATADIR_PEACHMOT },
    { DATADIR_YOSHIMDL0, DATADIR_YOSHIMDL1, DATADIR_YOSHIMOT },
    { DATADIR_WARIOMDL0, DATADIR_WARIOMDL1, DATADIR_WARIOMOT },
    { DATADIR_DONKEYMDL0, DATADIR_DONKEYMDL1, DATADIR_DONKEYMOT },
    { DATADIR_DAISYMDL0, DATADIR_DAISYMDL1, DATADIR_DAISYMOT },
    { DATADIR_WALUIGIMDL0, DATADIR_WALUIGIMDL1, DATADIR_WALUIGIMOT },
};

static s16 char_model[CHARNO_MAX];
static int chars_open;
/* Each motion's MP4 data number (archive and file), by Hu3D motion id: how the wrappers tell
 * which motion a player is in. */
static s32 motion_data[HU3D_MOTION_MAX];

extern u8 mp4_model_hidden[HU3D_MODEL_MAX];

static void chars_init(void)
{
    s16 i;
    if (!chars_open) {
        chars_open = 1;
        for (i = 0; i < CHARNO_MAX; i++) {
            char_model[i] = HU3D_MODELID_NONE;
        }
    }
}

HU3DMODELID CharModelCreate(s16 charNo, s16 model)
{
    s32 dataNum;
    void *data;
    s16 modelId;
    chars_init();
    if (charNo < 0 || charNo >= CHARNO_MAX) {
        return HU3D_MODELID_NONE;
    }
    if (char_model[charNo] != HU3D_MODELID_NONE) {
        Hu3DModelKill(char_model[charNo]);
        char_model[charNo] = HU3D_MODELID_NONE;
    }
    if (model & CHAR_MODEL0) {
        dataNum = charDirTbl[charNo][0];
    } else if (model & CHAR_MODEL1) {
        dataNum = charDirTbl[charNo][1];
    } else if (model & CHAR_MODEL2) {
        dataNum = charDirTbl[charNo][1] | 1;
    } else {
        dataNum = charDirTbl[charNo][1] | 2;
    }
    data = HuDataSelHeapReadNum(dataNum, MEMORY_DEFAULT_NUM, HEAP_DATA);
    if (data == NULL) {
        OSReport("[party] mp4: no model for character %d\n", charNo);
        return HU3D_MODELID_NONE;
    }
    modelId = Hu3DModelCreate(data);
    if (modelId != HU3D_MODELID_NONE) {
        mp4_model_hidden[modelId] = 1;   /* the Melee fighter is drawn instead */
    }
    char_model[charNo] = modelId;
    return modelId;
}

/* As chrman.c: a motion number with no archive, or with any character's motion archive, is a file
 * of this character's own motion archive (m438 names its motions by file alone); any other is
 * read as it is (m440's blast reactions). The motion is bound to the player's model
 * (Hu3DJointMotion), as every character motion is. */
HU3DMOTID CharMotionCreate(s16 charNo, s32 data_num)
{
    void *data;
    HU3DMOTID motId;
    s16 model = mp4_char_model(charNo);
    u32 dir = (u32) data_num & 0xFFFF0000u;
    s16 i;
    if (model == HU3D_MODELID_NONE) {
        return HU3D_MOTID_NONE;
    }
    for (i = 0; i < CHARNO_MAX; i++) {
        if (dir == (u32) charDirTbl[i][2]) {
            break;
        }
    }
    if (i != CHARNO_MAX || dir == 0) {
        data_num = (data_num & 0xFFFF) | charDirTbl[charNo][2];
    }
    data = HuDataSelHeapReadNum(data_num, MEMORY_DEFAULT_NUM, HEAP_DATA);
    if (data == NULL) {
        OSReport("[party] mp4: no motion %x for character %d\n", (u32) data_num, charNo);
        return HU3D_MOTID_NONE;
    }
    motId = Hu3DJointMotion(model, data);
    if (motId >= 0 && motId < HU3D_MOTION_MAX) {
        motion_data[motId] = data_num;
    }
    return motId;
}

s32 mp4_char_motion_data(s16 motId)
{
    return motId >= 0 && motId < HU3D_MOTION_MAX && Hu3DMotion[motId].hsf != NULL ? motion_data[motId] : -1;
}

void CharMotionDataClose(s16 charNo)
{
    (void) charNo;
}

void CharModelDataClose(s16 charNo)
{
    (void) charNo;
}

void CharMotionVoiceOnSet(s16 charNo, s16 motion, BOOL voiceOn)
{
    (void) charNo;
    (void) motion;
    (void) voiceOn;
}

void CharModelKill(s16 charNo)
{
    s16 i;
    chars_init();
    for (i = 0; i < CHARNO_MAX; i++) {
        if (charNo == -1 || charNo == i) {
            /* the models themselves go with Hu3DAllKill (omOvlKill): forget them */
            char_model[i] = HU3D_MODELID_NONE;
        }
    }
}

/* The motion calls a walking minigame makes on its players (m438): on the hidden model. */
void CharMotionSet(s16 charNo, HU3DMOTID motId)
{
    s16 model = mp4_char_model(charNo);
    if (model >= 0 && motId >= 0) {
        Hu3DMotionSet(model, motId);
    }
}

void CharMotionShiftSet(s16 charNo, HU3DMOTID motId, float start, float end, u32 attr)
{
    s16 model = mp4_char_model(charNo);
    if (model >= 0 && motId >= 0) {
        Hu3DMotionShiftSet(model, motId, start, end, attr);
    }
}

s16 CharMotionShiftIDGet(s16 charNo)
{
    s16 model = mp4_char_model(charNo);
    return model >= 0 ? Hu3DMotionShiftIDGet(model) : HU3D_MOTID_NONE;
}

s32 CharMotionEndCheck(s16 charNo)
{
    s16 model = mp4_char_model(charNo);
    return model >= 0 ? Hu3DMotionEndCheck(model) : 1;
}

float CharMotionMaxTimeGet(s16 charNo)
{
    s16 model = mp4_char_model(charNo);
    return model >= 0 ? Hu3DMotionMaxTimeGet(model) : 0.0f;
}

void CharModelStepFxSet(s16 charNo, s32 stepFx)
{
    (void) charNo;
    (void) stepFx;
}

s32 CharFXPlayPos(s16 charNo, s16 seId, Vec *pos)
{
    (void) charNo; (void) seId; (void) pos;
    return -1;
}

/* chrman.c: a bound motion's number, for the eyes and voices there and for mp4_player_motion
 * here (m441 names three of its own motions as the walk and the run). */
void CharMotionNoSet(s16 charNo, HU3DMOTID motId, s32 motNo)
{
    u32 dir = (u32) motNo & 0xFFFF0000u;
    s16 i;
    if (charNo < 0 || charNo >= CHARNO_MAX || motId < 0 || motId >= HU3D_MOTION_MAX) {
        return;
    }
    for (i = 0; i < CHARNO_MAX; i++) {
        if (dir == (u32) charDirTbl[i][2]) {
            break;
        }
    }
    if (i != CHARNO_MAX || dir == 0) {
        motNo = (motNo & 0xFFFF) | charDirTbl[charNo][2];
    }
    motion_data[motId] = motNo;
}

/* chrman.c: the bone an item hooks to (0 the right hand, 1 the left, 2 and 3 the feet, 4 the
 * body), the same names for every character. m441 hooks the net to the hidden model's hand. */
char* CharModelItemHookGet(s16 charNo, s16 model, s16 hookNo)
{
    static char* const names[5] = { "a-itemhook-r", "a-itemhook-l", "a-itemhook-fr", "a-itemhook-fl",
                                    "a-itemhook-body" };
    (void) charNo;
    (void) model;
    return names[hookNo >= 0 && hookNo < 5 ? hookNo : 0];
}

void CharModelLayerSetAll2(s16 layerNo)
{
    CharEffectLayerSet(layerNo);
}

void CharEffectLayerSet(s16 layerNo)
{
    (void) layerNo;   /* no character effects here (chrman.c's) */
}

s16 mp4_char_model(int charNo)
{
    chars_init();
    if (charNo < 0 || charNo >= CHARNO_MAX) {
        return -1;
    }
    return char_model[charNo];
}
