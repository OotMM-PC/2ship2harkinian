#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

void OotmmRegionState_TriggerWarp(struct PlayState* play, int params);
void OotmmRegionState_SetDungeonFlags(int dungeonId, int flags);
void OotmmRegionState_ApplyBossWarp(int bossId);
int OotmmRegionState_BossCleared(int bossId, int vanillaCleared);
int OotmmRegionState_BossWarpPadActive(int pad, int vanillaActive);

#ifdef __cplusplus
}

void OotmmRegionState_Init();
#endif
