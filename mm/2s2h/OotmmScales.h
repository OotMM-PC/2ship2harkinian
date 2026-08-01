#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void OotmmScales_Init(void);

/// 0 none, 1 bronze, 2 silver, 3 golden. Bronze is swim-only and never writes UPG_SCALE.
int OotmmScales_Tier(void);

#ifdef __cplusplus
}
#endif
