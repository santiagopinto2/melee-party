# Melee Party (next release)

Notes collected for the next release. When it is cut, rename this file to
`RELEASE_NOTES_<version>.md`, add the title and Install section, and start a new one.

## Fixes

- Source Port: fixed a crash that could happen when a match loads, for example at the start of a Melee Party board turn. A few game files ended their lists of contents in a way that only worked when leftover memory happened to be zero, so the crash came and went from one PC to the next.
- Source Port: fixed the same kind of crash in two more places, in the loader of a fighter's
  costume. It hit when a match started from the character select (a Party Minigames game, or the
  title screen's demo), depending on the PC.
- Melee Party: without a Mario Party 4 disc the Party Minigames menu no longer lists the MP4
  minigames, and the online lobby never does.
