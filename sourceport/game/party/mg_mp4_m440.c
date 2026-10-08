/* Melee Party: Bowser's Bigger Blast, Mario Party 4's m440, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m440), run by the MP4 runtime (mp4/mp4_party.c): its
 * stage, Bowser, the detonators, its rules, timings, CPUs and banners. The four players are MP4's
 * player objects, each driving a hidden MP4 character model; the Melee fighters stand where those
 * models stand and face their way (mp4_player_pose), and play Melee's nearest animation for the
 * MP4 motion the model is in (mp4_player_motion): a walk, a run, a crouch at the plunger, a taunt,
 * a hit when the blast goes off. Nobody walks by the stick in this game, so the fighters only
 * follow. The match ends when the minigame returns to MP4's boot overlay; the placements are the
 * elimination order, which m440 writes to the players' coin_win (3 for the first blasted, 0 for
 * the survivor).
 *
 * The Melee side: Final Destination, with each fighter colliding at a fixed x of its own and the
 * MP4 world drawn 600 units to the side, clear of the stage's model. */
#include <math.h>
#include <string.h>

#include <melee/cm/camera.h>
#include <melee/cm/types.h>
#include <melee/ft/fighter.h>
#include <melee/ft/forward.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>

#include "mp4/mp4.h"
#include "party.h"

#define WORLD_X 600.0f

extern Camera game_camera;

/* m440's motions (m440/object.c, its motion table): files of mariomot.bin, which every MP4
 * character plays, and the blast reaction, a file of m440.bin (MP4 archive 0x47) per character. */
#define MOT_FILE(data) ((data) & 0xFFFF)
#define MOT_FROM_M440(data) (((data) >> 16) == 0x47)
enum { MOT_IDLE = 0x00, MOT_WALK = 0x02, MOT_RUN = 0x03, MOT_PLUNGER = 0x38, MOT_VOICE = 0x48,
       MOT_WIN = 0x17, MOT_TURN = 0x36 };

/* A blasted fighter collides this high over the stage, so it stays in its fly animation. */
#define FLY_HEIGHT 100.0f

static struct {
    int ended;
    float look[3], eye[3], fov;
    struct {
        s32 motion;   /* the MP4 motion seen last frame, a data number */
        int flying;   /* blasted off the platform: kept airborne until the model is hidden */
    } p[PARTY_PLAYERS];
} bb;

static int human(int slot)
{
    return party.p[slot].slot_type != Gm_PKind_Cpu;
}

/* MP4's camera on the Melee match camera. */
static void camera_frame(void)
{
    HSD_CObj* cobj;
    if (!Camera_80030178()) {
        Camera_8003006C();
    }
    if (mp4_camera(bb.eye, bb.look, &bb.fov)) {
        cm_80453004.free_int_pos.x = bb.look[0];
        cm_80453004.free_int_pos.y = bb.look[1];
        cm_80453004.free_int_pos.z = bb.look[2];
        cm_80453004.free_eye_pos.x = bb.eye[0];
        cm_80453004.free_eye_pos.y = bb.eye[1];
        cm_80453004.free_eye_pos.z = bb.eye[2];
        cm_80453004.free_fov = bb.fov;
    }
    cobj = game_camera.gobj != NULL ? game_camera.gobj->hsd_obj : NULL;
    if (cobj != NULL) {
        HSD_CObjSetNear(cobj, 1.0f);
        HSD_CObjSetFar(cobj, 16384.0f);
    }
}

static void bb_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    bb.ended = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        bb.p[i].motion = -1;
        bb.p[i].flying = 0;
    }
    mp4_match_begin(mp4_overlay_m440, WORLD_X, 0.0f, 0.0f);
}

