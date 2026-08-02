#include "OotmmSession.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Enhancements/Saving/SavingEnhancements.h"
#include "OotmmIpc.h"
#include "OotmmItemApply.h"
#include "OotmmItemProbe.h"
#include "OotmmScales.h"

#include <cstddef>
#include <cstring>
#include <fstream>
#include <libultraship/bridge/consolevariablebridge.h>
#include <optional>
#include <spdlog/spdlog.h>

extern "C" {
#include "functions.h"
#include "global.h"
#include "overlays/actors/ovl_Bg_F40_Switch/z_bg_f40_switch.h"
#include "overlays/gamestates/ovl_select/z_select.h"
}

namespace {

Ship::OotmmGameState sGameState;
bool sBootedIntoGame = false;
bool sPlayerExitPending = false;
bool sCrossGamePending = false;
bool sCrossGameAccepted = false;
uint32_t sCrossGameWaitFrames = 0;
std::optional<uint32_t> sLastResolvedEntrance;
uint16_t sGrottoReturnEntrance = 0;

// respawnFlag values: reload the entrance only, restore respawn[TOP], restore the grotto slot.
constexpr int32_t kRespawnSceneEntrance = -2;
constexpr int32_t kRespawnFromOwlSave = -6;
constexpr int32_t kRespawnGrottoPopOut = 4;

constexpr uint32_t kGrottoGenericBase = 0x10000;
constexpr uint32_t kGrottoCowField = 0x1000D;
constexpr uint32_t kGrottoCowCoast = 0x1000E;
constexpr uint32_t kGrottoExitBase = 0x10100;
constexpr uint32_t kGrottoExitCount = 24;
constexpr uint16_t kGrottoTypeGeneric = 0x1440;
constexpr uint16_t kGrottoTypeCow = 0x14A0;

constexpr uint8_t kGrottoDataGeneric[13] = {
    0x1A, 0x1F, 0x1E, 0x1C, 0x1D, 0x1B, 0x19, 0x13, 0x17, 0x15, 0x16, 0x18, 0x14,
};

struct MmGrottoExit {
    uint16_t Entrance;
    uint8_t Room;
    int16_t Position[3];
};

constexpr MmGrottoExit kGrottoExits[kGrottoExitCount] = {
    { 0x5470, 0, { 2367, 315, -192 } },   { 0x5460, 0, { 1012, -221, 3642 } },
    { 0x7A00, 0, { 104, -182, 2202 } },   { 0xC200, 2, { 2, 0, -889 } },
    { 0x8480, 1, { -1700, 38, 1800 } },   { 0x9A80, 1, { 2406, 1168, -1197 } },
    { 0xB400, 0, { -1309, 320, 143 } },   { 0xB010, 0, { -987, 360, -2339 } },
    { 0x6840, 0, { 1359, 80, 5018 } },    { 0x6A00, 0, { -562, 80, 2707 } },
    { 0xA000, 0, { -428, 200, -335 } },   { 0x8040, 1, { 106, 314, -1777 } },
    { 0x2000, 2, { -2475, -505, 2475 } }, { 0x5460, 0, { -375, -222, 3976 } },
    { 0x6870, 0, { 2077, 333, -215 } },   { 0x5480, 0, { 192, 48, -3138 } },
    { 0x5470, 0, { 4450, 254, 925 } },    { 0x5400, 0, { -2782, 48, -1654 } },
    { 0x5460, 0, { -1592, -222, 4622 } }, { 0x5480, 0, { -2425, -281, -3291 } },
    { 0x5470, 0, { 3223, 219, 1417 } },   { 0x5460, 0, { -2317, -221, 3418 } },
    { 0x5400, 0, { -5159, -281, -571 } }, { 0xB400, 0, { 589, 195, 53 } },
};

constexpr MmGrottoExit kMountainVillageWinterExit = { 0x9A80, 0, { 345, 8, -150 } };
int8_t sCowGrottoVariant = -1;

void ApplyGrottoExit(const MmGrottoExit& exit) {
    RespawnData* respawn = &gSaveContext.respawn[RESPAWN_MODE_UNK_3];
    respawn->pos.x = static_cast<float>(exit.Position[0]);
    respawn->pos.y = static_cast<float>(exit.Position[1]);
    respawn->pos.z = static_cast<float>(exit.Position[2]);
    respawn->yaw = 0;
    respawn->entrance = exit.Entrance;
    respawn->playerParams = 0x04FF;
    respawn->data = 0;
    respawn->roomIndex = exit.Room;
    respawn->tempSwitchFlags = 0;
    respawn->unk_18 = 0;
    respawn->tempCollectFlags = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN] = *respawn;
    gSaveContext.respawn[RESPAWN_MODE_TOP] = *respawn;
    gSaveContext.respawnFlag = kRespawnGrottoPopOut;
    sGrottoReturnEntrance = exit.Entrance;
}

