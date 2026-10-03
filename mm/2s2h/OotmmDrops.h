#pragma once

#include <string>

struct PlayState;

/// Spawns a dropped collectible that draws as the given OoTMM item and runs the
/// presentation pipeline on pickup; an empty checkId makes it a debug drop that
/// reports nothing to the launcher.
bool OotmmDrops_Spawn(PlayState* play, float x, float y, float z, const std::string& itemId,
                      const std::string& itemName, const std::string& itemGame,
                      const std::string& checkId);
void OotmmDrops_Init();
