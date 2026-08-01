#ifndef Z_OOTMM_MAGIC_WIND_H
#define Z_OOTMM_MAGIC_WIND_H

#include "global.h"

struct OotmmMagicWind;

typedef void (*OotmmMagicWindActionFunc)(struct OotmmMagicWind*, PlayState*);

typedef struct OotmmMagicWind {
    /* 0x000 */ Actor actor;
    /* 0x144 */ SkelCurve skelCurve;
    /* 0x164 */ OotmmMagicWindActionFunc actionFunc;
    /* 0x168 */ s16 timer;
    /* marker (params 2) state */
    s16 markerAlpha;
    f32 markerRatio;
    f32 markerDrift;
    f32 markerYOffset;
    LightInfo lightInfo;
    LightNode* lightNode;
} OotmmMagicWind;

#endif
