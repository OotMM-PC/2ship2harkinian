#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <libultraship/bridge/OotmmGameState.h>

extern "C" {
#endif

int32_t OotmmSession_TryBootDirectly(void* gameState);
int32_t OotmmSession_IsActive(void);
void OotmmSession_NotePlayerExitTransition(void);
/// Runs after the conditional-trigger pass has landed a moon-crash reset in the Clock Tower.
void OotmmSession_RedirectMoonCrashRespawn(void);
/// Marks the result so the seed's entrance remap does not translate it twice. -1 when unresolvable.
int32_t OotmmSession_ApplyResolvedMmEntrance(uint32_t entrance);

#ifdef __cplusplus
}

void OotmmSession_Init();
const Ship::OotmmGameState& OotmmSession_GetState();
/// On success the session suppresses local transitions until the launcher answers.
bool OotmmSession_BeginCrossGameTransition(const Ship::OotmmEntranceMapping& mapping);
#endif
