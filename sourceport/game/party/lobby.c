/* Melee Party online: the lobby. Between online games the host (the first port) picks what to
 * play: a board party, on the board they pick, or a minigame, which comes back here when it is
 * over so they can pick the next one.
 *
 * The lobby is an ordinary party match on Final Destination with the menu written over it. Only
 * the host's pad moves the menu, and the pads are what online play keeps in step, so every side
 * makes the same pick on the same frame: nothing else is sent. Nobody else's input does anything,
 * and there is no time limit: the lobby waits for the host. */
#include <string.h>

#include <melee/ft/inlines.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/controller.h>

#include "party.h"
#include "party_hud.h"

char* getenv(const char* name);

#define ROWS 8
#define ROW_Y0 (-95.0f)
#define ROW_STEP 34.0f
#define REPEAT_FIRST 18   /* frames a held direction waits before it repeats */
#define REPEAT_EVERY 6
#define PICKED_FRAMES 60  /* the pick shows this long, then the match ends */
#define TEST_PICK_FRAME 60

enum Screen { SCREEN_MODE, SCREEN_BOARDS, SCREEN_MINIGAMES };
enum { MODE_BOARD, MODE_MINIGAMES };

static struct {
    int screen;
    int cursor;
    int mode_cursor;  /* where the mode screen's cursor was, for B */
    int frame;
    int held, held_for;   /* the direction held (-1 up, 1 down) and for how long */
    int picked;           /* frames since the pick, 0 before it */
    int paused;
    HSD_Text* text;
    int line_title, line_hint, line_msg;
    int line_row[ROWS];
} lb;

static Fighter* fighter(int slot)
{
    HSD_GObj* gobj = Player_GetEntity(slot);
    return gobj != NULL ? GET_FIGHTER(gobj) : NULL;
}

/* The host's pad: the first port's. */
static HSD_PadStatus* host_pad(void)
{
    Fighter* fp = fighter(0);
    int port = fp != NULL ? fp->pad_port : 0;
    return &HSD_PadGameStatus[port & 3];
}

static int rows(void)
{
    int n = lb.screen == SCREEN_MODE ? 2
          : lb.screen == SCREEN_BOARDS ? board_count() : minigame_offered_count(1);
    return n > ROWS ? ROWS : n;
}

static const char* row_name(int i)
{
    if (lb.screen == SCREEN_MODE) {
        return i == MODE_BOARD ? "Board" : "Minigames";
    }
    return lb.screen == SCREEN_BOARDS ? board_name(i) : minigame_get(minigame_offered_at(i, 1))->name;
}

static void show(void)
{
    int i;
    if (lb.text == NULL) {
        return;
    }
    party_hud_set(lb.text, lb.line_title, lb.screen == SCREEN_MODE ? "P1, choose what to play"
                                          : lb.screen == SCREEN_BOARDS ? "P1, choose a board"
                                                                       : "P1, choose a minigame");
    for (i = 0; i < ROWS; i++) {
        party_hud_set(lb.text, lb.line_row[i], "%s", i < rows() ? row_name(i) : "");
        party_hud_color(lb.text, lb.line_row[i], i == lb.cursor ? PARTY_GOLD : PARTY_WHITE);
    }
    party_hud_set(lb.text, lb.line_hint,
                  lb.screen == SCREEN_MODE ? "Up / Down to move,  A to choose"
                                           : "Up / Down to move,  A to choose,  B to go back");
}

static void open_screen(int screen, int cursor)
{
    lb.screen = screen;
    lb.cursor = cursor >= 0 && cursor < rows() ? cursor : 0;
    show();
}

static void pick_board(int board)
{
    party.lobby_minigames = 0;
    party.board = board;
    party_hud_set(lb.text, lb.line_msg, "%s!", board_name(board));
    party_log("lobby: P1 picks the board %s", board_name(board));
    lb.picked = 1;
}

static void pick_minigame(int minigame)
{
    party.lobby_minigames = 1;
    party.minigame = minigame;
    party.round = 0;
    party_hud_set(lb.text, lb.line_msg, "%s!", minigame_get(minigame)->name);
    party_log("lobby: P1 picks the minigame %s", minigame_get(minigame)->name);
    lb.picked = 1;
}