std::optional<uint32_t> CurrentGrottoExitId() {
    if (gPlayState == nullptr || gPlayState->sceneId != SCENE_KAKUSIANA) {
        return std::nullopt;
    }

    switch (gPlayState->roomCtx.curRoom.num) {
        case 0x00:
            return kGrottoExitBase + 17;
        case 0x01:
            return kGrottoExitBase + 18;
        case 0x02:
            return kGrottoExitBase + 16;
        case 0x03:
            return kGrottoExitBase + 15;
        case 0x04: {
            const uint8_t data = gSaveContext.respawn[RESPAWN_MODE_UNK_3].data & 0x1F;
            for (uint32_t i = 0; i < 13; ++i) {
                if (kGrottoDataGeneric[i] == data) {
                    return kGrottoExitBase + i;
                }
            }
            return std::nullopt;
        }
        case 0x07:
            return kGrottoExitBase + 19;
        case 0x09:
            return kGrottoExitBase + 20;
        case 0x0A:
            if (sCowGrottoVariant >= 0) {
                return kGrottoExitBase + (sCowGrottoVariant == 0 ? 13 : 14);
            }
            if (gSaveContext.respawn[RESPAWN_MODE_UNK_3].data == -1) {
                return kGrottoExitBase + 14;
            }
            if (gSaveContext.respawn[RESPAWN_MODE_UNK_3].data == 31) {
                return kGrottoExitBase + 13;
            }
            return kGrottoExitBase +
                   ((static_cast<uint16_t>(gSaveContext.respawn[RESPAWN_MODE_UNK_3].entrance) & 0xFE00) == 0x6800
                        ? 14
                        : 13);
        case 0x0B:
            return kGrottoExitBase + 22;
        case 0x0D:
            return kGrottoExitBase + 21;
        case 0x0E:
            return kGrottoExitBase + 23;
        default:
            return std::nullopt;
    }
}

std::optional<uint32_t> ExtendedEntranceSource() {
    if (gPlayState == nullptr) {
        return std::nullopt;
    }
    if (gSaveContext.respawnFlag == 4 && gPlayState->sceneId == SCENE_KAKUSIANA) {
        return CurrentGrottoExitId();
    }

    const uint16_t next = gPlayState->nextEntrance;
    if (next == kGrottoTypeGeneric) {
        const uint8_t data = gSaveContext.respawn[RESPAWN_MODE_UNK_3].data & 0x1F;
        for (uint32_t i = 0; i < 13; ++i) {
            if (kGrottoDataGeneric[i] == data) {
                return kGrottoGenericBase + i;
            }
        }
    } else if (next == kGrottoTypeCow) {
        return gPlayState->sceneId == SCENE_30GYOSON ? kGrottoCowCoast : kGrottoCowField;
    }
    return std::nullopt;
}

