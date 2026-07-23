#include "OotmmSession.h"

#include "2s2h/GameInteractor/GameInteractor.h"

#include <cstddef>
#include <cstring>
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

extern "C" {
#include "functions.h"
#include "global.h"
#include "overlays/actors/ovl_Bg_F40_Switch/z_bg_f40_switch.h"
#include "overlays/gamestates/ovl_select/z_select.h"
extern GameState* gGameState;
}

namespace {

Ship::OotmmGameState sGameState;
bool sBootedIntoGame = false;

int DamageMultiplier(const std::string& value) {
    if (value == "double") {
        return 1;
    }
    if (value == "quadruple") {
        return 2;
    }
    if (value == "octuple") {
        return 3;
    }
    return value == "ohko" ? 8 : 0;
}

void ApplyEnhancements() {
    CVarSetInteger("gEnhancements.Playback.SkipScarecrowSong",
                   sGameState.GetBoolSetting("freeScarecrowMm", false) ? 1 : 0);
    CVarSetInteger("gEnhancements.Masks.FastTransformation",
                   sGameState.GetBoolSetting("fastMasks", false) ? 1 : 0);
    CVarSetInteger("gEnhancements.Masks.GoronRollingFastSpikes",
                   sGameState.GetBoolSetting("lenientSpikes", true) ? 1 : 0);
    CVarSetInteger("gCheats.HookshotAnywhere",
                   sGameState.GetStringSetting("hookshotAnywhereMm", "off") != "off" ? 1 : 0);
    CVarSetInteger("gCheats.ClimbAnywhere",
                   sGameState.GetStringSetting("climbMostSurfacesMm", "off") != "off" ? 1 : 0);
    CVarSetInteger("gEnhancements.DifficultyOptions.DamageMultiplier",
                   DamageMultiplier(sGameState.GetStringSetting("damageMultiplierMm", "normal")));
    CVarSetInteger("gEnhancements.DifficultyOptions.HyperEnemies",
                   sGameState.GetBoolSetting("mmHyperEnemies", false) ? 1 : 0);
    CVarSetInteger("gEnhancements.Cycle.SaveOnMoonCrash",
                   sGameState.GetStringSetting("moonCrash", "reset") == "cycle" ? 1 : 0);
    CVarSetInteger("gEnhancements.Masks.FierceDeitysAnywhere",
                   sGameState.GetBoolSetting("fierceDeityAnywhere", false) ? 1 : 0);

    CVarSetInteger("gCheats.EasyFrameAdvance", 1);
    CVarSetInteger("gEnhancements.Restorations.PauseBufferWindow", 1);
    CVarSetInteger("gEnhancements.Dpad.DpadEquips", 1);
    CVarSetInteger("gEnhancements.Saving.PauseSave", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipIntroSequence", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipFirstCycle", 0);
    CVarSetInteger("gEnhancements.Cutscenes.SkipStoryCutscenes", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipMiscInteractions", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipEntranceCutscenes", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipOnePointCutscenes", 1);
}

void ApplySaveFlags() {
    if (!sGameState.HasSeed()) {
        return;
    }

    static constexpr struct {
        const char* Key;
        OwlWarpId Warp;
    } owls[] = {
        { "clocktown", OWL_WARP_CLOCK_TOWN },
        { "milkroad", OWL_WARP_MILK_ROAD },
        { "swamp", OWL_WARP_SOUTHERN_SWAMP },
        { "woodfall", OWL_WARP_WOODFALL },
        { "mountain", OWL_WARP_MOUNTAIN_VILLAGE },
        { "snowhead", OWL_WARP_SNOWHEAD },
        { "greatbay", OWL_WARP_GREAT_BAY_COAST },
        { "zoracape", OWL_WARP_ZORA_CAPE },
        { "canyon", OWL_WARP_IKANA_CANYON },
        { "tower", OWL_WARP_STONE_TOWER },
    };
    for (const auto& owl : owls) {
        if (sGameState.WorldFlagContains("mmPreActivatedOwls", owl.Key)) {
            Sram_ActivateOwl(static_cast<uint8_t>(owl.Warp));
        }
    }

    if (sGameState.WorldFlagContains("openDungeonsMm", "WF")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_20_01);
    }
    if (sGameState.WorldFlagContains("openDungeonsMm", "SH")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_30_01);
    }
    if (sGameState.WorldFlagContains("openDungeonsMm", "GB")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_53_20);
    }

    if (sGameState.WorldFlagContains("clearStateDungeonsMm", "WF") ||
        sGameState.IsDungeonPreCompleted("WF") || sGameState.IsDungeonPreCompleted("Woodfall")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE);
    }
    if (sGameState.IsDungeonPreCompleted("SH") || sGameState.IsDungeonPreCompleted("Snowhead")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_SNOWHEAD_TEMPLE);
    }
    if (sGameState.WorldFlagContains("clearStateDungeonsMm", "GB") ||
        sGameState.IsDungeonPreCompleted("GB") || sGameState.IsDungeonPreCompleted("GreatBay")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_GREAT_BAY_TEMPLE);
    }
    if (sGameState.IsDungeonPreCompleted("ST") || sGameState.IsDungeonPreCompleted("StoneTower")) {
        SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_STONE_TOWER_TEMPLE);
    }

    if (sGameState.GetStringSetting("smallKeyShuffleMm", "ownDungeon") == "removed") {
        DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE) = 1;
        DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE) = 3;
        DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE) = 1;
        DUNGEON_KEY_COUNT(DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE) = 4;
    }
    if (sGameState.GetStringSetting("bossKeyShuffleMm", "ownDungeon") == "removed") {
        SET_DUNGEON_ITEM(DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE);
        SET_DUNGEON_ITEM(DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE);
        SET_DUNGEON_ITEM(DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE);
        SET_DUNGEON_ITEM(DUNGEON_BOSS_KEY, DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE);
    }
}

