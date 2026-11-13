/* Melee Party: Paratrooper Plunge, Mario Party 4's m408, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m408), run by the MP4 runtime (mp4/mp4_party.c): the plane,
 * the jump, the sky full of coins, coin bags and things to dodge on the way down, the landing,
 * its rules, CPUs, coin counters and banners. The four players are MP4's player objects, each
 * driving a hidden MP4 character model under its parachute; the Melee fighters are drawn where
 * those models are, through party_arena.c for the pads and the motions. Nobody walks in this
 * game: the stick steers the player through the sky as MP4 reads it, with MP4's own drift and
 * speed (the skydive is the game), so the fighters follow their players throughout, in Melee's
 * fall the whole way down. A hit spins the player and shrinks its parachute, and sends its
 * fighter tumbling; the fourth hit takes the parachute and puts the player out. MP4's characters
 * are small in this game (its player models at 0.15, 0.075 in the plane), so each fighter is drawn
 * at its MP4 player's scale.
 *
 * The match ends when the minigame returns to MP4's boot overlay. MP4 lets each player keep the
 * coins it caught (GWPlayerCoinCollectAdd: it never sets coin_win here), so the places follow the
 * coins: more coins place higher, equal coins share a place.
 *
 * The Melee side: Final Destination, each fighter colliding on a lane of its own with the MP4
 * world drawn 600 units to the side. While its player is in the air the fighter is held in the air
 * at the player's height, but no higher than HOVER_Y (the dive starts 1200 units up, past the top
 * blast zone); it is drawn at the player's real height. */
#include <math.h>
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/forward.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/kinds/ftCommon/ftCo_Fall.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f
#define HOVER_Y 100.0f   /* the highest a fighter collides while its player falls */

/* m408's player states (mp4_m408_player) */
enum { ST_PLANE = 0, ST_JUMP = 1, ST_DIVE = 2, ST_HIT = 3, ST_OUT = 4, ST_LANDING = 5, ST_LANDED = 6 };

int mp4_m408_player(int player, s32* state, s32* chute, s32* coins, float* scale);

static struct {
    int ended;
    struct {
        s32 state;     /* last frame's (-1 before the player exists) */
        s32 chute;     /* its parachute's size, 4 down to 0 */
        s32 coins;     /* the coins it caught, kept after the game frees its objects */
        float scale;   /* its MP4 model's scale */
    } p[PARTY_PLAYERS];
} pp;

static void pp_reset(void)
{
    int i;
    memset(&pp, 0, sizeof pp);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        pp.p[i].state = -1;
        pp.p[i].scale = 1.0f;
    }
}

static void pp_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    pp_reset();
    party_arena_begin(mp4_overlay_m408, WORLD_X, 0.0f, 0.0f);
}

static void pp_frame(void)
{
    int i;
    party_arena_frame();
    for (i = 0; i < PARTY_PLAYERS && !pp.ended; i++) {
        s32 state, chute, coins;
        float scale;
        if (mp4_m408_player(i, &state, &chute, &coins, &scale)) {
            pp.p[i].chute = chute;
            pp.p[i].coins = coins;
        }
    }
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!pp.ended && (mp4_match_over() || !mp4_available())) {
        pp.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m408: P%d coins %d parachute %d", i + 1, pp.p[i].coins, pp.p[i].chute);
        }
        gm_8016B328();
    }
}

static void pp_setup(StartMeleeData* start)
{
    pp_reset();
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = pp_start;
    start->rules.on_frame_start = pp_frame;
}

/* Fighter_procInput: the fighter follows its player, in the air while the player is above the
 * ground; a hit (or the last one, which puts the player out) sends it tumbling once, and it falls
 * normally again when the spin is over. */
static void pp_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, chute, coins, prev;
    float scale, x, y, z, yaw;
    int airborne;
    if (!party_arena_input_begin(fp) || pp.ended) {
        return;
    }
    if (!mp4_m408_player(slot, &state, &chute, &coins, &scale)) {
        return;
    }
    prev = pp.p[slot].state;
    pp.p[slot].state = state;
    pp.p[slot].scale = scale;
    airborne = mp4_player_pose(slot, &x, &y, &z, &yaw) && y > 0.5f;
    if ((state == ST_HIT || state == ST_OUT) && prev != ST_HIT && prev != ST_OUT) {
        party_arena_hit(fp);
        return;
    }
    if (prev == ST_HIT && state != ST_HIT && state != ST_OUT && fp->ground_or_air == GA_Air) {
        ftCo_Fall_Enter(fp->gobj);
    }
    party_arena_follow(fp, airborne);
}

/* fighter_map: its lane, and in the air at its player's height, held under HOVER_Y; down to the
 * floor (where it lands) once the player is on the ground. */
static void pp_fighter_map(Fighter* fp)
{
    int slot = fp->player_idx;
    float x, y, z, yaw;
    party_arena_map(fp);
    if (slot < 0 || slot >= PARTY_PLAYERS || fp->is_sub_fighter || fp->ground_or_air != GA_Air ||
        !mp4_player_pose(slot, &x, &y, &z, &yaw)) {
        return;
    }
    fp->cur_pos.y = y < 0.0f ? 0.0f : y > HOVER_Y ? HOVER_Y : y;
}

/* fighter_drawn: at its MP4 player's place, height and yaw, at the player's scale (the root
 * joint's scale as Fighter_UpdateModelScale sets it, times MP4's); out of sight once the game
 * hid the player. */
static void pp_fighter_drawn(Fighter* fp)
{
    int slot = fp->player_idx;
    float x, y, z, yaw, s, model;
    Vec3 pos, scale;
    if (slot < 0 || slot >= PARTY_PLAYERS || !mp4_player_pose(slot, &x, &y, &z, &yaw)) {
        return;
    }
    s = pp.p[slot].scale > 0.0f ? pp.p[slot].scale : 1.0f;
    pos.x = x;
    pos.y = mp4_player_shown(slot) ? y : -4000.0f;
    pos.z = z;
    if (fp->is_sub_fighter) {
        pos.x -= sinf(yaw) * 3.0f * s;
        pos.z -= cosf(yaw) * 3.0f * s;
    }
    ftPartSetRotY(fp, 0, yaw);
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &pos);
    model = ftCommon_GetModelScale(fp);
    scale.x = (fp->x34_scale.z != 1.0f ? fp->x34_scale.z : model) * s;
    scale.y = model * s;
    scale.z = model * s;
    HSD_JObjSetScale(fp->gobj->hsd_obj, &scale);
}

static float pp_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* More coins place higher; equal coins share a place. */
static void pp_result(s8 place[PARTY_PLAYERS])
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int above = 0;
        for (j = 0; j < PARTY_PLAYERS; j++) {
            above += pp.p[j].coins > pp.p[i].coins;
        }
        place[i] = (s8) above;
    }
}

const PartyMinigame mg_mp4_m408 = {
    "Paratrooper Plunge", "paratrooper-plunge", pp_setup, pp_fighter_input, pp_result, 0, NULL,
    St_Kind_Last, -1, pp_fighter_map, pp_fighter_drawn, pp_knockback, 1, NULL, NULL, 1,
};
