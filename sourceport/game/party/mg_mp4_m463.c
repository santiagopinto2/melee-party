/* Melee Party: Panel Panic, Mario Party 4's m463, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m463), run by the MP4 runtime (mp4/mp4_party.c): nine
 * panels in a 3 by 3 grid, the four players and five extra characters (the MP4 characters nobody
 * plays, and a Koopa) each on one, the roulette blocks, the panels that drop, its rules, CPUs and
 * banners. First the four drop onto the corners and have five seconds to pick a panel: the stick
 * steps the player one panel at a time and A stands on it. Then the other five drop onto the free
 * ones and the panels spread out. Each round, everyone still up gets a roulette block over their
 * head cycling through the panels left: A hits it, and every panel the blocks stop on drops away
 * with whoever stands on it. The last one left wins; nobody does if the last panels all go at
 * once. The four players are MP4's player objects, each driving a hidden MP4 character model; the
 * Melee fighters are drawn where those models are, at their size (MP4 draws everyone here at 2.5
 * times its usual size), through party_arena.c for the pads and the motions. Nobody walks by the
 * stick: a step from panel to panel is MP4's slide to the next grid place, so the fighters follow
 * their players throughout (running while MP4 runs them, in Melee's fall while dropping in or
 * falling with a panel), hop as the player hits its block, and the winner taunts. The extra five
 * are MP4's own models, drawn by MP4.
 *
 * The match ends when the minigame returns to MP4's boot overlay. MP4 gives the one left 10 coins
 * if it is one of the four, and nobody anything else; the places follow how long each player
 * stayed up: the one left first, then by the round each player's panel dropped (a later round
 * places higher, the same round shares a place). When one of the extra five wins, or the last
 * panels all drop at once, the players who lasted longest share first place.
 *
 * The Melee side: Final Destination, each fighter colliding on a lane of its own with the MP4
 * world drawn 600 units to the side. While its player is in the air the fighter is held in the air
 * at the player's height, but no higher than HOVER_Y (the drop starts above the top blast zone)
 * and at AIR_MIN while the player falls (the fall goes far below the bottom one); it is drawn at
 * the player's real height. */
#include <math.h>
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/forward.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f
#define HOVER_Y 100.0f   /* the highest a fighter collides while its player drops in */
#define AIR_MIN 30.0f    /* the lowest, while its player falls with a panel */
/* A player stands on its panel between MP4 heights 0 and 100: above or below these it is
 * dropping in or falling (MP4 units). */
#define AIR_ABOVE 150.0f
#define AIR_BELOW -50.0f
/* m463's motion table: the player hits its roulette block (mp4_m463_player) */
#define MOTION_BLOCK 5
#define HOP_FRAMES 45    /* how long the hop's height is drawn */

int mp4_m463_player(int player, s32* phase, s32* motion, s32* out, s32* won, float* y, float* scale);

static struct {
    int ended;
    int frame;                 /* match frames, from 1 */
    struct {
        int out_frame;         /* the frame its panel was seen gone (0: still up) */
        int won;               /* the one left */
        int air;               /* dropping in or falling this frame */
        s32 motion;            /* last frame's motion (-1 before the player exists) */
        int hop;               /* frames left of the hop on the roulette block */
        float scale;           /* its MP4 model's scale */
    } p[PARTY_PLAYERS];
} pp;

static void pp_reset(void)
{
    int i;
    memset(&pp, 0, sizeof pp);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        pp.p[i].motion = -1;
        pp.p[i].scale = 1.0f;
    }
}

static void pp_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    pp_reset();
    party_arena_begin(mp4_overlay_m463, WORLD_X, 0.0f, 0.0f);
}

