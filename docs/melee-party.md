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

With a Mario Party 4 (USA) disc as a plain `.iso` next to the Melee ISO, named `mp4.iso` (or
given with `--mp4-iso <path>` or `MELEE_PARTY_MP4_ISO`), the party also plays MP4's own
minigames, run from MP4's code and drawn from the disc (the table below says which). Without the
disc they are left out of the list and the rotation, and the log says why.

1. **Pick characters.** Choose your characters as in a VS match; any empty slots become CPUs
   with random characters. The VS character select only starts a match with at least two
   players, so add a CPU if you play alone.
2. **Play the turns.** Each turn every player rolls a die, which runs 1 to 10. Press A to stop
   it; you get the number shown. Your fighter then walks that many spaces along the board by
   itself; the space it walks to next blinks gold. Players cannot move, jump or attack on the
   board: the only inputs are A on the die and choosing a path where it splits. Start pauses
   the board and every minigame as in a normal match; on the board the pause screen shows the
   turn number.
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
| Dungeon Duos | Final Destination (a dungeon drawn away from it) | After Mario Party 4's, on a split screen with its camera. Random teams of 2 each run their own copy of the dungeon, and the first team out wins. Mash B at a switch to open your partner's gate; mash A at a crank to turn a bar over a pit, jump on and ride it across; find the hole that leads on (the others send you to another hole); then alternate L and R at the pump, and the team's strokes add up to 1000. The stick moves you in any direction with Melee's walk, dash and run, and X/Y jump and double jump. A player who falls into a pit comes back at the last checkpoint. Nobody wins after 5 minutes. |
| Bowser's Bigger Blast | Final Destination (MP4's stage drawn away from it); needs the MP4 disc | Mario Party 4's own. Each player in turn pushes one of five plungers; one of them sets Bowser's bomb off, and the player who pushed it is out. The last one standing wins. The fighters only follow the game: nobody walks by the stick. |
| Chain Chomp Fever | Final Destination (MP4's arena drawn away from it); needs the MP4 disc | Mario Party 4's own. Chain Chomps charge across a round arena ringed with fire for 60 seconds. The stick walks you with Melee's walk, dash and run (each fighter's own speed); a Chomp that runs into you flings you out, and so does the edge. The survivors win, placed by who lasted longest. No jumping. |

## Playing online

Online party works over Slippi Online with the existing connect codes, connection and rollback
netcode. There are two ways in:

| Players | Mode | How to connect |
|---|---|---|
| 2 | **Direct** | Enter each other's connect code. Two CPUs join as P3 and P4. |
| 4 | **Teams** | All four enter the same connect code, the way Slippi Teams forms a private group. Slippi's matchmaking server only forms that group once four players have entered the code, so three players cannot start a Teams party this way. |

1. **Connect.** Every player runs Melee Party with `--party on` (the default) and no other mod
   active. Open Online Play → Direct or Teams, enter the code, and pick a character. The team
   colours on the Teams character select do not matter, because the party decides its own teams.
2. **Pick what to play.** When everyone locks in, the party opens its lobby: a match on Final
   Destination with a menu over it. Only P1 (the first port) picks, with up/down and A (B goes
   back); everyone else sees the same menu. There is no time limit.
   - **Board**, then a board: a board party with minigames between the turns. After its podium
     everyone is back in the lobby, and a new board party starts from scratch.
   - **Minigames**, then a minigame: that minigame alone. When it is over everyone is back in the
     lobby on the minigame list, so P1 can pick the next one, as many times as they like.
3. **Play.** Every lobby, board turn, minigame and the podium is a separate online game. Between
   games there is a short pause, about 2 seconds, while the next game is agreed, then it starts
   by itself. You do not go back to the character select.
4. **Finish.** L+R+A+Start in any party game ends the party and returns everyone to the online
   character select. A disconnect, or an opponent who does not start the next game within 30
   seconds, also ends it there.

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
- For 3 or 4 players through the Teams menus and connect-code entry, connected to each other
  directly rather than through Slippi's matchmaking server, use
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

