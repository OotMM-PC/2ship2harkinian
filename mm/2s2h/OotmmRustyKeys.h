#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct PlayState;

int OotmmRustyDoorLocked(struct PlayState* play, struct Actor* actor);

#ifdef __cplusplus
}
#endif
