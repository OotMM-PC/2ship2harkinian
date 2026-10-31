#include "OotmmSession.h"
#include "OotmmDeathLink.h"
#include "SaveManager/SaveManager.h"
#include "BenGui/Notification.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Enhancements/Saving/SavingEnhancements.h"
#include "OotmmBottles.h"
#include "OotmmCheckLedger.h"
#include "OotmmChecks.h"
#include "OotmmNpcChecks.h"
#include "OotmmHints.h"
#include "OotmmItemDescriptions.h"
#include "OotmmDekuShield.h"
#include "OotmmAgony.h"
#include "OotmmShops.h"
#include "OotmmCsmc.h"
#include "OotmmDrops.h"
#include "OotmmFaroresWind.h"
#include "OotmmXflags.h"
#include "OotmmDebugChecks.h"
#include "OotmmIpc.h"
#include "OotmmSongEvents.h"
#include "OotmmItemApply.h"
#include "OotmmAdultLink.h"
#include "OotmmItemProbe.h"
#include "OotmmScales.h"
#include "OotmmStrength.h"
#include "OotmmClockSpeed.h"
#include "OotmmCycle.h"
#include "OotmmRegionState.h"
#include "OotmmSpecialConds.h"
#include "OotmmPresence.h"

#include <cstddef>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <libultraship/bridge/consolevariablebridge.h>
#include <optional>
#include <string>
#include <spdlog/spdlog.h>
#include <ship/utils/AtomicFile.h>
#include <ship/window/Window.h>

extern "C" {
#include "functions.h"
#include "global.h"
#include "overlays/actors/ovl_Bg_F40_Switch/z_bg_f40_switch.h"
#include "overlays/gamestates/ovl_select/z_select.h"
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
}

namespace {

Ship::OotmmGameState sGameState;
bool sBootedIntoGame = false;
constexpr int kBootInventoryWaitFrames = 300;
int sBootInventoryWait = 0;

bool AwaitingBootInventory() {
    if (OotmmIpc_GetInventory().GetRevision() != 0 || sBootInventoryWait >= kBootInventoryWaitFrames) {
        return false;
    }
    sBootInventoryWait++;
    return true;
}
std::optional<std::array<uint8_t, SAVE_BUFFER_SIZE>> sInitialSave;
uint32_t sInitialSavePage = 0;
std::chrono::steady_clock::time_point sInitialSaveRetryAt{};
bool sPlayerExitPending = false;
bool sCrossGamePending = false;
bool sCrossGameAccepted = false;
uint32_t sCrossGameWaitFrames = 0;
Ship::OotmmEntranceMapping sCrossGameRequest;
std::optional<Ship::OotmmEntranceMapping> sCrossGameRetry;
uint32_t sCrossGameRetryFrames = 0;
std::optional<uint32_t> sLastResolvedEntrance;
std::optional<uint32_t> sPendingWallmasterSource;
uint16_t sGrottoReturnEntrance = 0;
std::string sCrossGameWaitReason;

void QueueCrossGameRetry(const Ship::OotmmEntranceMapping& mapping, const char* reason) {
    sCrossGameRetry = mapping;
    sCrossGameRetryFrames = 0;
    gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
    if (sCrossGameWaitReason != reason) {
        sCrossGameWaitReason = reason;
        Notification::Emit({ .prefix = "OoTMM", .message = reason, .remainingTime = 8.0f });
    }
}


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
    if (sPendingWallmasterSource.has_value()) {
        return sPendingWallmasterSource;
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
    CVarSetInteger("gCheats.HookshotAnywhere",
                   sGameState.GetStringSetting("hookshotAnywhereMm", "off") != "off" ? 1 : 0);
    CVarSetInteger("gCheats.ClimbAnywhere",
                   sGameState.GetBoolSetting("climbMostSurfacesMm", false) ? 1 : 0);
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

    gSaveContext.save.isFirstCycle = true;
    SET_WEEKEVENTREG(WEEKEVENTREG_59_04);

    // Refill form B slots emptied by the old swordless start: an empty form B triggers the
    // "Weird B" bug, where attacking uses the item sharing the do-action label's id.
    if (BUTTON_ITEM_EQUIP(PLAYER_FORM_GORON, EQUIP_SLOT_B) == ITEM_NONE) {
        BUTTON_ITEM_EQUIP(PLAYER_FORM_GORON, EQUIP_SLOT_B) = ITEM_SWORD_KOKIRI;
    }
    if (BUTTON_ITEM_EQUIP(PLAYER_FORM_ZORA, EQUIP_SLOT_B) == ITEM_NONE) {
        BUTTON_ITEM_EQUIP(PLAYER_FORM_ZORA, EQUIP_SLOT_B) = ITEM_SWORD_KOKIRI;
    }
    if (BUTTON_ITEM_EQUIP(PLAYER_FORM_DEKU, EQUIP_SLOT_B) == ITEM_NONE) {
        BUTTON_ITEM_EQUIP(PLAYER_FORM_DEKU, EQUIP_SLOT_B) = ITEM_FD;
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

    if (sGameState.GetStringSetting("beneathWell", "vanilla") == "open") {
        gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_REDEAD].switch0 |= 0xC0000082;
        gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_REDEAD].switch1 |= 0x000007FF;
        gSaveContext.cycleSceneFlags[SCENE_REDEAD].switch0 |= 0xC0000082;
        gSaveContext.cycleSceneFlags[SCENE_REDEAD].switch1 |= 0x000007FF;
    }
}

