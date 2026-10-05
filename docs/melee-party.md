# Melee Party

A Mario Party style board game built into the Source Port. Up to four players move around a board
and play physics minigames between turns. It needs only a vanilla NTSC 1.02 ISO.

## Playing

Start the Source Port (`melee_source.exe`). Party is on by default; `--party off` turns it off.
Then go to **VS. Mode → Melee Party** (the retail Tournament Melee entry) and pick a board
(only Goomba's Greedy Gala so far). To play one minigame on its own, go to **VS. Mode → Party
Minigames** (the retail Special Melee entry) and pick it: after the CSS it plays, then comes
back to the CSS. **VS. Mode → Debug Boards** (the retail Custom Rules entry) lists the same
boards, but a party started there plays board turn after board turn with no minigames, for
testing a board. After a party the menu opens on the list it was started from.

1. **Pick characters.** Choose your characters as in a VS match; any empty slots become CPUs
   with random characters. The VS character select only starts a match with at least two
   players, so add a CPU if you play alone.
2. **Play the turns.** Each turn every player rolls a die, which runs 1 to 10. Press A to stop
   it; you get the number shown. Your fighter then walks that many spaces along the board by
   itself; the space it walks to next blinks gold. Players cannot move, jump or attack on the
   board: the only inputs are A on the die and choosing a path where it splits.
   - The board is Mario Party 4's Goomba's Greedy Gala. Where the path splits, point the stick
     at the way you want (the gold arrow) and press A.
   - The path into the middle ends on the Goomba's wheel, which spins and sends you out one of
     its four coloured ways at random.
   - A blue space gives 3 coins.
   - A red space takes 3 coins.
   - A green space does nothing yet.
   - Passing the star space buys a star for 20 coins without using a step, and the star then
     moves to one of the board's other seven star spaces.
   - The arrows by the path (shops, the Boo house, the lottery) and the green pipes are not in
     play yet.
3. **Play a minigame.** After everyone has moved there is a minigame. Placements pay
   10, 5, 3 and 0 coins.
4. **See who won.** After the last turn (10 by default) the players line up in front of a
   podium. The ranking is by stars, then coins.

### Minigames

| Minigame | Stage | Rules |
|---|---|---|
| Volleyball | Final Destination | 2 against 2 with random teams. Hit the ball over the net; nobody can cross it. It is a point when the ball lands on a side. First to 5, or the higher score at 2:00. The camera follows the ball up. |
| Bag Bash | Final Destination | Everyone against the Sandbag for 60 s. Damage you deal to it scores, and knocking it out of the stage scores 40. The Sandbags take half the knockback. With 10 s left a yellow Sandbag joins, worth 3 times the points. |
| Food Frenzy | Battlefield | Food rains for 30 s. Pick food up with A to eat it. The player who eats the most wins. |
| Domination | Final Destination (a field over it) | After Mario Party 4's. Everyone holds a hammer; each A press is one swing at your Wobbuffet, and each swing drops a Snorlax into your lane. After 10 s the Snorlaxes fall over like dominoes while each lane counts up. The most Snorlaxes wins. The lanes start full of Snorlaxes that jump into the sky before the start; at the end a Snorlax holding a parasol comes out of the ground in each winner's lane, and the winners drop onto it from the sky and taunt. |

## Playing online

Online party works over Slippi Online with the existing connect codes, connection and rollback
netcode. There are two ways in:

| Players | Mode | How to connect |
|---|---|---|
| 2 | **Direct** | Enter each other's connect code. Two CPUs join as P3 and P4. |
| 3 or 4 | **Teams** | Everyone enters the same connect code, the way Slippi Teams forms a private group. Any seat left empty becomes a CPU. |

1. **Connect.** Every player runs Melee Party with `--party on` (the default) and no other mod
   active. Open Online Play → Direct or Teams, enter the code, and pick a character. The team
   colours on the Teams character select do not matter, because the party decides its own teams.
2. **Play.** When everyone locks in, the party starts. Every board turn, minigame and the podium
   is a separate online game.
   Between games there is a short pause, about 2 seconds, while the next game is agreed, then it
   starts by itself. You do not go back to the character select.
3. **Finish.** When the party ends, both players return to the online character select.
   L+R+A+Start in any party game, a disconnect, or an opponent who does not start the next game
   within 30 seconds also ends the party there.

How it fits together:

- **Who can connect.** In Direct and Teams, the host tells the other players it is the "Melee
  Party" build (an identity of its own, `kPartyFingerprint` in `port/app/source_host.cpp`). The
  existing build check (`build_verdict`, which now applies the same-build rule to Teams for this
  build) then only connects Melee Party builds of the same protocol. The retail game, Slippi
  Dolphin and other mods are refused. Change the protocol string whenever a change to the party
  would make two builds play different matches.
- **Unranked.** This mode is unchanged and still plays normal Melee.
- **Rollback.** The party's state lives in the game DLL's memory, which the rollback snapshots
  already cover. Nothing about rollback is party-specific.
- **Idle players.** If a human doesn't press A on the board, they roll automatically after
  10 seconds, so an idle player cannot stall the game.
- **Replays.** Party games are not recorded as `.slp` replays, because a replay of one could not
  be played back.

### Testing online locally

```
MELEE_PARTY_ONLINE_TEST=1 MELEE_PARTY_TURNS=2 MELEE_PARTY_AUTO_ROLL=1   python tools/online_pair.py --exe build-review/port/Release/melee_source.exe     --script port/scripts/online_bot.txt --frames 16000
```

- `MELEE_PARTY_ONLINE_TEST=1` lets the local test pair, which connects through Unranked, play the
  party.
- `MELEE_PARTY_AUTO_ROLL=1` rolls for idle humans after half a second.
- To force rollbacks, add `MELEE_NET_LAG_MS=80` to one of the two instances (run them with
  `--only A` and `--only B`).
- Each instance logs `checksums agree ... 0 mismatched` and `re-simulations` counts.
- For 3 or 4 players through the real Teams menus and connect-code entry, use
  `MELEE_PARTY_TURNS=1 MELEE_PARTY_AUTO_ROLL=1 python tools/online_quad.py --players 4
  --engines build-review/port/Release/melee_source.exe --lag-ms 40 --frames 14000`.

## Building

Building follows [build-source-port.md](build-source-port.md).
`python tools/prepare_native_sources.py` applies `sourceport/patches/melee-native.patch` and then
`sourceport/patches/melee-party.patch`. The second patch adds the party's hooks into the
decomp. Each hook is a few lines under `#ifdef MU_NATIVE`.

## How it works

- **Menu.** The VS. Mode entries' names are textures in MnMaAll, and no disc asset ships with
  the build, so `party/party_menu.c` makes the new names when the menu loads: each letter is cut
  out of a label that has it (along the slant for the italic ones), the letters are laid out
  and squeezed to fit as the game's long labels are, and the outlined labels get their outline
  drawn again. F is E without its bottom bar, the slanted headers' B is P's bowl over D's lower
  half, and z and the apostrophe are drawn; a name with a letter that no label has logs
  `menu: no letter`. Melee Party, Party Minigames and Debug Boards all open Special Melee's
  submenu, whose header and rows are written again with the boards' or the minigames' names
  each time one of them opens it. The letter positions were measured from the NTSC 1.02
  disc. The Special Melee submenu shows one row per minigame, and the descriptions of the changed
  entries are the party's own text.
- **Mode.** The party takes the unused `GM_HANYU_SSS` mode slot, the same way the Slippi online
  menus take `GM_HANYU_CSS`. The mode has four states: the CSS, a board turn, a minigame and the
  results. Each of the last three is an ordinary VS match driven by callbacks.
- **Board.** The board is a VS match on Final Destination, seen from above: Goomba's Greedy Gala
  from Mario Party 4, seen from the same side as MP4's board camera and scaled so its wide side
  fits between the stage's edges, on a floor the board draws itself (Final Destination is far wider than it is
  deep, and fighters can stand anywhere in depth). Its 135 spaces and path nodes, with their MP4
  types, flags and links, are in `board_ggg.h`, which `tools/extract_mp4_board.py` writes from a
  Mario Party 4 (USA) disc (`data/w02.bin`; format from partyboard's `src/game/board/space.c`).
  `board.c` maps the MP4 types: Mushroom spaces are blue; Bowser spaces are red; Happening, Battle,
  Fortune and Warp spaces are green. Hidden path nodes (type 0) do not count as steps. Pipes
  (Mini Mushroom paths in MP4) are never offered. The match camera runs in its debug free mode,
  which has no update of its own, and the board sets its eye and target every frame.
  Melee only moves fighters along its 2D line, so the board keeps each fighter's place on the
  floor (x and depth z) itself:
  - `mu_party_fighter_input` (`Fighter_procInput`) writes every fighter's inputs. Holding the
    stick toward the side it faces makes it walk or run with its own animations and speed.
  - `mu_party_fighter_map` (`Fighter_procMap`, before collision) moves that place by the
    distance the fighter's ground speed covered, in the direction the board chose. It puts the
    fighter there and turns its model to face that way.

  The spaces, the net and the podium are drawn with flat GX shapes and need no
  assets (`party_draw.c`). Text is drawn with the game's own font (`party_hud.c`).
- **Party state.** Players, coins, stars, the turn and a private random number generator live
  in the DLL. They carry over from one match to the next.
- **Retail behaviour.** The hooks only act when the host sets `MU_GAME_OPTION_PARTY` (and, for
  online, `MU_GAME_OPTION_PARTY_ONLINE`). The host never sets either during replay playback. The
  online party runs only in Direct, against another Melee Party build. With party off, the menus,
  VS and online play are unchanged.

| File | Contents |
|---|---|
| `sourceport/game/party/party.c` | The mode, its states, the hooks and the party state |
| `board.c` | The board: dice, walking, paths and splits, the roulette, the star and the camera |
| `board_ggg.h` | Goomba's Greedy Gala's layout (generated by `tools/extract_mp4_board.py`) |
| `minigames.c` | The minigame table, picking a minigame, and rewards |
| `mg_volley.c`, `mg_sandbag.c`, `mg_food.c`, `mg_domination.c` | The four minigames |
| `results.c` | The podium |
| `party_online.c` | The party over Slippi Direct |

### Adding a minigame

1. Write `mg_<name>.c` that defines a `const PartyMinigame`. It holds a name, an id, a setup
   function, optional CPU input, a result function that returns placements, and the stage plus
   any extra fighter (so the next scene can preload them). An optional `fighter_map` runs in
   `Fighter_procMap` before collision, to place fighters off the 2D line (Domination uses it).
2. Add it to `table[]` in `minigames.c`.

## Testing knobs

Set these as environment variables for `melee_source.exe`.

| Variable | Effect |
|---|---|
| `MELEE_PARTY_BOOT=1` | Boot straight into the party. |
| `MELEE_PARTY_SKIP_CSS=1` | Skip the character select: P1 is human and the others are random CPUs. |
| `MELEE_PARTY_ALL_CPU=1` | With the previous knob, every player is a CPU. |
| `MELEE_PARTY_MINIGAME=volley\|sandbag\|food\|domination` | Play only this minigame, over and over. |
| `MELEE_PARTY_ORDER=food,volley,...` | Minigames in this order. |
| `MELEE_PARTY_TURNS=n` | Number of turns. The default is 10. |
| `MELEE_PARTY_SEED=n` | A fixed seed for the party's own random numbers. |
| `MELEE_PARTY_AUTO_ROLL=1` | Idle humans roll after half a second instead of 10 seconds. |
| `MELEE_PARTY_ONLINE_TEST=1` | Online party in any online mode, for the local test pair. |
| `MELEE_PARTY_DEBUG_BOARDS=1` | No minigames between board turns, as when started from Debug Boards. |

For example, to watch a whole three-turn party with CPUs:

    MELEE_PARTY_BOOT=1 MELEE_PARTY_SKIP_CSS=1 MELEE_PARTY_ALL_CPU=1 MELEE_PARTY_TURNS=3 \
      melee_source.exe --iso <iso> --slippi-menus off

## Known limits

- The Party Minigames and board rows keep the Special Melee modes' preview pictures.
- Debug Boards replaces Custom Rules while the party is on; `--party off` brings it back.
- Items, shops, the Boo house, the lottery, pipes and the Gamble Goombas are not implemented.
  Happening, Battle, Fortune and Warp spaces have no events (they are plain green spaces).
- The roulette has no Goomba bribe and no free-choice pockets: it picks one of its four ways at
  random.
- The CPU scripts are simple, and the ball in Volleyball needs tuning in real play.
- Online parties need 2 to 4 players. 3 or 4 players go through Teams, which requires the Slippi
  matchmaking server to group everyone who entered the same code.
- Between online games the screen holds for about 2 seconds while the next game is agreed.
