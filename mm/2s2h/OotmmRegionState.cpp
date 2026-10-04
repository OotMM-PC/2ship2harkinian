#include "OotmmRegionState.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmSession.h"
#include "OotmmSpecialConds.h"

#include <string>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"

extern PlayState* gPlayState;
}

namespace {

constexpr u8 kWispSwamp = 1 << 0;
constexpr u8 kWispMountain = 1 << 1;
constexpr u8 kWispOcean = 1 << 2;
constexpr u8 kWispValley = 1 << 3;

constexpr int kDungeonWoodfall = 8;
constexpr int kDungeonSnowhead = 9;
constexpr int kDungeonGreatBay = 10;
constexpr int kDungeonStoneTowerInverted = 11;
constexpr int kDungeonStoneTower = 12;

constexpr int kBossOdolwa = 8;
constexpr int kBossTwinmold = 11;

constexpr int kClearBoss = 1 << 0;
constexpr int kClearEffect = 1 << 1;
constexpr int kClearWisp = 1 << 2;

enum class Warp : int {
    Spring,
    SwampClear,
    CoastClear,
    ValleyClear,
    OpenMoon,
    OpenStoneTower,
    OpenStoneTowerInverted,
};

const Ship::OotmmGameState& State() {
    return OotmmSession_GetState();
}

bool WispEnabled(u8 wisp, bool rewardOwned) {
    const std::string mode = State().GetStringSetting("regionState", "dungeonBeaten");
    if (mode == "free") {
        return true;
    }
    if (mode == "reward" && rewardOwned) {
        return true;
    }
    return (gSaveContext.save.shipSaveInfo.ootmmWisps & wisp) != 0;
}

bool sWarpSpawned = false;

void SpawnWarp(PlayState* play, Warp warp, f32 x, f32 y, f32 z) {
    sWarpSpawned = Actor_Spawn(&play->actorCtx, play, ACTOR_OOTMM_CUSTOM_WARP, x, y, z, 0, 0, 0,
                               static_cast<s16>(warp)) != nullptr;
}

void SpawnWarps(PlayState* play) {
    switch (play->sceneId) {
        case SCENE_10YUKIYAMANOMURA:
            if (WispEnabled(kWispMountain, CHECK_QUEST_ITEM(QUEST_REMAINS_GOHT))) {
                SpawnWarp(play, Warp::Spring, -1200.0f, 30.0f, 600.0f);
            }
            break;
        case SCENE_20SICHITAI:
            if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE) &&
                WispEnabled(kWispSwamp, CHECK_QUEST_ITEM(QUEST_REMAINS_ODOLWA))) {
                SpawnWarp(play, Warp::SwampClear, -910.0f, 50.0f, -550.0f);
            }
            break;
        case SCENE_30GYOSON:
            if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_GREAT_BAY_TEMPLE) &&
                WispEnabled(kWispOcean, CHECK_QUEST_ITEM(QUEST_REMAINS_GYORG))) {
                SpawnWarp(play, Warp::CoastClear, -3020.0f, 240.0f, 3921.0f);
            }
            break;
        case SCENE_IKANA:
            if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_STONE_TOWER_TEMPLE) &&
                WispEnabled(kWispValley, CHECK_QUEST_ITEM(QUEST_REMAINS_TWINMOLD))) {
                SpawnWarp(play, Warp::ValleyClear, -700.0f, 200.0f, 2500.0f);
            }
            break;
        case SCENE_OKUJOU:
            if (State().GetBoolSetting("openMoon", false) && OotmmSpecialConds_MoonAccessible()) {
                SpawnWarp(play, Warp::OpenMoon, 212.0f, 30.0f, 0.0f);
            }
            break;
        case SCENE_F40:
            if (State().WorldFlagContains("openDungeonsMm", "ST")) {
                SpawnWarp(play, Warp::OpenStoneTower, 560.0f, -560.0f, 3000.0f);
            }
            break;
        case SCENE_F41:
            if (State().WorldFlagContains("openDungeonsMm", "ST")) {
                SpawnWarp(play, Warp::OpenStoneTowerInverted, 242.0f, 854.0f, -690.0f);
            }
            break;
        default:
            break;
    }
}

void ApplyOpenDungeons() {
    if (State().WorldFlagContains("openDungeonsMm", "WF")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_20_01);
    }
    if (State().WorldFlagContains("openDungeonsMm", "SH")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_30_01);
    }
    if (State().WorldFlagContains("openDungeonsMm", "GB")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_53_20);
    }
}

u8 BossBeatenBit(int dungeonId) {
    switch (dungeonId) {
        case kDungeonWoodfall:
            return 1 << 0;
        case kDungeonSnowhead:
            return 1 << 1;
        case kDungeonGreatBay:
            return 1 << 2;
        case kDungeonStoneTowerInverted:
        case kDungeonStoneTower:
            return 1 << 3;
        default:
            return 0;
    }
}

void ApplyPreCompletedDungeons() {
    const int flags =
        kClearBoss | (State().GetStringSetting("regionState", "dungeonBeaten") == "dungeonBeaten" ? kClearWisp : 0);
    static constexpr struct {
        const char* Key;
        int Dungeon;
    } kDungeons[] = {
        { "WF", kDungeonWoodfall },
        { "SH", kDungeonSnowhead },
        { "GB", kDungeonGreatBay },
        { "IST", kDungeonStoneTowerInverted },
        { "ST", kDungeonStoneTower },
    };
    for (const auto& dungeon : kDungeons) {
        if (State().IsDungeonPreCompleted(dungeon.Key)) {
            OotmmRegionState_SetDungeonFlags(dungeon.Dungeon, flags);
        }
    }
}