- **Dungeon Duos.** The dungeon is Mario Party 4's own layout: `dungeon_m432.h` holds the
  locators, the collision map's floors and walls, and the turntables' and gates' sizes, which
  `tools/extract_mp4_dungeon.py` reads from a Mario Party 4 (USA) disc (`data/m432.bin`; HSF
  models, format from partyboard's `src/game/hsfload.c`). The screen is split as in MP4: the
  match camera draws the scene once per view (`mu_party_camera_views` in `cm/camera.c`), the
  right view first, because every pass renders shadows into the frame's top-left corner.
  Melee keeps the fighters' height (jumps, double jumps, gravity, landing) and speeds; the
  minigame keeps their place on the dungeon floor and the height of the floor under them.
  Every fighter collides on Final Destination at a fixed x of its own, on a floor that stands
  for the dungeon floor under it, and is drawn in the dungeon. Melee works the collision box
  out from the bones, so the model goes back to where the fighter collides at the start of
  each frame (`Fighter_procInput`). Over a pit Melee's height is held still and the fall is
  the floor's.

  The spaces, the net, the podium and the dungeon are drawn with flat GX shapes and need no
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
| `dungeon_m432.h` | Dungeon Duos' layout (generated by `tools/extract_mp4_dungeon.py`) |
| `minigames.c` | The minigame table, picking a minigame, and rewards |
| `mg_volley.c`, `mg_sandbag.c`, `mg_food.c`, `mg_domination.c`, `mg_dungeon.c` | The five minigames |
| `results.c` | The podium |
| `party_online.c` | The party over Slippi Direct |
| `lobby.c` | Online: the lobby where P1 picks a board party or the next minigame |
| `mp4/` | The Mario Party 4 runtime (below) |

### The Mario Party 4 runtime

The MP4 minigames run MP4's own code and draw MP4's own models, loaded from the player's Mario
Party 4 disc. `party/mp4/` holds that code, ported from the MP4 decompilation
[partyboard](https://github.com/mariopartyrd/partyboard) (CC0), mostly as its PC port builds it.

- **The disc.** The host opens a Mario Party 4 (USA) disc, GMPE01 Rev 0 or 1, as a plain `.iso`
  (not `.ciso`, `.gcz`, `.rvz` or NKit). It looks first at `--mp4-iso <path>`, then at
  `MELEE_PARTY_MP4_ISO`, then at `mp4.iso` beside the Melee ISO. It reads the disc in place
  and hands the game reads of it (host API 17, `mp4_disc_read`). The game checks the header
  and reads the file system table (`mp4_disc.c`, `mp4_format.c`). Without such a disc, or with
  `--party off`, everything MP4 stays off and the log says why
  (`[party] mp4: ...: the MP4 minigames are off`): the Party Minigames menu does not list the MP4
  minigames and a board party never picks one (`minigame_offered`). The online lobby never lists
  them, disc or not: online play keeps no MP4 state in step.
- **Archives.** MP4 data numbers (`DATADIR_*`, an archive in the high half and a file in the
  low half) read one file of a `data/*.bin` archive off the disc and unpack it: none, LZ, SLIDE,
  FSLIDE and RLE, types 0 to 5 (`mp4_dir.c`, `mp4_decode.c`). Only each archive's offset table
  stays in memory.
- **Memory.** MP4's heaps (`HuMem`, `mp4_mem.c`) sit in one pool in the game image, not in
  Melee's main memory: MP4's own sizes (a 9 MB data heap for models and motions, a 1 MB DVD heap)
  but a 2 MB system heap, twice MP4's, since the structures are bigger here and Chain Chomp Fever
  keeps two effects per Chomp lane in it. GX reads the textures, vertex arrays and display lists of
  MP4 models in place, and it can only address MEM1 and the image (below 0x84000000); after the
  heaps comes the per-frame scratch for big-endian copies of vertex arrays, which takes whatever
  still fits under that limit (1.5 MB at most, a frame of m438 uses 0.3 MB). Each minigame linked
  in moves the pool up by its code's size; when less than 1 MB of scratch is left the MP4
  minigames switch themselves off and the log says so. The pool is zero-filled `.bss` and costs
  nothing without an MP4 disc. Rollback snapshots leave it out, because the MP4 minigames are
  offline only for now. MP4's processes run on coroutines (`mp4_coro.c`) whose 64 KB stacks come
  from the C runtime's heap: GX never reads them, and a process that outlives a match (the boot
  overlay's watcher, the banners) outlives Melee's heap.
- **Where the 0x84000000 limit comes from.** The host (`port/app/source_host.cpp`,
  `reserve_memory`) puts MEM1 at 0x80000000, 40 MB of it (the console's 24 MB grown for 8-byte
  pointers), and links the game image right after it at 0x82800000, so that the game's statics
  have an address the emulated GX texture and display-list registers can hold: 26 bits, the first
  64 MB from 0x80000000. The game image is already 24 MB, so its last sections end past the
  limit; the MP4 pool is placed by the linker like any other `.bss` and happens to end right at it
  (`mp4_mem_fits`). Each MP4 minigame linked in adds about 70 KB of code, which moves everything
  after it up by that much. Room, when it runs out: the DVD, music and misc heaps (1.3 MB) could
  leave the pool, since GX never reads them (the system heap stays: sprite bitmaps and m438's
  effect display lists live there); a smaller MEM1 would let the image start lower; or the port's
  GX could decode wider addresses. None of this is done.
- **Models.** `hsfload.c` (with `hsf_byteswap.c`), `hsfdraw.c`, `hsfman.c` (the Hu3D model,
  camera and light layer), `hsfmotion.c`, `EnvelopeExec.c`, `ShapeExec.c`, `ClusterExec.c` and
  `hsfex.c` are MP4's. They draw through Melee's GX, which now records display lists
  (`gxnative/GXFifo_native.c`), as MP4's loader builds one for every mesh. The PC port keeps
  vertex arrays in host order, so `mp4_gx.c` hands GX a big-endian copy of each array every
  frame. `mp4_party.c` draws the models from a GObj on the party's render link, with the match
  camera's view.
- **A minigame.** MP4's minigames are overlays (REL files) its object manager loads by number;
  here each one is linked in from `party/mp4/m4xx/` with its symbols prefixed by its name
  (`tools/mp4_rel_names.py`, run by CMake) and its variables reset at every start (`mp4_ovl.c`).
  `mp4_party.c` hosts it in a match: MP4's frame (pads, processes, banners, motions) runs from
  the match's frame hook, and its draw from a GObj on the party's render link. The runtime fills
  what the minigame reads of MP4's state: the pads from the fighters' inputs, the player table
  (`GWPlayerCfg`: human or CPU, and MP4's four difficulties from Melee's levels), no save flags.
- **The players** (`party_arena.c`, `mp4_char.c`). Each player's MP4 character model is loaded and
  animated as in MP4 but never drawn: the Melee fighter stands where it stands and faces its way.
  Every fighter collides on Final Destination on a lane of its own, with the MP4 world drawn 600
  units to the side. While the minigame has a player walking by the stick, Melee's physics give
  the fighter its speed and the MP4 player moves at that speed (`mp4_player_speed`, called from
  the minigame's walk code in place of its own); the rest of the time (dropping in, hit, out,
  won, lost) the minigame moves the player and the fighter follows, with the nearest Melee
  animation. Each minigame's wrapper (`mg_mp4_<name>.c`) decides which is which from the player's
  state.
- **Not yet**: MP4's sound and music, its shadow pass, the reflection, toon and highlight maps
  MP4 keeps in its executable, and its instruction screens.

### Adding a minigame

1. Write `mg_<name>.c` that defines a `const PartyMinigame`. It holds a name, an id, a setup
   function, optional CPU input, a result function that returns placements, and the stage plus
   any extra fighter (so the next scene can preload them). An optional `fighter_map` runs in
   `Fighter_procMap` before collision, to place fighters off the 2D line (Domination uses it).
   Optional `camera_views` and `camera_view` split the screen (Dungeon Duos uses them).
2. Add it to `table[]` in `minigames.c`.

For a Mario Party 4 minigame: copy its sources from partyboard's `src/REL/m4xxDll/` into
`party/mp4/m4xx/` with a credit header (and its `include/REL/m4xxDll.h`), add it to the table in
`mp4_ovl.c` with `OVERLAY_MARKERS`, add any MP4 runtime calls it makes that are still missing,
and write `mg_mp4_<name>.c` on `party_arena.h` (`mg_mp4_m438.c` is the walking example,
`mg_mp4_m440.c` one where nobody walks). In its player code, call `mp4_player_speed` where it sets
the walking speed. Set `needs_mp4` and add a line to `descriptions[]` in `party_menu.c`.

## Testing knobs

Set these as environment variables for `melee_source.exe`.

| Variable | Effect |
|---|---|
| `MELEE_PARTY_BOOT=1` | Boot straight into the party. |
| `MELEE_PARTY_SKIP_CSS=1` | Skip the character select: P1 is human and the others are random CPUs. |
| `MELEE_PARTY_ALL_CPU=1` | With the previous knob, every player is a CPU. |
| `MELEE_PARTY_MINIGAME=volley\|sandbag\|food\|domination\|dungeon` | Play only this minigame, over and over. |
| `MELEE_PARTY_ORDER=food,volley,...` | Minigames in this order. |
| `MELEE_PARTY_TURNS=n` | Number of turns. The default is 10. |
| `MELEE_PARTY_SEED=n` | A fixed seed for the party's own random numbers. |
| `MELEE_PARTY_AUTO_ROLL=1` | Idle humans roll after half a second instead of 10 seconds. |
| `MELEE_PARTY_ONLINE_TEST=1` | Online party in any online mode, for the local test pair. |
| `MELEE_PARTY_DEBUG_BOARDS=1` | No minigames between board turns, as when started from Debug Boards. |
| `MELEE_PARTY_LOBBY_PICK=board` or `=<minigame id>` | The online lobby picks that by itself (scripted tests). Set the same on both sides. |
| `MELEE_PARTY_MP4_ISO=<path>` | The Mario Party 4 disc, as `--mp4-iso` (that flag wins). |
| `MELEE_PARTY_MP4_MODEL=<archive>:<file>[:<motion>]` | Every party match draws this MP4 model at the stage's centre, for checking the MP4 runtime against a disc: the archive by name (`m440`), the file and an optional motion from the same archive by number (decimal or `0x` hex). The log says what loaded (`[party] mp4: model ...`). |
| `MELEE_PARTY_MP4_SCALE=s`, `MELEE_PARTY_MP4_Y=y` | That model's scale (default 0.1, MP4's units against Melee's) and height. |

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
- The MP4 minigames play offline only, without MP4's sound, music, shadows or instruction
  screens. The fighters' animations approximate the MP4 motions (walk, run, crouch, taunt, hit,
  flying), and the MP4 characters' voices and effects are not played.
- Dungeon Duos draws its dungeon from MP4's collision map in flat colours, not MP4's textured
  models; the rescue hook and the winners' ride out of the dungeon are not drawn (a fallen
  player reappears at the checkpoint, and the winners taunt at the pump).
- Online parties through Slippi's servers are for 2 players (Direct) or 4 (Teams). Slippi's
  matchmaking server forms a Teams group only once four players entered the code, so three
  players have no way in yet. The 3-player test (`tools/online_quad.py --players 3`) connects
  the instances to each other directly and does not go through that server.
- Between online games the screen holds for about 2 seconds while the next game is agreed.
