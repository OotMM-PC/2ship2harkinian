#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct EnGirlA;
struct PlayState;

void OotmmShops_SetupItem(struct EnGirlA* item, struct PlayState* play);
int OotmmShops_Draw(struct EnGirlA* item, struct PlayState* play);
int OotmmShops_CanSelectKotakeItem(const struct EnGirlA* item);
void OotmmShops_OfferGetItem(struct Actor* shopkeeper, struct PlayState* play, struct EnGirlA* item, float xzRange,
                             float yRange);

#ifdef __cplusplus
}

#include <libultraship/bridge/OotmmGameState.h>

void OotmmShops_Init();
bool OotmmShops_IsOffering();
int OotmmShops_RepeatItem(const Ship::OotmmPlacement& placement, int fallback);
#endif
