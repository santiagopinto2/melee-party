/* Melee Party: the mode, its states and the state that lives across them (see party.h). */
#include <stdarg.h>
#include <string.h>

#include <dolphin/os.h>
#include <melee/ft/types.h>
#include <melee/gm/gm_1601.h>
#include <melee/gm/gm_1A3F.h>
#include <melee/gm/gmscdata.h>
#include <melee/gm/gmvsmelee.h>
#include <melee/lb/lbaudio_ax.h>
#include <melee/lb/lbdvd.h>
#include <melee/lb/types.h>
#include <melee/mn/forward.h>
#include <sysdolphin/baselib/random.h>

#include "party.h"

/* From the host's C runtime: the game's own library (MSL) has neither (see mu_shim.h). */
char* getenv(const char* name);
int vsnprintf(char* buffer, __SIZE_TYPE__ size, const char* format, __builtin_va_list args);
int atoi(const char* s);

PartyState party;

static VsModeData party_vs;   /* the CSS's choice */
static int party_vs_ready;
static int installed;
static int flag = -1;

/* ---- gate ---- */

/* The host's bit, latched at boot like the Slippi menus flag, and never during replay playback
 * or online play: party matches are not recorded and cannot be played back. */
static int party_enabled(void)
{
    if (flag < 0) {
        flag = (mu_game_options() & MU_OPTION_PARTY) != 0;
    }
    return flag && !mu_replay_on() && !mu_online_active();
}

int mu_party_active(void)
{
    return (installed && party_enabled() && gm_GetCurrentGameMode() == PARTY_MODE) ||
           party_online_running();
}

/* ---- helpers ---- */

void party_log(const char* fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    OSReport("[party] %s\n", line);
}

int party_env_int(const char* name, int fallback)
{
    const char* v = getenv(name);
    if (v == NULL || *v == '\0') {
        return fallback;
    }
    return atoi(v);
}

/* xorshift32, seeded once per party from the game's RNG: party decisions never draw from the
 * match stream, so a board turn cannot change what a match would have rolled. */
int party_rand(int n)
{
    u32 x = party.rng ? party.rng : 0x2545F491u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    party.rng = x;
    return n > 0 ? (int) (x % (u32) n) : 0;
}

void party_rules_base(StartMeleeRules* rules, int stkind)
{
    gm_SetupRulesDefaults(rules);
    rules->stkind = (u16) stkind;
    rules->match_kind = MatchKind_Time;
    rules->timer_enabled = false;
    rules->time_limit = 0;
    rules->is_teams = false;
    rules->item_freq = -1;    /* no random items: minigames spawn their own */
    rules->x20 = 0;           /* item mask */
    rules->game_speed = 1.0f;
    rules->x30 = 1.0f;        /* damage ratio */
}

void party_fill_players(StartMeleeData* start)
{
    int i;
    for (i = 0; i < GM_MAX_PLAYERS; i++) {
        gm_SetupPlayerDefaults(&start->players[i]);
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        PlayerInitData* pd = &start->players[i];
        const PartyPlayer* pp = &party.p[i];
        pd->ckind = pp->ckind;
        pd->color = pp->color;
        pd->slot_type = pp->slot_type;
        pd->cpu_level = pp->cpu_level;
        pd->cpu_kind = 4;     /* a VS Mode CPU, as the CSS sets it */
        pd->nametag = pp->nametag;
        pd->team = (u8) i;
        pd->stocks = 0;
        pd->rumble_enabled = false;
    }
}

/* What gmVsMelee_ExitCss/ExitSss and the Slippi splash do before a VS scene: the fighters and the
 * stage into the preload cache, and their sound banks. */
