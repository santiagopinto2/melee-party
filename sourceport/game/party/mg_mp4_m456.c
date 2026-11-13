/* Melee Party: Take a Breather, Mario Party 4's m456, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m456), run by the MP4 runtime (mp4/mp4_party.c): the raft on
 * the sea, the water with its refraction and caustics (MP4's own screen copies), the fish, the
 * timer, its rules, CPUs and banners. The four players stand on a raft; for five seconds each
 * presses L and R in turn to breathe in (only the presses in turn count), then everyone dives and
 * holds their breath for a fifth of a second per breath. The most breaths wins: those players
 * leap out of the water last, the others come up gasping before them. The four players are MP4's
 * player objects, each driving a hidden MP4 character model; the Melee fighters stand where those
 * models stand, through party_arena.c. Nobody walks in this game (MP4 moves every player), so the
 * fighters only follow: crouched while their player breathes in and while it holds its breath, in
 * the air for the dive and the winners' leap, a taunt for the winners at the end. The match ends
 * when the minigame returns to MP4's boot overlay; MP4 gives the players with the most breaths 10
 * coins and the rest nothing, and the placements go by breaths (the most first, ties share, none
 * at all last).
 *
 * MP4 reads L and R as two buttons, the GameCube's clicks, pressed in turn, so a human's triggers
 * reach MP4 apart (mp4_match_triggers_apart): otherwise every R would also press L and void it.
 *
 * The fighters stay on the ground in Melee while their player is under the water or floating:
 * they are drawn at the player's height anyway (party_arena_drawn), and party_arena_follow's
 * airborne mode would pin them under Final Destination's floor. */
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

/* m456's player states (mp4_m456_player) */
enum {
    ST_BREATHE = 0,   /* on the raft, breathing in */
    ST_READY = 1,     /* about to dive */
    ST_DIVE = 2,      /* leaping off the raft into the water */
    ST_HOLD = 3,      /* holding its breath under the water */
    ST_GASP = 4,      /* out of breath: back up */
    ST_LEAP = 5,      /* a winner leaping out of the water */
    ST_FLOAT = 6,     /* at the surface (or on the raft, for one who never breathed in) */
    ST_RESULT = 7,
};
enum { MOTION_BREATH_IN = 1 };

int mp4_m456_player(int player, s16* state, s16* motion, s16* breaths, int* winner, float* y,
                    s32* phase);

static struct {
    int ended;
    struct {
        int breaths;   /* MP4's count, kept for the result: the player objects go with the game */
        int taunted;   /* the winner's taunt is done */
    } p[PARTY_PLAYERS];
} tb;

static void tb_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&tb, 0, sizeof tb);
    mp4_match_triggers_apart();
    party_arena_begin(mp4_overlay_m456, WORLD_X, 0.0f, 0.0f);
}

static void tb_frame(void)
{
    int i;
    s16 state, motion, breaths;
    int winner;
    float y;
    s32 phase;
    party_arena_frame();
    for (i = 0; i < PARTY_PLAYERS; i++) {
        if (mp4_m456_player(i, &state, &motion, &breaths, &winner, &y, &phase)) {
            tb.p[i].breaths = breaths;
        }
    }
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!tb.ended && (mp4_match_over() || !mp4_available())) {
        tb.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m456: P%d breaths %d coins %d", i + 1, tb.p[i].breaths, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void tb_setup(StartMeleeData* start)
{
    memset(&tb, 0, sizeof tb);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = tb_start;
    start->rules.on_frame_start = tb_frame;
}

/* Fighter_procInput: the fighter follows its player throughout, on the ground except at the top
 * of the dive and of the winners' leap. Its Melee inputs come from the
 * player's state: a crouch while it breathes in, gets ready and holds its breath, a taunt for the
 * winners at the end. */
static void tb_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s16 state, motion, breaths;
    int winner;
    float y;
    s32 phase;
    if (!party_arena_input_begin(fp) || tb.ended) {
        return;
    }
    if (!mp4_m456_player(slot, &state, &motion, &breaths, &winner, &y, &phase)) {
        return;
    }
    /* in the air over the raft's height (it floats at about 25; the dive and the leap peak at 300) */
    party_arena_follow(fp, (state == ST_DIVE || state == ST_LEAP) && y > 40.0f);
    fp->input.lstick[0].x = fp->input.lstick[0].y = 0.0f;
    fp->input.held_buttons[0] = 0;
    switch (state) {
    case ST_BREATHE:
        if (motion == MOTION_BREATH_IN) {
            fp->input.lstick[0].y = -1.0f;
        }
        break;
    case ST_READY:
    case ST_HOLD:
        fp->input.lstick[0].y = -1.0f;
        break;
    case ST_RESULT:
        if (winner && !tb.p[slot].taunted && fp->ground_or_air == GA_Ground &&
            fp->motion_id == ftCo_MS_Wait) {
            fp->input.held_buttons[0] = HSD_PAD_DPADUP;
            tb.p[slot].taunted = 1;
        }
        break;
    default:
        break;
    }
    (void) breaths;
    (void) phase;
}

static float tb_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the players with the most breaths 10 coins and the others nothing (nobody, when
 * nobody breathed in: a draw). The placements go by breaths, MP4's own measure (each one is a
 * fifth of a second under the water): the most first, ties share, a player who never breathed in
 * last. A player MP4 paid is first whatever was read of its breaths. */
static void tb_result(s8 place[PARTY_PLAYERS])
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int better = 0;
        if (mp4_player_coins(i) > 0) {
            place[i] = 0;
            continue;
        }
        if (tb.p[i].breaths <= 0) {
            place[i] = 3;
            continue;
        }
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (tb.p[j].breaths > tb.p[i].breaths) {
                better++;
            }
        }
        place[i] = (s8) (better > 3 ? 3 : better);
    }
}

const PartyMinigame mg_mp4_m456 = {
    "Take a Breather", "take-a-breather", tb_setup, tb_fighter_input, tb_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, tb_knockback, 1, NULL, NULL, 1,
};
