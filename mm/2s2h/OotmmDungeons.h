#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Small keys the seed can place in this dungeon (DungeonSceneIndex 0-3); 0 when keys are removed.
int OotmmDungeons_MaxSmallKeys(int dungeonIndex);
int OotmmDungeons_SmallKeysRemoved(void);
int OotmmDungeons_BossDoorIsOpen(void);

#ifdef __cplusplus
}
#endif
