/* Melee Party: Mr. Blizzard's Brigade, Mario Party 4's m412, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m412), run by the MP4 runtime (mp4/mp4_party.c): the frozen
 * lake, the Mr. Blizzards and their snowballs, the camera that circles the lake, its rules, timer,
 * CPUs and banners. The four players are MP4's player objects, each driving a hidden MP4 character
 * model; the Melee fighters stand where those models stand, through party_arena.c. In this game
 * the minigame keeps the movement: the ice is the game, a player slides on it with MP4's
 * acceleration and friction, and Melee's traction would take that away. So the fighters follow
 * their players (the speed hook in m412's walk code is there, and off). A snowball hit freezes a
 * player inside a snowman until the end; the survivors win. The match ends when the minigame
 * returns to MP4's boot overlay; MP4 gives each survivor 10 coins and the rest nothing, so the
 * survivors share first place and the frozen share last. */
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

/* m412's player states (mp4_m412_player) */
enum { ST_FROZEN = 3, ST_OVER = 4 };

int mp4_m412_player(int player, s16* state);

static struct {
    int ended;
} mb;

static void mb_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&mb, 0, sizeof mb);
    party_arena_begin(mp4_overlay_m412, WORLD_X, 0.0f, 0.0f);
}

static void mb_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!mb.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        mb.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m412: P%d coins %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void mb_setup(StartMeleeData* start)
{
    memset(&mb, 0, sizeof mb);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = mb_start;
    start->rules.on_frame_start = mb_frame;
}

/* Fighter_procInput: the fighter follows its player; frozen in its snowman, it stands still. */
static void mb_fighter_input(Fighter* fp)
{
    s16 state;
    if (!party_arena_input_begin(fp) || mb.ended) {
        return;
    }
    if (mp4_m412_player(fp->player_idx, &state) && state == ST_FROZEN) {
        return;
    }
    party_arena_follow(fp, 0);
}

static float mb_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each survivor 10 coins and a frozen player nothing. */
static void mb_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m412 = {
    "Mr. Blizzard's Brigade", "blizzard-brigade", mb_setup, mb_fighter_input, mb_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, mb_knockback, 1, NULL, NULL, 1,
};
