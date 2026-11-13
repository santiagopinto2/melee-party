/* Melee Party: Hide and Go BOOM!, Mario Party 4's m418, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m418), run by the MP4 runtime (mp4/mp4_party.c): one
 * against three. While the one looks away, the three each pick one of four cannons with B, A, Y
 * or X and hide in it (one who picks none drops through a trapdoor); then the one gets three
 * shots, picking a cannon with the same buttons, and a cannon with someone inside blasts them off
 * into the sky. The three win if any of them is still hidden after the third shot. MP4 tells the
 * sides apart by GWPlayerCfg.group, which its board decides; the party picks the one at random
 * and hands the groups to the runtime (mp4_match_groups). Nobody walks by the stick (MP4 walks
 * each hider to its cannon), so the fighters follow their players throughout
 * (party_arena_follow): out of sight while hidden or gone, flying when blasted. The match ends
 * when the minigame returns to MP4's boot overlay; MP4 gives each winner 10 coins, so the winning
 * side shares first place and the other last. */
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/jobj.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f
/* MP4 parks a hidden player at -5000; one falling through a trapdoor is gone below this */
#define GONE_Y -500.0f
/* m418's phases (mp4_m418_player): blast-offs only happen from play on */
enum { PHASE_PLAY = 5 };

int mp4_m418_player(int player, s32* seeker, s32* jumping, s32* flight, float* y, s32* phase);

static struct {
    int ended;
    int solo;                     /* the one who seeks */
    int blasted[PARTY_PLAYERS];   /* knocked flying (party_arena_hit, once a flight) */
} hb;

static void hb_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == hb.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m418, WORLD_X, 0.0f, 0.0f);
}

static void hb_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!hb.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        hb.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m418: P%d coins %d%s", i + 1, mp4_player_coins(i), i == hb.solo ? " (the one)" : "");
        }
        gm_8016B328();
    }
}

static void hb_setup(StartMeleeData* start)
{
    memset(&hb, 0, sizeof hb);
    hb.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = hb_start;
    start->rules.on_frame_start = hb_frame;
}

/* Fighter_procInput: the fighter follows its player, in the air while it jumps out of a cannon;
 * blasted off, it goes flying once and is drawn along MP4's flight (not pinned: the flight climbs
 * far higher than Melee's stage goes). */
static void hb_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 seeker, jumping, flight, phase;
    float y;
    if (!party_arena_input_begin(fp) || hb.ended) {
        return;
    }
    if (!mp4_m418_player(slot, &seeker, &jumping, &flight, &y, &phase)) {
        return;
    }
    if (flight >= 0 && phase >= PHASE_PLAY) {
        if (!hb.blasted[slot]) {
            hb.blasted[slot] = 1;
            party_arena_hit(fp);
        }
        party_arena_follow(fp, 0);
        return;
    }
    hb.blasted[slot] = 0;
    party_arena_follow(fp, jumping && y > 0.0f);
}

/* fighter_drawn: where its player stands, out of sight while hidden in a cannon, through a
 * trapdoor or blasted into a star. */
static void hb_fighter_drawn(Fighter* fp)
{
    s32 seeker, jumping, flight, phase;
    float y;
    party_arena_drawn(fp);
    if (fp->player_idx < 0 || fp->player_idx >= PARTY_PLAYERS ||
        !mp4_m418_player(fp->player_idx, &seeker, &jumping, &flight, &y, &phase)) {
        return;
    }
    if (y < GONE_Y || (flight >= 4 && phase >= PHASE_PLAY)) {
        Vec3 pos;
        HSD_JObjGetTranslation(fp->gobj->hsd_obj, &pos);
        pos.y = -4000.0f;
        HSD_JObjSetTranslate(fp->gobj->hsd_obj, &pos);
    }
}

static float hb_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing. */
static void hb_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m418 = {
    "Hide and Go BOOM!", "hide-and-go-boom", hb_setup, hb_fighter_input, hb_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, hb_fighter_drawn, hb_knockback, 1, NULL, NULL, 1,
};
