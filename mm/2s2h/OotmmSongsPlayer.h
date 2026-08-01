#pragma once

#include <stdint.h>

// PlayState and Player are anonymous typedefs, so include this after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

/// Records the OoT song the ocarina just matched.
void OotmmSongs_NotePlayed(int32_t song);
int32_t OotmmSongs_Played(void);
void OotmmSongs_ClearPlayed(void);

/// Whether the "You played" textbox needs the custom text path (OoT songs and MM's empty Sun's Song entry).
int32_t OotmmSongs_ShouldOverridePlayedText(PlayState* play);
void OotmmSongs_ShowPlayedText(PlayState* play);
/// Ocarina effect actor for the song just played, or -1 when it has none.
int32_t OotmmSongs_EffectActorId(void);
uint16_t OotmmSongs_EffectActorParams(void);

void OotmmSongs_Update(PlayState* play, Player* player);
/// Redirects the soaring owl's destination while an OoT warp song is pending; 1 when handled.
int32_t OotmmSongs_OverrideSoaringDestination(PlayState* play);

#ifdef __cplusplus
}
#endif