void Reload(PlayState* play) {
    Play_SetupRespawnPoint(play, RESPAWN_MODE_RETURN, 0xDFF);
    gSaveContext.respawnFlag = 2;
    play->nextEntrance = gSaveContext.save.entrance;
}

} // namespace

extern "C" void OotmmRegionState_SetDungeonFlags(int dungeonId, int flags) {
    if (flags & kClearBoss) {
        gSaveContext.save.shipSaveInfo.ootmmBossBeaten |= BossBeatenBit(dungeonId);
    }
    switch (dungeonId) {
        case kDungeonWoodfall:
            if (flags & kClearEffect) {
                SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE);
                if (State().WorldFlagContains("clearStateDungeonsMm", "WF")) {
                    SET_WEEKEVENTREG(WEEKEVENTREG_20_01);
                }
            }
            if (flags & kClearWisp) {
                gSaveContext.save.shipSaveInfo.ootmmWisps |= kWispSwamp;
            }
            break;
        case kDungeonSnowhead:
            if (flags & kClearEffect) {
                SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_SNOWHEAD_TEMPLE);
            }
            if (flags & kClearWisp) {
                gSaveContext.save.shipSaveInfo.ootmmWisps |= kWispMountain;
            }
            break;
        case kDungeonGreatBay:
            if (flags & kClearEffect) {
                SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_GREAT_BAY_TEMPLE);
                if (State().WorldFlagContains("clearStateDungeonsMm", "GB")) {
                    SET_WEEKEVENTREG(WEEKEVENTREG_53_20);
                }
            }
            if (flags & kClearWisp) {
                gSaveContext.save.shipSaveInfo.ootmmWisps |= kWispOcean;
            }
            break;
        case kDungeonStoneTowerInverted:
        case kDungeonStoneTower:
            if (flags & kClearEffect) {
                SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_STONE_TOWER_TEMPLE);
            }
            if (flags & kClearWisp) {
                gSaveContext.save.shipSaveInfo.ootmmWisps |= kWispValley;
            }
            break;
        default:
            break;
    }
}

extern "C" void OotmmRegionState_ApplyBossWarp(int bossId) {
    if (bossId >= kBossOdolwa && bossId <= kBossTwinmold) {
        gSaveContext.save.shipSaveInfo.ootmmBossCycle |= static_cast<u8>(1 << (bossId - kBossOdolwa));
    }
    const auto dungeon = State().GetBossDungeon(static_cast<uint32_t>(bossId));
    if (!dungeon.has_value()) {
        return;
    }
    int flags = kClearBoss;
    if (State().GetStringSetting("regionState", "dungeonBeaten") == "dungeonBeaten") {
        flags |= kClearEffect | (State().GetBoolSetting("erMajorDungeons", false) ? kClearWisp : 0);
    }
    OotmmRegionState_SetDungeonFlags(static_cast<int>(*dungeon), flags);
}

extern "C" int OotmmRegionState_BossWarpPadActive(int pad, int vanillaActive) {
    if (!OotmmSession_IsActive() || pad < 0 || pad > 3 ||
        State().GetStringSetting("bossWarpPads", "bossBeaten") != "bossBeaten") {
        return vanillaActive;
    }
    return (gSaveContext.save.shipSaveInfo.ootmmBossBeaten >> pad) & 1;
}

extern "C" int OotmmRegionState_BossCleared(int bossId, int vanillaCleared) {
    if (!OotmmSession_IsActive() || bossId < kBossOdolwa || bossId > kBossTwinmold) {
        return vanillaCleared;
    }
    return (gSaveContext.save.shipSaveInfo.ootmmBossCycle >> (bossId - kBossOdolwa)) & 1;
}

extern "C" void OotmmRegionState_TriggerWarp(PlayState* play, int params) {
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    switch (static_cast<Warp>(params)) {
        case Warp::Spring:
            OotmmRegionState_SetDungeonFlags(kDungeonSnowhead, kClearEffect);
            play->nextEntrance = ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 7);
            break;
        case Warp::SwampClear:
            OotmmRegionState_SetDungeonFlags(kDungeonWoodfall, kClearEffect);
            Reload(play);
            break;
        case Warp::CoastClear:
            OotmmRegionState_SetDungeonFlags(kDungeonGreatBay, kClearEffect);
            Reload(play);
            break;
        case Warp::ValleyClear:
            OotmmRegionState_SetDungeonFlags(kDungeonStoneTower, kClearEffect);
            Reload(play);
            break;
        case Warp::OpenMoon:
            play->nextEntrance = ENTRANCE(THE_MOON, 0);
            gSaveContext.timerStates[TIMER_ID_MOON_CRASH] = TIMER_STATE_OFF;
            break;
        case Warp::OpenStoneTower:
            play->nextEntrance = ENTRANCE(STONE_TOWER_INVERTED, 0);
            Flags_SetSwitch(play, 0x14);
            break;
        case Warp::OpenStoneTowerInverted:
            play->nextEntrance = ENTRANCE(STONE_TOWER, 1);
            Flags_UnsetSwitch(play, 0x14);
            break;
    }
}

void OotmmRegionState_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::BeforeEndOfCycleSave>(
        []() { gSaveContext.save.shipSaveInfo.ootmmBossCycle = 0; });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>([](s16) {
        if (OotmmSession_IsActive()) {
            ApplyPreCompletedDungeons();
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s8, s8) {
        sWarpSpawned = false;
        if (OotmmSession_IsActive()) {
            ApplyOpenDungeons();
        }
    });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorUpdate>(ACTOR_PLAYER, [](Actor*) {
        if (!sWarpSpawned && OotmmSession_IsActive() && gPlayState != nullptr) {
            SpawnWarps(gPlayState);
        }
    });
}
