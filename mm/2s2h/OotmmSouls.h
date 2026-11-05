#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

int32_t OotmmSouls_SuppressSpawn(struct PlayState* play, int16_t actorId, int32_t params);
int32_t OotmmSouls_AdjustSpawnParams(struct PlayState* play, int16_t actorId, int32_t params);
int32_t OotmmSouls_HasComposerBros(void);
int32_t OotmmSouls_HasTwinmold(void);
void OotmmSouls_ResetRoomState(void);
int32_t OotmmSouls_RoomClearBlocked(void);

#ifdef __cplusplus
}
#endif
