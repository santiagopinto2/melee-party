/* Melee Party: Tree Stomp, Mario Party 4's m419, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m419), run by the MP4 runtime (mp4/mp4_party.c): one
 * against three in a forest clearing. The three each ride a walking tree stump and drive it at
 * the one (the stick turns, A goes forward, B back); a stump that touches the one knocks it
 * flying and the three win, and the one wins if it lasts 45 seconds. Two monkeys throw banana
 * peels to slip on, and a golden banana that speeds up whoever gets it. MP4 tells the sides apart
 * by GWPlayerCfg.group, which its board decides; the party picks the one at random and hands the
 * groups to the runtime (mp4_match_groups). On party_arena.c: while the one walks by the stick,
 * its fighter walks by Melee's physics and the player moves at that speed (times the golden
 * banana's boost); slipping, flying and the ending, the fighter follows. The three's fighters
 * stand on their stumps (the runtime places a model hooked onto another where it is drawn) and
 * follow them. The match ends when the minigame returns to MP4's boot overlay; MP4 gives each
 * winner 10 coins, so the winning side shares first place and the other last. */
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

/* m419's phases (mp4_m419_player) */
enum { PHASE_PLAY = 3 };

int mp4_m419_player(int player, s32* lone, s32* state, s32* out, s16* stick_x, s16* stick_y,
                    float* y, s32* phase, s32* winner);

static struct {
    int ended;
    int solo;     /* the one on foot */
    int flung;    /* the one was knocked flying (party_arena_hit, once) */
} ts;

static void ts_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == ts.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m419, WORLD_X, 0.0f, 0.0f);
}

static void ts_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!ts.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        ts.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m419: P%d coins %d%s", i + 1, mp4_player_coins(i), i == ts.solo ? " (the one)" : "");
        }
        gm_8016B328();
    }
}

static void ts_setup(StartMeleeData* start)
{
    memset(&ts, 0, sizeof ts);
    ts.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = ts_start;
    start->rules.on_frame_start = ts_frame;
}

/* Fighter_procInput: the one walks by the stick while it is free in play; the three follow
 * their stumps throughout (their stick and buttons drive the stump, as MP4 reads them). */
static void ts_fighter_input(Fighter* fp)
{
    s32 lone, state, out, phase, winner;
    s16 sx, sy;
    float y;
    if (!party_arena_input_begin(fp) || ts.ended) {
        return;
    }
    if (!mp4_m419_player(fp->player_idx, &lone, &state, &out, &sx, &sy, &y, &phase, &winner)) {
        return;
    }
    if (!lone) {
        party_arena_follow(fp, 0);   /* standing on its stump, drawn at the seat's height */
        return;
    }
    if (out && winner != fp->player_idx) {
        if (!ts.flung) {
            ts.flung = 1;
            party_arena_hit(fp);
        }
        party_arena_follow(fp, y > 0.0f);
        return;
    }
    if (phase == PHASE_PLAY && !out && (state == 0 || state == 2 || state == 3)) {
        party_arena_walk(fp, sx, sy);
    } else {
        party_arena_follow(fp, 0);
    }
}

static float ts_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing. */
static void ts_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m419 = {
    "Tree Stomp", "tree-stomp", ts_setup, ts_fighter_input, ts_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, ts_knockback, 1, NULL, NULL, 1,
};
