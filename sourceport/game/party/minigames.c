/* Melee Party: the minigame table, the choice of the next one, and its rewards. */
#include <string.h>

#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>

#include "mp4/mp4.h"
#include "party.h"

extern const PartyMinigame mg_volleyball;
extern const PartyMinigame mg_sandbag;
extern const PartyMinigame mg_food;
extern const PartyMinigame mg_domination;
extern const PartyMinigame mg_dungeon;
extern const PartyMinigame mg_mp4_m440;
extern const PartyMinigame mg_mp4_m438;
extern const PartyMinigame mg_mp4_m412;
extern const PartyMinigame mg_mp4_m403;
extern const PartyMinigame mg_mp4_m441;
extern const PartyMinigame mg_mp4_m404;
extern const PartyMinigame mg_mp4_m416;
extern const PartyMinigame mg_mp4_m422;
extern const PartyMinigame mg_mp4_m421;
extern const PartyMinigame mg_mp4_m434;
extern const PartyMinigame mg_mp4_m429;
extern const PartyMinigame mg_mp4_m453;

static const PartyMinigame* const table[] = {
    &mg_volleyball,
    &mg_sandbag,
    &mg_food,
    &mg_domination,
    &mg_dungeon,
    &mg_mp4_m440,
    &mg_mp4_m438,
    &mg_mp4_m412,
    &mg_mp4_m403,
    &mg_mp4_m441,
    &mg_mp4_m404,
    &mg_mp4_m416,
    &mg_mp4_m422,
    &mg_mp4_m421,
    &mg_mp4_m434,
    &mg_mp4_m429,
    &mg_mp4_m453,
};

/* Set by the minigame being played; called from the party patch's food hook. */
void (*minigame_on_eaten)(int slot, int item_kind);

void minigame_item_eaten(int slot, int item_kind)
{
    if (minigame_on_eaten != NULL && slot >= 0 && slot < PARTY_PLAYERS) {
        minigame_on_eaten(slot, item_kind);
    }
}

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

char* getenv(const char* name);

/* MELEE_PARTY_ORDER=volley,food,...: the minigames in this order, round and round (testing). */
static int pick_from_order(void)
{
    static int next;
    const char* order = getenv("MELEE_PARTY_ORDER");
    char id[16];
    int n = 0, want, i = 0;
    if (order == NULL || *order == '\0') {
        return -1;
    }
    want = next++;
    for (;;) {
        int len = 0;
        while (order[i] != ',' && order[i] != '\0' && len < (int) sizeof id - 1) {
            id[len++] = order[i++];
        }
        id[len] = '\0';
        if (n == want) {
            return minigame_find(id);
        }
        n++;
        if (order[i] == '\0') {
            next = 1;   /* wrap: the first one again, then on */
            i = 0;
            n = 0;
            want = 0;
            continue;
        }
        i++;
    }
}

/* The minigames a list offers. The MP4 ones need the MP4 disc, and play offline only: online
 * play keeps no MP4 state in step, so they stay off every online list whatever the discs. */
int minigame_offered(int index, int online)
{
    const PartyMinigame* mg = minigame_get(index);
    return !mg->needs_mp4 || (!online && mp4_available());
}

int minigame_offered_count(int online)
{
    int i, n = 0;
    for (i = 0; i < COUNT; i++) {
        n += minigame_offered(i, online);
    }
    return n;
}

/* The index of the minigame on a list's row; the list's first for a row it does not have. */
int minigame_offered_at(int row, int online)
{
    int i, n = 0, first = 0;
    for (i = 0; i < COUNT; i++) {
        if (!minigame_offered(i, online)) {
            continue;
        }
        if (n == 0) {
            first = i;
        }
        if (n++ == row) {
            return i;
        }
    }
    return first;
}

/* The row a minigame sits on in a list; 0 for one the list does not offer. */
int minigame_offered_row(int index, int online)
{
    int i, n = 0;
    for (i = 0; i < COUNT; i++) {
        if (i == index) {
            return minigame_offered(i, online) ? n : 0;
        }
        n += minigame_offered(i, online);
    }
    return 0;
}

int minigame_pick(void)
{
    /* the minigames a board party can play: the MP4 ones only offline, with an MP4 disc */
    u32 all = 0;
    int online = party_online_running();
    int i, pick = pick_from_order();
    for (i = 0; i < COUNT; i++) {
        if (minigame_offered(i, online)) {
            all |= 1u << i;
        }
    }
    if (pick >= 0) {
        return pick;
    }
    if ((party.mg_played & all) == all) {
        party.mg_played = 0;
    }
    do {
        pick = party_rand(COUNT);
    } while (!(all & (1u << pick)) || (party.mg_played & (1u << pick)));
    party.mg_played |= 1u << pick;
    return pick;
}

void minigame_setup(StartMeleeData* start)
{
    const PartyMinigame* mg = minigame_get(party.minigame);
    minigame_on_eaten = NULL;
    party_rules_base(&start->rules, St_Kind_Battle);
    party_fill_players(start);
    mg->setup(start);
    party_log("turn %d/%d: minigame %s (round %d)", party.turn, party.max_turns, mg->name,
              party.round + 1);
}

int minigame_round_end(void)
{
    const PartyMinigame* mg = minigame_get(party.minigame);
    if (mg->rounds <= 1) {
        party.round = 0;
        return 0;
    }
    if (mg->round_end != NULL) {
        mg->round_end();
    }
    if (++party.round < mg->rounds) {
        return 1;
    }
    party.round = 0;
    return 0;
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
