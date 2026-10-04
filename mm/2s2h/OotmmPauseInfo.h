#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

int OotmmPauseInfo_Update(struct PlayState* play);
int OotmmPauseInfo_Draw(struct PlayState* play);

#ifdef __cplusplus
}
#endif