void party_preload(StartMeleeData* start)
{
    PreloadedGameModeState* cache = lbDvd_GetPreloadCacheScene();
    u64 mask = 0;
    int i;

    lbDvd_SetupVsPreloadCache();
    /* Every slot a minigame uses, the fifth (Bag Bash's Sandbag) included. */
    for (i = 0; i < GM_MAX_PLAYERS; i++) {
        int used = start->players[i].slot_type != Gm_PKind_NA;
        cache->game_cache.entries[i].char_id = used ? start->players[i].ckind : ChKind_None;
        cache->game_cache.entries[i].color = used ? start->players[i].color : 0;
    }
    cache->game_cache.stkind = start->rules.stkind;
    lbDvd_80018254();
    /* The loads are queued, not done: wait for them as the Slippi splash does, or a fighter new
     * to this scene (Bag Bash's Sandbag after a board turn) starts with its files half read. */
    lbDvd_80018C2C(199);
    lbDvd_80017700(4);

    lbAudioAx_80026F2C(0x1C);
    for (i = 0; i < GM_MAX_PLAYERS; i++) {
        if (start->players[i].slot_type != Gm_PKind_NA) {
            mask |= lbAudioAx_80026E84((CharacterKind) start->players[i].ckind);
        }
    }
    mask |= lbAudioAx_80026EBC((StKind) start->rules.stkind);
    lbAudioAx_8002702C(4, mask);
    lbAudioAx_80027168();
}

/* From a state's decide: the next match's fighters and stage, so they load while this scene ends
 * and before the next one is prepared (the state runner preloads before the prep callback). */
void party_preload_next(int stkind, int extra_ckind)
{
    static StartMeleeData next;
    party_fill_players(&next);
    if (extra_ckind >= 0) {
        gm_SetupPlayerDefaults(&next.players[4]);
        next.players[4].ckind = (s8) extra_ckind;
        next.players[4].slot_type = Gm_PKind_Cpu;
    }
    next.rules.stkind = (u16) stkind;
    party_preload(&next);
}

/* ---- the party ---- */

static const s8 cpu_pool[] = {
    CKind_Mario, CKind_Luigi, CKind_Peach, CKind_Yoshi, CKind_Donkey, CKind_Fox, CKind_Falco,
    CKind_Pikachu, CKind_Kirby, CKind_Link, CKind_Samus, CKind_Captain, CKind_Ness, CKind_Purin,
    CKind_Koopa, CKind_Mars,
};

static int ckind_taken(s8 ckind, int upto)
{
    int i;
    for (i = 0; i < upto; i++) {
        if (party.p[i].ckind == ckind) {
            return 1;
        }
    }
    return 0;
}

/* The CSS's players; empty slots become CPUs with characters nobody picked. seed 0: one from the
 * game's generator (offline); online both sides pass the match's shared seed. */
void party_start(const VsModeData* vs, u32 seed)
{
    int i;
    int forced = party_env_int("MELEE_PARTY_SEED", 0);

    if (seed == 0) {
        seed = (u32) HSD_Randi(0x7FFFFFFF) ^ 0x9E3779B9u;
    }
    memset(&party, 0, sizeof party);
    party.rng = forced ? (u32) forced : (seed ? seed : 1);
    party.max_turns = party_env_int("MELEE_PARTY_TURNS", 10);
    party.minigame = -1;

    for (i = 0; i < PARTY_PLAYERS; i++) {
        const PlayerInitData* src = vs != NULL ? &vs->start.players[i] : NULL;
        PartyPlayer* pp = &party.p[i];
        if (src != NULL && src->slot_type <= Gm_PKind_Cpu && src->ckind < CKind_Playable_Count) {
            pp->ckind = src->ckind;
            pp->color = src->color;
            pp->slot_type = src->slot_type;
            pp->cpu_level = src->slot_type == Gm_PKind_Cpu ? src->cpu_level : 0;
            pp->nametag = src->nametag;
        } else {
            s8 ck;
            do {
                ck = cpu_pool[party_rand((int) (sizeof cpu_pool / sizeof cpu_pool[0]))];
            } while (ckind_taken(ck, i));
            pp->ckind = ck;
            pp->color = 0;
            pp->slot_type = (vs == NULL && i == 0 && !party_env_int("MELEE_PARTY_ALL_CPU", 0))
                                ? Gm_PKind_Human
                                : Gm_PKind_Cpu;
            pp->cpu_level = 5;
            pp->nametag = GM_NAMETAG_COUNT;
        }
        pp->coins = 10;
    }
    /* Two players on one character: tell them apart by costume. */
    for (i = 1; i < PARTY_PLAYERS; i++) {
        int j;
        for (j = 0; j < i; j++) {
            if (party.p[j].ckind == party.p[i].ckind && party.p[j].color == party.p[i].color) {
                party.p[i].color = (u8) (party.p[i].color + 1);
                j = -1;
            }
        }
    }
    board_reset();
    party_log("start: %d turns, players %d/%d/%d/%d (%s %s %s %s)", party.max_turns,
              party.p[0].ckind, party.p[1].ckind, party.p[2].ckind, party.p[3].ckind,
              party.p[0].slot_type == Gm_PKind_Human ? "hmn" : "cpu",
              party.p[1].slot_type == Gm_PKind_Human ? "hmn" : "cpu",
              party.p[2].slot_type == Gm_PKind_Human ? "hmn" : "cpu",
              party.p[3].slot_type == Gm_PKind_Human ? "hmn" : "cpu");
}

