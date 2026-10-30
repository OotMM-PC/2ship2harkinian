#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

typedef enum {
    OOTMM_MM_SONG_EVENT_TEMPLE_WOODFALL,
    OOTMM_MM_SONG_EVENT_TEMPLE_SNOWHEAD,
    OOTMM_MM_SONG_EVENT_TEMPLE_GREAT_BAY,
    OOTMM_MM_SONG_EVENT_HEALING_POE_HUT,
    OOTMM_MM_SONG_EVENT_HEALING_DARMANI,
    OOTMM_MM_SONG_EVENT_HEALING_PAMELA_FATHER,
    OOTMM_MM_SONG_EVENT_HEALING_KAMARO,
    OOTMM_MM_SONG_EVENT_HEALING_MIKAU,
    OOTMM_MM_SONG_EVENT_AWAKENING_KEETA,
    OOTMM_MM_SONG_EVENT_AWAKENING_SCRUB,
    OOTMM_MM_SONG_EVENT_LULLABY_KID,
    OOTMM_MM_SONG_EVENT_STORMS_COMPOSER,
    OOTMM_MM_SONG_EVENT_CLOCK_TOWER_ROOF,
    OOTMM_MM_SONG_EVENT_MAX,
} OotmmMmSongEvent;

int32_t OotmmSongEvents_Assigned(int32_t eventId);
int32_t OotmmSongEvents_Matches(int32_t eventId, int32_t playedSong);
void OotmmSongEvents_ClearPlayed(struct PlayState* play);
int32_t OotmmSongEvents_PromptAction(struct PlayState* play, int32_t action);
int32_t OotmmSongEvents_IsPadPrompt(struct PlayState* play);
void OotmmSongEvents_OpenPadPromptText(struct PlayState* play);
int32_t OotmmSongEvents_PadOwnsPlayedSong(struct PlayState* play);

#ifdef __cplusplus
}

void OotmmSongEvents_Init();
#endif