/* A, B and up/down from the host's pad. A held direction steps once, then repeats. */
static void menu_input(void)
{
    HSD_PadStatus* p = host_pad();
    int dir = (p->button & HSD_PAD_DPADUP) || p->nml_stickY > 0.6f ? -1
            : (p->button & HSD_PAD_DPADDOWN) || p->nml_stickY < -0.6f ? 1 : 0;
    int step = 0;

    if (dir != lb.held) {
        lb.held = dir;
        lb.held_for = 0;
        step = dir;
    } else if (dir != 0 && ++lb.held_for >= REPEAT_FIRST &&
               (lb.held_for - REPEAT_FIRST) % REPEAT_EVERY == 0) {
        step = dir;
    }
    if (step != 0 && rows() > 1) {
        lb.cursor = (lb.cursor + step + rows()) % rows();
        show();
    }

    if (p->trigger & HSD_PAD_A) {
        switch (lb.screen) {
        case SCREEN_MODE:
            lb.mode_cursor = lb.cursor;
            open_screen(lb.cursor == MODE_BOARD ? SCREEN_BOARDS : SCREEN_MINIGAMES,
                 lb.cursor == MODE_BOARD ? party.board : minigame_offered_row(party.minigame, 1));
            break;
        case SCREEN_BOARDS:
            pick_board(lb.cursor);
            break;
        default:
            pick_minigame(minigame_offered_at(lb.cursor, 1));
            break;
        }
    } else if ((p->trigger & HSD_PAD_B) && lb.screen != SCREEN_MODE) {
        open_screen(SCREEN_MODE, lb.screen == SCREEN_BOARDS ? MODE_BOARD : MODE_MINIGAMES);
    }
}

/* MELEE_PARTY_LOBBY_PICK=board or a minigame's id: the lobby picks it by itself (scripted
 * tests). Both sides must set the same. */
static void test_pick(void)
{
    const char* want = getenv("MELEE_PARTY_LOBBY_PICK");
    int mg;
    if (want == NULL || want[0] == '\0' || lb.frame != TEST_PICK_FRAME) {
        return;
    }
    if (strcmp(want, "board") == 0) {
        pick_board(0);
    } else if ((mg = minigame_find(want)) >= 0) {
        pick_minigame(mg);
    }
}

static void lobby_frame(void)
{
    int paused = party_paused();
    if (paused != lb.paused) {
        if (!paused) {
            ifAll_802F3394();   /* unpausing brings back the damage percents and stocks */
        }
        lb.paused = paused;
    }
    if (paused) {
        return;
    }
    lb.frame++;
    if (lb.frame == 1) {
        /* Start pauses only once the match's HUD is on, which "GO!" does; the lobby hides that
         * HUD before it, so it turns pausing on itself. */
        gmVs_GetSceneController()->state.hud_enabled = 1;
    }
    if (lb.picked) {
        if (++lb.picked == PICKED_FRAMES) {
            gm_8016B328();   /* the match ends; the party goes on to the pick (party_advance) */
        }
        return;
    }
    test_pick();
    if (!lb.picked) {
        menu_input();
    }
}

static void lobby_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks in the lobby */
    party_hud_init();
    lb.text = party_hud_text();
    if (lb.text == NULL) {
        return;
    }
    lb.line_title = party_hud_line(lb.text, 0.0f, -170.0f, 0.8f, PARTY_GOLD);
    for (i = 0; i < ROWS; i++) {
        lb.line_row[i] = party_hud_line(lb.text, 0.0f, ROW_Y0 + ROW_STEP * (float) i, 0.7f, PARTY_WHITE);
    }
    lb.line_msg = party_hud_line(lb.text, 0.0f, 105.0f, 1.0f, PARTY_GOLD);
    lb.line_hint = party_hud_line(lb.text, 0.0f, 165.0f, 0.45f, PARTY_WHITE);   /* above the online name tags */
    show();
}

void lobby_setup(StartMeleeData* start)
{
    memset(&lb, 0, sizeof lb);
    /* After a minigame, straight back to the minigames, on the one just played; otherwise the
     * choice between a board party and the minigames. */
    if (party.lobby_minigames) {
        lb.screen = SCREEN_MINIGAMES;
        lb.cursor = party.minigame >= 0 ? party.minigame : 0;
        lb.mode_cursor = MODE_MINIGAMES;
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x30 = 0.0f;   /* no damage */
    start->rules.on_match_start = lobby_start;
    start->rules.on_frame_start = lobby_frame;
    party_fill_players(start);
    party_log("lobby: waiting for P1 (%s)", lb.screen == SCREEN_MODE ? "mode" : "minigames");
}

/* Everyone stands still: the host's pad drives the menu only. */
void lobby_fighter_input(struct Fighter* fp)
{
    fp->input.lstick[0].x = 0.0f;
    fp->input.lstick[0].y = 0.0f;
    fp->input.lstick[1] = fp->input.lstick[0];
    fp->input.cstick[0].x = 0.0f;
    fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    fp->input.held_buttons[0] = 0;
}