std::optional<uint16_t> ResolveMmEntrance(uint32_t entrance) {
    if (entrance >= kGrottoGenericBase && entrance < kGrottoGenericBase + 13) {
        RespawnData* respawn = &gSaveContext.respawn[RESPAWN_MODE_UNK_3];
        respawn->data = static_cast<int8_t>((respawn->data & ~0x1F) |
                                            kGrottoDataGeneric[entrance - kGrottoGenericBase]);
        sCowGrottoVariant = -1;
        if (gSaveContext.respawnFlag == 4) {
            gSaveContext.respawnFlag = 0;
        }
        return kGrottoTypeGeneric;
    }
    if (entrance == kGrottoCowField || entrance == kGrottoCowCoast) {
        sCowGrottoVariant = entrance == kGrottoCowCoast ? 1 : 0;
        gSaveContext.respawn[RESPAWN_MODE_UNK_3].data =
            static_cast<int8_t>(entrance == kGrottoCowCoast ? -1 : 31);
        if (gSaveContext.respawnFlag == 4) {
            gSaveContext.respawnFlag = 0;
        }
        return kGrottoTypeCow;
    }
    if (entrance >= kGrottoExitBase && entrance < kGrottoExitBase + kGrottoExitCount) {
        const uint32_t index = entrance - kGrottoExitBase;
        const MmGrottoExit& exit =
            index == 5 && !CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_SNOWHEAD_TEMPLE)
                ? kMountainVillageWinterExit
                : kGrottoExits[index];
        ApplyGrottoExit(exit);
        sCowGrottoVariant = -1;
        return exit.Entrance;
    }
    if (entrance > UINT16_MAX) {
        return std::nullopt;
    }
    if (gSaveContext.respawnFlag == 4) {
        gSaveContext.respawnFlag = 0;
    }
    sCowGrottoVariant = -1;
    return static_cast<uint16_t>(entrance);
}

bool IsResolvedReloadTarget(uint32_t entrance) {
    for (int i = 0; i < RESPAWN_MODE_MAX; ++i) {
        if (entrance == static_cast<uint16_t>(gSaveContext.respawn[i].entrance)) {
            return true;
        }
    }
    return entrance == static_cast<uint16_t>(gSaveContext.save.entrance);
}

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
    CVarSetInteger("gEnhancements.Restorations.JPGrottos",
                   sGameState.WorldFlagContains("jpLayouts", "DekuPalace") ? 1 : 0);

    CVarSetInteger("gCheats.EasyFrameAdvance", 1);
    CVarSetInteger("gEnhancements.Restorations.PauseBufferWindow", 1);
    CVarSetInteger("gEnhancements.Dpad.DpadEquips", 1);
    CVarSetInteger("gEnhancements.Saving.PauseSave", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipIntroSequence", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipFirstCycle", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipStoryCutscenes", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipMiscInteractions", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipEntranceCutscenes", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipOnePointCutscenes", 1);
    CVarSetInteger("gEnhancements.Cutscenes.SkipEnemyCutscenes", 1);
    CVarSetInteger("gEnhancements.Songs.FasterSongPlayback", 1);
    CVarSetInteger("gEnhancements.Songs.SkipSoTCutscenes", 1);
    CVarSetInteger("gEnhancements.Songs.SkipSoaringCutscene", 1);
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
    // The seed hands out MM's sword and shield; Fierce Deity's own blade is part of the form.
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
    for (auto& form : gSaveContext.save.saveInfo.equips.buttonItems) {
        for (auto& item : form) {
            if (item == ITEM_SWORD_KOKIRI) {
                item = ITEM_NONE;
            }
        }
    }
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

uint32_t BootSlot() {
    // OoT picks its file at its own file select, so the marker it leaves outranks the launcher slot.
    std::ifstream in(sGameState.GetBootConfig().StatePath + ".last-file");
    int32_t fileNum = -1;
    if (in >> fileNum && fileNum >= 0 && fileNum < 3) {
        return static_cast<uint32_t>(fileNum);
    }
    return sGameState.GetBootConfig().LogicalSlot.value_or(0);
}

void PersistNewSave(uint8_t* saveBuffer) {
    gSaveContext.save.saveInfo.checksum = 0;
    gSaveContext.save.saveInfo.checksum = Sram_CalcChecksum(&gSaveContext.save, sizeof(Save));
    std::memset(saveBuffer, 0, SAVE_BUFFER_SIZE);
    std::memcpy(saveBuffer, &gSaveContext.save, sizeof(Save));
    std::memcpy(saveBuffer + SAVE_BUFFER_SIZE_HALF, &gSaveContext.save, sizeof(Save));
    const uint32_t page = FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE + BootSlot() * FLASH_SAVE_MAIN_MULTIPLIER;
    SysFlashrom_WriteDataSync(saveBuffer, gFlashSaveStartPages[page], gFlashSpecialSaveNumPages[page]);
}

