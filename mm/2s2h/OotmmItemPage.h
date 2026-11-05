#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

int OotmmItemPage_Active(void);
int OotmmItemPage_ConsumePageToggle(struct PlayState* play);
int OotmmItemPage_EquipOutlineIndex(uint8_t cButtonSlot);
void OotmmItemPage_UpdateCursor(struct PlayState* play);
void OotmmItemPage_Draw(struct PlayState* play);
void OotmmItemPage_DrawPageIndicator(struct PlayState* play);
void OotmmItemPage_FillDpadDefaults(void);

#ifdef __cplusplus
}
#endif