static void bb_frame(void)
{
    mp4_frame();
    camera_frame();
    if (!bb.ended && mp4_match_over()) {
        int i;
        bb.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m440: P%d order %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void bb_setup(StartMeleeData* start)
{
    memset(&bb, 0, sizeof bb);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = bb_start;
    start->rules.on_frame_start = bb_frame;
}

/* Fighter_procInput: a human's pad goes to MP4, and Melee gets the stick and buttons that put
 * the fighter in the nearest animation to its MP4 player's motion (the fighter never moves by
 * them: bb_fighter_map pins it). The blast is a hit: a flinch for the players watching, a fly for
 * the one blasted (its model rises; the fighter goes airborne and stays so, see FLY_HEIGHT). */
static void bb_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 motion;
    int began;
    float x, y, z, yaw;
    if (slot >= 0 && slot < PARTY_PLAYERS && !fp->is_sub_fighter && human(slot)) {
        mp4_pad(slot, fp->input.held_buttons[0], fp->input.lstick[0].x, fp->input.lstick[0].y,
                fp->input.cstick[0].x, fp->input.cstick[0].y, fp->input.triggers[0]);
    }
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &fp->cur_pos);
    fp->input.lstick[0].x = fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    fp->input.held_buttons[0] = 0;
    if (slot < 0 || slot >= PARTY_PLAYERS || fp->is_sub_fighter) {
        return;
    }
    motion = mp4_player_motion(slot);
    began = motion != bb.p[slot].motion;
    bb.p[slot].motion = motion;
    if (motion < 0) {
        return;
    }
    if (MOT_FROM_M440(motion)) {
        if (began) {
            Fighter_ChangeMotionState(fp->gobj, ftCo_MS_DamageN3, 0, 0.0f, 1.0f, 0.0f, NULL);
        }
        if (!bb.p[slot].flying && mp4_player_pose(slot, &x, &y, &z, &yaw) && y > 3.0f) {
            bb.p[slot].flying = 1;
            ftCommon_8007D5D4(fp);
            Fighter_ChangeMotionState(fp->gobj, ftCo_MS_DamageFlyHi, 0, 0.0f, 1.0f, 0.0f, NULL);
        }
        return;
    }
    switch (MOT_FILE(motion)) {
    case MOT_WALK:
        fp->input.lstick[0].x = 0.5f * fp->facing_dir;
        break;
    case MOT_RUN:
        fp->input.lstick[0].x = fp->facing_dir;
        break;
    case MOT_PLUNGER:
        fp->input.lstick[0].y = -1.0f;
        break;
    case MOT_VOICE:
    case MOT_WIN:
        if (began) {
            fp->input.held_buttons[0] = HSD_PAD_DPADUP;
        }
        break;
    default:
        break;
    }
}

/* Where it collides on Final Destination: a lane of its own, in the air once blasted. */
static void bb_fighter_map(Fighter* fp)
{
    int slot = fp->player_idx;
    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    fp->cur_pos.x = -45.0f + 30.0f * (float) slot;
    fp->cur_pos.z = 0.0f;
    if (bb.p[slot].flying) {
        fp->cur_pos.y = FLY_HEIGHT;
    }
}

/* After collision: drawn where its MP4 player stands, turned its way; out of sight once the game
 * hid the player. */
static void bb_fighter_drawn(Fighter* fp)
{
    int slot = fp->player_idx;
    Vec3 pos;
    float x, y, z, yaw;
    if (slot < 0 || slot >= PARTY_PLAYERS || !mp4_player_pose(slot, &x, &y, &z, &yaw)) {
        return;
    }
    pos.x = x;
    pos.y = y + fp->cur_pos.y - (bb.p[slot].flying ? FLY_HEIGHT : 0.0f);
    pos.z = z;
    if (!mp4_player_shown(slot)) {
        pos.y = -4000.0f;
    }
    if (fp->is_sub_fighter) {
        pos.x -= sinf(yaw) * 3.0f;
        pos.z -= cosf(yaw) * 3.0f;
    }
    ftPartSetRotY(fp, 0, yaw);
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &pos);
}

static float bb_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* m440 writes each player's elimination order to coin_win as it goes: 3 for the first blasted,
 * then 2 and 1, and 0 (never set) for the survivor. That is the placement. */
static void bb_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int order = mp4_player_coins(i);
        place[i] = (s8) (order < 0 ? 0 : order > 3 ? 3 : order);
    }
}

const PartyMinigame mg_mp4_m440 = {
    "Bowser's Bigger Blast", "bigger-blast", bb_setup, bb_fighter_input, bb_result, 0, NULL,
    St_Kind_Last, -1, bb_fighter_map, bb_fighter_drawn, bb_knockback, 1, NULL, NULL,
};