void PersistTransitionSave() {
    const bool wasOwlSave = gSaveContext.save.isOwlSave;
    gSaveContext.save.isOwlSave = true;
    SavingEnhancements_PersistSaveEntranceInfo();
    SavingEnhancements_AdvancePlaytime();
    Play_SaveCycleSceneFlags(gPlayState);
    gSaveContext.save.saveInfo.playerData.savedSceneId = gPlayState->sceneId;
    func_8014546C(&gPlayState->sramCtx);
    Sram_SetFlashPagesOwlSave(&gPlayState->sramCtx,
                              gFlashOwlSaveStartPages[gSaveContext.fileNum * FLASH_SAVE_MAIN_MULTIPLIER],
                              gFlashOwlSaveNumPages[gSaveContext.fileNum * FLASH_SAVE_MAIN_MULTIPLIER]);
    Sram_StartWriteToFlashOwlSave(&gPlayState->sramCtx);
    gSaveContext.save.isOwlSave = wasOwlSave;
    SavingEnhancements_ClearSaveEntranceInfo();
}

std::optional<uint16_t> InitialMmSpawn() {
    const bool adult = sGameState.GetStringSetting("startingAge", "child") == "adult";
    const auto* mapping =
        sGameState.FindEntrance(Ship::OotmmGame::Oot, adult ? uint32_t{ 0x0F20 } : uint32_t{ 0x00BB });
    if (mapping == nullptr || mapping->ToGame != Ship::OotmmGame::Mm || !mapping->ToNativeId.has_value()) {
        return std::nullopt;
    }
    return ResolveMmEntrance(*mapping->ToNativeId);
}

void UpdateEntranceTransition() {
    if (gPlayState == nullptr) {
        return;
    }
    if (sCrossGamePending) {
        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
        sCrossGameAccepted |= OotmmIpc_ConsumeTransitionAccepted();
        if (!sCrossGameAccepted && ++sCrossGameWaitFrames > 120) {
            SPDLOG_ERROR("[OoTMM] Launcher did not accept the cross-game transition");
            sCrossGamePending = false;
            sCrossGameWaitFrames = 0;
        }
        return;
    }
    if (gPlayState->transitionTrigger != TRANS_TRIGGER_START) {
        sPlayerExitPending = false;
        sLastResolvedEntrance.reset();
        return;
    }

    const bool playerExit = sPlayerExitPending;
    sPlayerExitPending = false;
    const uint32_t nextEntrance = static_cast<uint16_t>(gPlayState->nextEntrance);
    if (sLastResolvedEntrance.has_value() && nextEntrance == *sLastResolvedEntrance) {
        return;
    }
    if (!playerExit && (gSaveContext.respawnFlag == 1 || gSaveContext.respawnFlag == 2 ||
                        gSaveContext.respawnFlag == kRespawnGrottoPopOut ||
                        gSaveContext.respawnFlag == 8 || gSaveContext.respawnFlag < 0) &&
        IsResolvedReloadTarget(nextEntrance)) {
        sLastResolvedEntrance = nextEntrance;
        return;
    }

    const auto extendedSource = ExtendedEntranceSource();
    if (gSaveContext.respawnFlag == 4 && gPlayState->sceneId == SCENE_KAKUSIANA &&
        !extendedSource.has_value()) {
        return;
    }
    const uint32_t source = extendedSource.value_or(nextEntrance);
    if (sLastResolvedEntrance.has_value() && source == *sLastResolvedEntrance) {
        return;
    }
    sGrottoReturnEntrance = 0;
    const auto* mapping = sGameState.FindEntrance(Ship::OotmmGame::Mm, source);
    if (mapping == nullptr || !mapping->ToNativeId.has_value()) {
        return;
    }
    if (!mapping->IsCrossGame()) {
        if (const auto target = ResolveMmEntrance(*mapping->ToNativeId); target.has_value()) {
            gPlayState->nextEntrance = *target;
            sLastResolvedEntrance = *target;
            if (gSaveContext.respawnFlag == -2 &&
                gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance == static_cast<uint16_t>(nextEntrance)) {
                gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance = *target;
            }
        } else {
            SPDLOG_ERROR("[OoTMM] Unsupported MM entrance target {}", *mapping->ToNativeId);
        }
        return;
    }

    SPDLOG_INFO("[OoTMM] Cross-game entrance 0x{:X} -> {} (0x{:X})", source, mapping->To,
                *mapping->ToNativeId);
    PersistTransitionSave();
    if (!OotmmIpc_SendCrossGameTransition(*mapping, sGameState.GetBootConfig().OotAge)) {
        SPDLOG_ERROR("[OoTMM] Cross-game transition requires the launcher IPC connection");
        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
        return;
    }

    sCrossGamePending = true;
    sCrossGameAccepted = false;
    sCrossGameWaitFrames = 0;
    sLastResolvedEntrance = source;
    gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
}

