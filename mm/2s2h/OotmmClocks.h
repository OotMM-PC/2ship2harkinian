#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Whether the seed shuffles Clocks and the launcher inventory has been received.
int OotmmClocks_Enabled(void);
/// Six-bit mask of owned half-days (bit 0 = Day 1 ... bit 5 = Night 3); 0x3F when clocks are not shuffled.
uint32_t OotmmClocks_HalfDayMask(void);
/// Skips a dawn-of-first-day spawn forward to the earliest owned half-day; must run before Play_Init consumes time.
void OotmmClocks_FixupSpawnTime(void);

#ifdef __cplusplus
}
#endif
