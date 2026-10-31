#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int OotmmTriforce_Active(void);
int OotmmTriforce_Count(void);
int OotmmTriforce_Goal(void);
int OotmmTriforce_DisplayMax(void);
int OotmmTriforce_HasWon(void);
void OotmmTriforce_Update(void);
int OotmmTriforce_OnMajoraDefeated(struct PlayState* play);

#ifdef __cplusplus
}
#endif
