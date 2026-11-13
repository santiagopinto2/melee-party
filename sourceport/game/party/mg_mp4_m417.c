/* Melee Party: Makin' Waves, Mario Party 4's m417, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m417), run by the MP4 runtime (mp4/mp4_party.c): one
 * against three on a pool. The one stands on a raft in the middle and walks on it to keep it
 * level; the three ride floats around the edge (the stick slides them round, A jumps, A again in
 * the air pounds down) and every pound sends a wave across the pool. The three win as soon as the
 * raft throws the one off, the one if it stays on for 30 seconds. MP4 tells the sides apart by
 * GWPlayerCfg.group, which its board decides; the party picks the one at random and hands the
 * groups to the runtime (mp4_match_groups). On party_arena.c: while the one walks on the raft by
 * the stick, its fighter walks by Melee's physics and the player moves at that speed; thrown off,
 * it goes flying into the water. The three's fighters follow their floats, in the air while they
 * jump. The match ends when the minigame returns to MP4's boot overlay; MP4 gives each winner 10
 * coins, so the winning side shares first place and the other last. */
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

/* m417's game state and player modes (mp4_m417_player) */
enum { PHASE_PLAY = 2 };
enum { MODE_PLAY = 0, MODE_END = 1, MODE_THROWN = 2 };

int mp4_m417_player(int player, s32* lone, s32* mode, float* stick, s32* airborne, s32* phase);

static struct {
    int ended;
    int solo;     /* the one on the raft */
    int thrown;   /* the one went flying (party_arena_hit, once) */
} mw;

static void mw_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == mw.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m417, WORLD_X, 0.0f, 0.0f);
}

static void mw_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!mw.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        mw.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m417: P%d coins %d%s", i + 1, mp4_player_coins(i), i == mw.solo ? " (the one)" : "");
        }
        gm_8016B328();
    }
}

static void mw_setup(StartMeleeData* start)
{
    memset(&mw, 0, sizeof mw);
    mw.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = mw_start;
    start->rules.on_frame_start = mw_frame;
}

/* Fighter_procInput: the one walks on its raft by the stick in play and follows the rest of the
 * time (in the air, then afloat, once the raft threw it off); the three follow their floats. */
static void mw_fighter_input(Fighter* fp)
{
    s32 lone, mode, airborne, phase;
    float stick;
    if (!party_arena_input_begin(fp) || mw.ended) {
        return;
    }
    if (!mp4_m417_player(fp->player_idx, &lone, &mode, &stick, &airborne, &phase)) {
        return;
    }
    if (lone && mode == MODE_THROWN) {
        if (!mw.thrown) {
            mw.thrown = 1;
            party_arena_hit(fp);
        }
        party_arena_follow(fp, 1);
    } else if (lone && mode == MODE_PLAY && phase == PHASE_PLAY && stick > 0.0f) {
        party_arena_walk(fp, (s16) (stick * 72.0f), 0);
    } else {
        party_arena_follow(fp, airborne);
    }
}

static float mw_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing. */
static void mw_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m417 = {
    "Makin' Waves", "makin-waves", mw_setup, mw_fighter_input, mw_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, mw_knockback, 1, NULL, NULL, 1,
};
