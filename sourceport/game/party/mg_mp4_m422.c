/* Melee Party: Money Belts, Mario Party 4's m422, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m422), run by the MP4 runtime (mp4/mp4_party.c): the two
 * conveyor belts, the coins and bags riding them, its rules, CPUs, counters and banners. One
 * player collects alone on the upper belt while the other three share the lower one; the party
 * picks the one at random and tells the runtime the groups (mp4_match_groups), as MP4's board
 * would. The four players are MP4's player objects, each driving a hidden MP4 character model;
 * the Melee fighters stand where those models stand, through party_arena.c. While a player walks
 * by the stick, its fighter walks by Melee's physics and the player moves at the fighter's speed;
 * when the belt carries it off the end and it falls, and at the start and the end, the fighter
 * follows. The match ends when the minigame returns to MP4's boot overlay; the side with more
 * coins wins (MP4 keeps the coins as coins: GWPlayerCoinCollectSet), so the winning side shares
 * first place and the other last, everyone first on a tie. */
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

/* m422's player states (mp4_m422_player) */
enum { ST_WALK = 1, ST_CARRIED = 2 };

int mp4_m422_player(int player, s32* state, s32* seat, s16* stick_x, s16* stick_y, float* height,
                    s32* coins_mine, s32* coins_theirs, s32* phase);

static struct {
    int ended;
    int solo;                       /* the player alone on the upper belt */
    s32 coins[PARTY_PLAYERS];       /* each player's side's coins at the end */
    s32 theirs[PARTY_PLAYERS];
} mb;

static void mb_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == mb.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m422, WORLD_X, 0.0f, 0.0f);
}

static void mb_frame(void)
{
    int i;
    party_arena_frame();
    for (i = 0; i < PARTY_PLAYERS && !mb.ended; i++) {
        s32 state, seat, mine, theirs, phase;
        s16 sx, sy;
        float h;
        if (mp4_m422_player(i, &state, &seat, &sx, &sy, &h, &mine, &theirs, &phase)) {
            mb.coins[i] = mine;
            mb.theirs[i] = theirs;
        }
    }
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!mb.ended && (mp4_match_over() || !mp4_available())) {
        mb.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m422: P%d side %d coins %d against %d", i + 1, i == mb.solo ? 0 : 1, mb.coins[i], mb.theirs[i]);
        }
        gm_8016B328();
    }
}

static void mb_setup(StartMeleeData* start)
{
    memset(&mb, 0, sizeof mb);
    mb.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = mb_start;
    start->rules.on_frame_start = mb_frame;
}

/* Fighter_procInput: walking by the stick while the player walks its belt, following it the rest
 * of the time (in the air once the belt has dropped it). */
static void mb_fighter_input(Fighter* fp)
{
    s32 state, seat, mine, theirs, phase;
    s16 sx, sy;
    float h;
    if (!party_arena_input_begin(fp) || mb.ended) {
        return;
    }
    if (!mp4_m422_player(fp->player_idx, &state, &seat, &sx, &sy, &h, &mine, &theirs, &phase)) {
        return;
    }
    if (state == ST_WALK && phase >= 1002 && phase <= 1004) {
        party_arena_walk(fp, sx, sy);
    } else {
        party_arena_follow(fp, state == ST_CARRIED && h < -1.0f);
    }
}

static float mb_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* The side with more coins wins; a tie puts everyone first. */
static void mb_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mb.coins[i] < mb.theirs[i] ? 3 : 0);
    }
}

const PartyMinigame mg_mp4_m422 = {
    "Money Belts", "money-belts", mb_setup, mb_fighter_input, mb_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, mb_knockback, 1, NULL, NULL, 1,
};
