#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

int OotmmPauseInfo_Update(struct PlayState* play);
int OotmmPauseInfo_Draw(struct PlayState* play);
int OotmmPauseInfo_Screen(void);
int OotmmPauseInfo_Cursor(void);

#ifdef __cplusplus
}
#endif
