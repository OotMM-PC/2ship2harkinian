#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Ten-bit Song of Soaring destination mask indexed by OwlWarpId; 0 when owl shuffle is off.
uint32_t OotmmOwls_ActivatedMask(void);

#ifdef __cplusplus
}
#endif
