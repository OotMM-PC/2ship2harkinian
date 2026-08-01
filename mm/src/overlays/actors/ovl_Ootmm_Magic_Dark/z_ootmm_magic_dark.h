#ifndef Z_OOTMM_MAGIC_DARK_H
#define Z_OOTMM_MAGIC_DARK_H

#include "global.h"

typedef struct OotmmMagicDark {
    /* 0x000 */ Actor actor;
    /* 0x144 */ s16 timer;
    /* 0x146 */ u8 primAlpha;
    /* 0x148 */ Vec3f orbOffset;
    /* 0x154 */ f32 scale;
} OotmmMagicDark;

#endif
