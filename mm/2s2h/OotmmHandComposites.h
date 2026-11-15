#pragma once


#define OOTMM_HOLD_HEROS_SHIELD "objects/object_link_child/gLinkHumanRightHandHoldingHerosShieldDL"
#define OOTMM_HOLD_MIRROR_SHIELD "objects/object_link_child/gLinkHumanRightHandHoldingMirrorShieldDL"
#define OOTMM_HOLD_BOW "objects/object_link_child/gLinkHumanRightHandHoldingBowDL"
#define OOTMM_HOLD_HOOKSHOT "objects/object_link_child/gLinkHumanRightHandHoldingHookshotDL"
#define OOTMM_HOLD_KOKIRI_SWORD "objects/object_link_child/gLinkHumanLeftHandHoldingKokiriSwordDL"
#define OOTMM_HOLD_RAZOR_SWORD "objects/object_link_child/gLinkHumanLeftHandHoldingRazorSwordDL"
#define OOTMM_HOLD_GILDED_SWORD "objects/object_link_child/gLinkHumanLeftHandHoldingGildedSwordDL"
#define OOTMM_HOLD_GREAT_FAIRYS_SWORD "objects/object_link_child/gLinkHumanLeftHandHoldingGreatFairysSwordDL"

#ifdef __cplusplus
extern "C" {
#endif

Gfx* OotmmHandComposites_Build(const char* nativePath, const char* portedFistPath);
const char* OotmmHandComposites_NativePath(const void* commands);

#ifdef __cplusplus
}
#endif
