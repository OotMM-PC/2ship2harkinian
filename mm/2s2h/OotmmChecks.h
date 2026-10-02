#pragma once

struct PlayState;
struct Player;
struct Actor;

#ifdef __cplusplus
void OotmmChecks_Init();
/// Arms a chest open whose vanilla give is suppressed; nothing is granted.
void OotmmChecks_OpenChestSilently(struct PlayState* play, struct Player* player);

extern "C" {
#endif

int OotmmChecks_BossContainerDone(struct PlayState* play, int vanillaDone);
int OotmmChecks_BossContainerIsShuffled(struct PlayState* play);
int OotmmChecks_TryCollectBossContainer(struct Actor* actor, struct PlayState* play);
int OotmmChecks_DrawBossContainer(struct Actor* actor, struct PlayState* play);

#ifdef __cplusplus
}
#endif
