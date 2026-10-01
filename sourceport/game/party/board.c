/* Melee Party: the board. One board turn is one VS match in which every player moves once. */
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>

#include "party.h"

static int frames;

void board_reset(void)
{
    party.turn = 0;
    party.mover = 0;
}

static void board_frame(void)
{
    if (++frames == 120) {
        gm_8016B328();
    }
}

void board_setup(StartMeleeData* start)
{
    party.turn++;
    frames = 0;
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.on_frame_start = board_frame;
    party_fill_players(start);
    party_log("turn %d/%d: board", party.turn, party.max_turns);
}

void board_fighter_input(struct Fighter* fp)
{
    (void) fp;
}

int board_turn_done(void)
{
    return 1;
}
