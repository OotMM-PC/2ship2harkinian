#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct CollisionHeader;

void OotmmJpLayouts_PatchCollision(struct PlayState* play, struct CollisionHeader* colHeader);

#ifdef __cplusplus
}

void OotmmJpLayouts_Init();
#endif
