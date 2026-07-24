#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OOTMM_OCARINA_BUTTON_A = 0,
    OOTMM_OCARINA_BUTTON_C_DOWN,
    OOTMM_OCARINA_BUTTON_C_RIGHT,
    OOTMM_OCARINA_BUTTON_C_LEFT,
    OOTMM_OCARINA_BUTTON_C_UP,
} OotmmOcarinaButton;

int32_t Ootmm_IsOcarinaButtonAvailable(int32_t button);
uint32_t Ootmm_FilterOcarinaButtons(uint32_t buttons);

#ifdef __cplusplus
}
#endif
