#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

int OotmmItemCycle_Active(struct PlayState* play);
// True only in a session, for a trade slot whose current item has another owned item to switch to.
int OotmmItemCycle_CanCycle(struct PlayState* play, uint8_t slot);
// Runs the picker of the slot under the item cursor; call every frame the cursor rests on a slot.
void OotmmItemCycle_Update(struct PlayState* play, uint8_t slot);
void OotmmItemCycle_Draw(struct PlayState* play);

#ifdef __cplusplus
}
#endif
