#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#define OOTMM_SONG_STAFF_BASE 0x80

uint16_t OotmmSongs_AvailableMask(void);
uint16_t OotmmSongs_OwnedMask(void);
uint16_t OotmmSongs_ScarecrowBlockMask(void);
int32_t OotmmSongs_FanfareSeqId(int32_t song);

void Ootmm_OcarinaSetSongPlayback(int32_t song);

#ifdef __cplusplus
}
#endif
