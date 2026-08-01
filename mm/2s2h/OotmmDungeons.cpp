#include "OotmmDungeons.h"

#include "OotmmSession.h"

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

extern "C" int OotmmDungeons_SmallKeysRemoved(void) {
    return OotmmSession_GetState().GetStringSetting("smallKeyShuffleMm", "ownDungeon") == "removed";
}

extern "C" int OotmmDungeons_MaxSmallKeys(int dungeonIndex) {
    if (OotmmDungeons_SmallKeysRemoved()) {
        return 0;
    }
    switch (dungeonIndex) {
        case DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE:
            return 1;
        case DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE:
            return 3;
        case DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE:
            return 1;
        case DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE:
            return 4;
        default:
            return 0;
    }
}

extern "C" int OotmmDungeons_BossDoorIsOpen(void) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    return OotmmSession_GetState().GetStringSetting("bossKeyShuffleMm", "") == "removed";
}
