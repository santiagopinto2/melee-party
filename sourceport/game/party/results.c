/* Melee Party: the end of the party. A last scripted match on Final Destination: the players walk
 * to their places in front of a podium (drawn, not solid) in the order of their stars (then coins),
 * the standings show, and the winner taunts. It ends after a while, or when a human presses A once the standings are up. */
#include <math.h>
#include <string.h>

#include <melee/ft/inlines.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>

#include "party.h"
#include "party_draw.h"
#include "party_hud.h"

#define SHOW_AT 150     /* frames before the standings appear */
#define END_AT 720

static const float podium_x[PARTY_PLAYERS] = { 0.0f, -28.0f, 28.0f, -56.0f };
static const float podium_h[PARTY_PLAYERS] = { 14.0f, 9.0f, 5.0f, 0.0f };

static struct {
    int frame;
    int rank[PARTY_PLAYERS];    /* rank of each player, 0 = winner */
    int order[PARTY_PLAYERS];   /* players by rank */
    HSD_Text* text;
    int line_title;
    int line_rank[PARTY_PLAYERS];
} rs;

static Fighter* fighter(int slot)
{
    HSD_GObj* gobj = Player_GetEntity(slot);
    return gobj != NULL ? GET_FIGHTER(gobj) : NULL;
}

static int better(int a, int b)
{
    if (party.p[a].stars != party.p[b].stars) {
        return party.p[a].stars > party.p[b].stars;
    }
    return party.p[a].coins > party.p[b].coins;
}

static void rank_players(void)
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        rs.order[i] = i;
    }
    for (i = 1; i < PARTY_PLAYERS; i++) {
        for (j = i; j > 0 && better(rs.order[j], rs.order[j - 1]); j--) {
            int t = rs.order[j];
            rs.order[j] = rs.order[j - 1];
            rs.order[j - 1] = t;
        }
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        rs.rank[rs.order[i]] = i;
    }
}

static void draw_podium(void)
{
    static const GXColor col[PARTY_PLAYERS] = {
        { 255, 200, 40, 255 }, { 200, 200, 210, 255 }, { 205, 127, 50, 255 }, { 90, 90, 100, 255 },
    };
    int i;
    for (i = 0; i < PARTY_PLAYERS - 1; i++) {
        party_draw_quad(col[i], podium_x[i] - 11.0f, 0.0f, podium_x[i] + 11.0f, podium_h[i], -6.0f);
    }
}

static void results_start(void)
{
    int i;
    ifAll_802F3394();
    party_hud_init();
    rs.text = party_hud_text();
    rs.line_title = party_hud_line(rs.text, 0.0f, -190.0f, 0.95f, PARTY_GOLD);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        rs.line_rank[i] = party_hud_line(rs.text, 0.0f, -140.0f + 26.0f * (float) i, 0.55f,
                                         i == 0 ? PARTY_GOLD : PARTY_WHITE);
    }
    party_hud_set(rs.text, rs.line_title, "THE PARTY IS OVER!");
    party_draw_init(draw_podium);
}

static int any_human_a(void)
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        if (party.p[i].slot_type == Gm_PKind_Human && (HSD_PadGameStatus[i].trigger & HSD_PAD_A)) {
            return 1;
        }
    }
    return 0;
}

static void results_frame(void)
{
    static const char* const nth[PARTY_PLAYERS] = { "1ST", "2ND", "3RD", "4TH" };
    int i;
    rs.frame++;
    if (rs.frame == SHOW_AT) {
        party_hud_set(rs.text, rs.line_title, "P%d WINS THE PARTY!", rs.order[0] + 1);
        for (i = 0; i < PARTY_PLAYERS; i++) {
            const PartyPlayer* pp = &party.p[rs.order[i]];
            party_hud_set(rs.text, rs.line_rank[i], "%s   P%d   %d STAR   %d COIN", nth[i],
                          rs.order[i] + 1, pp->stars, pp->coins);
        }
    }
    if (rs.frame == END_AT || (rs.frame > SHOW_AT + 60 && any_human_a())) {
        gm_8016B328();
    }
}

void results_setup(StartMeleeData* start)
{
    memset(&rs, 0, sizeof rs);
    rank_players();
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.disable_pausing = true;
    start->rules.x30 = 0.0f;
    start->rules.on_match_start = results_start;
    start->rules.on_frame_start = results_frame;
    party_fill_players(start);
    party_log("results: P%d wins (%d stars, %d coins)", rs.order[0] + 1, party.p[rs.order[0]].stars,
              party.p[rs.order[0]].coins);
}

void results_fighter_input(struct Fighter* fp)
{
    int slot = fp->player_idx;
    float dx;

    fp->input.lstick[0].x = fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    fp->input.held_buttons[0] = 0;
    if (slot < 0 || slot >= PARTY_PLAYERS || rs.frame < 30) {
        return;
    }
    dx = podium_x[rs.rank[slot]] - fp->cur_pos.x;
    if (fabsf(dx) > 2.0f) {
        fp->input.lstick[0].x = dx > 0.0f ? (fabsf(dx) < 12.0f ? 0.55f : 1.0f)
                                          : (fabsf(dx) < 12.0f ? -0.55f : -1.0f);
    } else if (rs.rank[slot] == 0 && rs.frame > SHOW_AT && (rs.frame % 90) == 0) {
        fp->input.held_buttons[0] = HSD_PAD_DPADUP;   /* the winner taunts */
    }
}
