#include "OotmmChecks.h"
#include "OotmmCheckLedger.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmIpc.h"
#include "OotmmItemPresentation.h"
#include "OotmmItemModels.h"
#include "OotmmSession.h"

#include <cmath>

extern "C" {
#include "functions.h"
#include "global.h"
#include "overlays/actors/ovl_En_Box/z_en_box.h"

extern void func_80848294(PlayState* play, Player* player);
extern void Player_SetAction_PreserveMoveFlags(PlayState* play, Player* player, PlayerActionFunc actionFunc,
                                               s32 flags);
extern s32 Player_SetupWaitForPutAway(PlayState* play, Player* player, AfterPutAwayFunc func);
extern void Player_StopCutscene(Player* player);
}

namespace {

// The chest placement bound while its give sequence runs; the slow-chest cutscene
// decision fires later in the same open and reads it.
const Ship::OotmmPlacement* sActiveChest = nullptr;
bool sWasConnected = false;

const Ship::OotmmPlacement* FindChest(uint32_t scene, uint32_t flag) {
    return OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm, scene, "chest", flag);
}

bool IsBossScene(uint32_t scene) {
    return scene == SCENE_MITURIN_BS || scene == SCENE_HAKUGIN_BS ||
           scene == SCENE_SEA_BS || scene == SCENE_INISIE_BS;
}

const Ship::OotmmPlacement* FindBossContainer(uint32_t scene) {
    return IsBossScene(scene)
               ? OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm, scene, "collectible", 0x1F)
               : nullptr;
}

bool IsCollected(const Ship::OotmmPlacement& placement) {
    return OotmmCheckLedger_Contains(placement.CheckId) || OotmmIpc_IsCheckCompleted(placement.CheckId);
}

void Report(const Ship::OotmmPlacement& placement, bool present = false) {
    OotmmIpc_SendCheckCollected(placement.CheckId, present && !IsCollected(placement));
    OotmmCheckLedger_Record(placement.CheckId);
}

void SweepSavedBossContainers() {
    for (const auto& placement : OotmmSession_GetState().GetPlacements()) {
        if (placement.CheckGame != Ship::OotmmGame::Mm || placement.CheckType != "collectible" ||
            !placement.CheckScene.has_value() || !placement.CheckFlag.has_value() ||
            *placement.CheckFlag != 0x1F || !IsBossScene(*placement.CheckScene)) {
            continue;
        }
        const uint32_t scene = *placement.CheckScene;
        uint32_t flags = gSaveContext.cycleSceneFlags[scene].collectible;
        if (gPlayState != nullptr && static_cast<uint32_t>(Play_GetOriginalSceneId(gPlayState->sceneId)) == scene) {
            flags |= gPlayState->actorCtx.sceneFlags.collectible[0];
        }
        if ((flags & (1u << 0x1F)) != 0 && !OotmmIpc_IsCheckCompleted(placement.CheckId)) {
            Report(placement);
        }
    }
}

// Chest-open continuation without the vanilla give: the open animation plays to its
// end and control returns; the queued presentation then runs the get-item raise.
void ChestOpenAction(Player* player, PlayState* play) {
    if (PlayerAnimation_Update(play, &player->skelAnime)) {
        Player_StopCutscene(player);
        func_80848294(play, player);
    }
}

void ChestOpenAfterPutAway(PlayState* play, Player* player) {
    Player_SetAction_PreserveMoveFlags(play, player, ChestOpenAction, 0);
    player->stateFlags1 |= (PLAYER_STATE1_400 | PLAYER_STATE1_20000000);
}

void SweepSavedChests() {
    for (const auto& placement : OotmmSession_GetState().GetPlacements()) {
        if (placement.CheckGame != Ship::OotmmGame::Mm || placement.CheckType != "chest" ||
            !placement.CheckScene.has_value() || !placement.CheckFlag.has_value()) {
            continue;
        }
        const uint32_t scene = *placement.CheckScene;
        const uint32_t flag = *placement.CheckFlag;
        if (scene >= ARRAY_COUNT(gSaveContext.cycleSceneFlags) || flag > 31) {
            continue;
        }
        uint32_t chestBits = gSaveContext.cycleSceneFlags[scene].chest;
        if (gPlayState != nullptr &&
            static_cast<uint32_t>(Play_GetOriginalSceneId(gPlayState->sceneId)) == scene) {
            chestBits |= gPlayState->actorCtx.sceneFlags.chest;
        }
        if ((chestBits & (1u << flag)) != 0 && !OotmmIpc_IsCheckCompleted(placement.CheckId)) {
            Report(placement);
        }
    }
}

} // namespace