void InitializeSave() {
    gSaveContext.save.time = CLOCK_TIME(8, 0);
    gSaveContext.save.day = 1;
    gSaveContext.save.cutsceneIndex = 0;
    gSaveContext.nextCutsceneIndex = 0;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.linkAge = 0;
    gSaveContext.save.entrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    ApplySaveFlags();
}

bool HasValidFileMagic(const Save& save) {
    static constexpr char magic[] = "ZELDA3";
    return std::memcmp(save.saveInfo.playerData.newf, magic, sizeof(magic) - 1) == 0;
}

bool RestoreSave(SaveContext& target, const uint8_t* data, bool owlSave) {
    Save source{};
    std::memcpy(&source, data, sizeof(source));
    if (!HasValidFileMagic(source)) {
        return false;
    }
    if (owlSave) {
        std::memcpy(&target, data, offsetof(SaveContext, fileNum));
    } else {
        std::memcpy(&target.save, &source, sizeof(source));
        std::memset(target.eventInf, 0, sizeof(target.eventInf));
    }
    return true;
}

void StampFileMagic() {
    static constexpr char magic[] = "ZELDA3";
    std::memcpy(gSaveContext.save.saveInfo.playerData.newf, magic, sizeof(magic) - 1);
}

void PersistNewSave(uint8_t* saveBuffer) {
    gSaveContext.save.saveInfo.checksum = 0;
    gSaveContext.save.saveInfo.checksum = Sram_CalcChecksum(&gSaveContext.save, sizeof(Save));
    std::memset(saveBuffer, 0, SAVE_BUFFER_SIZE);
    std::memcpy(saveBuffer, &gSaveContext.save, sizeof(Save));
    std::memcpy(saveBuffer + SAVE_BUFFER_SIZE_HALF, &gSaveContext.save, sizeof(Save));
    SysFlashrom_WriteDataSync(saveBuffer, gFlashSaveStartPages[FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE],
                              gFlashSpecialSaveNumPages[FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE]);
}

