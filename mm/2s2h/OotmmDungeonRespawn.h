#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <optional>

constexpr uint16_t kOotmmDungeonRespawnEntrance = 0xFFFC;

std::optional<uint16_t> OotmmDungeonRespawn_PrepareArrival();
void OotmmDungeonRespawn_Init();

extern "C" {
#endif

struct PlayState;

int32_t OotmmDungeonRespawn_Available(void);
int32_t OotmmDungeonRespawn_Warp(struct PlayState* play);
int32_t OotmmDungeonRespawn_RedirectRespawn(struct PlayState* play);
void OotmmDungeonRespawn_Clear(void);
void OotmmDungeonRespawn_SaveForFaroresWind(void);
void OotmmDungeonRespawn_RestoreForFaroresWind(void);

#ifdef __cplusplus
}
#endif
