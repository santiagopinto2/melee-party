# Melee Party

A Mario Party style board game built into the Source Port. Up to four players move around a board
and play physics minigames between turns. It needs only a vanilla NTSC 1.02 ISO.

## Playing

Start the Source Port (`melee_source.exe`). Party is on by default; `--party off` turns it off.
Then go to **VS. Mode → Tournament Melee**.

1. **Pick characters.** Choose your characters as in a VS match; any empty slots become CPUs
   with random characters. The VS character select only starts a match with at least two
   players, so add a CPU if you play alone.
2. **Play the turns.** Each turn every player rolls a die, which runs 1 to 10. Press A to stop
   it; you get the number shown. Your fighter then walks that many spaces:
   - A blue space gives 3 coins.
   - A red space takes 3 coins.
   - Passing the star space buys a star for 20 coins, and the star then moves somewhere else.
3. **Play a minigame.** After everyone has moved there is a minigame. Placements pay
   10, 5, 3 and 0 coins.
4. **See who won.** After the last turn (10 by default) the players line up in front of a
   podium. The ranking is by stars, then coins.

### Minigames

| Minigame | Stage | Rules |
|---|---|---|
| Volleyball | Final Destination | 2 against 2 with random teams. Hit the ball over the net. It is a point when the ball lands on a side. First to 5, or the higher score at 2:00. |
| Bag Bash | Final Destination | Everyone against one Sandbag for 25 s. Damage you deal to it scores, and knocking it out of the stage scores 40. |
| Food Frenzy | Battlefield | Food rains for 30 s. Pick food up with A to eat it. The player who eats the most wins. |

## Building

Building follows [build-source-port.md](build-source-port.md).
`python tools/prepare_native_sources.py` applies `sourceport/patches/melee-native.patch` and then
`sourceport/patches/melee-party.patch`. The second patch adds the party's hooks into the
decomp. Each hook is a few lines under `#ifdef MU_NATIVE`.

## How it works

- **Mode.** The party takes the unused `GM_HANYU_SSS` mode slot, the same way the Slippi online
  menus take `GM_HANYU_CSS`. The mode has four states: the CSS, a board turn, a minigame and the
  results. Each of the last three is an ordinary VS match driven by callbacks.
- **Board.** The board is a VS match on Final Destination. The board writes every fighter's
  inputs each frame (`mu_party_fighter_input`), so the fighters walk between spaces with their
  own animations. The spaces, the net and the podium are drawn with flat GX shapes and need no
  assets (`party_draw.c`). Text is drawn with the game's own font (`party_hud.c`).
- **Party state.** Players, coins, stars, the turn and a private random number generator live
  in the DLL. They carry over from one match to the next.
- **Retail behaviour.** The hooks only act when the host sets `MU_GAME_OPTION_PARTY`. The host
  never sets it during replay playback, and the game keeps it off online. With party off, the
  menus, VS and online play are unchanged.

| File | Contents |
|---|---|
| `sourceport/game/party/party.c` | The mode, its states, the hooks and the party state |
| `board.c` | The board: dice, walking, spaces, the star and the camera |
| `minigames.c` | The minigame table, picking a minigame, and rewards |
| `mg_volley.c`, `mg_sandbag.c`, `mg_food.c` | The three minigames |
| `results.c` | The podium |

### Adding a minigame

1. Write `mg_<name>.c` that defines a `const PartyMinigame`. It holds a name, an id, a setup
   function, optional CPU input, a result function that returns placements, and the stage plus
   any extra fighter (so the next scene can preload them).
2. Add it to `table[]` in `minigames.c`.

## Testing knobs

Set these as environment variables for `melee_source.exe`.

| Variable | Effect |
|---|---|
| `MELEE_PARTY_BOOT=1` | Boot straight into the party. |
| `MELEE_PARTY_SKIP_CSS=1` | Skip the character select: P1 is human and the others are random CPUs. |
| `MELEE_PARTY_ALL_CPU=1` | With the previous knob, every player is a CPU. |
| `MELEE_PARTY_MINIGAME=volley\|sandbag\|food` | Play only this minigame, over and over. |
| `MELEE_PARTY_ORDER=food,volley,...` | Minigames in this order. |
| `MELEE_PARTY_TURNS=n` | Number of turns. The default is 10. |
| `MELEE_PARTY_SEED=n` | A fixed seed for the party's own random numbers. |

For example, to watch a whole three-turn party with CPUs:

    MELEE_PARTY_BOOT=1 MELEE_PARTY_SKIP_CSS=1 MELEE_PARTY_ALL_CPU=1 MELEE_PARTY_TURNS=3 \
      melee_source.exe --iso <iso> --slippi-menus off

## Known limits

- The Tournament Melee button still shows its retail art and description.
- The board is a single 12-space loop on Final Destination.
- Coin space events, items and shops are not implemented.
- The CPU scripts are simple, and the ball in Volleyball needs tuning in real play.
- Party is local only. The host does not offer it online or in replays.
