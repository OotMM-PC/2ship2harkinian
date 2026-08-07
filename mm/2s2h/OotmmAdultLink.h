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
/// True when the adult skeleton comes from a model pack rather than the vanilla assets.
int32_t OotmmAdultLink_CustomModelActive(void);
/// The human form's UNPATCHED root-limb scale; the live row holds adult metrics while mapped.
float OotmmAdultLink_VanillaHumanRootScale(void);
/// OoT's adult bow string, or the given child string outside crossAge adult.
Gfx* OotmmAdultLink_BowStringDL(Gfx* childDL);
/// Adult blade reach for the held sword, or the given child length.
float OotmmAdultLink_MeleeWeaponLength(int32_t meleeWeapon, float childLength);
/// OoT cloth (mapped adult or ported child) reads the tunic from ENV color, which MM
/// never sets; emits it (and the segment-0x0C cull DL OoT limbs jump through) each draw.
void OotmmAdultLink_SetTunicColor(struct PlayState* play);
/// The cull DL OoT limb display lists reach through segment 0x0C.
Gfx* OotmmAdultLink_CullSegmentDL(void);

#ifdef __cplusplus
}
#endif
