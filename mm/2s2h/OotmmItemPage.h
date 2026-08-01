#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

int OotmmItemPage_Active(void);
/// Cycles the Select Item page on L; returns 1 when it consumed the press.
int OotmmItemPage_ConsumePageToggle(struct PlayState* play);
/// Inventory grid index to source the equipped-item outline from, or -1 to hide it.
int OotmmItemPage_EquipOutlineIndex(uint8_t cButtonSlot);
void OotmmItemPage_UpdateCursor(struct PlayState* play);
void OotmmItemPage_Draw(struct PlayState* play);
void OotmmItemPage_DrawPageIndicator(struct PlayState* play);

#ifdef __cplusplus
}
#endif
