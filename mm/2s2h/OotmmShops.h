#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct EnGirlA;
struct PlayState;

void OotmmShops_SetupItem(struct EnGirlA* item, struct PlayState* play);
int OotmmShops_Draw(struct EnGirlA* item, struct PlayState* play);
void OotmmShops_OfferGetItem(struct Actor* shopkeeper, struct PlayState* play, struct EnGirlA* item, float xzRange,
                             float yRange);

#ifdef __cplusplus
}

void OotmmShops_Init();
bool OotmmShops_IsOffering();
#endif
