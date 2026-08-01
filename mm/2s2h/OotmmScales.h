#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void OotmmScales_Init(void);

/// 0 none, 1 bronze, 2 silver, 3 golden. Bronze is swim-only and never writes UPG_SCALE.
int OotmmScales_Tier(void);

/// How deep human Link may dive, raised by the Silver and Golden Scales.
float OotmmScales_MaxDiveDepth(void);
/// DO_ACTION for the dive countdown, or -1 to leave MM's two-step vanilla counter alone.
int OotmmScales_DiveDoAction(float depthInWater);
/// True when MM's scales give human Link OoT's underwater grace instead of MM's 80 frames.
int OotmmScales_ExtendsUnderwaterTime(void);

#ifdef __cplusplus
}
#endif