/* ---- states ---- */

static int test_minigame = -1;   /* MELEE_PARTY_MINIGAME: play only this one, over and over */
/* Party Minigames (party_menu.c): the one picked in the menu, played after the CSS and then back
 * to it. */
static int menu_minigame = -1;
static int from_menu;

void party_menu_pick(int index)
{
    menu_minigame = index;
}

/* The menu's Vs. entries are the party's. */
int mu_party_menu_on(void)
{
    return installed && party_enabled();
}

static void css_prep(GameModeState* state)
{
    gmVsMelee_EnterCss(state, &party_vs, VS_MELEE);
}

static void css_decide(GameModeState* state)
{
    CSSData* css = gm_GetGameModeStateExitData(state);
    if (css->pending_scene_change == CSSPendingSceneChange_2) {
        gm_ChangeGameModeAfterCurrentScene(GM_MENU);
        return;
    }
    gmVsMelee_ExitCss(state, &party_vs);
    party_start(&party_vs, 0);
    if (from_menu) {
        party.minigame = test_minigame;
        party_preload_phase(PARTY_STATE_MINIGAME);
        gm_SetNextGameModeStateId(PARTY_STATE_MINIGAME);
        return;
    }
    gm_SetNextGameModeStateId(PARTY_STATE_BOARD);
}

int party_test_minigame(void)
{
    return test_minigame;
}

/* What follows a party match: the next phase (a PARTY_STATE_*), or PARTY_END. Shared by the
 * offline mode's states and the online party (party_online.c), so both play the same party. */
int party_advance(int phase)
{
    switch (phase) {
    case PARTY_STATE_BOARD:
        party.minigame = minigame_pick();
        party_log("turn %d: board done, minigame %s", party.turn, minigame_get(party.minigame)->name);
        return PARTY_STATE_MINIGAME;
    case PARTY_STATE_MINIGAME:
        if (minigame_round_end()) {
            return PARTY_STATE_MINIGAME;
        }
        minigame_finish();
        if (test_minigame >= 0) {
            return from_menu ? PARTY_STATE_CSS : PARTY_STATE_MINIGAME;
        }
        return party.turn >= party.max_turns ? PARTY_STATE_RESULTS : PARTY_STATE_BOARD;
    default:
        party_log("party over");
        return PARTY_END;
    }
}

/* Sets up the match of a phase into start (stage, rules, players, callbacks). */
void party_setup_phase(int phase, StartMeleeData* start)
{
    party.phase = phase;
    switch (phase) {
    case PARTY_STATE_BOARD:
        board_setup(start);
        break;
    case PARTY_STATE_MINIGAME:
        if (test_minigame >= 0) {
            party.minigame = test_minigame;
        }
        minigame_setup(start);
        break;
    default:
        results_setup(start);
        break;
    }
}

/* The next phase's fighters and stage, preloaded while the scene before it ends. */
void party_preload_phase(int phase)
{
    if (phase == PARTY_STATE_MINIGAME) {
        party_preload_next(minigame_get(party.minigame)->stkind,
                           minigame_get(party.minigame)->extra_ckind);
    } else if (phase == PARTY_STATE_BOARD || phase == PARTY_STATE_RESULTS) {
        party_preload_next(St_Kind_Last, -1);
    }
}