void BootIntoGame(GameState* gameState) {
    static uint8_t saveBuffer[SAVE_BUFFER_SIZE];
    bool loaded = false;

    const uint32_t slot = BootSlot();
    const uint32_t owlPage = slot * FLASH_SAVE_MAIN_MULTIPLIER;
    std::memset(saveBuffer, 0, sizeof(saveBuffer));
    if (SysFlashrom_ReadData(saveBuffer, gFlashOwlSaveStartPages[owlPage],
                             gFlashOwlSaveNumPages[owlPage]) == 0) {
        loaded = RestoreSave(gSaveContext, saveBuffer, true);
    }
    if (!loaded) {
        std::memset(saveBuffer, 0, sizeof(saveBuffer));
        if (SysFlashrom_ReadData(saveBuffer, gFlashSaveStartPages[slot], gFlashSaveNumPages[slot]) == 0) {
            loaded = RestoreSave(gSaveContext, saveBuffer, false);
        }
    }

    if (loaded) {
        gSaveContext.save.isOwlSave = false;
    } else {
        Sram_InitNewSave();
        StampFileMagic();
        GameInteractor_ExecuteOnSaveInit(static_cast<s16>(slot));
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

    uint32_t target = static_cast<uint16_t>(gSaveContext.save.entrance);
    const auto bootEntrance = sGameState.GetBootConfig().BootEntrance;
    if (!loaded && !bootEntrance.has_value()) {
        if (const auto spawn = InitialMmSpawn(); spawn.has_value()) {
            target = *spawn;
            gSaveContext.save.entrance = *spawn;
        }
    } else if (bootEntrance.has_value()) {
        const auto entrance = bootEntrance;
        if (const auto resolved = ResolveMmEntrance(*entrance); resolved.has_value()) {
            target = *resolved;
            gSaveContext.save.entrance = *resolved;
        } else {
            SPDLOG_ERROR("[OoTMM] Unsupported MM boot entrance {}", *entrance);
        }
    }
    SPDLOG_INFO("[OoTMM] Booting MM at entrance 0x{:X}", target);
    gSaveContext.fileNum = 0xFE;
    MapSelect_LoadGame(reinterpret_cast<MapSelectState*>(gameState), target, 0);
    gSaveContext.fileNum = static_cast<s16>(slot);
    GameInteractor_ExecuteOnSaveLoad(static_cast<s16>(slot));
    if (!loaded) {
        PersistNewSave(saveBuffer);
    }
}

} // namespace

