/* Melee Party: a board game with physics minigames, native (sourceport/game/party).
 *
 * The mode takes over GM_HANYU_SSS (0x09, unused in retail), the same way the Slippi online major
 * takes GM_HANYU_CSS. It is reached from the Vs. menu's Tournament Melee entry and only exists
 * while the host set MU_OPTION_PARTY: with the bit off every hook below returns the retail value,
 * so retail play, online and replays are untouched.
 *
 * States: 0 CSS, 1 board turn (a scripted VS match), 2 minigame (a VS match), 3 results.
 * PartyState lives in this DLL and survives the scene changes in between. */
#ifndef MU_PARTY_H
#define MU_PARTY_H

#include <melee/ft/forward.h>
#include <melee/gm/forward.h>
#include <melee/gm/types.h>
#include <melee/mn/types.h>
#include <melee/pl/forward.h>

#define PARTY_PLAYERS 4
#define PARTY_MODE GM_HANYU_SSS

enum {
    PARTY_STATE_CSS = 0,
    PARTY_STATE_BOARD = 1,
    PARTY_STATE_MINIGAME = 2,
    PARTY_STATE_RESULTS = 3,
};

typedef struct PartyPlayer {
    s8 ckind;        /* CharacterKind */
    u8 color;
    u8 slot_type;    /* Gm_PKind_Human or Gm_PKind_Cpu (every party slot plays) */
    u8 cpu_level;
    u8 nametag;
    s16 coins;
    s16 stars;
    s16 space;       /* board space index */
    s8 place;        /* last minigame placement, 0 = first */
} PartyPlayer;

typedef struct PartyState {
    PartyPlayer p[PARTY_PLAYERS];
    int turn;        /* 1-based */
    int max_turns;
    int mover;       /* whose board turn it is, 0..3 */
    int star_space;
    int minigame;    /* index into the minigame table of the one being played */
    u32 mg_played;   /* bit per minigame already played this cycle */
    u32 rng;
} PartyState;

extern PartyState party;

/* party.c */
int party_rand(int n);                         /* 0..n-1 from the party's own stream */
void party_fill_players(StartMeleeData* start); /* the four party players into a VS start */
void party_preload(StartMeleeData* start);     /* fighters, stage and sound banks */
void party_rules_base(StartMeleeRules* rules, int stkind);
int party_env_int(const char* name, int fallback);
void party_log(const char* fmt, ...);

/* board.c: one board turn is one VS match. */
void board_reset(void);
void board_setup(StartMeleeData* start);
void board_fighter_input(struct Fighter* fp);
int board_turn_done(void);                     /* 1 once every player moved this turn */

/* minigames.c */
typedef struct PartyMinigame {
    const char* name;
    const char* id;                            /* --party-minigame / MELEE_PARTY_MINIGAME */
    void (*setup)(StartMeleeData* start);      /* stage, rules and teams; players are filled */
    void (*fighter_input)(struct Fighter* fp); /* optional: after the pad or the CPU */
    /* Placements, 0 = first; ties share a place. Called once, when the match ends. */
    void (*result)(s8 place[PARTY_PLAYERS]);
} PartyMinigame;

int minigame_count(void);
const PartyMinigame* minigame_get(int index);
int minigame_find(const char* id);             /* -1 if none */
int minigame_pick(void);                       /* random, no repeat until all were played */
void minigame_setup(StartMeleeData* start);    /* the chosen one, party.minigame */
void minigame_finish(void);                    /* placements into coins */

#endif
