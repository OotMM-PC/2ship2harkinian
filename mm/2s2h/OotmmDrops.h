#pragma once

#include <string>

#include <libultraship/bridge/OotmmGameState.h>

struct Actor;
struct PlayState;

/// Spawns a dropped collectible that draws as the given OoTMM item and runs the
/// presentation pipeline on pickup; an empty checkId makes it a debug drop that
/// reports nothing to the launcher.
bool OotmmDrops_Spawn(PlayState* play, float x, float y, float z, const std::string& itemId,
                      const std::string& itemName, const std::string& itemGame,
                      const std::string& checkId);
Actor* OotmmDrops_SpawnPlacement(PlayState* play, float x, float y, float z,
                                 const Ship::OotmmPlacement& placement, int collectibleFlag = -1);
bool OotmmDrops_Adopt(Actor* actor, const Ship::OotmmPlacement& placement, int collectibleFlag);
bool OotmmDrops_HasCheck(const std::string& checkId);
void OotmmDrops_Report(const std::string& checkId, const std::string& itemId, bool present);
void OotmmDrops_Init();
