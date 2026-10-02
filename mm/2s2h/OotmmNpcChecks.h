#pragma once

struct Actor;
struct PlayState;

#ifdef __cplusplus
extern "C" {
#endif

int OotmmNpcChecks_IsShuffled(const char* nativeId);
int OotmmNpcChecks_IsCollected(const char* nativeId);
int OotmmNpcChecks_Collect(const char* nativeId);
int OotmmNpcChecks_CanPurchase(const char* nativeId);
int OotmmNpcChecks_CollectPurchased(const char* nativeId);
int OotmmNpcChecks_Done(const char* nativeId, int vanillaDone);
void OotmmNpcChecks_SongTaught(int songItemId);
int OotmmNpcChecks_CollectRemains(int sceneId);
int OotmmNpcChecks_RemainsIsShuffled(int sceneId);
int OotmmNpcChecks_RemainsReady(struct PlayState* play);
void OotmmNpcChecks_DrawRemainsReward(struct Actor* actor, struct PlayState* play);
int OotmmNpcChecks_DrawReward(struct Actor* actor, struct PlayState* play, const char* nativeId);
int OotmmNpcChecks_SuppressCutsceneText(unsigned short textId);
int OotmmNpcChecks_GreatFairyHuman(int vanillaHuman);
const char* OotmmNpcChecks_TingleSite(int map);
int OotmmNpcChecks_CanFinishSilentOffer(const char* nativeId);
int OotmmNpcChecks_PurchaseAllowed(struct Actor* actor, int getItemId, int vanillaAllowed);
int OotmmNpcChecks_SmithyStage(void);
int OotmmNpcChecks_CanFinishMessageOffer(struct Actor* actor);
void OotmmNpcChecks_FinishMessageOffer(struct Actor* actor);
void OotmmNpcChecks_Init(void);

#ifdef __cplusplus
}

#include <libultraship/bridge/OotmmGameState.h>

bool OotmmNpcChecks_CanPurchasePlacement(const Ship::OotmmPlacement& placement);
bool OotmmNpcChecks_CollectPurchasedPlacement(const Ship::OotmmPlacement& placement);
bool OotmmNpcChecks_IsPlacementCollected(const Ship::OotmmPlacement& placement);
std::string OotmmNpcChecks_PlacementName(const Ship::OotmmPlacement& placement);
#endif
