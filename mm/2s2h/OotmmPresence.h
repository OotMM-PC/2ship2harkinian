#pragma once


#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void OotmmPresence_Init(void);

// What the local player's draw chose for the four equipment limbs this frame.
extern const char* gOotmmEquipDlCapture[4];
// Which custom model a hand limb drew; loaded display lists have no resource path to sync.
typedef enum OotmmCustomHandDl {
    OOTMM_CUSTOM_HAND_NONE,
    OOTMM_CUSTOM_HAND_HAMMER,
    OOTMM_CUSTOM_HAND_BOOMERANG,
    OOTMM_CUSTOM_HAND_SLINGSHOT,
    OOTMM_CUSTOM_HAND_DEKU_SHIELD,
} OotmmCustomHandDl;

void OotmmPresence_CaptureCustomHand(int32_t index, int32_t which);
/// Stores slot i when the pointer is a resource path; composed DLs fall back to the bare limb.
void OotmmPresence_CaptureEquipDl(int32_t index, void* dl);

#ifdef __cplusplus
}
#endif
