#pragma once

#include <libultraship/bridge/OotmmGameState.h>

void OotmmSession_Init();
const Ship::OotmmGameState& OotmmSession_GetState();
