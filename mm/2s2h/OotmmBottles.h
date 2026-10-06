#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

void OotmmBottles_Init(void);
int32_t OotmmBottles_Shared(void);
uint32_t OotmmBottles_ContentOf(uint8_t item);
uint8_t OotmmBottles_ItemFor(uint32_t content);
void OotmmBottles_SetSlot(int32_t bottle, uint32_t content);
int32_t OotmmBottles_RutoLetterHeld(void);
void OotmmBottles_UseRutoLetter(struct PlayState* play);

#ifdef __cplusplus
}
#endif
