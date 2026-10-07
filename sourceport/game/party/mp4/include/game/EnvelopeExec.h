/* From partyboard (github.com/mariopartyrd/partyboard include/game/EnvelopeExec.h, CC0). */
#ifndef _GAME_ENVELOPE_EXEC_H
#define _GAME_ENVELOPE_EXEC_H

#include "game/hsfformat.h"

void InitEnvelope(HSFDATA *arg0);
void EnvelopeProc(HSFDATA *arg0);
void InitVtxParm(HSFDATA *arg0);

extern Vec *Vertextop;

#endif