void BootIntoGame() {
    static uint8_t saveBuffer[SAVE_BUFFER_SIZE];
    bool loaded = false;

    std::memset(saveBuffer, 0, sizeof(saveBuffer));
    if (SysFlashrom_ReadData(saveBuffer, gFlashOwlSaveStartPages[0], gFlashOwlSaveNumPages[0]) == 0) {
        loaded = RestoreSave(gSaveContext, saveBuffer, true);
    }
    if (!loaded) {
        std::memset(saveBuffer, 0, sizeof(saveBuffer));
        if (SysFlashrom_ReadData(saveBuffer, gFlashSaveStartPages[0], gFlashSaveNumPages[0]) == 0) {
            loaded = RestoreSave(gSaveContext, saveBuffer, false);
        }
    }

    if (loaded) {
        gSaveContext.save.isOwlSave = false;
    } else {
        Sram_InitNewSave();
        StampFileMagic();
        InitializeSave();
        std::memset(gSaveContext.eventInf, 0, sizeof(gSaveContext.eventInf));
    }

    gSaveContext.save.saveInfo.playerData.magicLevel = 0;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.sceneLayer = 0;
    gSaveContext.save.cutsceneIndex = 0;
    gSaveContext.nextCutsceneIndex = 0;
    for (size_t i = 0; i < ARRAY_COUNT(gSaveContext.cycleSceneFlags); ++i) {
        gSaveContext.cycleSceneFlags[i].chest = gSaveContext.save.saveInfo.permanentSceneFlags[i].chest;
        gSaveContext.cycleSceneFlags[i].switch0 = gSaveContext.save.saveInfo.permanentSceneFlags[i].switch0;
        gSaveContext.cycleSceneFlags[i].switch1 = gSaveContext.save.saveInfo.permanentSceneFlags[i].switch1;
        gSaveContext.cycleSceneFlags[i].clearedRoom = gSaveContext.save.saveInfo.permanentSceneFlags[i].clearedRoom;
        gSaveContext.cycleSceneFlags[i].collectible = gSaveContext.save.saveInfo.permanentSceneFlags[i].collectible;
    }

    const auto entrance = sGameState.GetBootConfig().BootEntrance;
    if (entrance.has_value()) {
        SPDLOG_WARN("[OoTMM] Deferring boot entrance {} until the shared entrance resolver is active", *entrance);
    }
    const uint32_t target = static_cast<uint32_t>(gSaveContext.save.entrance);
    gSaveContext.fileNum = 0xFE;
    MapSelect_LoadGame(reinterpret_cast<MapSelectState*>(gGameState), target, 0);
    gSaveContext.fileNum = 0;
    if (!loaded) {
        GameInteractor_ExecuteOnSaveInit(0);
    }
    GameInteractor_ExecuteOnSaveLoad(0);
    if (!loaded) {
        PersistNewSave(saveBuffer);
    }
}

} // namespace

void OotmmSession_Init() {
    if (sGameState.LoadFromEnvironment()) {
        ApplyEnhancements();
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>(
            [](s16) { InitializeSave(); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>(
            [](s16) { ApplySaveFlags(); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnConsoleLogoUpdate>([]() {
            if (sBootedIntoGame) {
                return;
            }
            sBootedIntoGame = true;
            Rand_Seed(osGetTime());
            gSaveContext.seqId = NA_BGM_DISABLED;
            gSaveContext.ambienceId = AMBIENCE_ID_DISABLED;
            BootIntoGame();
        });
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorInit>(
            ACTOR_BG_F40_SWITCH, [](Actor* actor, bool* should) {
                if (!sGameState.WorldFlagContains("openDungeonsMm", "ST")) {
                    return;
                }
                const s32 switchFlag = BGF40SWITCH_GET_SWITCH_FLAG(actor);
                if (switchFlag > 0 && switchFlag < 0x80 && gPlayState != nullptr) {
                    Flags_SetSwitch(gPlayState, switchFlag);
                }
                *should = false;
            });
        SPDLOG_INFO("[OoTMM] Loaded seed startup state {}", sGameState.GetSeedId());
    } else if (!sGameState.GetLastError().empty()) {
        SPDLOG_ERROR("[OoTMM] {}", sGameState.GetLastError());
    }
}

const Ship::OotmmGameState& OotmmSession_GetState() {
    return sGameState;
}
