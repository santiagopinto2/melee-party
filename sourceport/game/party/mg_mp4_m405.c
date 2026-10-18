/* Melee Party: Mario Medley, Mario Party 4's m405, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m405), run by the MP4 runtime (mp4/mp4_party.c): the pool,
 * its water (MP4's own render passes: ripples drawn and copied from the screen, the scene under
 * the water copied and bent through it), the referee, its rules, timer, CPUs and banners. The four
 * players dive in and swim a medley of three lengths: butterfly (A and B together), backstroke (L
 * and R in turn) and freestyle (A and B in turn), each press a stroke; the first to finish the
 * third length wins, and nobody wins after five minutes. The four players are MP4's player
 * objects, each driving a hidden MP4 character model; the Melee fighters stand where those models
 * stand, through party_arena.c. Nobody walks by the stick in this game (MP4 moves every player),
 * so the fighters only follow, with a Melee animation picked from the player's motion: a short
 * hop off the block for the dive, a run while the player strokes, a taunt for the winner. The
 * match ends when the minigame returns to MP4's boot overlay; MP4 gives the winner 10 coins and
 * the others nothing, so the winner takes first place and the others share last.
 *
 * The fighters stay on the ground in Melee: they are drawn at the player's height anyway
 * (party_arena_drawn), in the water. */
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/forward.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/controller.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f

/* m405's player modes, motion slots and game phases (mp4_m405_player) */
enum { ST_DIVE = 2002 };
enum { SLOT_STROKE_FIRST = 2, SLOT_STROKE_LAST = 4 };
enum { PHASE_RESULT = 1008 };

int mp4_m405_player(int player, s32* state, s32* slot, s32* lap, int* winner, s32* phase);

static struct {
    int ended;
    struct {
        s32 state;     /* the player's mode last frame */
        int taunted;   /* the winner's taunt is done */
    } p[PARTY_PLAYERS];
} mm;

static void mm_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&mm, 0, sizeof mm);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        mm.p[i].state = -1;
    }
    party_arena_begin(mp4_overlay_m405, WORLD_X, 0.0f, 0.0f);
}

static void mm_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!mm.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        mm.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m405: P%d coins %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void mm_setup(StartMeleeData* start)
{
    memset(&mm, 0, sizeof mm);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = mm_start;
    start->rules.on_frame_start = mm_frame;
}

/* Fighter_procInput: the fighter follows its player throughout, on the ground. Its Melee inputs
 * come from the player's motion slot rather than from party_arena_follow's motion table: m405's
 * own motions (MP4 archive m405, files 0 to 47) share file numbers with the walk, the run and the
 * win of every character's motions, so Peach's dive would read as a walk and Waluigi's
 * backstroke as a win. */
static void mm_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, motion, lap, phase;
    int winner, began;
    if (!party_arena_input_begin(fp) || mm.ended) {
        return;
    }
    if (!mp4_m405_player(slot, &state, &motion, &lap, &winner, &phase)) {
        return;
    }
    party_arena_follow(fp, 0);
    fp->input.lstick[0].x = 0.0f;
    fp->input.held_buttons[0] = 0;
    began = state != mm.p[slot].state;
    mm.p[slot].state = state;
    if (state == ST_DIVE && began) {
        fp->input.held_buttons[0] = HSD_PAD_X;   /* a short hop off the block */
    }
    if (motion >= SLOT_STROKE_FIRST && motion <= SLOT_STROKE_LAST) {
        fp->input.lstick[0].x = fp->facing_dir;
    }
    if (winner && phase >= PHASE_RESULT && !mm.p[slot].taunted && fp->ground_or_air == GA_Ground &&
        fp->motion_id == ftCo_MS_Wait) {
        fp->input.held_buttons[0] = HSD_PAD_DPADUP;
        mm.p[slot].taunted = 1;
    }
    (void) lap;
}

static float mm_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the winner 10 coins and the others nothing; nobody when time ran out, so then all
 * four share last place. */
static void mm_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m405 = {
    "Mario Medley", "mario-medley", mm_setup, mm_fighter_input, mm_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, mm_knockback, 1, NULL, NULL, 1,
};
