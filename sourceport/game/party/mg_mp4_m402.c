/* Melee Party: Slime Time, Mario Party 4's m402, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m402), run by the MP4 runtime (mp4/mp4_party.c): the four
 * blobs of slime that drop behind the players, the slime each player is stuck to, its rules,
 * timer, CPUs and banners. Each player mashes A to pull forward against the slime, a few presses
 * a step; a player who goes a second without finishing a step is snapped back to the start. The
 * first to the end of the slime wins; nobody wins after 30 seconds. The four players are MP4's
 * player objects, each driving a hidden MP4 character model; the Melee fighters stand where those
 * models stand, through party_arena.c. Nobody walks by the stick in this game (MP4 moves every
 * player), so the fighters only follow, with a Melee animation picked from the player's mode: a
 * run while it pulls forward, a flinch when the slime snaps it back, a taunt for the winner. The
 * match ends when the minigame returns to MP4's boot overlay; MP4 gives the winner 10 coins and
 * the others nothing, so the winner takes first place and the others share last.
 *
 * The fighters stay on the ground in Melee even when the slime lifts their player: they are drawn
 * at the player's height anyway (party_arena_drawn), and party_arena_follow's airborne mode pins
 * a fighter at the player's height over MP4's origin, while this game's floor is 388 MP4 units up
 * (the fighter would land 38.8 units too high when it ends). */
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

/* m402's player modes and game phases (mp4_m402_player) */
enum {
    ST_WALK_IN = 2001,     /* walks up to its place */
    ST_PULLED = 2006,      /* the show before the start: running against the slime */
    ST_PULLED_ON = 2007,
    ST_SNAP_SHOW = 2008,   /* the show's snap back */
    ST_WIN_SLIDE = 2012,   /* the winner sliding to the middle */
    ST_SNAP_LOSE = 2014,   /* the losers snapped back at the end */
    ST_PLAY = 2016,        /* in play: idle at progress 0, pulling forward after */
    ST_SNAP = 2017,        /* stopped too long: snapped back */
};
enum { PHASE_RESULT = 1008 };

int mp4_m402_player(int player, s32* state, s32* progress, float* height, int* winner, s32* phase);

static struct {
    int ended;
    struct {
        s32 state;     /* the player's mode last frame */
        int taunted;   /* the winner's taunt is done */
    } p[PARTY_PLAYERS];
} sl;

static void sl_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&sl, 0, sizeof sl);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        sl.p[i].state = -1;
    }
    party_arena_begin(mp4_overlay_m402, WORLD_X, 0.0f, 0.0f);
}

static void sl_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!sl.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        sl.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m402: P%d coins %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void sl_setup(StartMeleeData* start)
{
    memset(&sl, 0, sizeof sl);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = sl_start;
    start->rules.on_frame_start = sl_frame;
}

/* Fighter_procInput: the fighter follows its player throughout, on the ground. Its Melee inputs
 * come from the player's mode rather than from party_arena_follow's motion table: m402's own
 * motions (MP4 archive m402, files 0 to 31) share file numbers with the walk and the run of
 * every character's motions, so Peach's snap back would read as a walk and Yoshi's as a run. */
static void sl_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, progress, phase;
    float height;
    int winner, began;
    if (!party_arena_input_begin(fp) || sl.ended) {
        return;
    }
    if (!mp4_m402_player(slot, &state, &progress, &height, &winner, &phase)) {
        return;
    }
    party_arena_follow(fp, 0);
    fp->input.lstick[0].x = 0.0f;
    fp->input.held_buttons[0] = 0;
    began = state != sl.p[slot].state;
    sl.p[slot].state = state;
    switch (state) {
    case ST_WALK_IN:
        fp->input.lstick[0].x = 0.5f * fp->facing_dir;
        break;
    case ST_PULLED:
    case ST_PULLED_ON:
    case ST_WIN_SLIDE:
        fp->input.lstick[0].x = fp->facing_dir;
        break;
    case ST_PLAY:
        if (progress > 0) {
            fp->input.lstick[0].x = fp->facing_dir;
        }
        break;
    case ST_SNAP_SHOW:
    case ST_SNAP_LOSE:
    case ST_SNAP:
        if (began) {
            Fighter_ChangeMotionState(fp->gobj, ftCo_MS_DamageN3, 0, 0.0f, 1.0f, 0.0f, NULL);
        }
        break;
    default:
        break;
    }
    if (winner && phase >= PHASE_RESULT && !sl.p[slot].taunted && fp->ground_or_air == GA_Ground &&
        fp->motion_id == ftCo_MS_Wait) {
        fp->input.held_buttons[0] = HSD_PAD_DPADUP;
        sl.p[slot].taunted = 1;
    }
    (void) height;
}

static float sl_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the winner 10 coins and the others nothing; nobody when time ran out, so then all
 * four share last place. */
static void sl_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m402 = {
    "Slime Time", "slime-time", sl_setup, sl_fighter_input, sl_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, sl_knockback, 1, NULL, NULL, 1,
};
