#pragma once

#include <stdint.h>

// Gfx comes from the game headers; include this after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

void OotmmAdultLink_Init(void);

/// True when the seed's crossAge setting keeps Link adult inside MM.
int32_t OotmmAdultLink_IsAdult(void);
/// OoT's adult bow string, or the given child string outside crossAge adult.
Gfx* OotmmAdultLink_BowStringDL(Gfx* childDL);
/// Adult blade reach for the held sword, or the given child length.
float OotmmAdultLink_MeleeWeaponLength(int32_t meleeWeapon, float childLength);
/// Adult cloth reads the tunic from ENV color, which MM never sets; emits it each player draw.
void OotmmAdultLink_SetTunicColor(struct PlayState* play);

#ifdef __cplusplus
}
#endif
