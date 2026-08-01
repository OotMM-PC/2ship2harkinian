#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Actor;

/// True when this small-key door neither requires nor consumes a key. Boss doors are never covered.
int OotmmSmallKeyDoorIsOpen(struct PlayState* play, struct Actor* actor);

#ifdef __cplusplus
}
#endif
