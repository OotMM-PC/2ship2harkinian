#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t OotmmChildLink_Active(void);
int32_t OotmmChildLink_CustomModelActive(void);

#ifdef __cplusplus
}

namespace OotmmChildLink {
bool Wanted();
void Apply();
void Restore();
} // namespace OotmmChildLink
#endif
