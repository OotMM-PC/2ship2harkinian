#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void OotmmScales_Init(void);

int OotmmScales_Tier(void);

float OotmmScales_MaxDiveDepth(void);
int OotmmScales_DiveDoAction(float depthInWater);
int OotmmScales_ExtendsUnderwaterTime(void);

#ifdef __cplusplus
}
#endif