extern "C" int OotmmChecks_BossContainerDone(PlayState* play, int vanillaDone) {
    const auto* placement = play != nullptr ? FindBossContainer(Play_GetOriginalSceneId(play->sceneId)) : nullptr;
    return placement == nullptr ? vanillaDone : IsCollected(*placement);
}

extern "C" int OotmmChecks_BossContainerIsShuffled(PlayState* play) {
    return play != nullptr && FindBossContainer(Play_GetOriginalSceneId(play->sceneId)) != nullptr;
}

extern "C" int OotmmChecks_TryCollectBossContainer(Actor* actor, PlayState* play) {
    if (actor == nullptr || play == nullptr) {
        return 0;
    }
    const auto* placement = FindBossContainer(Play_GetOriginalSceneId(play->sceneId));
    if (placement == nullptr) {
        return 0;
    }
    if (IsCollected(*placement)) {
        return 1;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr || actor->xzDistToPlayer >= 30.0f || std::fabs(actor->playerHeightRel) >= 80.0f ||
        OotmmItemPresentation_IsPending() || Message_GetState(&play->msgCtx) != TEXT_STATE_NONE ||
        play->transitionTrigger != TRANS_TRIGGER_OFF || Player_InBlockingCsMode(play, player) ||
        (player->stateFlags1 &
         (PLAYER_STATE1_DEAD | PLAYER_STATE1_400 | PLAYER_STATE1_TALKING | PLAYER_STATE1_20000000)) ||
        player->getItemId > GI_NONE) {
        return 0;
    }
    Report(*placement, true);
    return 1;
}

extern "C" int OotmmChecks_DrawBossContainer(Actor* actor, PlayState* play) {
    if (actor == nullptr || play == nullptr) {
        return 0;
    }
    const auto* placement = FindBossContainer(Play_GetOriginalSceneId(play->sceneId));
    if (placement == nullptr) {
        return 0;
    }
    if (IsCollected(*placement)) {
        return 1;
    }
    const std::string& itemId = placement->CloakItemId.empty() ? placement->ItemId : placement->CloakItemId;
    const std::string& itemName = placement->CloakItemId.empty() ? placement->ItemName : placement->CloakItemName;
    Matrix_Push();
    if (actor->scale.x > 0.0001f) {
        const float scale = 0.35f / actor->scale.x;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }
    if (!OotmmItemModel_DrawById(play, itemId, itemName)) {
        GetItem_Draw(play, GID_RUPEE_GREEN);
    }
    Matrix_Pop();
    return 1;
}

void OotmmChecks_OpenChestSilently(PlayState* play, Player* player) {
    Player_SetupWaitForPutAway(play, player, ChestOpenAfterPutAway);
}

void OotmmChecks_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneFlagSet>(
        [](s16 sceneId, FlagType flagType, u32 flag) {
            if (flagType != FLAG_CYCL_SCENE_CHEST) {
                return;
            }
            if (const auto* placement = FindChest(Play_GetOriginalSceneId(sceneId), flag)) {
                if (!OotmmCheckLedger_Contains(placement->CheckId)) {
                    Report(*placement);
                }
            }
        });

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_CHEST, {
        EnBox* chest = va_arg(args, EnBox*);
        Player* player = GET_PLAYER(gPlayState);
        sActiveChest = FindChest(Play_GetOriginalSceneId(gPlayState->sceneId),
                                 ENBOX_GET_CHEST_FLAG(&chest->dyna.actor));
        if (sActiveChest == nullptr && gPlayState->sceneId == SCENE_TAKARAYA && chest->getItemId == GI_HEART_PIECE) {
            sActiveChest = OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm, "npc", "MM_CHEST_GAME");
            if (sActiveChest != nullptr && IsCollected(*sActiveChest) && player != nullptr) {
                player->getItemId = -GI_RECOVERY_HEART;
                sActiveChest = nullptr;
                return;
            }
        }
        if (sActiveChest != nullptr && player != nullptr) {
            OotmmChecks_OpenChestSilently(gPlayState, player);
            Report(*sActiveChest, true);
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAY_SLOW_CHEST_CS, {
        if (sActiveChest != nullptr) {
            *should = sActiveChest->Major;
        }
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>(
        [](s16) {
            SweepSavedChests();
            SweepSavedBossContainers();
        });

    // A reconnect replays anything the launcher never acknowledged.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        const bool connected = OotmmIpc_IsConnected();
        if (connected && !sWasConnected) {
            SweepSavedChests();
            SweepSavedBossContainers();
        }
        sWasConnected = connected;
    });
}