/* L+R+A+Start (No Contest) in any party match leaves the party for the menu. */
static int quit(GameModeState* state)
{
    MatchExitInfo* exit = gm_GetGameModeStateExitData(state);
    if (exit == NULL || exit->match_end.outcome != OUTCOME_NO_CONTEST) {
        return 0;
    }
    party_log("quit (No Contest) at turn %d", party.turn);
    party.round = 0;
    gm_ChangeGameModeAfterCurrentScene(GM_MENU);
    return 1;
}

static void party_finish(void)
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        party_log("final: P%d stars %d coins %d", i + 1, party.p[i].stars, party.p[i].coins);
    }
    gm_ChangeGameModeAfterCurrentScene(GM_MENU);
}

static void match_prep(GameModeState* state)
{
    StartMeleeData* start = gm_GetGameModeStateEnterData(state);
    party_setup_phase(state->id, start);
    party_preload(start);
}

static void match_decide(GameModeState* state)
{
    int next;
    if (quit(state)) {
        return;
    }
    next = party_advance(state->id);
    if (next == PARTY_END) {
        party_finish();
        return;
    }
    party_preload_phase(next);
    gm_SetNextGameModeStateId((u8) next);
}

static void party_on_load(void)
{
    const char* mg = getenv("MELEE_PARTY_MINIGAME");
    if (!party_vs_ready) {
        party_vs = *gmVsMelee_GetVsData();   /* the player's VS rules and last picks */
        party_vs_ready = 1;
    }
    party_vs.start.rules.is_teams = false;
    test_minigame = mg != NULL ? minigame_find(mg) : -1;
    from_menu = test_minigame < 0 && menu_minigame >= 0;
    if (from_menu) {
        test_minigame = menu_minigame;
    }

    if (!from_menu && (party_env_int("MELEE_PARTY_SKIP_CSS", 0) || test_minigame >= 0)) {
        party_start(NULL, 0);
        gm_SetGameModeStateId(test_minigame >= 0 ? PARTY_STATE_MINIGAME : PARTY_STATE_BOARD);
    }
    party_log("mode loaded%s", from_menu ? " (Party Minigames)"
                               : test_minigame >= 0 ? " (single minigame test)" : "");
}

static void party_on_unload(void)
{
    party_log("mode unloaded");
}

static GameModeState party_states[] = {
    { PARTY_STATE_CSS, lbDvdPreload_3, 0, css_prep, css_decide,
      { GS_CSS, &gmVsMelee_CssData, &gmVsMelee_CssData } },
    { PARTY_STATE_BOARD, lbDvdPreload_3, 0, match_prep, match_decide,
      { GS_VS, &gmVsMelee_StartData, &gmVsMelee_VsExitInfo } },
    { PARTY_STATE_MINIGAME, lbDvdPreload_3, 0, match_prep, match_decide,
      { GS_VS, &gmVsMelee_StartData, &gmVsMelee_VsExitInfo } },
    { PARTY_STATE_RESULTS, lbDvdPreload_3, 0, match_prep, match_decide,
      { GS_VS, &gmVsMelee_StartData, &gmVsMelee_VsExitInfo } },
    { GM_GAMEMODESTATE_TERMINATE },
};

/* ---- hooks (declared in mu_native.h) ---- */

/* gm_801A4510, once at boot, after the Slippi install: the party takes GM_HANYU_SSS. */
void mu_party_scene_install(void)
{
    GameMode* mode;
    if (!party_enabled()) {
        return;
    }
    for (mode = gm_GetAllGameModes(); mode->kind != GM_COUNT; mode++) {
        if (mode->kind == PARTY_MODE) {
            mode->preloaded = true;
            mode->on_load = party_on_load;
            mode->on_unload = party_on_unload;
            mode->states = party_states;
            installed = 1;
            party_log("installed in mode slot 0x%02X", PARTY_MODE);
            break;
        }
    }
}

/* gmboot.c: MELEE_PARTY_BOOT=1 boots straight into the party (testing). */
unsigned char mu_party_boot_mode(unsigned char mode)
{
    if (installed && party_enabled() && party_env_int("MELEE_PARTY_BOOT", 0)) {
        return PARTY_MODE;
    }
    return mode;
}

