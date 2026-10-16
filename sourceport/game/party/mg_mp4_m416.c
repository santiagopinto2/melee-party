/* Melee Party: Candlelight Flight, Mario Party 4's m416, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m416), run by the MP4 runtime (mp4/mp4_party.c): the dark
 * hall, the candle, the puffs of breath, its rules, CPUs and banners. One player carries the lit
 * candle across the room while the other three chase it and blow at it with A; the party picks
 * the one at random and tells the runtime the groups (mp4_match_groups), as MP4's board would.
 * The four players are MP4's player objects, each driving a hidden MP4 character model; the
 * Melee fighters stand where those models stand, through party_arena.c. While a player walks by
 * the stick, its fighter walks by Melee's physics and the player moves at the fighter's speed;
 * a blow is MP4's, on the hidden model, and the fighter jabs with it; the rest of the time (the
 * start, the ending) the fighter follows. The candle is MP4's model, hooked to the hidden
 * model's hand. The match ends when the minigame returns to MP4's boot overlay; MP4 gives each
 * winner 10 coins, so the winning side shares first place and the other last. */
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

/* m416's blow states (mp4_m416_player) */
enum { BLOW_NONE = 0, BLOW_START = 1 };

int mp4_m416_player(int player, s16* state, int* blowing, s16* stick_x, s16* stick_y, int* group,
                    int* blow_state, int* playing);

static struct {
    int ended;
    int solo;                  /* the player with the candle */
    int blew[PARTY_PLAYERS];   /* the fighter jabbed for this blow */
} cf;

static void cf_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == cf.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m416, WORLD_X, 0.0f, 0.0f);
}

static void cf_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!cf.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        cf.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m416: P%d coins %d%s", i + 1, mp4_player_coins(i), i == cf.solo ? " (the candle)" : "");
        }
        gm_8016B328();
    }
}

static void cf_setup(StartMeleeData* start)
{
    memset(&cf, 0, sizeof cf);
    cf.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = cf_start;
    start->rules.on_frame_start = cf_frame;
}

/* Fighter_procInput: walking by the stick while the player is free, a jab as a blow begins,
 * following the player the rest of the time. */
static void cf_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s16 state, sx, sy;
    int blowing, group, blow_state, playing;
    if (!party_arena_input_begin(fp) || cf.ended) {
        return;
    }
    if (!mp4_m416_player(slot, &state, &blowing, &sx, &sy, &group, &blow_state, &playing)) {
        return;
    }
    if (playing && state == 0 && !blowing) {
        cf.blew[slot] = 0;
        party_arena_walk(fp, sx, sy);
        return;
    }
    party_arena_follow(fp, 0);
    if (blow_state == BLOW_START && !cf.blew[slot]) {
        cf.blew[slot] = 1;
        fp->input.held_buttons[0] |= HSD_PAD_A;
    }
}

static float cf_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing. */
static void cf_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m416 = {
    "Candlelight Flight", "candlelight-flight", cf_setup, cf_fighter_input, cf_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, cf_knockback, 1, NULL, NULL, 1,
};
