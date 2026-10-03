#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct PlayState;

int OotmmXflags_IsShuffled(struct Actor* actor);
int OotmmXflags_Drop(struct Actor* actor, struct PlayState* play);
int OotmmXflags_DropAt(struct Actor* actor, struct PlayState* play, float x, float y, float z);
int OotmmXflags_DropSlice(struct Actor* actor, struct PlayState* play, int slice, float x, float y, float z);
int OotmmXflags_DropStill(struct Actor* actor, struct PlayState* play);
struct Actor* OotmmXflags_SpawnSlice(struct Actor* actor, struct PlayState* play, int slice, float x, float y, float z,
                                     int* handled);
int OotmmXflags_IsAnySliceShuffled(struct Actor* actor, int count);
void OotmmXflags_TagChild(struct Actor* child, struct Actor* parent, int slice);
void OotmmXflags_TagGrassGroup(struct Actor* manager, int group, struct Actor* unit);
int OotmmXflags_Collect(struct Actor* actor);
void OotmmXflags_PrepareSpawn(struct Actor* parent, int slice);
void OotmmXflags_ClearSpawn(void);
void OotmmXflags_ClaimSpawn(struct Actor* actor);
int OotmmXflags_PrepareFairy(struct Actor* butterfly);

#ifdef __cplusplus
}

#include <cstdint>

void OotmmXflags_Init();
int64_t OotmmXflags_NativeId(struct Actor* actor);
int OotmmXflags_LastScene();
#endif
