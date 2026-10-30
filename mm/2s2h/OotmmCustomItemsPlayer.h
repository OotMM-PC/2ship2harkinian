#pragma once

#include <stdint.h>

#include "OotmmCustomItems.h"


#ifdef __cplusplus
extern "C" {
#endif

Actor* OotmmCustomItems_SpawnMagicSpellActor(PlayState* play, Player* player, int32_t magicSpell);
int OotmmCustomItems_FaroresWindPlaced(void);
void OotmmCustomItems_PlaceFaroresWind(PlayState* play);
void OotmmCustomItems_DispelFaroresWind(void);
void OotmmCustomItems_ClearFaroresWind(void);
void OotmmCustomItems_WarpToFaroresWind(PlayState* play);
int OotmmCustomItems_FaroresWindSceneMatch(PlayState* play);
void OotmmCustomItems_StartFaroresWindPrompt(void);
void OotmmCustomItems_AfterPlayerInit(PlayState* play);

void OotmmCustomItems_SyncFaroresWind(PlayState* play);

void OotmmCustomItems_MaskBeforeUpdate(Player* player);
void OotmmCustomItems_MaskAfterUpdate(Player* player);
void OotmmCustomItems_OnNativeMaskUse(Player* player, int32_t itemAction);
int OotmmCustomItems_SuppressVanillaMaskDraw(Player* player);

#ifdef __cplusplus
}
#endif
