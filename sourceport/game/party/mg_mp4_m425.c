/* Melee Party: The Great Deflate, Mario Party 4's m425, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m425), run by the MP4 runtime (mp4/mp4_party.c): the two
 * Thwomps (soft bodies MP4 draws and squashes itself), their air, the dust and the debris, its
 * rules, CPUs, timer and banners, and its single camera. Two against two: MP4 tells the pairs
 * apart by GWPlayerCfg.group, which its board decides; the party pairs the players at random (as
 * Cheep Cheep Sweep does) and hands the groups to the runtime (mp4_match_groups). Each pair jumps
 * on the Thwomp of its group (A to jump, A or L in the air to ground-pound) and the Thwomp that
 * loses its air first wins for its pair. The four players are MP4's player objects, each driving
 * a hidden MP4 character model; the Melee fighters stand where those models stand, through
 * party_arena.c. Nobody walks by the stick in this game (the decompiled player code reads no
 * stick), so MP4 moves every player the whole way, jumping, pounding, dropping in, winning and
 * losing, and the fighters only follow. A human's A and L go to MP4's pad as they are (a fighter's
 * Z also presses A, and R is read as L). The match ends when the minigame returns to MP4's boot
 * overlay; MP4 gives each member of the winning pair 10 coins, so that pair shares first place
 * and the other pair last. */
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f

int mp4_m425_player(int player, s16* state, int* grounded, int* team, int* phase);

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
} dg;

static void dg_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(dg.groups);
    party_arena_begin(mp4_overlay_m425, WORLD_X, 0.0f, 0.0f);
}

static void dg_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!dg.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        dg.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m425: P%d pair %d coins %d", i + 1, dg.groups[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void dg_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&dg, 0, sizeof dg);
    /* random pairs, as Cheep Cheep Sweep and Dungeon Duos pair their teams */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        dg.groups[order[i]] = (s8) (i >> 1);
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = dg_start;
    start->rules.on_frame_start = dg_frame;
}

/* Fighter_procInput: the fighter follows its player all the time, in the air while the player is
 * off its Thwomp (a jump, a pound, the drop in at the start). */
static void dg_fighter_input(Fighter* fp)
{
    s16 state;
    int grounded, team, phase;
    if (!party_arena_input_begin(fp) || dg.ended) {
        return;
    }
    if (!mp4_m425_player(fp->player_idx, &state, &grounded, &team, &phase)) {
        return;
    }
    party_arena_follow(fp, !grounded);
}

static float dg_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each member of the winning pair 10 coins and the others nothing; nobody when neither
 * Thwomp lost any air (a draw): then everyone is last. */
static void dg_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m425 = {
    "The Great Deflate", "the-great-deflate", dg_setup, dg_fighter_input, dg_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, dg_knockback, 1, NULL, NULL, 1,
};
