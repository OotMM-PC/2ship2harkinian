#ifndef Z_OOTMM_SLINGSHOT_SEED_H
#define Z_OOTMM_SLINGSHOT_SEED_H

#include "global.h"

struct OotmmSlingshotSeed;

typedef void (*OotmmSlingshotSeedActionFunc)(struct OotmmSlingshotSeed*, PlayState*);

typedef struct OotmmSlingshotSeed {
    /* 0x000 */ Actor actor;
    /* 0x144 */ ColliderQuad collider;
    /* 0x1C4 */ WeaponInfo weaponInfo;
    /* 0x1E0 */ s16 timer;
    /* 0x1E2 */ s16 activeTimer;
    /* 0x1E4 */ s32 touchedPoly;
    /* 0x1E8 */ OotmmSlingshotSeedActionFunc actionFunc;
} OotmmSlingshotSeed;

#endif // Z_OOTMM_SLINGSHOT_SEED_H
