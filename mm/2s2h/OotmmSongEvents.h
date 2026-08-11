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

/// The song id (0-19) this event demands; the vanilla song when the seed does not shuffle.
int32_t OotmmSongEvents_Assigned(int32_t eventId);
/// Whether the just-played song satisfies the event, native or cross-game.
int32_t OotmmSongEvents_Matches(int32_t eventId, int32_t playedSong);
/// Clears the played-song latches so an accepted event cannot double-fire.
void OotmmSongEvents_ClearPlayed(struct PlayState* play);
/// Swaps free play for a song-prompt action when a nearby pad's asked song is playable.
int32_t OotmmSongEvents_PromptAction(struct PlayState* play, int32_t action);
/// Whether the active ocarina action is a pad-initiated prompt.
int32_t OotmmSongEvents_IsPadPrompt(struct PlayState* play);
/// Opens the pad prompt's staff text, which advertises B to Stop like free play.
void OotmmSongEvents_OpenPadPromptText(struct PlayState* play);
/// Whether a nearby pad's event claims the song just played, owning its resolution.
int32_t OotmmSongEvents_PadOwnsPlayedSong(struct PlayState* play);

#ifdef __cplusplus
}

void OotmmSongEvents_Init();
#endif
