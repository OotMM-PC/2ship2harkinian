#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <libultraship/bridge/OotmmGameState.h>

extern "C" {
#endif

int32_t OotmmSession_TryBootDirectly(void* gameState);
int32_t OotmmSession_IsActive(void);
void OotmmSession_NotePlayerExitTransition(void);

#ifdef __cplusplus
}

void OotmmSession_Init();
const Ship::OotmmGameState& OotmmSession_GetState();
#endif
