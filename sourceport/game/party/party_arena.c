/* Melee Party: a Melee fighter as the player of a Mario Party 4 minigame (party_arena.h).
 *
 * The Melee side is Final Destination, with each fighter colliding on a lane of its own (a fixed
 * x) and the MP4 world drawn to the side, clear of the stage's model: the fighter never moves
 * along the stage, it is drawn where its MP4 player is. Melee's physics still run on it, and the
 * speed they give it along its lane is the speed its MP4 player walks at. */
#include <math.h>
#include <string.h>

#include <melee/cm/camera.h>
#include <melee/cm/types.h>
#include <melee/ft/fighter.h>
#include <melee/ft/forward.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/kinds/ftCommon/ftCo_Fall.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

extern Camera game_camera;

/* The character motions every MP4 character has (mariomot.bin), by file. */
enum { MOT_WALK = 0x02, MOT_RUN = 0x03, MOT_WIN = 0x17, MOT_VOICE = 0x48, MOT_WIN2 = 0x4B };

static struct {
    float offset_y;   /* where MP4's origin sits in Melee's world */
    struct {
        int drive;        /* the player walks by Melee's speed this frame */
        float speed;      /* Melee's forward speed, in MP4's units a frame */
        int airborne;     /* the minigame holds the player off the floor: Melee's y is its y */
        float pinned_y;   /* the height the fighter was set to this frame (0: Melee's own) */
        s32 motion;       /* the MP4 motion seen last frame, a data number */
    } p[PARTY_PLAYERS];
} a;

static int slot_of(Fighter* fp)
{
    int slot = fp->player_idx;
    return slot >= 0 && slot < PARTY_PLAYERS && !fp->is_sub_fighter ? slot : -1;
}

static int human(int slot)
{
    return party.p[slot].slot_type != Gm_PKind_Cpu;
}

void party_arena_begin(int overlay, float offset_x, float offset_y, float offset_z)
{
    int i;
    memset(&a, 0, sizeof a);
    a.offset_y = offset_y;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        a.p[i].motion = -1;
    }
    mp4_match_begin(overlay, offset_x, offset_y, offset_z);
}

/* MP4's camera on the Melee match camera. */
static void camera_frame(void)
{
    HSD_CObj* cobj;
    float look[3], eye[3], fov;
    if (!Camera_80030178()) {
        Camera_8003006C();
    }
    if (mp4_camera(eye, look, &fov)) {
        cm_80453004.free_int_pos.x = look[0];
        cm_80453004.free_int_pos.y = look[1];
        cm_80453004.free_int_pos.z = look[2];
        cm_80453004.free_eye_pos.x = eye[0];
        cm_80453004.free_eye_pos.y = eye[1];
        cm_80453004.free_eye_pos.z = eye[2];
        cm_80453004.free_fov = fov;
    }
    cobj = game_camera.gobj != NULL ? game_camera.gobj->hsd_obj : NULL;
    if (cobj != NULL) {
        HSD_CObjSetNear(cobj, 1.0f);
        HSD_CObjSetFar(cobj, 16384.0f);
    }
}

void party_arena_frame(void)
{
    mp4_frame();
    camera_frame();
}

int party_arena_input_begin(Fighter* fp)
{
    int slot = fp->player_idx;
    if (slot >= 0 && slot < PARTY_PLAYERS && !fp->is_sub_fighter && human(slot)) {
        mp4_pad(slot, fp->input.held_buttons[0], fp->input.lstick[0].x, fp->input.lstick[0].y,
                fp->input.cstick[0].x, fp->input.cstick[0].y, fp->input.triggers[0]);
    }
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &fp->cur_pos);
    fp->input.lstick[0].x = fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    fp->input.held_buttons[0] = 0;
    return slot_of(fp) >= 0;
}

void party_arena_walk(Fighter* fp, s16 stick_x, s16 stick_y)
{
    int slot = slot_of(fp);
    float mag;
    if (slot < 0) {
        return;
    }
    a.p[slot].drive = 1;
    a.p[slot].airborne = 0;
    a.p[slot].motion = mp4_player_motion(slot);
    mag = sqrtf((float) (stick_x * stick_x + stick_y * stick_y)) / 72.0f;
    if (mag > 1.0f) {
        mag = 1.0f;
    }
    /* always a push forward: the fighter never turns round, the player's yaw turns instead */
    fp->input.lstick[0].x = mag * fp->facing_dir;
}

void party_arena_follow(Fighter* fp, int airborne)
{
    int slot = slot_of(fp);
    s32 motion;
    int began;
    if (slot < 0) {
        return;
    }
    a.p[slot].drive = 0;
    if (airborne && !a.p[slot].airborne && fp->ground_or_air == GA_Ground) {
        ftCommon_8007D5D4(fp);
        ftCo_Fall_Enter(fp->gobj);
    }
    a.p[slot].airborne = airborne;
    motion = mp4_player_motion(slot);
    began = motion != a.p[slot].motion;
    a.p[slot].motion = motion;
    if (motion < 0) {
        return;
    }
    switch (motion & 0xFFFF) {
    case MOT_WALK:
        fp->input.lstick[0].x = 0.5f * fp->facing_dir;
        break;
    case MOT_RUN:
        fp->input.lstick[0].x = fp->facing_dir;
        break;
    case MOT_VOICE:
    case MOT_WIN:
    case MOT_WIN2:   /* m412's winners */
        if (began) {
            fp->input.held_buttons[0] = HSD_PAD_DPADUP;
        }
        break;
    default:
        break;
    }
}

void party_arena_hit(Fighter* fp)
{
    int slot = slot_of(fp);
    if (slot < 0) {
        return;
    }
    a.p[slot].drive = 0;
    a.p[slot].airborne = 1;
    ftCommon_8007D5D4(fp);
    Fighter_ChangeMotionState(fp->gobj, ftCo_MS_DamageFlyHi, 0, 0.0f, 1.0f, 0.0f, NULL);
}

/* The minigame's player code, for its walking speed this frame (mp4.h). */
float mp4_player_speed(int player, float speed)
{
    if (player < 0 || player >= PARTY_PLAYERS || !a.p[player].drive) {
        return speed;
    }
    return a.p[player].speed;
}

void party_arena_map(Fighter* fp)
{
    int slot = fp->player_idx;
    float x, y, z, yaw, v;
    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    fp->cur_pos.x = -45.0f + 30.0f * (float) slot;
    fp->cur_pos.z = 0.0f;
    if (fp->is_sub_fighter) {
        return;
    }
    v = fp->ground_or_air == GA_Ground ? fp->gr_vel * fp->facing_dir : fp->self_vel.x * fp->facing_dir;
    a.p[slot].speed = v / MP4_SCALE;
    a.p[slot].pinned_y = 0.0f;
    if (a.p[slot].airborne && mp4_player_pose(slot, &x, &y, &z, &yaw)) {
        a.p[slot].pinned_y = y - a.offset_y;
        fp->cur_pos.y = a.p[slot].pinned_y;
    }
}

void party_arena_drawn(Fighter* fp)
{
    int slot = fp->player_idx;
    Vec3 pos;
    float x, y, z, yaw;
    if (slot < 0 || slot >= PARTY_PLAYERS || !mp4_player_pose(slot, &x, &y, &z, &yaw)) {
        return;
    }
    pos.x = x;
    pos.y = y + fp->cur_pos.y - a.p[slot].pinned_y;
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
