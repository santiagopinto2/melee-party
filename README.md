# Melee Party

A Mario Party style board game for Super Smash Bros. Melee (NTSC 1.02), running natively on
Windows. Up to four players roll dice around a board, collect coins and stars, and play physics
minigames between turns, locally or online over Slippi.

Based on [Melee Unlocked](https://github.com/Hero88go/melee-unlocked) by Hero88go and
contributors (GPL-3.0).

Nothing from the game is included. You supply your own Melee NTSC 1.02 ISO.

This project is not affiliated with, endorsed by, or supported by the Slippi team, Nintendo or
HAL Laboratory. Questions and bugs about this build go to this repository, not to the Slippi team.

## Playing

Start the Source Port (`melee_source.exe`) and go to **VS. Mode → Melee Party**. To play a
single minigame, use **VS. Mode → Party Minigames**.

Each turn every player rolls a die (1 to 10) and walks that many spaces. Blue spaces give 3
coins, red spaces take 3, and passing the star space buys a star for 20 coins. After everyone
moves there is a minigame, paying 10, 5, 3 and 0 coins by placement. After the last turn the
player with the most stars (then coins) wins.

| Minigame | Rules |
|---|---|
| Volleyball | 2 against 2. Hit the ball over the net; first to 5. |
| Bag Bash | Everyone against the Sandbag for 60 s. Damage and knockouts score. |
| Food Frenzy | Food rains for 30 s. Eat the most to win. |
| Domination | Mash A to drop Snorlaxes into your lane; the most Snorlaxes wins. |

Online: two players connect with **Direct** codes, three or four with **Teams** (everyone
enters the same code). Empty seats become CPUs. Only Melee Party builds of the same version can
connect to each other.

Full rules, online details and test commands: [docs/melee-party.md](docs/melee-party.md).

## Build from source

Windows 10/11, your own ISO. Nothing from the ISO enters the repository.

```powershell
git clone --recurse-submodules https://github.com/santiagopinto2/melee-party.git
cd melee-party
```

Then follow [docs/build-source-port.md](docs/build-source-port.md) to build `melee_source.exe`.
Add the target `melee_party` to build the optional launcher (`MeleePartyLauncher.exe`).

For online play you need a Slippi account, created and logged in through the
[Slippi Launcher](https://slippi.gg/downloads). The game picks up that login automatically.
The Slippi Launcher also installs the WinUSB driver a GameCube adapter needs.

## Features of the underlying port

- Unlocked frame rate with sub-frame animation
- Slippi online (Unranked, Direct, Teams) with rollback netcode and replay recording
- GameCube adapter, DualShock 4, keyboard fallback
- Direct3D 12 and Direct3D 11 renderers, DLSS / DLAA, high internal resolutions
- PC settings overlay (F1), Lab view (F3), cosmetic imports, mod support

See [docs/v0.7-feature-list.md](docs/v0.7-feature-list.md) and `PORT_COMPLETION.md` for details.

## Bug reports

Open a [GitHub issue](https://github.com/santiagopinto2/melee-party/issues). Attach
`melee_port.log` from the game folder and your `port-settings.ini`.

## Repository layout

- `sourceport/game/party/`: the board, minigames, HUD and menu
- `sourceport/patches/`: changes applied to the Melee decompilation (`sourceport/extern/melee`)
- `port/runtime/`: host runtime, renderers, Slippi device and netcode
- `port/app/`: game executables and the optional launcher
- `tools/`: validation, testing and packaging scripts

Unit tests: `ctest --test-dir build-review -C Release`.

## License

GPL-3.0-or-later. See `LICENSE` and `NOTICE`, which list every third-party component and its
license. Super Smash Bros. Melee is the property of Nintendo and HAL Laboratory; this project
contains none of its data.
