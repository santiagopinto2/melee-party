/* Melee Party: Stamp Out!, Mario Party 4's m415, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m415), run by the MP4 runtime (mp4/mp4_party.c): the floor
 * of tiles, the stamps that come down and print the player's shadow where they land, its rules,
 * CPUs, counters and banners; the most tiles stamped wins. The four players are MP4's player
 * objects, each driving a hidden MP4 character model; the Melee fighters stand where those models
 * stand, through party_arena.c. While a player walks by the stick, its fighter walks by Melee's
 * physics and the player moves at the fighter's speed; when the game moves the player (a stamp
 * landing on it, the end) the fighter follows. The match ends when the minigame returns to MP4's
 * boot overlay; MP4 gives the top stampers 10 coins and the rest nothing, so they share first
 * place and the rest last. The stamp's print is the shadow map's bytes, which the renderer writes
 * back to guest memory on the game's request (mu_gx_copy_readback). */
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

/* m415's player states (mp4_m415_player) */
enum { ST_IDLE = 0, ST_WALK = 1 };

int mp4_m415_player(int player, s16* state, s16* stick_x, s16* stick_y, int* grounded, int* alive,
                    int* flat, int* playing);

static struct {
    int ended;
} so;

static void so_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&so, 0, sizeof so);
    party_arena_begin(mp4_overlay_m415, WORLD_X, 0.0f, 0.0f);
}

static void so_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!so.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        so.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m415: P%d coins %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void so_setup(StartMeleeData* start)
{
    memset(&so, 0, sizeof so);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = so_start;
    start->rules.on_frame_start = so_frame;
}

/* Fighter_procInput: walking by the stick while the game lets the player walk (in play, on the
 * floor, not under a stamp), following it the rest of the time. */
static void so_fighter_input(Fighter* fp)
{
    s16 state, sx, sy;
    int grounded, alive, flat, playing;
    if (!party_arena_input_begin(fp) || so.ended) {
        return;
    }
    if (!mp4_m415_player(fp->player_idx, &state, &sx, &sy, &grounded, &alive, &flat, &playing)) {
        return;
    }
    if (playing && alive && !flat && grounded && state <= ST_WALK) {
        party_arena_walk(fp, sx, sy);
    } else {
        party_arena_follow(fp, !grounded);
    }
}

static float so_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the top stampers 10 coins and the rest nothing. */
static void so_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m415 = {
    "Stamp Out!", "stamp-out", so_setup, so_fighter_input, so_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, so_knockback, 1, NULL, NULL, 1,
};
