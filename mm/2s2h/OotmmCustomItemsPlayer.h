#pragma once

#include <stdint.h>

#include "OotmmCustomItems.h"

// PlayState, Actor and Player come from the game headers; include this after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

Actor* OotmmCustomItems_SpawnMagicSpellActor(PlayState* play, Player* player, int32_t magicSpell);
int OotmmCustomItems_FaroresWindPlaced(void);
/// Stores the current void-out point as the Farore's Wind warp point, vanilla style.
void OotmmCustomItems_PlaceFaroresWind(PlayState* play);
void OotmmCustomItems_DispelFaroresWind(void);
/// Silently drops the warp point once its marker finishes the return absorb.
void OotmmCustomItems_ClearFaroresWind(void);
void OotmmCustomItems_WarpToFaroresWind(PlayState* play);
/// 0 when the warp point is elsewhere, 1 in its scene, 2 in its inverted-Stone-Tower pair.
int OotmmCustomItems_FaroresWindSceneMatch(PlayState* play);
/// Opens the vanilla Return / Dispel / Exit three-choice textbox.
void OotmmCustomItems_StartFaroresWindPrompt(void);
/// Re-arms Nayru's Love and the Farore's Wind point and marker after a scene load.
void OotmmCustomItems_AfterPlayerInit(PlayState* play);

/// Clears the Spooky-as-Gibdo spoof before Player_Update reads currentMask.
void OotmmCustomItems_MaskBeforeUpdate(Player* player);
/// Re-applies the custom mask behaviour after Player_Update ran.
void OotmmCustomItems_MaskAfterUpdate(Player* player);
void OotmmCustomItems_OnNativeMaskUse(Player* player, int32_t itemAction);
int OotmmCustomItems_SuppressVanillaMaskDraw(Player* player);

#ifdef __cplusplus
}
#endif
