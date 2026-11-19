#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

int OotmmSongPage_Active(void);
void OotmmSongPage_ConsumePageToggle(struct PlayState* play);
// The other game's song shown in a song slot of the second page, counted from the first song slot; 0 when none.
int32_t OotmmSongPage_SlotSong(int slot);
// The song slot whose note colour a slot uses, so each song keeps its own game's colour on the second page.
int OotmmSongPage_ColorSlot(int slot);
// The second page song under the cursor; 0 on the first page.
int32_t OotmmSongPage_HoverSong(struct PlayState* play);
// The item the cursor names: the native one, or on the second page the slot's item when its song has a name.
uint16_t OotmmSongPage_CursorItem(struct PlayState* play, uint16_t nativeItem);
void OotmmSongPage_EnterHover(struct PlayState* play);
const char* OotmmSongPage_NameTexture(void);
void OotmmSongPage_DrawPageIndicator(struct PlayState* play);

#ifdef __cplusplus
}
#endif
