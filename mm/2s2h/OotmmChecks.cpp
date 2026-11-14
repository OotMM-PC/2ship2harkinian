#include "OotmmChecks.h"
#include "OotmmCheckLedger.h"
#include "OotmmCsmc.h"
#include "OotmmDrops.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmIpc.h"
#include "OotmmItemPresentation.h"
#include "OotmmItemModels.h"
#include "OotmmNpcChecks.h"
#include "OotmmShops.h"
#include "OotmmXflags.h"
#include "OotmmSession.h"

#include <cmath>

extern "C" {
#include "functions.h"
#include "global.h"
#include "overlays/actors/ovl_En_Box/z_en_box.h"
#include "overlays/actors/ovl_En_Cow/z_en_cow.h"
#include "z64horse.h"
#include "overlays/actors/ovl_En_Elforg/z_en_elforg.h"
#include "overlays/actors/ovl_En_Si/z_en_si.h"

extern void func_80848294(PlayState* play, Player* player);
extern void Player_SetAction_PreserveMoveFlags(PlayState* play, Player* player, PlayerActionFunc actionFunc,
                                               s32 flags);
extern s32 Player_SetupWaitForPutAway(PlayState* play, Player* player, AfterPutAwayFunc func);
extern void Player_StopCutscene(Player* player);
}

namespace {

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

const Ship::OotmmPlacement* FindToken(const Actor* actor, const PlayState* play) {
    return FindChest(OotmmChecks_CheckScene(play->sceneId), ENSI_GET_CHEST_FLAG(actor));
}

constexpr const char* kTownFairy = "MM_STRAY_FAIRY_TOWN";

const Ship::OotmmPlacement* FindStrayFairy(const Actor* actor, const PlayState* play) {
    switch (STRAY_FAIRY_TYPE(actor)) {
        case STRAY_FAIRY_TYPE_FAIRY_FOUNTAIN:
        case STRAY_FAIRY_TYPE_CLOCK_TOWN:
        case STRAY_FAIRY_TYPE_CHEST:
        case STRAY_FAIRY_TYPE_RETURNING_TO_FOUNTAIN:
            return nullptr;
        default:
            return OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm, OotmmChecks_CheckScene(play->sceneId),
                                                     "sf", STRAY_FAIRY_GET_FLAG(actor));
    }
}

int CowId(const Actor* cow, const PlayState* play) {
    const int scene = play->sceneId == SCENE_KAKUSIANA ? OotmmXflags_LastScene() : play->sceneId;
    switch (scene) {
        case SCENE_00KEIKOKU:
            return cow->world.pos.z > 930.0f ? 0x13 : 0x14;
        case SCENE_30GYOSON:
            return cow->world.pos.z > 930.0f ? 0x15 : 0x16;
        case SCENE_OMOYA:
            return cow->world.pos.x < -100.0f ? 0x10 : cow->world.pos.z < -100.0f ? 0x12 : 0x11;
        case SCENE_REDEAD:
            return 0x17;
        default:
            return -1;
    }
}

Actor* NearestCow(PlayState* play) {
    Actor* nearest = nullptr;
    for (const auto& list : play->actorCtx.actorLists) {
        for (Actor* actor = list.first; actor != nullptr; actor = actor->next) {
            if (actor->id == ACTOR_EN_COW && actor->params != 1 &&
                (nearest == nullptr || actor->xzDistToPlayer < nearest->xzDistToPlayer)) {
                nearest = actor;
            }
        }
    }
    return nearest;
}

const Ship::OotmmPlacement* FindCow(const Actor* cow, const PlayState* play) {
    const int id = CowId(cow, play);
    return id < 0 ? nullptr
                  : OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm, "cow", std::to_string(id));
}

