/* Melee Party online: the party over Slippi Direct, through the existing online play.
 *
 * Both players connect with Direct (connect codes) and pick their characters on the online
 * character select as usual. The host advertises a Melee Party build there (source_host.cpp), so a
 * Direct connection only forms between two Melee Party builds of the same protocol, and both sides
 * run the same party code.
 *
 * Every party match (each board turn, minigame round and the podium) is one ordinary Slippi online
 * game: inputs exchanged, rollback, desync checks. Rollback needs nothing extra: the party's state
 * is in this DLL's data and bss, which the online savestates capture with the rest of the game.
 *
 * - The first game of a Direct session starts the party: the two humans from the negotiated match,
 *   two CPUs picked with the party's random generator, seeded with the match's shared seed, so
 *   both sides pick the same.
 * - Each match is set up by the party (party_setup_phase) and kept over the negotiated setup that
 *   the online start applies (mu_party_online_start_melee).
 * - Between matches there is no character select: both sides send their last selection again and
 *   wait until both are ready (the CSS lock-in, done here), then the next match starts.
 * - A disconnect, a failed handshake or L+R+A+Start ends the party: back to the online CSS.
 *
 * Party matches are not recorded: a replay of one could not be played back. */
#include <string.h>

#include <melee/gm/gm_1A3F.h>
#include <melee/gm/gmvsmelee.h>
#include <melee/gm/types.h>
#include <melee/gr/forward.h>
#include <melee/mn/types.h>

#include "party.h"

#define HANDSHAKE_FRAMES (60 * 30)   /* the opponent's match end, within reason */
#define SETTLE_FRAMES 90

static struct {
    int running;
    int phase;                 /* the phase of the match being set up or played */
    StartMeleeData start;      /* that match, as the party set it up */
} po;

static int online_allowed(void)
{
    if (!(mu_game_options() & MU_OPTION_PARTY_ONLINE) || mu_replay_on()) {
        return 0;
    }
    /* Direct (two players) and Teams (up to four, everyone entering the same code): the modes
     * in which every player is known to run this build. A local test group (--local-peer) can play
     * it in any mode with MELEE_PARTY_ONLINE_TEST=1. */
    return mu_slippi_state()->mode == MU_SLP_MODE_DIRECT ||
           mu_slippi_state()->mode == MU_SLP_MODE_TEAMS ||
           party_env_int("MELEE_PARTY_ONLINE_TEST", 0) != 0;
}

int party_online_running(void)
{
    return po.running;
}

int mu_party_online_running(void)
{
    return po.running;
}

static void end_party(const char* why)
{
    party_log("online: party ends (%s)", why);
    po.running = 0;
    party.phase = 0;
}

/* The online VS prep. Returns 1 when the party set the match up (the caller arms online play). */
int mu_party_online_vs_prep(GameModeState* state)
{
    StartMeleeData* start = gm_GetGameModeStateEnterData(state);

    if (!po.running) {
        const MuMatchState* ms = mu_slippi_match_state();
        if (!online_allowed()) {
            return 0;
        }
        /* The negotiated match is in the VS data (the splash or this decide applied it). */
        party_start(gmVsMelee_GetVsData(), ms->rng_offset ^ 0x5A17A9F1u);
        /* Nothing from this side's offline menus: both sides must start alike. The host picks
         * what to play in the lobby (lobby.c). */
        party.board = 0;
        party.no_minigames = 0;
        party.lobby = 1;
        po.running = 1;
        po.phase = PARTY_STATE_LOBBY;
        party_log("online: party starts, local port %d", ms->local_index);
    }
    memset(&po.start, 0, sizeof po.start);
    party_setup_phase(po.phase, &po.start);
    po.start.rules.is_vs = true;
    *start = po.start;
    party_preload(start);
    return 1;
}

/* At the online match start, after the negotiated setup was applied: the party's match again. */
void mu_party_online_start_melee(StartMeleeData* data)
{
    if (po.running) {
        *data = po.start;
    }
}

/* Lock in again with the selection made on the character select, and wait for the opponent.
 *
 * First a pause for the match just played to drain: its last pads and acknowledgements can still
 * be in flight, and once the next game has started (its frame 1 resets the netplay queues) a late
 * acknowledgement of an old frame would tell this side the opponent has frames it never got, and
 * the new game stalls. Slippi's own flow never meets this: its results screen and character
 * select take seconds between games. */
static int handshake(void)
{
    MuSlippiSelections sel = *mu_slippi_last_selections();
    MuMatchState ms;
    int i;

    for (i = 0; i < SETTLE_FRAMES; i++) {
        mu_slippi_load_match_state(&ms);   /* keeps the connection serviced, as the CSS does */
        mu_poll();
    }

    sel.char_opt = 1;
    sel.stage_id = St_Kind_Last;
    sel.stage_opt = MU_SLP_STAGE_PICK;
    mu_slippi_set_selections(&sel);
    for (i = 0; i < HANDSHAKE_FRAMES; i++) {
        if (mu_slippi_load_match_state(&ms) != 0) {
            return 0;
        }
        if (ms.connection_state != MU_SLP_MM_CONNECTION_SUCCESS) {
            return 0;
        }
        if (ms.local_ready && ms.remote_ready) {
            return 1;
        }
        mu_poll();
    }
    return 0;
}

/* The online VS decide. Returns 1 when the party chose the next state. */
int mu_party_online_vs_decide(GameModeState* state)
{
    MatchExitInfo* exit = gm_GetGameModeStateExitData(state);
    int next;

    if (!po.running) {
        return 0;
    }
    if (exit != NULL && exit->match_end.outcome == OUTCOME_NO_CONTEST) {
        end_party("No Contest");
        gm_SetNextGameModeStateId(MU_SLP_STATE_CSS);
        return 1;
    }
    next = party_advance(po.phase);
    if (next == PARTY_END) {
        end_party("finished");
        gm_SetNextGameModeStateId(MU_SLP_STATE_CSS);
        return 1;
    }
    party_preload_phase(next);
    if (!handshake()) {
        end_party("the opponent did not start the next match");
        gm_SetNextGameModeStateId(MU_SLP_STATE_CSS);
        return 1;
    }
    po.phase = next;
    mu_slippi_splash_init();   /* the negotiated match into the VS data, as the splash does */
    gm_SetNextGameModeStateId(MU_SLP_STATE_VS);
    return 1;
}
