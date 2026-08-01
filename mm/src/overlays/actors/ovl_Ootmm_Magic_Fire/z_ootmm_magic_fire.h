#ifndef Z_OOTMM_MAGIC_FIRE_H
#define Z_OOTMM_MAGIC_FIRE_H

#include "global.h"

typedef struct OotmmMagicFire {
    /* 0x000 */ Actor actor;
    /* 0x144 */ ColliderCylinder collider;
    /* 0x190 */ f32 alphaMultiplier;
    /* 0x194 */ f32 screenTintIntensity;
    /* 0x198 */ f32 scalingSpeed;
    /* 0x19C */ s16 action;
    /* 0x19E */ s16 screenTintBehaviour;
    /* 0x1A0 */ s16 actionTimer;
    /* 0x1A2 */ s16 screenTintBehaviourTimer;
} OotmmMagicFire;

#endif
