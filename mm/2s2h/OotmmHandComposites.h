#pragma once

// Gfx is an anonymous typedef, so include this after z64.h.

#define OOTMM_HOLD_HEROS_SHIELD "objects/object_link_child/gLinkHumanRightHandHoldingHerosShieldDL"
#define OOTMM_HOLD_MIRROR_SHIELD "objects/object_link_child/gLinkHumanRightHandHoldingMirrorShieldDL"
#define OOTMM_HOLD_KOKIRI_SWORD "objects/object_link_child/gLinkHumanLeftHandHoldingKokiriSwordDL"
#define OOTMM_HOLD_RAZOR_SWORD "objects/object_link_child/gLinkHumanLeftHandHoldingRazorSwordDL"
#define OOTMM_HOLD_GILDED_SWORD "objects/object_link_child/gLinkHumanLeftHandHoldingGildedSwordDL"
#define OOTMM_HOLD_GREAT_FAIRYS_SWORD "objects/object_link_child/gLinkHumanLeftHandHoldingGreatFairysSwordDL"

#ifdef __cplusplus
extern "C" {
#endif

/// Copies a native hand-holding display list with its MM fist call replaced by the ported
/// fist; returns NULL when the native list cannot be loaded or holds no fist call.
Gfx* OotmmHandComposites_Build(const char* nativePath, const char* portedFistPath);

#ifdef __cplusplus
}
#endif
