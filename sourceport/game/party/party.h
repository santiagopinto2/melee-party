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
    PARTY_STATE_RESULTS = 3,   /* a scripted match: the podium */
    PARTY_STATE_LOBBY = 4,     /* online: the host picks board or minigame mode (lobby.c) */
    PARTY_END = -1,
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
    int board;       /* index into the board table */
    int no_minigames; /* Debug Boards: board turn after board turn */
    int lobby;       /* online: the host picks what to play in a lobby between games */
    int lobby_minigames; /* the lobby picked minigame mode: back to it after each minigame */
    int star_space;
    int minigame;    /* index into the minigame table of the one being played */
    u32 mg_played;   /* bit per minigame already played this cycle */
    int round;       /* sub-match of a minigame played in rounds, 0-based */
    int phase;       /* the PARTY_STATE_* of the match being played */
    u32 rng;
} PartyState;

extern PartyState party;

/* party.c */
void party_start(const VsModeData* vs, u32 seed); /* NULL vs: CPUs (and P1 human) */
int party_advance(int phase);                  /* the phase after a match, or PARTY_END */
void party_reset_scores(void);                 /* coins, stars and the board, for a new party */
void party_setup_phase(int phase, StartMeleeData* start);
void party_preload_phase(int phase);
int party_test_minigame(void);
void party_menu_pick(int index);               /* Party Minigames: the next party plays this one */
void party_board_pick(int index, int debug);   /* the next party is on this board; debug: no minigames */
int party_menu_board(void);                    /* the board last picked in the menu */
int mu_party_menu_on(void);                    /* the menu's Vs. entries are the party's */

/* party_online.c: the party over Slippi Direct (the online major's VS state). */
int party_online_running(void);
int party_rand(int n);                         /* 0..n-1 from the party's own stream */
void party_fill_players(StartMeleeData* start); /* the four party players into a VS start */
void party_preload(StartMeleeData* start);     /* fighters, stage and sound banks */
void party_preload_next(int stkind, int extra_ckind);   /* the next scene's, from a decide */
void party_rules_base(StartMeleeRules* rules, int stkind);
int party_env_int(const char* name, int fallback);
int party_paused(void);                        /* the match is paused (Start) */
void party_log(const char* fmt, ...);

/* board.c: one board turn is one VS match. */
void board_reset(void);
void board_setup(StartMeleeData* start);
void board_fighter_input(struct Fighter* fp);
void board_fighter_map(struct Fighter* fp);     /* its place on the board, off the 2D line */
int board_turn_done(void);                     /* 1 once every player moved this turn */
int board_count(void);                         /* the boards the Melee Party menu lists */
const char* board_name(int index);
const char* board_description(int index);      /* two lines, for the menu */

/* lobby.c: online, the host's choice between games. */
void lobby_setup(StartMeleeData* start);
void lobby_fighter_input(struct Fighter* fp);

/* party_menu.c */
enum { PARTY_MENU_MINIGAMES, PARTY_MENU_BOARDS, PARTY_MENU_DEBUG_BOARDS };
void party_menu_show(int list);                /* what the submenu lists: a PARTY_MENU_* */

/* minigames.c */
typedef struct PartyMinigame {
    const char* name;
    const char* id;                            /* --party-minigame / MELEE_PARTY_MINIGAME */
    void (*setup)(StartMeleeData* start);      /* stage, rules and teams; players are filled */
    void (*fighter_input)(struct Fighter* fp); /* optional: after the pad or the CPU */
    /* Placements, 0 = first; ties share a place. Called once, after the last round. */
    void (*result)(s8 place[PARTY_PLAYERS]);
    int rounds;                                /* sub-matches (party.round); 0 or 1 = one */
    void (*round_end)(void);                   /* after each sub-match, before the next setup */
    /* What the match loads, known before it is set up: the files are preloaded while the scene
     * before it ends (party_preload_next), as Home-Run Contest's CSS preloads the Sandbag. */
    u16 stkind;
    s8 extra_ckind;                            /* a fifth fighter (slot 4), or -1 */
    /* optional: Fighter_procMap, before collision (where it stands, off the 2D line) */
    void (*fighter_map)(struct Fighter* fp);
    /* optional: after collision, where its model is drawn (its collision stays where it is) */
    void (*fighter_drawn)(struct Fighter* fp);
    /* optional: the knockback a hit gives the fighter, from what Melee worked out */
    float (*knockback)(struct Fighter* fp, float kb);
    /* fighter_input drops every pad input (the minigame plays the fighters itself) */
    u8 owns_input;
} PartyMinigame;

int minigame_count(void);
const PartyMinigame* minigame_get(int index);
int minigame_find(const char* id);             /* -1 if none */
int minigame_pick(void);                       /* random, no repeat until all were played */
void minigame_setup(StartMeleeData* start);    /* the chosen one, party.minigame */
int minigame_round_end(void);                  /* 1 if the minigame has another round */
void minigame_finish(void);                    /* placements into coins */

/* minigames.c: a fighter ate an item (the party patch's hook in ftpickupitem.c). */
void minigame_item_eaten(int slot, int item_kind);
extern void (*minigame_on_eaten)(int slot, int item_kind);   /* set in a minigame's setup */

/* results.c: the party's last match, the standings on a podium. */
void results_setup(StartMeleeData* start);
void results_fighter_input(struct Fighter* fp);

#endif
