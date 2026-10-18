/* Melee Party: Beach Volley Folly, Mario Party 4's m433, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m433), run by the MP4 runtime (mp4/mp4_party.c): the beach
 * court, the ball and its flight, the serves, bumps, sets, spikes and blocks, the score, its
 * rules, CPUs and banners, and its one Hu3D camera. Two against two: MP4 tells the pairs apart by
 * GWPlayerCfg.group, which its board decides; the party pairs the players at random (as Cheep
 * Cheep Sweep does) and hands the groups to the runtime (mp4_match_groups). The four players are
 * MP4's player objects, each driving a hidden MP4 character model; the Melee fighters stand where
 * those models stand, through party_arena.c. While a player runs about its half of the court by
 * the stick, its fighter walks by Melee's physics and the player moves at the fighter's speed.
 * Everything else is MP4's and the fighter follows: waiting for a serve, the serve itself, the
 * reach for a ball (A bumps or sets it), the jumps (B jumps to spike or block, A in the air
 * spikes), the cheer after a point and the walk-off at the end; the fighter jabs as a hit lands
 * on the sand, and attacks in the air as a serve, spike or block lands up there. The first pair
 * to 5 points wins; MP4 gives each winner 10 coins, so the winning pair shares first place and
 * the other last. */
#include <math.h>
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/controller.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f

/* m433's player phases (mp4_m433_player) */
enum { ST_WAIT = 0, ST_SERVE = 1, ST_DEFEND = 2, ST_ATTACK = 3, ST_POINT = 4 };
/* m433's motion slots (lbl_1_data_24C) where the player strikes the ball */
enum {
    MO_SERVE_HIT = 0x05, MO_SWING = 0x06, MO_SET = 0x07, MO_SWING2 = 0x09, MO_SPIKE = 0x0B,
    MO_BLOCK = 0x0D
};

int mp4_m433_player(int player, s32* state, float* stick, float* heading, s32* motion, int* busy,
                    int* airborne, s32* team, int* playing);

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
    s32 motion[PARTY_PLAYERS];   /* the motion slot seen last frame */
} bv;

static void bv_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        bv.motion[i] = -1;
    }
    mp4_match_groups(bv.groups);
    party_arena_begin(mp4_overlay_m433, WORLD_X, 0.0f, 0.0f);
}

static void bv_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!bv.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        bv.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m433: P%d pair %d coins %d", i + 1, bv.groups[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void bv_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&bv, 0, sizeof bv);
    /* random pairs, as Dungeon Duos pairs its teams */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        bv.groups[order[i]] = (s8) (i >> 1);
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = bv_start;
    start->rules.on_frame_start = bv_frame;
}

/* Fighter_procInput: running about the court by the stick while the rally is on and the player is
 * free, following the player the rest of the time, with a jab or an aerial as it strikes. */
static void bv_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, motion, team;
    float stick, heading;
    int busy, airborne, playing, struck;
    if (!party_arena_input_begin(fp) || bv.ended) {
        return;
    }
    if (!mp4_m433_player(slot, &state, &stick, &heading, &motion, &busy, &airborne, &team,
                         &playing)) {
        return;
    }
    struck = motion != bv.motion[slot] &&
             (motion == MO_SERVE_HIT || motion == MO_SWING || motion == MO_SET ||
              motion == MO_SWING2 || motion == MO_SPIKE || motion == MO_BLOCK);
    bv.motion[slot] = motion;
    if (playing && (state == ST_DEFEND || state == ST_ATTACK) && !busy && !airborne) {
        /* the stick back from MP4's fraction and heading (atan2d(x, -y)) */
        float a = heading * 3.14159265f / 180.0f;
        party_arena_walk(fp, (s16) (72.0f * stick * sinf(a)), (s16) (-72.0f * stick * cosf(a)));
        return;
    }
    party_arena_follow(fp, airborne);
    if (playing && struck) {
        fp->input.held_buttons[0] |= HSD_PAD_A;
    }
}

static float bv_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing; if it gives nobody any (a game cut
 * short), everyone is last. */
static void bv_result(s8 place[PARTY_PLAYERS])
{
    int i, any = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        any |= mp4_player_coins(i) > 0;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (any && mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m433 = {
    "Beach Volley Folly", "beach-volley-folly", bv_setup, bv_fighter_input, bv_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, bv_knockback, 1, NULL, NULL, 1,
};
