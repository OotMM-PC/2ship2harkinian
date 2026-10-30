#include "OotmmDrops.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmCheckLedger.h"
#include "OotmmIpc.h"
#include "OotmmItemModels.h"
#include "OotmmSession.h"

#include <random>
#include <unordered_map>
#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"

extern PlayState* gPlayState;
}

namespace {

struct Drop {
    std::string ItemId;
    std::string ItemName;
    std::string ItemGame;
    std::string CheckId;
    std::string DrawItemId;
    std::string DrawItemName;
    int CollectibleFlag = -1;
};

void PickDisguise(std::string& drawItemId, std::string& drawItemName) {
    std::vector<const Ship::OotmmPlacement*> majors;
    for (const auto& placement : OotmmSession_GetState().GetPlacements()) {
        if (placement.Major && !placement.ItemId.empty() &&
            !Ship::OotmmItemCatalog::IsTrap(placement.ItemId)) {
            majors.push_back(&placement);
        }
    }
    if (majors.empty()) {
        return;
    }
    static std::mt19937 sRng{ std::random_device{}() };
    const auto* pick = majors[std::uniform_int_distribution<size_t>(0, majors.size() - 1)(sRng)];
    drawItemId = pick->ItemId;
    drawItemName = pick->ItemName;
}

std::unordered_map<Actor*, Drop> sDrops;

constexpr s16 kDespawnRefresh = 220;
constexpr float kWorldScale = 0.25f;

void Present(const Drop& drop) {
    if (drop.CheckId.empty()) {
        OotmmIpc_RequestDebugItemPresentation(drop.ItemId, OotmmSession_GetState().GetPlayerId());
        return;
    }
    const bool collected = OotmmCheckLedger_Contains(drop.CheckId) || OotmmIpc_IsCheckCompleted(drop.CheckId);
    OotmmDrops_Report(drop.CheckId, drop.ItemId, !collected);
}

} // namespace

extern "C" void OotmmDrops_DrawActor(Actor* actor, PlayState* play) {
    const auto it = sDrops.find(actor);
    if (it == sDrops.end()) {
        return;
    }
    if (actor->scale.x > 0.0001f) {
        const float scale = kWorldScale / actor->scale.x;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }
    if (OotmmItemModel_DrawById(play, it->second.DrawItemId, it->second.DrawItemName)) {
        return;
    }
    GetItem_Draw(play, GID_RUPEE_GREEN);
}

namespace {

void Track(Actor* item, Drop drop) {
    item->draw = OotmmDrops_DrawActor;
    reinterpret_cast<EnItem00*>(item)->unk152 = kDespawnRefresh;
    if (Ship::OotmmItemCatalog::IsTrap(drop.ItemId)) {
        PickDisguise(drop.DrawItemId, drop.DrawItemName);
    }
    sDrops.insert_or_assign(item, std::move(drop));
}

Actor* Spawn(PlayState* play, float x, float y, float z, Drop drop) {
    if (play == nullptr) {
        return nullptr;
    }
    Vec3f pos = { x, y, z };
    Actor* item = Item_DropCollectible(play, &pos, ITEM00_RUPEE_GREEN);
    if (item == nullptr) {
        return nullptr;
    }
    Track(item, std::move(drop));
    return item;
}

Drop PlacementDrop(const Ship::OotmmPlacement& placement, int collectibleFlag) {
    const std::string& drawItemId = placement.CloakItemId.empty() ? placement.ItemId : placement.CloakItemId;
    const std::string& drawItemName = placement.CloakItemId.empty() ? placement.ItemName : placement.CloakItemName;
    return Drop{ placement.ItemId, placement.ItemName, placement.ItemGame, placement.CheckId,
                 drawItemId,       drawItemName,       collectibleFlag };
}

} // namespace

bool OotmmDrops_Spawn(PlayState* play, float x, float y, float z, const std::string& itemId,
                      const std::string& itemName, const std::string& itemGame,
                      const std::string& checkId) {
    return Spawn(play, x, y, z, Drop{ itemId, itemName, itemGame, checkId, itemId, itemName, -1 }) != nullptr;
}

Actor* OotmmDrops_SpawnPlacement(PlayState* play, float x, float y, float z,
                                 const Ship::OotmmPlacement& placement, int collectibleFlag) {
    return Spawn(play, x, y, z, PlacementDrop(placement, collectibleFlag));
}

bool OotmmDrops_Adopt(Actor* actor, const Ship::OotmmPlacement& placement, int collectibleFlag) {
    if (actor == nullptr || actor->id != ACTOR_EN_ITEM00) {
        return false;
    }
    Track(actor, PlacementDrop(placement, collectibleFlag));
    return true;
}

void OotmmDrops_Report(const std::string& checkId, const std::string& itemId, bool present) {
    const bool fast = Ship::OotmmItemCatalog::IsFast(itemId);
    OotmmIpc_SendCheckCollected(checkId, present && !fast);
    OotmmCheckLedger_Record(checkId);
    if (fast) {
        Audio_PlaySfx(itemId.find("_RUPEE_") != std::string::npos ? NA_SE_SY_GET_RUPY : NA_SE_SY_GET_BOXITEM);
    }
}

bool OotmmDrops_HasCheck(const std::string& checkId) {
    for (const auto& [actor, drop] : sDrops) {
        if (drop.CheckId == checkId) {
            return true;
        }
    }
    return false;
}

void OotmmDrops_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s8, s8) { sDrops.clear(); });

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_ITEM00, {
        EnItem00* item = va_arg(args, EnItem00*);
        const auto it = sDrops.find(&item->actor);
        if (it != sDrops.end()) {
            if (it->second.CollectibleFlag >= 0 && gPlayState != nullptr) {
                Flags_SetCollectible(gPlayState, it->second.CollectibleFlag);
            }
            Present(it->second);
            Actor_Kill(&item->actor);
            sDrops.erase(it);
            *should = false;
        }
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        if (gPlayState == nullptr || sDrops.empty()) {
            return;
        }
        std::unordered_map<Actor*, Drop> alive;
        for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_MISC].first; actor != nullptr;
             actor = actor->next) {
            const auto it = sDrops.find(actor);
            if (it != sDrops.end()) {
                reinterpret_cast<EnItem00*>(actor)->unk152 = kDespawnRefresh;
                alive.emplace(actor, std::move(it->second));
            }
        }
        sDrops = std::move(alive);
    });
}
