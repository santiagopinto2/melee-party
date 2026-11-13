/* Melee Party: Trace Race, Mario Party 4's m404, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m404), run by the MP4 runtime (mp4/mp4_party.c): the
 * drawing board scrolling under the players, the dotted line, the brushes, its rules, CPUs,
 * scores and banners. The four players are MP4's player objects, each driving a hidden MP4
 * character model; the Melee fighters stand where those models stand, through party_arena.c.
 * This game keeps MP4's movement: the stick steers the brush at MP4's own speed and tracing the
 * line is the game, so the fighters follow their players throughout. The brush is MP4's model,
 * hooked to the hidden model's hand. The match ends when the minigame returns to MP4's boot
 * overlay; the placements are MP4's ranks by how much of the line was traced, written to the
 * players' coin_win (0 first; 3 for everyone when nobody traced more than 30 percent). */
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

static struct {
    int ended;
} tr;

static void tr_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&tr, 0, sizeof tr);
    party_arena_begin(mp4_overlay_m404, WORLD_X, 0.0f, 0.0f);
}

static void tr_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!tr.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        tr.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m404: P%d place %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void tr_setup(StartMeleeData* start)
{
    memset(&tr, 0, sizeof tr);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = tr_start;
    start->rules.on_frame_start = tr_frame;
}

/* Fighter_procInput: the fighter follows its player; the player's brush is steered by MP4. */
static void tr_fighter_input(Fighter* fp)
{
    if (!party_arena_input_begin(fp) || tr.ended) {
        return;
    }
    party_arena_follow(fp, 0);
}

static float tr_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

static void tr_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int p = mp4_player_coins(i);
        place[i] = (s8) (p < 0 ? 0 : p > 3 ? 3 : p);
    }
}

const PartyMinigame mg_mp4_m404 = {
    "Trace Race", "trace-race", tr_setup, tr_fighter_input, tr_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, tr_knockback, 1, NULL, NULL, 1,
};
