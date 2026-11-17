#pragma once


#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void OotmmPresence_Init(void);

extern int32_t gOotmmPvpHitsSent;
extern int32_t gOotmmPvpHitsReceived;
extern int32_t gOotmmPvpHitsApplied;
extern int32_t gOotmmPvpLastSentKind;
extern int32_t gOotmmPvpLastAppliedKind;

extern const char* gOotmmEquipDlCapture[4];
typedef enum OotmmCustomHandDl {
    OOTMM_CUSTOM_HAND_NONE,
    OOTMM_CUSTOM_HAND_HAMMER,
    OOTMM_CUSTOM_HAND_BOOMERANG,
    OOTMM_CUSTOM_HAND_SLINGSHOT,
    OOTMM_CUSTOM_HAND_DEKU_SHIELD,
} OotmmCustomHandDl;

void OotmmPresence_CaptureCustomHand(int32_t index, int32_t which);
void OotmmPresence_CaptureEquipDl(int32_t index, void* dl);

void* OotmmPuppet_EyeTexture(int32_t playerForm, int32_t eyeIndex);
void* OotmmPuppet_MouthTexture(int32_t playerForm, int32_t mouthIndex);

struct Actor;
struct PlayState;
struct struct_801F58B0;
void OotmmPuppet_SetTunicColor(struct PlayState* play);
void OotmmPuppet_PrepareMaskDraw(struct PlayState* play, struct Actor* puppet, int32_t mask,
                                 struct struct_801F58B0* fairyHair, int32_t resetFairyHair);

#ifdef __cplusplus
}
#endif
