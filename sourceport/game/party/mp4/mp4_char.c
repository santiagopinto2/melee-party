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

HU3DMOTID CharMotionCreate(s16 charNo, s32 data_num)
{
    void *data;
    (void) charNo;
    HU3DMOTID motId;
    data = HuDataSelHeapReadNum(data_num, MEMORY_DEFAULT_NUM, HEAP_DATA);
    if (data == NULL) {
        return HU3D_MOTID_NONE;
    }
    motId = Hu3DMotionCreate(data);
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

s16 mp4_char_model(int charNo)
{
    chars_init();
    if (charNo < 0 || charNo >= CHARNO_MAX) {
        return -1;
    }
    return char_model[charNo];
}
