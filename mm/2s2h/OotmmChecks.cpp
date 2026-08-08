#include "OotmmChecks.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmIpc.h"
#include "OotmmItemPresentation.h"
#include "OotmmSession.h"

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

Ship::GameIpcItemPresentation MakePresentation(const Ship::OotmmPlacement& placement) {
    Ship::GameIpcItemPresentation presentation;
    presentation.ItemId = placement.ItemId;
    presentation.ItemName = placement.ItemName;
    presentation.ItemGame = placement.ItemGame;
    presentation.DestinationGame = placement.ItemGame;
    presentation.SourcePlayer = OotmmSession_GetState().GetPlayerId();
    presentation.RecipientPlayer = placement.OwnerPlayer;
    presentation.ViewerPlayer = presentation.SourcePlayer;
    return presentation;
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

// Chests already opened in this cycle whose checks never reached the launcher —
// a crash between the flag landing and the acknowledgement — are reported here.
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
            OotmmIpc_SendCheckCollected(placement.CheckId);
        }
    }
}

} // namespace

void OotmmChecks_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneFlagSet>(
        [](s16 sceneId, FlagType flagType, u32 flag) {
            if (flagType != FLAG_CYCL_SCENE_CHEST) {
                return;
            }
            if (const auto* placement = FindChest(Play_GetOriginalSceneId(sceneId), flag)) {
                OotmmIpc_SendCheckCollected(placement->CheckId);
            }
        });

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_CHEST, {
        EnBox* chest = va_arg(args, EnBox*);
        Player* player = GET_PLAYER(gPlayState);
        sActiveChest = FindChest(Play_GetOriginalSceneId(gPlayState->sceneId),
                                 ENBOX_GET_CHEST_FLAG(&chest->dyna.actor));
        if (sActiveChest != nullptr && player != nullptr) {
            Player_SetupWaitForPutAway(gPlayState, player, ChestOpenAfterPutAway);
            // The Song of Time recloses chests; a check the launcher already holds
            // opens empty instead of presenting its item again.
            if (!OotmmIpc_IsCheckCompleted(sActiveChest->CheckId)) {
                OotmmItemPresentation_Queue(MakePresentation(*sActiveChest));
            }
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_PLAY_SLOW_CHEST_CS, {
        if (sActiveChest != nullptr) {
            *should = sActiveChest->Major;
        }
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>(
        [](s16) { SweepSavedChests(); });

    // A reconnect replays anything the launcher never acknowledged.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        const bool connected = OotmmIpc_IsConnected();
        if (connected && !sWasConnected) {
            SweepSavedChests();
        }
        sWasConnected = connected;
    });
}
