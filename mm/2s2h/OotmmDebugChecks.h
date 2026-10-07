#pragma once

#include <stdint.h>

#ifdef __cplusplus

void OotmmDebugChecks_Init();
void OotmmDebugChecks_DrawTab();

extern "C" {
#endif
#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
int OotmmDebugChecks_ReadRuntimePad(uint16_t* buttons, int8_t* x, int8_t* y);
#endif
#ifdef __cplusplus
}
#endif