/* mn/mnmain.c: the Vs. menu's Tournament Melee entry opens the party. */
int mu_party_vs_menu_mode(int retail_mode)
{
    if (!installed || !party_enabled()) {
        return retail_mode;
    }
    menu_minigame = -1;   /* the party itself, not a minigame */
    return PARTY_MODE;
}

/* gm/gmmenumode.c: back from the party, the cursor is on its entry again. */
int mu_party_menu_enter(int previous_mode, unsigned char* menu_kind, unsigned char* hovered)
{
    if (!installed || !party_enabled() || previous_mode != PARTY_MODE) {
        return 0;
    }
    if (menu_minigame >= 0) {
        *menu_kind = MENU_KIND_SPECIAL;   /* Party Minigames, on the one just played */
        *hovered = (unsigned char) menu_minigame;
    } else {
        *menu_kind = MENU_KIND_VS;
        *hovered = SEL_VS_TOURNAMENT;
    }
    return 1;
}

/* ft/kinds/ftCommon/ftpickupitem.c: a fighter ate a healing item (food). */
void mu_party_item_eaten(int slot, int item_kind)
{
    if (mu_party_active() && party.phase == PARTY_STATE_MINIGAME) {
        minigame_item_eaten(slot, item_kind);
    }
}

/* ft/fighter.c Fighter_procInput, after the pad (or the CPU) and the replay hook. */
void mu_party_fighter_input(struct Fighter* fp)
{
    if (!mu_party_active()) {
        return;
    }
    switch (party.phase) {
    case PARTY_STATE_BOARD:
        board_fighter_input(fp);
        break;
    case PARTY_STATE_RESULTS:
        results_fighter_input(fp);
        break;
    case PARTY_STATE_MINIGAME:
        if (party.minigame >= 0 && minigame_get(party.minigame)->fighter_input != NULL) {
            minigame_get(party.minigame)->fighter_input(fp);
        }
        break;
    default:
        break;
    }
}

/* ft/fighter.c Fighter_procMap, before the fighter's collision. */
void mu_party_fighter_map(struct Fighter* fp)
{
    if (!mu_party_active()) {
        return;
    }
    if (party.phase == PARTY_STATE_BOARD) {
        board_fighter_map(fp);
    } else if (party.phase == PARTY_STATE_MINIGAME && party.minigame >= 0 &&
               minigame_get(party.minigame)->fighter_map != NULL) {
        minigame_get(party.minigame)->fighter_map(fp);
    }
}

/* shim/mu_gecko.c (UCF): 1 while the party drops every pad input of this fighter, so nothing
 * reads the pad again after it. */
int mu_party_owns_input(struct Fighter* fp)
{
    (void) fp;
    if (!mu_party_active()) {
        return 0;
    }
    switch (party.phase) {
    case PARTY_STATE_BOARD:
    case PARTY_STATE_RESULTS:
        return 1;
    case PARTY_STATE_MINIGAME:
        return party.minigame >= 0 && minigame_get(party.minigame)->owns_input;
    default:
        return 0;
    }
}

/* ft/kinds/ftCommon/ftCo_Damage.c ftCo_Damage_CalcKnockback: the knockback of a hit. */
float mu_party_knockback(struct Fighter* fp, float kb)
{
    if (mu_party_active() && party.phase == PARTY_STATE_MINIGAME && party.minigame >= 0 &&
        minigame_get(party.minigame)->knockback != NULL) {
        return minigame_get(party.minigame)->knockback(fp, kb);
    }
    return kb;
}

/* ft/fighter.c, once the fighter's model is placed at cur_pos (Fighter_procMap's end and
 * Fighter_procAccessory). */
void mu_party_fighter_drawn(struct Fighter* fp)
{
    if (mu_party_active() && party.phase == PARTY_STATE_MINIGAME && party.minigame >= 0 &&
        minigame_get(party.minigame)->fighter_drawn != NULL) {
        minigame_get(party.minigame)->fighter_drawn(fp);
    }
}
