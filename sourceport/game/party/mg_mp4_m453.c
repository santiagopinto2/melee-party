/* Melee Party: Challenge Booksquirm, Mario Party 4's m453, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m453), run by the MP4 runtime (mp4/mp4_party.c): the giant
 * book, its pages turning down with cutouts in them, faster as they go, its rules, CPUs and
 * banners. In MP4 this is the one-player challenge, the humans against the pages for a record;
 * here all four play at once and when one player is left, that one keeps going, endless as in
 * MP4, until they lose (Santi, 2026-10-08): every player is in the challenge and the CPUs keep
 * seeking cutouts after the humans are out. The four players are MP4's player objects, each
 * driving a hidden MP4 character model; the Melee fighters stand where those models stand,
 * through party_arena.c. While a player walks by the stick, its fighter walks by Melee's physics
 * and the player moves at the fighter's speed; when the game moves the player the fighter
 * follows. The match ends when the minigame returns to MP4's boot overlay; the placements go by
 * the page each player went out on, the last out first (MP4 pays nobody here: it is a record). */
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

/* m453's player states (mp4_m453_player) */
enum { ST_IDLE = 0, ST_WALK = 1, ST_RUN = 2 };

int mp4_m453_player(int player, s16* state, s16* stick_x, s16* stick_y, int* grounded, int* alive,
                    int* flat, int* playing, int* out_page);
int mp4_m453_pages(void);

static struct {
    int ended;
} bs;
static int cb_out[PARTY_PLAYERS];   /* the page each player went out on (0: in) */

static void bs_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&bs, 0, sizeof bs);
    memset(cb_out, 0, sizeof cb_out);
    party_arena_begin(mp4_overlay_m453, WORLD_X, 0.0f, 0.0f);
}

static void bs_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!bs.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        bs.ended = 1;
        party_log("m453: %d pages", mp4_m453_pages());
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m453: P%d out on page %d", i + 1, cb_out[i]);
        }
        gm_8016B328();
    }
}

static void bs_setup(StartMeleeData* start)
{
    memset(&bs, 0, sizeof bs);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = bs_start;
    start->rules.on_frame_start = bs_frame;
}

/* Fighter_procInput: walking by the stick while the game lets the player walk (in play, on the
 * book, not under a page), following it the rest of the time. */
static void bs_fighter_input(Fighter* fp)
{
    s16 state, sx, sy;
    int grounded, alive, flat, playing, out_page;
    if (!party_arena_input_begin(fp) || bs.ended) {
        return;
    }
    if (!mp4_m453_player(fp->player_idx, &state, &sx, &sy, &grounded, &alive, &flat, &playing, &out_page)) {
        return;
    }
    cb_out[fp->player_idx] = out_page;
    if (playing && alive && !flat && grounded && state <= ST_RUN) {
        party_arena_walk(fp, sx, sy);
    } else {
        party_arena_follow(fp, !grounded);
    }
}

static float bs_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* The page each player went out on, the last out first; still in (the challenge's own cap of 99
 * pages ended it) counts as later than any page; ties share a place. */
static void bs_result(s8 place[PARTY_PLAYERS])
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int mine = cb_out[i] > 0 ? cb_out[i] : 1000;
        int ahead = 0;
        for (j = 0; j < PARTY_PLAYERS; j++) {
            int theirs = cb_out[j] > 0 ? cb_out[j] : 1000;
            ahead += theirs > mine;
        }
        place[i] = (s8) ahead;
    }
}

const PartyMinigame mg_mp4_m453 = {
    "Challenge Booksquirm", "challenge-booksquirm", bs_setup, bs_fighter_input, bs_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, bs_knockback, 1, NULL, NULL, 1,
};
