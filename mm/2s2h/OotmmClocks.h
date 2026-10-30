#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int OotmmClocks_Enabled(void);
uint32_t OotmmClocks_HalfDayMask(void);
void OotmmClocks_FixupSpawnTime(void);

#ifdef __cplusplus
}
#endif
