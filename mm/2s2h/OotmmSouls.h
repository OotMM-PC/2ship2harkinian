#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

/// Returns 1 when the seed withholds this actor's Soul, in which case the spawn must be abandoned.
int32_t OotmmSouls_SuppressSpawn(struct PlayState* play, int16_t actorId, int32_t params);
/// Strips soul-gated riders from an actor's spawn params; returns the params to spawn with.
int32_t OotmmSouls_AdjustSpawnParams(struct PlayState* play, int16_t actorId, int32_t params);
/// Forgets which enemies were withheld; call before a room's actor list is spawned.
void OotmmSouls_ResetRoomState(void);
int32_t OotmmSouls_RoomClearBlocked(void);

#ifdef __cplusplus
}
#endif
