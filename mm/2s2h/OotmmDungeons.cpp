#include "OotmmDungeons.h"

#include "OotmmSession.h"

#include <libultraship/bridge/OotmmProgress.h>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

extern "C" int OotmmDungeons_SmallKeysRemoved(void) {
    return OotmmSession_GetState().GetStringSetting("smallKeyShuffleMm", "ownDungeon") == "removed";
}

extern "C" int OotmmDungeons_MaxSmallKeys(int dungeonIndex) {
    const char* code = nullptr;
    switch (dungeonIndex) {
        case DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE:
            code = "WF";
            break;
        case DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE:
            code = "SH";
            break;
        case DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE:
            code = "GB";
            break;
        case DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE:
            code = "ST";
            break;
        default:
            return 0;
    }
    return static_cast<int>(Ship::OotmmMaxSmallKeys(OotmmSession_GetState(), Ship::OotmmGame::Mm, code));
}

extern "C" int OotmmDungeons_BossDoorIsOpen(void) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    return OotmmSession_GetState().GetStringSetting("bossKeyShuffleMm", "") == "removed";
}
