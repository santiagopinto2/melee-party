# Melee Party (next release)

Notes collected for the next release. When it is cut, rename this file to
`RELEASE_NOTES_<version>.md`, add the title and Install section, and start a new one.

## Fixes

- Source Port: fixed a crash that could happen when a match loads, for example at the start of a Melee Party board turn. A few game files ended their lists of contents in a way that only worked when leftover memory happened to be zero, so the crash came and went from one PC to the next.
