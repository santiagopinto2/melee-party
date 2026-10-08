# Melee Party (next release)

Notes collected for the next release. When it is cut, rename this file to
`RELEASE_NOTES_<version>.md`, add the title and Install section, and start a new one.

## New

- Melee Party: with a Mario Party 4 (USA) disc as a plain `.iso` next to the Melee ISO (or given
  with `--mp4-iso`), the party plays ten of MP4's own minigames, run from MP4's code and drawn
  from the disc's models with the Melee fighters as the players: Bowser's Bigger Blast, Chain
  Chomp Fever, Mr. Blizzard's Brigade, Booksquirm, Butterfly Blitz, Trace Race, Candlelight
  Flight, Money Belts, Hop or Pop and Cheep Cheep Sweep. They are in the Party Minigames list
  and the board's rotation, offline only. Without the disc nothing changes.

## Fixes

- Source Port: fixed a crash that could happen when a match loads, for example at the start of a Melee Party board turn. A few game files ended their lists of contents in a way that only worked when leftover memory happened to be zero, so the crash came and went from one PC to the next.
- Source Port: fixed the same kind of crash in two more places, in the loader of a fighter's
  costume. It hit when a match started from the character select (a Party Minigames game, or the
  title screen's demo), depending on the PC.
- Source Port: the C library's character classes had the wrong bit values, so the game's own
  `isalpha` was false for every lowercase letter and `isdigit` for every digit. It showed up as a
  crash at the start of Chain Chomp Fever (MP4 object names cut short), and it could affect any
  text the game parses, like a number's width in a formatted string.
- Melee Party: without a Mario Party 4 disc the Party Minigames menu no longer lists the MP4
  minigames, and the online lobby never does.