static void pp_frame(void)
{
    int i;
    party_arena_frame();
    if (!pp.ended) {
        pp.frame++;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            s32 phase, motion, out, won;
            float y, scale;
            if (!mp4_m463_player(i, &phase, &motion, &out, &won, &y, &scale)) {
                continue;
            }
            if (out && pp.p[i].out_frame == 0) {
                pp.p[i].out_frame = pp.frame;
            }
            if (won) {
                pp.p[i].won = 1;
            }
        }
    }
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!pp.ended && (mp4_match_over() || !mp4_available())) {
        pp.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m463: P%d coins %d out at frame %d%s", i + 1, mp4_player_coins(i),
                      pp.p[i].out_frame, pp.p[i].won ? " (the one left)" : "");
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

/* Fighter_procInput: the fighter follows its player, in the air while the player drops in or
 * falls with its panel; it hops once as the player hits its roulette block. */
static void pp_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 phase, motion, out, won;
    float y, scale;
    if (!party_arena_input_begin(fp) || pp.ended) {
        return;
    }
    if (!mp4_m463_player(slot, &phase, &motion, &out, &won, &y, &scale)) {
        return;
    }
    pp.p[slot].scale = scale;
    pp.p[slot].air = y > AIR_ABOVE || y < AIR_BELOW;
    if (pp.p[slot].hop > 0) {
        pp.p[slot].hop--;
    }
    party_arena_follow(fp, pp.p[slot].air);
    if (!pp.p[slot].air && motion == MOTION_BLOCK && pp.p[slot].motion != MOTION_BLOCK &&
        fp->ground_or_air == GA_Ground) {
        fp->input.held_buttons[0] = HSD_PAD_X;   /* one frame: a short hop */
        pp.p[slot].hop = HOP_FRAMES;
    }
    pp.p[slot].motion = motion;
}

/* fighter_map: its lane, and in the air at its player's height, held under HOVER_Y; at AIR_MIN
 * while its player falls below the floor. */
static void pp_fighter_map(Fighter* fp)
{
    int slot = fp->player_idx;
    float x, y, z, yaw;
    party_arena_map(fp);
    if (slot < 0 || slot >= PARTY_PLAYERS || fp->is_sub_fighter || !pp.p[slot].air ||
        fp->ground_or_air != GA_Air || !mp4_player_pose(slot, &x, &y, &z, &yaw)) {
        return;
    }
    /* dropping in, it comes down with its player and lands; falling, it stays in the air */
    fp->cur_pos.y = y < 0.0f ? AIR_MIN : y > HOVER_Y ? HOVER_Y : y;
}

/* fighter_drawn: at its MP4 player's place, height and yaw, at the player's scale (the root
 * joint's scale as Fighter_UpdateModelScale sets it, times MP4's), plus the hop's height on the
 * roulette block; out of sight until the player drops in and once the game hid it. */
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
    pos.y = y;
    pos.z = z;
    if (!pp.p[slot].air && pp.p[slot].hop > 0) {
        pos.y += fp->cur_pos.y * s;
    }
    if (!mp4_player_shown(slot)) {
        pos.y = -4000.0f;
    }
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

/* The one left (MP4's 10 coins) first; the rest by the round their panel dropped, a later round
 * higher, the same round sharing a place. A player whose panel never dropped stayed up longest. */
static void pp_result(s8 place[PARTY_PLAYERS])
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int above = 0;
        int mine = pp.p[i].out_frame == 0 ? 0x7FFFFFFF : pp.p[i].out_frame;
        if (mp4_player_coins(i) > 0) {
            place[i] = 0;
            continue;
        }
        for (j = 0; j < PARTY_PLAYERS; j++) {
            int theirs = pp.p[j].out_frame == 0 ? 0x7FFFFFFF : pp.p[j].out_frame;
            above += j != i && (theirs > mine || mp4_player_coins(j) > 0);
        }
        place[i] = (s8) above;
    }
}

const PartyMinigame mg_mp4_m463 = {
    "Panel Panic", "panel-panic", pp_setup, pp_fighter_input, pp_result, 0, NULL,
    St_Kind_Last, -1, pp_fighter_map, pp_fighter_drawn, pp_knockback, 1, NULL, NULL, 1,
};
