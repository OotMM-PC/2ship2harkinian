#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Highest spin attack charge Link may reach; the vanilla week event when no seed is active.
float OotmmSpinUpgrade_ChargeLimit(void);

/// 1 when the seed's Spin Attack Upgrade is owned, 0 when not, -1 to let the week event decide.
int32_t OotmmSpinUpgrade_SpinLevel(void);

#ifdef __cplusplus
}
#endif
