#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cross-game OoT songs playable in MM, ordered to match upstream's ootSongs bits.
typedef enum OotmmMmSongId {
    OOTMM_SONG_NONE,
    OOTMM_SONG_MINUET,
    OOTMM_SONG_BOLERO,
    OOTMM_SONG_SERENADE,
    OOTMM_SONG_REQUIEM,
    OOTMM_SONG_NOCTURNE,
    OOTMM_SONG_PRELUDE,
    OOTMM_SONG_ZELDA,
    OOTMM_SONG_SARIA,
    OOTMM_SONG_MAX,
} OotmmMmSongId;

#define OOTMM_SONG_COUNT (OOTMM_SONG_MAX - 1)

// Ocarina staff states for OoT songs sit above MM's vanilla song ids (0..24, 0xFE, 0xFF free).
#define OOTMM_SONG_STAFF_BASE 0x80

/// Bit (1 << song) is set for every OoT song the player may play right now.
uint16_t OotmmSongs_AvailableMask(void);
/// Bit (1 << song) is set for every OoT song the scarecrow or Termina wall songs must not contain.
uint16_t OotmmSongs_ScarecrowBlockMask(void);
/// Custom sequence id for the song's fanfare, or -1 when the asset pack lacks it.
int32_t OotmmSongs_FanfareSeqId(int32_t song);

void Ootmm_OcarinaSetSongPlayback(int32_t song);

#ifdef __cplusplus
}
#endif
