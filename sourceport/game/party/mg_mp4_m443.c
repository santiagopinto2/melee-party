/* Melee Party: Mario Speedwagons, Mario Party 4's m443, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m443), run by the MP4 runtime (mp4/mp4_party.c): the drag
 * strip, the four cars and their engines (R is the throttle, as far as it is pressed; A shifts up
 * through four gears; too many revs at the start spin the wheels), its rules, CPUs and banners.
 * It opens on one camera flying over the strip, splits into four quarters that spin out from the
 * middle, one camera a car (party_arena_camera_views), and at the line the winner's quarter grows
 * over the screen; the podium is one camera again. The four players are MP4's player objects,
 * each driving a hidden MP4 character model seated in its car; the Melee fighters stand where
 * those models are, through party_arena.c. Nobody walks by the stick in this game: MP4 moves
 * every player all the time, so the fighters only follow, crouched in their cars as drivers and
 * standing on the podium at the end. The match ends when the minigame returns to MP4's boot
 * overlay. The placements are MP4's own order at the line, the podium it shows (the cars ranked
 * by distance when the first one crosses, a tie broken at random); MP4 pays the winner 10 coins.
 * After five minutes with nobody across (only when nobody drives), MP4 calls a draw and everyone
 * takes the loser's pose: everyone shares last place. */
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

/* m443's phases (mp4_m443_game): the places are set when the race finishes */
enum { PH_INTRO = 0, PH_START = 1, PH_RACE = 2, PH_FINISH = 3, PH_WIPE = 4, PH_PODIUM = 5 };

int mp4_m443_game(s32* phase, s32 place[4]);
int mp4_m443_player(int player, int* in_car, int* driving);

static struct {
    int ended;
    int ranked;                 /* MP4 ranked the cars at the line (no draw) */
    s32 place[PARTY_PLAYERS];   /* MP4's order, read once the race finished */
} sw;

static void sw_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&sw, 0, sizeof sw);
    party_arena_begin(mp4_overlay_m443, WORLD_X, 0.0f, 0.0f);
}

static void sw_frame(void)
{
    s32 phase, place[PARTY_PLAYERS];
    int i;
    party_arena_frame();
    if (sw.ended) {
        return;
    }
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (mp4_match_over() || !mp4_available()) {
        sw.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m443: P%d place %d coins %d", i + 1, sw.ranked ? (int) sw.place[i] : -1,
                      mp4_player_coins(i));
        }
        gm_8016B328();
        return;
    }
    /* the order is read while the game's objects are alive: they go with the overlay */
    if (!sw.ranked && mp4_m443_game(&phase, place) && phase >= PH_FINISH && place[0] >= 0) {
        sw.ranked = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            sw.place[i] = place[i];
        }
    }
}

static void sw_setup(StartMeleeData* start)
{
    memset(&sw, 0, sizeof sw);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = sw_start;
    start->rules.on_frame_start = sw_frame;
}

/* Fighter_procInput: the fighter follows its player all the time (MP4 drives the car), crouched
 * while it sits in the car, standing on the podium, with a taunt for the winner's pose there
 * (party_arena_follow). */
static void sw_fighter_input(Fighter* fp)
{
    int in_car, driving;
    if (!party_arena_input_begin(fp) || sw.ended) {
        return;
    }
    if (!mp4_m443_player(fp->player_idx, &in_car, &driving)) {
        return;
    }
    party_arena_follow(fp, 0);
    if (in_car) {
        fp->input.lstick[0].y = -1.0f;   /* seated in the car: Melee's crouch */
    }
}

static float sw_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4's order at the line: 0 first to 3 last, as its podium shows. Everyone last on a draw (and
 * without a disc). */
static void sw_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        s32 p = sw.ranked ? sw.place[i] : 3;
        place[i] = (s8) (p < 0 ? 3 : p > 3 ? 3 : p);
    }
}

const PartyMinigame mg_mp4_m443 = {
    "Mario Speedwagons", "mario-speedwagons", sw_setup, sw_fighter_input, sw_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, sw_knockback, 1,
    party_arena_camera_views, party_arena_camera_view, 1,
};
