/* Melee Party: GOOOOOOOAL!!, Mario Party 4's m423, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m423), run by the MP4 runtime (mp4/mp4_party.c): one
 * against three in a penalty shoot-out. The one keeps goal (the stick shuffles along the line, A
 * dives the way the stick points or jumps); the three shoot at once, each with its own balls (the
 * stick aims, holding A powers the kick). The three win when they have scored 10 goals between
 * them, the one if they have not after 30 seconds. The stadium's big screen is MP4's own render
 * pass: a second camera draws into a corner of the screen and copies it. MP4 tells the sides
 * apart by GWPlayerCfg.group, which its board decides; the party picks the one at random and
 * hands the groups to the runtime (mp4_match_groups). On party_arena.c: while the goalie walks by
 * the stick, its fighter walks by Melee's physics and the goalie moves at that speed; diving and
 * jumping, it follows in the air. The shooters' fighters follow theirs. The match ends when the
 * minigame returns to MP4's boot overlay; MP4 gives each winner 10 coins, so the winning side
 * shares first place and the other last. */
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

/* m423's phase and the goalie's walking state (mp4_m423_player) */
enum { PHASE_PLAY = 6 };
enum { GOALIE_WALK = 1 };

int mp4_m423_player(int player, s32* goalie, s32* state, s16* stick_x, float* y, s32* phase);

static struct {
    int ended;
    int solo;   /* the goalie */
} go;

static void go_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == go.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m423, WORLD_X, 0.0f, 0.0f);
}

static void go_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!go.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        go.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m423: P%d coins %d%s", i + 1, mp4_player_coins(i), i == go.solo ? " (the goalie)" : "");
        }
        gm_8016B328();
    }
}

static void go_setup(StartMeleeData* start)
{
    memset(&go, 0, sizeof go);
    go.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = go_start;
    start->rules.on_frame_start = go_frame;
}

/* Fighter_procInput: the goalie walks along the line by the stick (its x alone: MP4 reads no
 * other) while it is free in play, and follows the rest of the time, in the air on a dive or a
 * jump; the shooters follow theirs. */
static void go_fighter_input(Fighter* fp)
{
    s32 goalie, state, phase;
    s16 sx;
    float y;
    if (!party_arena_input_begin(fp) || go.ended) {
        return;
    }
    if (!mp4_m423_player(fp->player_idx, &goalie, &state, &sx, &y, &phase)) {
        return;
    }
    if (goalie && phase == PHASE_PLAY && state == GOALIE_WALK) {
        party_arena_walk(fp, sx, 0);
    } else {
        party_arena_follow(fp, goalie && y > 0.0f);
    }
}

static float go_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing. */
static void go_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m423 = {
    "GOOOOOOOAL!!", "gooooooal", go_setup, go_fighter_input, go_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, go_knockback, 1, NULL, NULL, 1,
};
