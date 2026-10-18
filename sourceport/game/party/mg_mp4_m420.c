/* Melee Party: Fish n' Drips, Mario Party 4's m420, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m420), run by the MP4 runtime (mp4/mp4_party.c): one
 * against three, each side filling its own water tank. The one works a pump by pressing the six
 * buttons it is shown in turn; the three pass a bucket down their line, one button each, and the
 * last pours it in. The first full tank wins; after 60 seconds, or when both fill at once, it is a
 * draw and nobody wins. MP4 tells the sides apart by GWPlayerCfg.group, which its board decides;
 * the party picks the one at random and hands the groups to the runtime (mp4_match_groups).
 * Nobody walks: the fighters stand where their players stand and follow their motions
 * (party_arena_follow). The match ends when the minigame returns to MP4's boot overlay; MP4 gives
 * each winner 10 coins, so the winning side shares first place and the other last (everyone last
 * on a draw, as MP4 gives nobody anything). */
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

int mp4_m420_player(int player, s32* slot, s32* state, s32* holding);

static struct {
    int ended;
    int solo;   /* the one at the pump */
} fd;

static void fd_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == fd.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m420, WORLD_X, 0.0f, 0.0f);
}

static void fd_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!fd.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        fd.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m420: P%d coins %d%s", i + 1, mp4_player_coins(i), i == fd.solo ? " (the one)" : "");
        }
        gm_8016B328();
    }
}

static void fd_setup(StartMeleeData* start)
{
    memset(&fd, 0, sizeof fd);
    fd.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = fd_start;
    start->rules.on_frame_start = fd_frame;
}

/* Fighter_procInput: a human's buttons go to MP4 without the analog trigger (the runtime turns a
 * pressed trigger into L, and the game asks for L and R apart: a hard R press would read as a
 * wrong L first); the fighter follows its player. */
static void fd_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    u32 held = fp->input.held_buttons[0];
    float sx = fp->input.lstick[0].x, sy = fp->input.lstick[0].y;
    float cx = fp->input.cstick[0].x, cy = fp->input.cstick[0].y;
    s32 place, state, holding;
    if (!party_arena_input_begin(fp) || fd.ended) {
        return;
    }
    if (party.p[slot].slot_type != Gm_PKind_Cpu) {
        mp4_pad(slot, held, sx, sy, cx, cy, 0.0f);
    }
    if (!mp4_m420_player(slot, &place, &state, &holding)) {
        return;
    }
    party_arena_follow(fp, 0);
}

static float fd_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing (nobody anything on a draw). */
static void fd_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m420 = {
    "Fish n' Drips", "fish-n-drips", fd_setup, fd_fighter_input, fd_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, fd_knockback, 1, NULL, NULL, 1,
};
