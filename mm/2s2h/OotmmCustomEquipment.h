#pragma once

#include <stdint.h>


// PlayState/Player/Gfx come from the game headers; include this after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

/// Draws the equipped iron/hover boots over Link's feet; call after the skeleton draw.
void OotmmEquipment_DrawBoots(struct PlayState* play, struct Player* player);
void OotmmEquipment_DrawHoverCircle(struct PlayState* play, struct Player* player);
/// Draws the worn OoT trade mask; call with the head limb matrix current.
void OotmmEquipment_DrawWornMask(struct PlayState* play, struct Player* player);
/// Draws the slingshot string; call with the right hand limb matrix current.
void OotmmEquipment_DrawSlingshotString(struct PlayState* play, struct Player* player);

/// Combined hand display lists from the OoT asset pack, or NULL when not shipped.
Gfx* OotmmEquipment_LeftHandHammerDList(void);
Gfx* OotmmEquipment_LeftHandBoomerangDList(void);
Gfx* OotmmEquipment_RightHandSlingshotDList(void);
Gfx* OotmmEquipment_FirstPersonSlingshotDList(void);
Gfx* OotmmEquipment_BoomerangFlightDList(void);
/// OoT's Deku shield for the sheath limb and the guarding fist, or NULL when not shipped.
Gfx* OotmmEquipment_DekuShieldBackDList(void);
Gfx* OotmmEquipment_DekuShieldHandDList(int lod);

void OotmmEquipment_GetTunicColor(uint8_t* r, uint8_t* g, uint8_t* b);
void OotmmEquipment_TunicColorOf(int32_t tunic, uint8_t* r, uint8_t* g, uint8_t* b);
void OotmmEquipment_DrawBootsOf(struct PlayState* play, int32_t boots);
/// Patches or restores human Link's cloth prim colors; call once per player draw.
void OotmmEquipment_UpdateTunicTint(void);

#ifdef __cplusplus
}
#endif
