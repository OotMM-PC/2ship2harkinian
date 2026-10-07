#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

void OotmmSaveMenu_Open(struct PlayState* play);
int OotmmSaveMenu_Active(void);
int OotmmSaveMenu_SaveSucceeded(void);
int OotmmSaveMenu_Update(struct PlayState* play);

#ifdef __cplusplus
}
#endif