void InitializeSave() {
    OotmmItemApply_ResetLedgerForNewSave();
    gSaveContext.save.time = CLOCK_TIME(8, 0);
    gSaveContext.save.day = 1;
    gSaveContext.save.cutsceneIndex = 0;
    gSaveContext.nextCutsceneIndex = 0;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.linkAge = 0;
    gSaveContext.save.hasTatl = true;
    gSaveContext.save.entrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    // The seed hands out MM's sword and shield; Fierce Deity's own blade is part of the form.
    // Only the human row's B goes empty: the form rows must keep their vanilla items (the Zora
    // one doubles as the fin-throw item action) or the Weird B bug fires on form attacks.
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
    if (BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) == ITEM_SWORD_KOKIRI) {
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_NONE;
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
        target.save.playerForm = PLAYER_FORM_HUMAN;
        target.save.equippedMask = 0;
        target.save.entrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    }
    return true;
}

void StampFileMagic() {
    static constexpr char magic[] = "ZELDA3";
    std::memcpy(gSaveContext.save.saveInfo.playerData.newf, magic, sizeof(magic) - 1);
}

uint32_t BootSlot() {
    if (const auto slot = sGameState.GetBootConfig().LogicalSlot; slot.has_value()) {
        return *slot;
    }
    std::ifstream in(sGameState.GetBootConfig().StatePath + ".last-file");
    int32_t fileNum = -1;
    if (in >> fileNum && fileNum >= 0 && fileNum < 3) {
        return static_cast<uint32_t>(fileNum);
    }
    return 0;
}

void RetryInitialSave() {
    if (!sInitialSave || std::chrono::steady_clock::now() < sInitialSaveRetryAt) {
        return;
    }
    sInitialSaveRetryAt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    if (SaveManager_WriteInitialOotmmSave(sInitialSave->data(), gFlashSaveStartPages[sInitialSavePage],
                                        gFlashSpecialSaveNumPages[sInitialSavePage]) == 0) {
        sInitialSave.reset();
        OotmmItemApply_RefreshSaveSnapshot();
    }
}

void PersistNewSave(uint8_t* saveBuffer) {
    gSaveContext.save.saveInfo.checksum = 0;
    gSaveContext.save.saveInfo.checksum = Sram_CalcChecksum(&gSaveContext.save, sizeof(Save));
    std::memset(saveBuffer, 0, SAVE_BUFFER_SIZE);
    std::memcpy(saveBuffer, &gSaveContext.save, sizeof(Save));
    std::memcpy(saveBuffer + SAVE_BUFFER_SIZE_HALF, &gSaveContext.save, sizeof(Save));
    sInitialSave.emplace();
    std::memcpy(sInitialSave->data(), saveBuffer, SAVE_BUFFER_SIZE);
    sInitialSavePage = FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE + BootSlot() * FLASH_SAVE_MAIN_MULTIPLIER;
    sInitialSaveRetryAt = {};
    RetryInitialSave();
}

