#pragma once

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

void OotmmSongs_NotePlayed(int32_t song);
const OcarinaSongButtons* AudioOcarina_OotmmSongButtons(int32_t song);
void AudioOcarina_SetOotmmAvailSongs(uint16_t mask);
int32_t OotmmSongs_Played(void);
void OotmmSongs_ClearPlayed(void);

int32_t OotmmSongs_ShouldOverridePlayedText(PlayState* play);
void OotmmSongs_ShowPlayedText(PlayState* play);
int32_t OotmmSongs_EffectActorId(void);
uint16_t OotmmSongs_EffectActorParams(void);

void OotmmSongs_Update(PlayState* play, Player* player);
int32_t OotmmSongs_OverrideSoaringDestination(PlayState* play, int32_t owlMode);

#ifdef __cplusplus
}
#endif