void DrawPlacementItem(PlayState* play, const Ship::OotmmPlacement& placement, s16 fallback) {
    const std::string& itemId = placement.CloakItemId.empty() ? placement.ItemId : placement.CloakItemId;
    const std::string& itemName = placement.CloakItemId.empty() ? placement.ItemName : placement.CloakItemName;
    if (!OotmmItemModel_DrawById(play, itemId, itemName)) {
        GetItem_Draw(play, fallback);
    }
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
        if (gPlayState != nullptr && static_cast<uint32_t>(OotmmChecks_CheckScene(gPlayState->sceneId)) == scene) {
            flags |= gPlayState->actorCtx.sceneFlags.collectible[0];
        }
        if ((flags & (1u << 0x1F)) != 0 && !OotmmIpc_IsCheckCompleted(placement.CheckId)) {
            Report(placement);
        }
    }
}

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
        uint32_t chestBits = gSaveContext.cycleSceneFlags[scene == SCENE_F41 ? SCENE_F40 : scene].chest;
        if (gPlayState != nullptr &&
            static_cast<uint32_t>(OotmmChecks_CheckScene(gPlayState->sceneId)) == scene) {
            chestBits |= gPlayState->actorCtx.sceneFlags.chest;
        }
        if ((chestBits & (1u << flag)) != 0 && !OotmmIpc_IsCheckCompleted(placement.CheckId)) {
            Report(placement);
        }
    }
}

} // namespace

extern "C" short OotmmChecks_CheckScene(short sceneId) {
    return sceneId == SCENE_F41 ? sceneId : Play_GetOriginalSceneId(sceneId);
}

extern "C" void* OotmmChecks_SkulltulaDL(Actor* skulltula, const char* part, void* vanilla) {
    if (gPlayState == nullptr || OotmmSession_GetState().GetStringSetting("housesSkulltulaTokens", "none") == "none") {
        return vanilla;
    }
    void* dl = OotmmCsmc_SkulltulaDL(FindToken(skulltula, gPlayState), part);
    return dl != nullptr ? dl : vanilla;
}

extern "C" void* OotmmChecks_CowRingDL(Actor* cow, void* vanilla) {
    if (gPlayState == nullptr || !OotmmSession_GetState().GetBoolSetting("cowShuffleMm", false)) {
        return nullptr;
    }
    const auto* placement = FindCow(cow, gPlayState);
    return OotmmCsmc_CowRingDL(placement, placement != nullptr && IsCollected(*placement), vanilla);
}

extern "C" int OotmmChecks_HeartPieceDone(PlayState* play, Actor* actor, int vanillaDone) {
    if (play == nullptr || actor->params != ITEM00_HEART_PIECE) {
        return vanillaDone;
    }
    const auto* placement = OotmmSession_GetState().FindCheck(
        Ship::OotmmGame::Mm, OotmmChecks_CheckScene(play->sceneId), "collectible",
        reinterpret_cast<EnItem00*>(actor)->collectibleFlag);
    return placement == nullptr ? vanillaDone : IsCollected(*placement);
}

extern "C" int OotmmChecks_BossContainerDone(PlayState* play, int vanillaDone) {
    const auto* placement = play != nullptr ? FindBossContainer(OotmmChecks_CheckScene(play->sceneId)) : nullptr;
    return placement == nullptr ? vanillaDone : IsCollected(*placement);
}

extern "C" int OotmmChecks_BossContainerIsShuffled(PlayState* play) {
    return play != nullptr && FindBossContainer(OotmmChecks_CheckScene(play->sceneId)) != nullptr;
}

extern "C" int OotmmChecks_TryCollectBossContainer(Actor* actor, PlayState* play) {
    if (actor == nullptr || play == nullptr) {
        return 0;
    }
    const auto* placement = FindBossContainer(OotmmChecks_CheckScene(play->sceneId));
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
        OotmmItemPresentation_PlayerReceiving(player)) {
        return 0;
    }
    Report(*placement, true);
    return 1;
}

extern "C" int OotmmChecks_DrawBossContainer(Actor* actor, PlayState* play) {
    if (actor == nullptr || play == nullptr) {
        return 0;
    }
    const auto* placement = FindBossContainer(OotmmChecks_CheckScene(play->sceneId));
    if (placement == nullptr) {
        return 0;
    }
    if (IsCollected(*placement)) {
        return 1;
    }
    Matrix_Push();
    if (actor->scale.x > 0.0001f) {
        const float scale = 0.35f / actor->scale.x;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }
    DrawPlacementItem(play, *placement, GID_RUPEE_GREEN);
    Matrix_Pop();
    return 1;
}

extern "C" int OotmmChecks_DrawToken(Actor* actor, PlayState* play) {
    if (actor == nullptr || play == nullptr) {
        return 0;
    }
    const auto* placement = FindToken(actor, play);
    if (placement == nullptr) {
        return 0;
    }
    DrawPlacementItem(play, *placement, GID_SKULL_TOKEN_2);
    return 1;
}

