#pragma once

#include <stdint.h>



#ifdef __cplusplus
extern "C" {
#endif

void OotmmEquipment_DrawBoots(struct PlayState* play, struct Player* player);
void OotmmEquipment_DrawHoverCircle(struct PlayState* play, struct Player* player);
void OotmmEquipment_DrawWornMask(struct PlayState* play, struct Player* player);
void OotmmEquipment_DrawSlingshotString(struct PlayState* play, struct Player* player);

Gfx* OotmmEquipment_LeftHandHammerDList(void);
Gfx* OotmmEquipment_LeftHandBoomerangDList(void);
Gfx* OotmmEquipment_RightHandSlingshotDList(void);
Gfx* OotmmEquipment_FirstPersonSlingshotDList(void);
Gfx* OotmmEquipment_BoomerangFlightDList(void);
Gfx* OotmmEquipment_DekuShieldBackDList(void);
Gfx* OotmmEquipment_DekuShieldHandDList(int lod);
Gfx* OotmmEquipment_RightHandFairyOcarinaDList(int lod);

void OotmmEquipment_GetTunicColor(uint8_t* r, uint8_t* g, uint8_t* b);
void OotmmEquipment_TunicColorOf(int32_t tunic, uint8_t* r, uint8_t* g, uint8_t* b);
void OotmmEquipment_DrawBootsOf(struct PlayState* play, int32_t boots);
void OotmmEquipment_UpdateTunicTint(void);

#ifdef __cplusplus
}
#endif
