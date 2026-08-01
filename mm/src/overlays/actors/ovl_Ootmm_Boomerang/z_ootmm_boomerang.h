#ifndef Z_OOTMM_BOOMERANG_H
#define Z_OOTMM_BOOMERANG_H

#include "global.h"

struct OotmmBoomerang;

typedef void (*OotmmBoomerangActionFunc)(struct OotmmBoomerang*, PlayState*);

typedef struct OotmmBoomerang {
    /* 0x000 */ Actor actor;
    /* 0x144 */ ColliderQuad collider;
    /* 0x1C4 */ Actor* moveTo;
    /* 0x1C8 */ Actor* grabbed;
    /* 0x1CC */ u8 returnTimer;
    /* 0x1CD */ u8 activeTimer;
    /* 0x1D0 */ s32 effectIndex;
    /* 0x1D4 */ WeaponInfo weaponInfo;
    /* 0x1F0 */ OotmmBoomerangActionFunc actionFunc;
} OotmmBoomerang;

#endif // Z_OOTMM_BOOMERANG_H