extern "C" int OotmmChecks_StrayFairyIsShuffled(Actor* actor, PlayState* play) {
    if (actor == nullptr || play == nullptr) {
        return 0;
    }
    if (STRAY_FAIRY_TYPE(actor) == STRAY_FAIRY_TYPE_CLOCK_TOWN) {
        return OotmmNpcChecks_IsShuffled(kTownFairy);
    }
    return FindStrayFairy(actor, play) != nullptr;
}

extern "C" int OotmmChecks_DrawStrayFairy(Actor* actor, PlayState* play) {
    if (actor == nullptr || play == nullptr) {
        return 0;
    }
    const Ship::OotmmPlacement* placement = nullptr;
    switch (play->sceneId) {
        case SCENE_TOWN:
        case SCENE_ALLEY:
            placement = OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm, "npc", kTownFairy);
            break;
        case SCENE_MITURIN:
        case SCENE_HAKUGIN:
        case SCENE_SEA:
        case SCENE_INISIE_N:
        case SCENE_INISIE_R:
            placement = FindStrayFairy(actor, play);
            break;
        default:
            break;
    }
    if (placement == nullptr) {
        return 0;
    }
    const bool cloaked = !placement->CloakItemId.empty();
    s16 angle = static_cast<s16>((play->state.frames % 100) * (0x10000 / 100));
    if (cloaked) {
        angle = -angle;
    }
    Matrix_Push();
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, MTXMODE_NEW);
    Matrix_Scale(0.35f, 0.35f, 0.35f, MTXMODE_APPLY);
    Matrix_RotateYS(angle, MTXMODE_APPLY);
    DrawPlacementItem(play, *placement, GID_FAIRY);
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
            if (const auto* placement = FindChest(OotmmChecks_CheckScene(sceneId), flag)) {
                if (!OotmmCheckLedger_Contains(placement->CheckId)) {
                    Report(*placement);
                }
            }
        });

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_CHEST, {
        EnBox* chest = va_arg(args, EnBox*);
        Player* player = GET_PLAYER(gPlayState);
        sActiveChest = FindChest(OotmmChecks_CheckScene(gPlayState->sceneId),
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

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_COW, {
        Actor* cow = *should ? NearestCow(gPlayState) : nullptr;
        const auto* placement = cow != nullptr ? FindCow(cow, gPlayState) : nullptr;
        if (placement == nullptr) {
            return;
        }
        *should = false;
        gHorsePlayedEponasSong = false;
        if (IsCollected(*placement)) {
            if (const int repeat = OotmmShops_RepeatItem(*placement, ITEM_RECOVERY_HEART); repeat != ITEM_NONE) {
                Item_Give(gPlayState, repeat);
            }
        } else {
            Report(*placement, true);
        }
    });

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_SI, {
        EnSi* token = va_arg(args, EnSi*);
        if (const auto* placement = FindToken(&token->actor, gPlayState)) {
            Report(*placement, true);
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_ELFORG, {
        Actor* fairy = va_arg(args, Actor*);
        if (STRAY_FAIRY_TYPE(fairy) == STRAY_FAIRY_TYPE_CLOCK_TOWN) {
            if (OotmmNpcChecks_IsShuffled(kTownFairy)) {
                OotmmNpcChecks_Collect(kTownFairy);
                *should = false;
            }
        } else if (const auto* placement = FindStrayFairy(fairy, gPlayState)) {
            Report(*placement, true);
            *should = false;
        }
    });

    REGISTER_VB_SHOULD(VB_KILL_CLOCK_TOWN_STRAY_FAIRY, {
        *should = OotmmNpcChecks_Done(kTownFairy, *should);
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](Actor* actor) {
        if (gPlayState == nullptr || actor->id != ACTOR_EN_ITEM00 || actor->update == nullptr ||
            actor->params != ITEM00_HEART_PIECE) {
            return;
        }
        const int flag = reinterpret_cast<EnItem00*>(actor)->collectibleFlag;
        const auto* placement = OotmmSession_GetState().FindCheck(
            Ship::OotmmGame::Mm, OotmmChecks_CheckScene(gPlayState->sceneId), "collectible", flag);
        if (placement != nullptr && !IsCollected(*placement)) {
            OotmmDrops_Adopt(actor, *placement, flag);
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

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        const bool connected = OotmmIpc_IsConnected();
        if (connected && !sWasConnected) {
            SweepSavedChests();
            SweepSavedBossContainers();
        }
        sWasConnected = connected;
    });
}
