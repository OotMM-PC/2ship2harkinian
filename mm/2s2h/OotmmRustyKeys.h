#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct PlayState;

int OotmmRustyDoorLocked(struct PlayState* play, struct Actor* actor);
void OotmmRustyDoorDrawLock(struct PlayState* play);

#ifdef __cplusplus
}
#endif
