#pragma once

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

void OotmmAdultLink_Init(void);

int32_t OotmmAdultLink_IsAdult(void);
int32_t OotmmAdultLink_CustomModelActive(void);
float OotmmAdultLink_VanillaHumanRootScale(void);
Gfx* OotmmAdultLink_BowStringDL(Gfx* childDL);
float OotmmAdultLink_MeleeWeaponLength(int32_t meleeWeapon, float childLength);
void OotmmAdultLink_SetTunicColor(struct PlayState* play);

#ifdef __cplusplus
}
#endif
