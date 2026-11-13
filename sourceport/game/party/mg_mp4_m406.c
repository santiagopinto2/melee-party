/* Melee Party: Avalanche!, Mario Party 4's m406, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m406), run by the MP4 runtime (mp4/mp4_party.c): the long
 * snowy course, the ramps and the rocks, the avalanche rolling down behind the players, its rules,
 * CPUs and banners. The four players are MP4's player objects, each driving a hidden MP4 character
 * model; the Melee fighters stand where those models stand, through party_arena.c. In this game
 * the minigame keeps the movement: the players ski, gravity pulls them down the slope, the stick
 * only steers and A pushes off, so the fighters follow their players and nobody walks by the
 * stick (m406 has no speed hook). A fighter skiing on the snow crouches, as a tuck; it stands up
 * to push off, off a ramp, rolled up after a crash, and once the avalanche caught it. The first
 * player down the mountain wins; anyone the avalanche catches is out, and if it catches everyone
 * nobody wins. At the end the winner drops in from the sky (the fighter falls with it) and
 * taunts with MP4's win pose; the losers are MP4's snowballs. The match ends when the minigame
 * returns to MP4's boot overlay; MP4 gives the winner 10 coins and the rest nothing, so the winner
 * is first and the others share last.
 *
 * The course runs about 35000 MP4 units downhill (3500 below Melee's stage): the fighters stay on
 * Final Destination and are only drawn down there, so they never follow MP4's height as Melee's
 * own (party_arena_follow airborne), which would put them under the blast zone. The one
 * exception is the winner's drop at the end, which happens at the course's top, 0 to 100 units
 * above the stage. */
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

/* m406's player phases and states (mp4_m406_player) */
enum { PH_START = 0, PH_RACE = 1, PH_WIN = 2, PH_LOSE = 3 };
enum { ST_SKI = 0, ST_WIN_DROP = 2 };
/* the winner's pose at the end: a file of mariomot.bin (m406's lbl_1_data_1078[8]) */
enum { MOT_WIN_POSE = 0x53 };

int mp4_m406_player(int player, int* phase, s16* state, int* grounded, int* pushing, int* caught);

static struct {
    int ended;
    u8 taunted[PARTY_PLAYERS];
} av;

static void av_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&av, 0, sizeof av);
    party_arena_begin(mp4_overlay_m406, WORLD_X, 0.0f, 0.0f);
}

static void av_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!av.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        av.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m406: P%d coins %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void av_setup(StartMeleeData* start)
{
    memset(&av, 0, sizeof av);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = av_start;
    start->rules.on_frame_start = av_frame;
}

/* Fighter_procInput: the fighter follows its player all the time (MP4 skis it), crouched while it
 * glides on the snow, in the air only for the winner's drop at the end, with one taunt for the
 * winner's pose. */
static void av_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    int phase, grounded, pushing, caught;
    s16 state;
    if (!party_arena_input_begin(fp) || av.ended) {
        return;
    }
    if (!mp4_m406_player(slot, &phase, &state, &grounded, &pushing, &caught)) {
        return;
    }
    if (phase == PH_WIN && state == ST_WIN_DROP) {
        party_arena_follow(fp, 1);
        return;
    }
    party_arena_follow(fp, 0);
    if ((phase == PH_START || phase == PH_RACE) && !caught && state == ST_SKI && grounded &&
        !pushing)
    {
        fp->input.lstick[0].y = -1.0f;   /* tucked: Melee's crouch */
    }
    if (phase == PH_WIN && !av.taunted[slot] && (mp4_player_motion(slot) & 0xFFFF) == MOT_WIN_POSE) {
        av.taunted[slot] = 1;
        fp->input.held_buttons[0] = HSD_PAD_DPADUP;
    }
}

static float av_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the winner 10 coins and everyone else nothing (nothing at all when the avalanche
 * caught everyone). */
static void av_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m406 = {
    "Avalanche!", "avalanche", av_setup, av_fighter_input, av_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, av_knockback, 1, NULL, NULL, 1,
};
