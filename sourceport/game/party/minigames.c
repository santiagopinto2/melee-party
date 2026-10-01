/* Melee Party: the minigame table, the choice of the next one, and its rewards. */
#include <string.h>

#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>

#include "party.h"

extern const PartyMinigame mg_volleyball;

static const PartyMinigame* const table[] = {
    &mg_volleyball,
};

#define COUNT ((int) (sizeof table / sizeof table[0]))

int minigame_count(void)
{
    return COUNT;
}

const PartyMinigame* minigame_get(int index)
{
    return table[index >= 0 && index < COUNT ? index : 0];
}

int minigame_find(const char* id)
{
    int i;
    for (i = 0; i < COUNT; i++) {
        if (strcmp(table[i]->id, id) == 0) {
            return i;
        }
    }
    return -1;
}

int minigame_pick(void)
{
    u32 all = COUNT >= 32 ? 0xFFFFFFFFu : (1u << COUNT) - 1;
    int pick;
    if ((party.mg_played & all) == all) {
        party.mg_played = 0;
    }
    do {
        pick = party_rand(COUNT);
    } while (party.mg_played & (1u << pick));
    party.mg_played |= 1u << pick;
    return pick;
}

void minigame_setup(StartMeleeData* start)
{
    const PartyMinigame* mg = minigame_get(party.minigame);
    party_rules_base(&start->rules, St_Kind_Battle);
    party_fill_players(start);
    mg->setup(start);
    party_log("turn %d/%d: minigame %s", party.turn, party.max_turns, mg->name);
}

/* Coins by placement: 10 / 5 / 3 / 0, a shared place pays the higher amount. */
void minigame_finish(void)
{
    static const s16 reward[PARTY_PLAYERS] = { 10, 5, 3, 0 };
    s8 place[PARTY_PLAYERS];
    int i;

    minigame_get(party.minigame)->result(place);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int p = place[i] < 0 ? PARTY_PLAYERS - 1 : place[i] >= PARTY_PLAYERS ? PARTY_PLAYERS - 1 : place[i];
        party.p[i].place = (s8) p;
        party.p[i].coins = (s16) (party.p[i].coins + reward[p]);
    }
    party_log("minigame %s: places %d %d %d %d, coins %d %d %d %d", minigame_get(party.minigame)->name,
              place[0], place[1], place[2], place[3], party.p[0].coins, party.p[1].coins,
              party.p[2].coins, party.p[3].coins);
}
