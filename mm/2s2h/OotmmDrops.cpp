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
    // What the drop draws as; differs from the item itself for disguised traps.
    std::string DrawItemId;
    std::string DrawItemName;
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

// Spawned drop -> its item; entries whose actors die without a pickup are pruned.
std::unordered_map<Actor*, Drop> sDrops;

// Item_DropCollectible's own starting countdown; refreshed every frame so drops never blink out.
constexpr s16 kDespawnRefresh = 220;
constexpr float kWorldScale = 0.25f;

void Present(const Drop& drop) {
    if (drop.CheckId.empty()) {
        OotmmIpc_RequestDebugItemPresentation(drop.ItemId, OotmmSession_GetState().GetPlayerId());
        return;
    }
    const bool collected = OotmmCheckLedger_Contains(drop.CheckId) || OotmmIpc_IsCheckCompleted(drop.CheckId);
    OotmmIpc_SendCheckCollected(drop.CheckId, !collected);
    OotmmCheckLedger_Record(drop.CheckId);
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

bool OotmmDrops_Spawn(PlayState* play, float x, float y, float z, const std::string& itemId,
                      const std::string& itemName, const std::string& itemGame,
                      const std::string& checkId) {
    if (play == nullptr) {
        return false;
    }
    Vec3f pos = { x, y, z };
    Actor* item = Item_DropCollectible(play, &pos, ITEM00_RUPEE_GREEN);
    if (item == nullptr) {
        return false;
    }
    item->draw = OotmmDrops_DrawActor;
    reinterpret_cast<EnItem00*>(item)->unk152 = kDespawnRefresh;
    std::string drawItemId = itemId;
    std::string drawItemName = itemName;
    if (Ship::OotmmItemCatalog::IsTrap(itemId)) {
        PickDisguise(drawItemId, drawItemName);
    }
    sDrops.emplace(item, Drop{ itemId, itemName, itemGame, checkId, drawItemId, drawItemName });
    return true;
}

void OotmmDrops_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s8, s8) { sDrops.clear(); });

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_ITEM00, {
        EnItem00* item = va_arg(args, EnItem00*);
        const auto it = sDrops.find(&item->actor);
        if (it != sDrops.end()) {
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
