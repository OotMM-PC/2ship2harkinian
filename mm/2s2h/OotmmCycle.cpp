#include "OotmmCycle.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmSession.h"

#include <array>
#include <cstring>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

struct PersistentMask {
    s16 Scene;
    u32 Switch0;
    u32 Switch1;
    u32 Collectible;
};

constexpr PersistentMask kDungeons[] = {
    { SCENE_MITURIN, 0x0070B000, 0x0007FC00 | 0x20000000 | 0x40000000, 0x70000000 },
    { SCENE_HAKUGIN, 0x01A00020, 0x0007B800 | 0x38000000 | 0x40000000, 0x40000000 },
    { SCENE_SEA, 0x00000070, 0x00078000 | 0x20000000 | 0x40000000, 0x7C000000 },
    { SCENE_INISIE_N, 0x04000000, 0x30180000 | 0x40000000, 0x00000000 },
    { SCENE_INISIE_R, 0x04000000, 0x30180000 | 0x40000000, 0x00000000 },
};

constexpr PersistentMask kWell = { SCENE_REDEAD, 0xC0000082, 0x000007FF, 0x00000000 };

struct Snapshot {
    std::array<CycleSceneFlags, ARRAY_COUNT(gSaveContext.cycleSceneFlags)> Cycle;
    std::array<PermanentSceneFlags, ARRAY_COUNT(gSaveContext.save.saveInfo.permanentSceneFlags)> Permanent;
    s16 PlayScene;
    u32 PlayChest;
    u32 PlaySwitches[2];
    u32 PlayCollectible;
    u8 DungeonItems[ARRAY_COUNT(gSaveContext.save.saveInfo.inventory.dungeonItems)];
    s8 DungeonKeys[ARRAY_COUNT(gSaveContext.save.saveInfo.inventory.dungeonKeys)];
    s8 StrayFairies[ARRAY_COUNT(gSaveContext.save.saveInfo.inventory.strayFairies)];
    u32 SkullTokens;
    u8 FierceDeity;
    u32 Sword;
    u8 SwordButton;
    u16 SwordHealth;
};

Snapshot sSnapshot;
bool sCaptured = false;

void Capture() {
    sCaptured = OotmmSession_IsActive() && gPlayState != nullptr;
    if (!sCaptured) {
        return;
    }
    auto& info = gSaveContext.save.saveInfo;
    std::memcpy(sSnapshot.Cycle.data(), gSaveContext.cycleSceneFlags, sizeof(gSaveContext.cycleSceneFlags));
    std::memcpy(sSnapshot.Permanent.data(), info.permanentSceneFlags, sizeof(info.permanentSceneFlags));
    sSnapshot.PlayScene = Play_GetOriginalSceneId(gPlayState->sceneId);
    sSnapshot.PlayChest = gPlayState->actorCtx.sceneFlags.chest;
    sSnapshot.PlaySwitches[0] = gPlayState->actorCtx.sceneFlags.switches[0];
    sSnapshot.PlaySwitches[1] = gPlayState->actorCtx.sceneFlags.switches[1];
    sSnapshot.PlayCollectible = gPlayState->actorCtx.sceneFlags.collectible[0];
    std::memcpy(sSnapshot.DungeonItems, info.inventory.dungeonItems, sizeof(sSnapshot.DungeonItems));
    std::memcpy(sSnapshot.DungeonKeys, info.inventory.dungeonKeys, sizeof(sSnapshot.DungeonKeys));
    std::memcpy(sSnapshot.StrayFairies, info.inventory.strayFairies, sizeof(sSnapshot.StrayFairies));
    sSnapshot.SkullTokens = info.skullTokenCount;
    sSnapshot.FierceDeity = INV_CONTENT(ITEM_MASK_FIERCE_DEITY);
    sSnapshot.Sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    sSnapshot.SwordButton = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
    sSnapshot.SwordHealth = info.playerData.swordHealth;
}

void KeepMasked(const PersistentMask& mask) {
    const bool current = sSnapshot.PlayScene == mask.Scene;
    const CycleSceneFlags& before = sSnapshot.Cycle[mask.Scene];
    const u32 switch0 = (current ? sSnapshot.PlaySwitches[0] : before.switch0) & mask.Switch0;
    const u32 switch1 = (current ? sSnapshot.PlaySwitches[1] : before.switch1) & mask.Switch1;
    const u32 collectible = (current ? sSnapshot.PlayCollectible : before.collectible) & mask.Collectible;
    gSaveContext.cycleSceneFlags[mask.Scene].switch0 |= switch0;
    gSaveContext.cycleSceneFlags[mask.Scene].switch1 |= switch1;
    gSaveContext.cycleSceneFlags[mask.Scene].collectible |= collectible;
    if (current) {
        gPlayState->actorCtx.sceneFlags.switches[0] |= switch0;
        gPlayState->actorCtx.sceneFlags.switches[1] |= switch1;
        gPlayState->actorCtx.sceneFlags.collectible[0] |= collectible;
    }
}

void Restore() {
    if (!sCaptured || !OotmmSession_IsActive() || gPlayState == nullptr) {
        return;
    }
    sCaptured = false;
    auto& info = gSaveContext.save.saveInfo;
    for (size_t scene = 0; scene < sSnapshot.Cycle.size(); scene++) {
        gSaveContext.cycleSceneFlags[scene].chest =
            static_cast<s16>(scene) == sSnapshot.PlayScene ? sSnapshot.PlayChest : sSnapshot.Cycle[scene].chest;
    }
    gPlayState->actorCtx.sceneFlags.chest = sSnapshot.PlayChest;
    for (const auto& dungeon : kDungeons) {
        KeepMasked(dungeon);
        info.permanentSceneFlags[dungeon.Scene].unk_14 = sSnapshot.Permanent[dungeon.Scene].unk_14;
        info.permanentSceneFlags[dungeon.Scene].rooms = sSnapshot.Permanent[dungeon.Scene].rooms;
    }
    if (OotmmSession_GetState().GetStringSetting("beneathWell", "vanilla") != "vanilla") {
        KeepMasked(kWell);
    }
    std::memcpy(info.inventory.dungeonItems, sSnapshot.DungeonItems, sizeof(sSnapshot.DungeonItems));
    std::memcpy(info.inventory.dungeonKeys, sSnapshot.DungeonKeys, sizeof(sSnapshot.DungeonKeys));
    std::memcpy(info.inventory.strayFairies, sSnapshot.StrayFairies, sizeof(sSnapshot.StrayFairies));
    info.skullTokenCount = sSnapshot.SkullTokens;
    if (sSnapshot.FierceDeity == ITEM_MASK_FIERCE_DEITY) {
        INV_CONTENT(ITEM_MASK_FIERCE_DEITY) = ITEM_MASK_FIERCE_DEITY;
    }
    if (sSnapshot.Sword == EQUIP_VALUE_SWORD_RAZOR && GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI) {
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_RAZOR);
        info.playerData.swordHealth = sSnapshot.SwordHealth;
        if (sSnapshot.SwordButton == ITEM_SWORD_RAZOR) {
            BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_SWORD_RAZOR;
            Interface_LoadItemIconImpl(gPlayState, EQUIP_SLOT_B);
        }
    }
}

} // namespace

void OotmmCycle_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::BeforeEndOfCycleSave>(Capture);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::AfterEndOfCycleSave>(Restore);
}
