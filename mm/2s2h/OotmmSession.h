#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <libultraship/bridge/OotmmGameState.h>

extern "C" {
#endif

int32_t OotmmSession_TryBootDirectly(void* gameState);
int32_t OotmmSession_IsActive(void);
int32_t OotmmSession_ErIndoors(void);
int32_t OotmmSession_SirloinIsStoredAway(struct PlayState* play);
int32_t OotmmSession_StoreSirloin(struct PlayState* play, struct Actor* sirloin);
void OotmmSession_FixupPlayInit(void);
int32_t OotmmSession_BoolSetting(const char* key);
int32_t OotmmSession_SeedColor(const char* key, uint8_t* r, uint8_t* g, uint8_t* b);
int32_t OotmmSession_RequestFileSelection(int32_t slot);
int32_t OotmmSession_FileSelectionPending(void);
int32_t OotmmSession_RequestFileCopy(int32_t source, int32_t target);
int32_t OotmmSession_SaveProgress(void);
void OotmmSession_NotePlayerExitTransition(void);
int32_t OotmmSession_ShuffledWallmaster(int16_t actorId);
int32_t OotmmSession_WallmasterTakePlayer(struct PlayState* play, struct Actor* wallmaster);
void OotmmSession_ApplyDeathRespawn(void);
int32_t OotmmSession_ReturnToSpawn(void);
void OotmmSession_RedirectMoonCrashRespawn(void);
int32_t OotmmSession_KeepsSaveOnMoonCrash(void);
int32_t OotmmSession_ReadMoonCrashSave(uint8_t* buffer);
int32_t OotmmSession_ApplyResolvedMmEntrance(uint32_t entrance);
int32_t OotmmSession_RedirectBossWarp(uint32_t bossId);
int32_t OotmmSession_TryFastMask(int32_t item);
int32_t OotmmSession_IsEntranceShuffle(void);
int32_t OotmmSession_ErRegionsOverworld(void);

#ifdef __cplusplus
}

void OotmmSession_Init();
bool OotmmSession_HasPendingInitialSave();
const Ship::OotmmGameState& OotmmSession_GetState();
bool OotmmSession_BeginCrossGameTransition(const Ship::OotmmEntranceMapping& mapping);
bool OotmmSession_SaveForLauncher();
#endif
