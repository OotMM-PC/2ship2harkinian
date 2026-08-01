#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Whether the seed's goal is Triforce Hunt or Triforce Quest.
int OotmmTriforce_Active(void);
int OotmmTriforce_Count(void);
/// Pieces needed to win, or 0 when the goal is not a triforce goal.
int OotmmTriforce_Goal(void);
/// Denominator for a pieces counter; the whole pool once the goal is met.
int OotmmTriforce_DisplayMax(void);
int OotmmTriforce_HasWon(void);
/// Polls the goal and starts the credits warp once it is safe to do so.
void OotmmTriforce_Update(void);

#ifdef __cplusplus
}
#endif