bool PersistTransitionSave() {
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
    const bool saved = SaveManager_SysFlashrom_WriteData(gPlayState->sramCtx.saveBuf,
                                                        gPlayState->sramCtx.curPage,
                                                        gPlayState->sramCtx.numPages) == 0;
    gPlayState->sramCtx.status = 0;
    gSaveContext.save.isOwlSave = wasOwlSave;
    SavingEnhancements_ClearSaveEntranceInfo();
    return saved;
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
    if (sCrossGameRetry.has_value()) {
        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
        if (++sCrossGameRetryFrames >= 60) {
            const auto mapping = *sCrossGameRetry;
            sCrossGameRetry.reset();
            OotmmSession_BeginCrossGameTransition(mapping);
        }
        return;
    }
    if (sCrossGamePending) {
        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
        sCrossGameAccepted |= OotmmIpc_ConsumeTransitionAccepted();
        if (!sCrossGameAccepted && ++sCrossGameWaitFrames > 120) {
            SPDLOG_ERROR("[OoTMM] Launcher did not accept the cross-game transition; retrying");
            QueueCrossGameRetry(sCrossGameRequest, "The launcher has not confirmed the switch. Retrying.");
            sCrossGamePending = false;
            sCrossGameWaitFrames = 0;
        }
        return;
    }
    if (gPlayState->transitionTrigger != TRANS_TRIGGER_START) {
        sPlayerExitPending = false;
        sPendingWallmasterSource.reset();
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
    sPendingWallmasterSource.reset();
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
    if (OotmmSession_BeginCrossGameTransition(*mapping)) {
        sLastResolvedEntrance = source;
    }
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
        if (SysFlashrom_ReadData(saveBuffer, gFlashSaveStartPages[owlPage], gFlashSaveNumPages[owlPage]) == 0) {
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
    } else if (bootEntrance == kOotmmFwCrossEntrance) {
        if (const auto arrival = OotmmFaroresWind_PrepareArrival(static_cast<int32_t>(slot)); arrival.has_value()) {
            target = *arrival;
            gSaveContext.save.entrance = *arrival;
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

uint32_t WallmasterSource(int16_t sceneId, int8_t room) {
    if (sceneId == SCENE_DANPEI2TEST) {
        return 0xFF00;
    }
    if (sceneId != SCENE_REDEAD) {
        return 0;
    }
    switch (room) {
        case 1:
            return 0xFF01;
        case 5:
            return 0xFF02;
        case 7:
            return 0xFF03;
        default:
            return 0;
    }
}

} // namespace

void OotmmSession_Init() {
    if (sGameState.LoadFromEnvironment()) {
        ApplyEnhancements();
        OotmmCheckLedger_Init();
        OotmmChecks_Init();
        OotmmNpcChecks_Init();
        OotmmShops_Init();
        OotmmHints_Init();
        OotmmItemDescriptions_Init();
        OotmmDekuShield_Init();
        OotmmAgony_Init();
        OotmmBottles_Init();
        OotmmCsmc_Init();
        OotmmDrops_Init();
        OotmmXflags_Init();
        OotmmDebugChecks_Init();
        OotmmSongEvents_Init();
        OotmmItemProbe_Init();
        OotmmItemApply_Init();
        OotmmScales_Init();
        OotmmStrength_Init();
        OotmmFaroresWind_Init();
        OotmmAdultLink_Init();
        OotmmPresence_Init();
        OotmmDeathLink_Init();
        OotmmSpecialConds_Init();
        OotmmRegionState_Init();
        OotmmClockSpeed_Init();
        OotmmCycle_Init();
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>(
            [](s16) { InitializeSave(); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>(
            [](s16) { ApplySaveFlags(); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(UpdateEntranceTransition);
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(RetryInitialSave);
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

bool OotmmSession_HasPendingInitialSave() {
    return sInitialSave.has_value();
}

extern "C" int32_t OotmmSession_SaveProgress(void) {
    return OotmmSession_IsActive() && gPlayState != nullptr && gSaveContext.fileNum >= 0 &&
           gSaveContext.fileNum < 3 && gSaveContext.rupeeAccumulator == 0 && PersistTransitionSave();
}

extern "C" int32_t OotmmSession_IsActive(void) {
    return sGameState.IsActive() && sGameState.HasSeed() ? 1 : 0;
}

extern "C" int32_t OotmmSession_BoolSetting(const char* key) {
    return OotmmSession_IsActive() && sGameState.GetBoolSetting(key, false) ? 1 : 0;
}

extern "C" int32_t OotmmSession_TryBootDirectly(void* gameState) {
    if (OotmmSession_IsActive() && !sBootedIntoGame && gameState != nullptr &&
        !sGameState.GetBootConfig().OpenFileSelect && AwaitingBootInventory()) {
        return 1;
    }
    if (OotmmSession_IsActive() && !sBootedIntoGame && gameState != nullptr &&
        sGameState.GetBootConfig().MmRecovery.has_value()) {
        const auto& boot = sGameState.GetBootConfig();
        const auto& recovery = *boot.MmRecovery;
        std::string error;
        const bool restored = SaveManager_RecoverOotmmBackup(recovery.Primary, recovery.Backup, error);
        const nlohmann::json result = {{"version", 1}, {"saveId", boot.SaveId}, {"slot", *boot.LogicalSlot},
                                       {"restored", restored}, {"reason", error}};
        std::error_code writeError;
        const bool reported = Ship::WriteFileAtomically(boot.StatePath + ".recovery-result.json", result.dump(), writeError);
        if (!restored || !reported) {
            sBootedIntoGame = true;
            SPDLOG_ERROR("[OoTMM] Save recovery stopped: {} {}", error, writeError.message());
            Ship::Context::GetInstance()->GetWindow()->Close();
            return 1;
        }
    }
    if (OotmmSession_IsActive() && !sBootedIntoGame && gameState != nullptr) {
        std::string error;
        if (!SaveManager_ValidateCurrentOotmmSave(error)) {
            const auto& boot = sGameState.GetBootConfig();
            const nlohmann::json result = {{"version", 1}, {"saveId", boot.SaveId}, {"slot", boot.LogicalSlot.value_or(0)},
                                           {"restored", false}, {"reason", error}};
            std::error_code writeError;
            Ship::WriteFileAtomically(boot.StatePath + ".recovery-result.json", result.dump(), writeError);
            sBootedIntoGame = true;
            SPDLOG_ERROR("[OoTMM] Save validation stopped: {}", error);
            Ship::Context::GetInstance()->GetWindow()->Close();
            return 1;
        }
    }
    if (OotmmSession_IsActive() && sGameState.GetBootConfig().OpenFileSelect && !sBootedIntoGame && gameState != nullptr) {
        sBootedIntoGame = true;
        auto* menuState = static_cast<GameState*>(gameState);
        menuState->running = false;
        SET_NEXT_GAMESTATE(menuState, FileSelect_Init, sizeof(FileSelectState));
        return 1;
    }

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

extern "C" int32_t OotmmSession_WallmasterTakePlayer(PlayState* play, Actor* wallmaster) {
    if (!OotmmSession_IsActive() || sGameState.GetStringSetting("erWallmasters", "none") == "none") {
        return 0;
    }
    const uint32_t source = WallmasterSource(play->sceneId, wallmaster->room);
    if (source == 0 || sGameState.FindEntrance(Ship::OotmmGame::Mm, source) == nullptr) {
        return 0;
    }
    sPendingWallmasterSource = source;
    sLastResolvedEntrance.reset();
    play->nextEntrance = gSaveContext.save.entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
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

extern "C" int32_t OotmmSession_RedirectBossWarp(uint32_t bossId) {
    constexpr uint32_t kForeignEntrance = 0x80000000;
    if (!OotmmSession_IsActive() || gPlayState == nullptr) {
        return 0;
    }
    OotmmRegionState_ApplyBossWarp(static_cast<int>(bossId));
    const bool woodfallTempleRaised = CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE) &&
                                      CHECK_WEEKEVENTREG(WEEKEVENTREG_20_01);
    const auto destination = sGameState.ResolveBossWarp(bossId, woodfallTempleRaised);
    if (!destination.has_value()) {
        return 0;
    }

    gSaveContext.nextCutsceneIndex = 0xFFEF;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    if ((*destination & kForeignEntrance) != 0) {
        const auto resolved = ResolveMmEntrance(*destination & ~kForeignEntrance);
        if (!resolved.has_value()) {
            SPDLOG_ERROR("[OoTMM] Unsupported MM boss-warp target 0x{:X}", *destination);
            return 0;
        }
        gPlayState->nextEntrance = *resolved;
        sLastResolvedEntrance = *resolved;
        return 1;
    }

    Ship::OotmmEntranceMapping mapping;
    mapping.FromGame = Ship::OotmmGame::Mm;
    mapping.From = "MM boss warp";
    mapping.ToGame = Ship::OotmmGame::Oot;
    mapping.To = "OoT dungeon exit";
    mapping.ToNativeId = *destination;
    if (!OotmmSession_BeginCrossGameTransition(mapping)) {
        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
    }
    return 1;
}

bool OotmmSession_BeginCrossGameTransition(const Ship::OotmmEntranceMapping& mapping) {
    if (!OotmmSession_IsActive() || gPlayState == nullptr || sCrossGamePending) {
        return false;
    }
    if (sCrossGameRetry.has_value()) {
        gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
        return true;
    }
    SPDLOG_INFO("[OoTMM] Cross-game warp {} -> {} (0x{:X})", mapping.From, mapping.To,
                mapping.ToNativeId.value_or(0));
    if (!PersistTransitionSave()) {
        SPDLOG_ERROR("[OoTMM] Cross-game transition waiting for a successful source save");
        QueueCrossGameRetry(mapping, "Could not save. Waiting to retry before switching games.");
        return true;
    }
    if (!OotmmIpc_SendCrossGameTransition(mapping, sGameState.GetBootConfig().OotAge)) {
        SPDLOG_ERROR("[OoTMM] Cross-game transition waiting for the launcher connection");
        QueueCrossGameRetry(mapping, "Waiting for the launcher to reconnect before switching games.");
        return true;
    }
    sCrossGameRequest = mapping;
    sCrossGamePending = true;
    sCrossGameAccepted = false;
    sCrossGameWaitFrames = 0;
    gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
    return true;
}

static bool IsEntranceShuffle() {
    for (const char* key : { "erBoss", "erRegions", "erWarps", "erGrottos", "erWallmasters", "erOverworld" }) {
        if (sGameState.GetStringSetting(key, "none") != "none") {
            return true;
        }
    }
    for (const char* key : { "erMajorDungeons", "erMinorDungeons", "erSpiderHouses", "erPirateFortress",
                             "erBeneathWell", "erIkanaCastle", "erSecretShrine", "erGanonCastle", "erGanonTower",
                             "erMoon", "erIndoorsMajor", "erIndoorsExtra", "erIndoorsTelescopes", "erOneWaysMajor",
                             "erOneWaysIkana", "erOneWaysSongs", "erOneWaysStatues", "erOneWaysOwls" }) {
        if (sGameState.GetBoolSetting(key, false)) {
            return true;
        }
    }
    return false;
}

extern "C" int32_t OotmmSession_KeepsSaveOnMoonCrash(void) {
    return OotmmSession_IsActive() && sGameState.GetStringSetting("moonCrash", "reset") != "cycle";
}

extern "C" int32_t OotmmSession_ReadMoonCrashSave(uint8_t* buffer) {
    if (!OotmmSession_KeepsSaveOnMoonCrash() || gSaveContext.fileNum < 0 || gSaveContext.fileNum >= 3) {
        return false;
    }
    const uint32_t owlPage = static_cast<uint32_t>(gSaveContext.fileNum) * FLASH_SAVE_MAIN_MULTIPLIER;
    if (SysFlashrom_ReadData(buffer, gFlashOwlSaveStartPages[owlPage], gFlashOwlSaveNumPages[owlPage]) != 0) {
        return false;
    }
    Save save{};
    std::memcpy(&save, buffer, sizeof(save));
    return HasValidFileMagic(save);
}

extern "C" void OotmmSession_RedirectMoonCrashRespawn(void) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    // Upstream play.c sends the Clock Tower moon-crash respawn to Clock Town, or with any entrance
    // shuffle to the entrance MM was entered from.
    if (static_cast<uint16_t>(gSaveContext.save.entrance) != ENTRANCE(CLOCK_TOWER_INTERIOR, 3)) {
        return;
    }
    std::optional<uint16_t> spawn;
    if (!IsEntranceShuffle()) {
        spawn = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    } else if (const auto bootEntrance = sGameState.GetBootConfig().BootEntrance; bootEntrance.has_value()) {
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
