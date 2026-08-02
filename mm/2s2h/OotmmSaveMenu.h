#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

/// Replaces the save confirmation with a choice textbox; no-op outside an OoTMM session.
void OotmmSaveMenu_Open(struct PlayState* play);
int OotmmSaveMenu_Active(void);
/// Runs the picked action; 1 once the pause menu may close, so the caller holds it open until then.
int OotmmSaveMenu_Update(struct PlayState* play);

#ifdef __cplusplus
}
#endif
