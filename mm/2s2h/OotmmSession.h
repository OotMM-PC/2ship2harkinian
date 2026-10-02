#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <libultraship/bridge/OotmmGameState.h>

extern "C" {
#endif

int32_t OotmmSession_TryBootDirectly(void* gameState);
int32_t OotmmSession_IsActive(void);
int32_t OotmmSession_RequestFileSelection(int32_t slot);
int32_t OotmmSession_FileSelectionPending(void);
int32_t OotmmSession_RequestFileCopy(int32_t source, int32_t target);
int32_t OotmmSession_SaveProgress(void);
void OotmmSession_NotePlayerExitTransition(void);
/// Gives death OoT's grotto rule: back at the grotto Link died in, or out of the one he arrived from.
void OotmmSession_ApplyDeathRespawn(void);
/// Warps Link to the entrance he entered MM from; 0 when the launcher gave no arrival entrance.
int32_t OotmmSession_ReturnToSpawn(void);
/// Runs after the conditional-trigger pass has landed a moon-crash reset in the Clock Tower.
void OotmmSession_RedirectMoonCrashRespawn(void);
/// Marks the result so the seed's entrance remap does not translate it twice. -1 when unresolvable.
int32_t OotmmSession_ApplyResolvedMmEntrance(uint32_t entrance);
int32_t OotmmSession_RedirectBossWarp(uint32_t bossId);

#ifdef __cplusplus
}

void OotmmSession_Init();
bool OotmmSession_HasPendingInitialSave();
const Ship::OotmmGameState& OotmmSession_GetState();
/// On success the session suppresses local transitions until the launcher answers.
bool OotmmSession_BeginCrossGameTransition(const Ship::OotmmEntranceMapping& mapping);
#endif