void OotmmSession_Init() {
    if (sGameState.LoadFromEnvironment()) {
        ApplyEnhancements();
        OotmmItemProbe_Init();
        OotmmItemApply_Init();
        OotmmScales_Init();
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>(
            [](s16) { InitializeSave(); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>(
            [](s16) { ApplySaveFlags(); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(UpdateEntranceTransition);
        // The seed owns the Blast Mask cooldown; vanilla hardcodes it to 310 frames.
        COND_VB_SHOULD(VB_SET_BLAST_MASK_COOLDOWN_TIMER, true, {
            *should = false;
            GET_PLAYER(gPlayState)->blastMaskTimer =
                static_cast<s16>(sGameState.GetBlastMaskCooldownFrames());
        });
        COND_VB_SHOULD(VB_TERMINA_FIELD_BE_EMPTY, true, { *should = false; });
        COND_VB_SHOULD(VB_FASTER_FIRST_CYCLE, true, { *should = false; });
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

extern "C" int32_t OotmmSession_IsActive(void) {
    return sGameState.IsActive() && sGameState.HasSeed() ? 1 : 0;
}

extern "C" int32_t OotmmSession_TryBootDirectly(void* gameState) {
    if (!OotmmSession_IsActive() || sBootedIntoGame || gameState == nullptr) {
        return 0;
    }
    sBootedIntoGame = true;
    Rand_Seed(osGetTime());
    gSaveContext.seqId = NA_BGM_DISABLED;
    gSaveContext.ambienceId = AMBIENCE_ID_DISABLED;
    BootIntoGame(static_cast<GameState*>(gameState));
    return 1;
}

extern "C" void OotmmSession_NotePlayerExitTransition(void) {
    if (sGameState.IsActive() && sGameState.HasSeed()) {
        sPlayerExitPending = true;
    }
}

extern "C" int32_t OotmmSession_ReturnToSpawn(void) {
    if (!OotmmSession_IsActive() || gPlayState == nullptr) {
        return 0;
    }
    // MM has no spawn of its own in the seed, so the arrival entrance is where Link came in.
    const auto& boot = sGameState.GetBootConfig();
    if (!boot.BootEntrance.has_value()) {
        return 0;
    }
    const auto target = ResolveMmEntrance(*boot.BootEntrance);
    if (!target.has_value()) {
        return 0;
    }
    gPlayState->nextEntrance = *target;
    gSaveContext.respawnFlag = 0;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
    sLastResolvedEntrance = *target;
    return 1;
}

extern "C" void OotmmSession_ApplyDeathRespawn(void) {
    if (!OotmmSession_IsActive() || gPlayState == nullptr) {
        return;
    }
    if (gPlayState->sceneId == SCENE_KAKUSIANA) {
        gPlayState->nextEntrance = gSaveContext.save.entrance;
        gSaveContext.respawnFlag = kRespawnSceneEntrance;
        return;
    }
    if (gSaveContext.respawnFlag == kRespawnFromOwlSave || sGrottoReturnEntrance == 0 ||
        sGrottoReturnEntrance != gSaveContext.save.entrance) {
        return;
    }
    gPlayState->nextEntrance = gSaveContext.respawn[RESPAWN_MODE_UNK_3].entrance;
    gSaveContext.respawnFlag = kRespawnGrottoPopOut;
}

extern "C" int32_t OotmmSession_ApplyResolvedMmEntrance(uint32_t entrance) {
    if (!OotmmSession_IsActive()) {
        return -1;
    }
    const auto resolved = ResolveMmEntrance(entrance);
    if (!resolved.has_value()) {
        return -1;
    }
    sLastResolvedEntrance = *resolved;
    return *resolved;
}

bool OotmmSession_BeginCrossGameTransition(const Ship::OotmmEntranceMapping& mapping) {
    if (!OotmmSession_IsActive() || gPlayState == nullptr || sCrossGamePending) {
        return false;
    }
    SPDLOG_INFO("[OoTMM] Cross-game warp {} -> {} (0x{:X})", mapping.From, mapping.To,
                mapping.ToNativeId.value_or(0));
    PersistTransitionSave();
    if (!OotmmIpc_SendCrossGameTransition(mapping, sGameState.GetBootConfig().OotAge)) {
        SPDLOG_ERROR("[OoTMM] Cross-game warp requires the launcher IPC connection");
        return false;
    }
    sCrossGamePending = true;
    sCrossGameAccepted = false;
    sCrossGameWaitFrames = 0;
    gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
    return true;
}

extern "C" void OotmmSession_RedirectMoonCrashRespawn(void) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    // Upstream play.c sends the Clock Tower moon-crash respawn to the seed's initial entrance,
    // which for a cross-game session is the MM entry spawn.
    if (static_cast<uint16_t>(gSaveContext.save.entrance) != ENTRANCE(CLOCK_TOWER_INTERIOR, 3)) {
        return;
    }
    std::optional<uint16_t> spawn;
    if (const auto bootEntrance = sGameState.GetBootConfig().BootEntrance; bootEntrance.has_value()) {
        spawn = ResolveMmEntrance(*bootEntrance);
    }
    if (!spawn.has_value()) {
        spawn = InitialMmSpawn();
    }
    if (spawn.has_value()) {
        SPDLOG_INFO("[OoTMM] Moon crash respawn redirected to entrance 0x{:X}", *spawn);
        gSaveContext.save.entrance = *spawn;
    }
}
