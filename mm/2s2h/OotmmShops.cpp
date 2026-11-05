#include "OotmmShops.h"

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmIpc.h"
#include "OotmmItemModels.h"
#include <libultraship/bridge/OotmmItemGrant.h>
#include "OotmmItemPresentation.h"
#include "OotmmNpcChecks.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmInventory.h>

#include <string>
#include <unordered_map>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_GirlA/z_en_girla.h"

extern PlayState* gPlayState;
void EnGirlA_Draw(Actor* thisx, PlayState* play);
}

namespace {

bool sOffering = false;
constexpr uint32_t kShopPriceBase = 0x6a;
constexpr uint32_t kCuriosityBombBagPrice = 0x80;

enum ShopState : uint8_t {
    kVanilla,
    kShuffled,
    kRenewable,
    kSoldOut,
};

bool IsAny(int params, std::initializer_list<int> values) {
    for (const int value : values) {
        if (params == value) {
            return true;
        }
    }
    return false;
}

int ShopId(const EnGirlA* item, const PlayState* play) {
    const int params = item->actor.params;
    const bool redPotion =
        IsAny(params, { SI_POTION_RED_1, SI_POTION_RED_2, SI_POTION_RED_3, SI_POTION_RED_4, SI_POTION_RED_5,
                        SI_POTION_RED_6 });
    const bool greenPotion = IsAny(params, { SI_POTION_GREEN_1, SI_POTION_GREEN_2, SI_POTION_GREEN_3 });
    const bool heroShield = IsAny(params, { SI_SHIELD_HERO_1, SI_SHIELD_HERO_2, SI_SHIELD_HERO_3 });
    const bool smallArrows = IsAny(params, { SI_ARROWS_SMALL_1, SI_ARROWS_SMALL_2, SI_ARROWS_SMALL_3 });
    const bool bombs = IsAny(params, { SI_BOMB_1, SI_BOMB_2, SI_BOMB_3 });
    switch (play->sceneId) {
        case SCENE_BOMYA:
            if (params == SI_BOMB_1) {
                return 0x00;
            }
            if (params == SI_BOMBCHU) {
                return 0x01;
            }
            if (IsAny(params, { SI_BOMB_BAG_20_1, SI_BOMB_BAG_20_2 })) {
                return 0x02;
            }
            if (IsAny(params, { SI_BOMB_BAG_30_1, SI_BOMB_BAG_30_2 })) {
                return 0x03;
            }
            return -1;
        case SCENE_AYASHIISHOP:
            if (params == SI_MASK_ALL_NIGHT) {
                return 0x04;
            }
            if (IsAny(params, { SI_BOMB_BAG_30_1, SI_BOMB_BAG_30_2 })) {
                return 0x03;
            }
            return -1;
        case SCENE_8ITEMSHOP:
            if (redPotion) {
                return 0x05;
            }
            if (greenPotion) {
                return 0x06;
            }
            if (heroShield) {
                return 0x07;
            }
            if (IsAny(params, { SI_FAIRY_1, SI_FAIRY_2 })) {
                return 0x08;
            }
            if (IsAny(params, { SI_STICK_1, SI_STICK_2 })) {
                return 0x09;
            }
            if (IsAny(params, { SI_ARROWS_MEDIUM_1, SI_ARROWS_MEDIUM_2 })) {
                return 0x0a;
            }
            if (IsAny(params, { SI_NUTS_1, SI_NUTS_2 })) {
                return 0x0b;
            }
            if (IsAny(params, { SI_ARROWS_LARGE_1, SI_ARROWS_LARGE_2 })) {
                return 0x0c;
            }
            return -1;
        case SCENE_WITCH_SHOP:
            if (params == SI_POTION_BLUE) {
                return 0x0d;
            }
            if (greenPotion) {
                return 0x0e;
            }
            return redPotion ? 0x0f : -1;
        case SCENE_GORONSHOP:
            if (bombs) {
                return 0x10;
            }
            if (smallArrows) {
                return 0x11;
            }
            return redPotion ? 0x12 : -1;
        case SCENE_BANDROOM:
            if (heroShield) {
                return 0x13;
            }
            if (smallArrows) {
                return 0x14;
            }
            return redPotion ? 0x15 : -1;
        default:
            return -1;
    }
}

const Ship::OotmmPlacement* FindShop(const EnGirlA* item) {
    if (item == nullptr || item->ootmmState == kVanilla) {
        return nullptr;
    }
    return OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm, "shop", std::to_string(item->ootmmShopId));
}

const std::unordered_map<std::string, uint8_t> kRenewals = {
    { "MM_ARROWS_10", ITEM_ARROWS_10 },     { "MM_ARROWS_30", ITEM_ARROWS_30 },
    { "MM_ARROWS_40", ITEM_ARROWS_40 },     { "MM_BOMB", ITEM_BOMB },
    { "MM_BOMBS_5", ITEM_BOMBS_5 },         { "MM_BOMBS_10", ITEM_BOMBS_10 },
    { "MM_BOMBS_20", ITEM_BOMBS_20 },       { "MM_BOMBS_30", ITEM_BOMBS_30 },
    { "MM_BOMBCHU", ITEM_BOMBCHUS_1 },      { "MM_BOMBCHU_5", ITEM_BOMBCHUS_5 },
    { "MM_BOMBCHU_10", ITEM_BOMBCHUS_10 },  { "MM_BOMBCHU_20", ITEM_BOMBCHUS_20 },
    { "MM_NUT", ITEM_DEKU_NUT },            { "MM_NUTS_5", ITEM_DEKU_NUTS_5 },
    { "MM_NUTS_10", ITEM_DEKU_NUTS_10 },    { "MM_STICK", ITEM_DEKU_STICK },
    { "MM_RECOVERY_HEART", ITEM_RECOVERY_HEART }, { "MM_SHIELD_HERO", ITEM_SHIELD_HERO },
    { "MM_MILK", ITEM_MILK_BOTTLE },        { "MM_BOTTLE_MILK", ITEM_MILK_BOTTLE },
    { "MM_CHATEAU", ITEM_CHATEAU },         { "MM_BOTTLE_CHATEAU", ITEM_CHATEAU },
    { "MM_FISH", ITEM_FISH },               { "MM_BUGS", ITEM_BUG },
    { "MM_FAIRY", ITEM_FAIRY },             { "MM_BOTTLE_FAIRY", ITEM_FAIRY },
    { "MM_POE", ITEM_POE },                 { "MM_BOTTLE_POE", ITEM_POE },
    { "MM_POTION_RED", ITEM_POTION_RED },   { "MM_BOTTLE_POTION_RED", ITEM_POTION_RED },
    { "MM_POTION_GREEN", ITEM_POTION_GREEN }, { "MM_BOTTLE_POTION_GREEN", ITEM_POTION_GREEN },
    { "MM_POTION_BLUE", ITEM_POTION_BLUE }, { "MM_BOTTLE_POTION_BLUE", ITEM_POTION_BLUE },
    { "MM_MAGIC_BEAN", ITEM_MAGIC_BEANS },  { "MM_MAGIC_JAR_SMALL", ITEM_MAGIC_JAR_SMALL },
    { "MM_MAGIC_JAR_LARGE", ITEM_MAGIC_JAR_BIG },
};

const uint8_t* FindRenewal(const EnGirlA* item, const Ship::OotmmPlacement& placement) {
    if (item->ootmmShopId >= 0x02 && item->ootmmShopId <= 0x04) {
        return nullptr;
    }
    if (!placement.CloakItemId.empty() || !placement.IsFor(OotmmSession_GetState().GetPlayerId())) {
        return nullptr;
    }
    const auto found = kRenewals.find(placement.ItemId);
    return found == kRenewals.end() ? nullptr : &found->second;
}


bool LedgerRenewable(const EnGirlA* item, const Ship::OotmmPlacement& placement) {
    return !(item->ootmmShopId >= 0x02 && item->ootmmShopId <= 0x04) && placement.CloakItemId.empty() &&
           placement.IsFor(OotmmSession_GetState().GetPlayerId()) &&
           !Ship::OotmmItemGrant::RenewalOf(placement.ItemId).empty();
}

s32 CanBuyShuffled(PlayState* play, EnGirlA* item) {
    const auto* placement = FindShop(item);
    if (placement == nullptr || !OotmmNpcChecks_CanPurchasePlacement(*placement)) {
        return CANBUY_RESULT_CANNOT_GET_NOW;
    }
    return gSaveContext.save.saveInfo.playerData.rupees < play->msgCtx.unk1206C ? CANBUY_RESULT_NEED_RUPEES
                                                                               : CANBUY_RESULT_SUCCESS_1;
}

s32 CanBuyRenewal(PlayState* play, EnGirlA* item) {
    const auto* placement = FindShop(item);
    if (placement == nullptr || !OotmmNpcChecks_CanPurchaseRepeat(placement->ItemId)) {
        return CANBUY_RESULT_NO_ROOM;
    }
    return gSaveContext.save.saveInfo.playerData.rupees < play->msgCtx.unk1206C ? CANBUY_RESULT_NEED_RUPEES
                                                                               : CANBUY_RESULT_SUCCESS_2;
}

s32 CanBuySoldOut(PlayState*, EnGirlA*) {
    return CANBUY_RESULT_CANNOT_GET_NOW;
}

void BuyRenewal(PlayState* play, EnGirlA* item) {
    const auto* placement = FindShop(item);
    if (const uint8_t* renewal = placement != nullptr ? FindRenewal(item, *placement) : nullptr) {
        Item_Give(play, *renewal);
    } else if (placement != nullptr) {
        const std::string ledger = Ship::OotmmItemGrant::RenewalOf(placement->ItemId);
        if (!ledger.empty()) {
            OotmmIpc_SendRenewalGrant(ledger);
        }
    }
    Rupees_ChangeBy(-play->msgCtx.unk1206C);
}

void Hide(PlayState*, EnGirlA* item) {
    item->isOutOfStock = true;
    item->actor.draw = nullptr;
}

void ApplyCollected(EnGirlA* item, const Ship::OotmmPlacement& placement) {
    if (FindRenewal(item, placement) != nullptr || LedgerRenewable(item, placement)) {
        item->ootmmState = kRenewable;
        item->canBuyFunc = CanBuyRenewal;
        item->buyFunc = BuyRenewal;
        return;
    }
    item->ootmmState = kSoldOut;
    item->canBuyFunc = CanBuySoldOut;
    item->restockFunc = Hide;
    Hide(nullptr, item);
}

void Restock(PlayState* play, EnGirlA* item) {
    const auto* placement = FindShop(item);
    if (placement != nullptr && OotmmNpcChecks_IsPlacementCollected(*placement)) {
        ApplyCollected(item, *placement);
        if (item->ootmmState == kSoldOut) {
            return;
        }
    }
    item->isOutOfStock = false;
    item->actor.draw = EnGirlA_Draw;
}

uint16_t Price(const EnGirlA* item, const PlayState* play) {
    const uint32_t index = (item->ootmmShopId == 0x03 && play->sceneId == SCENE_AYASHIISHOP)
                               ? kCuriosityBombBagPrice
                               : kShopPriceBase + item->ootmmShopId;
    return OotmmSession_GetState().GetPrice(index).value_or(0);
}

EnGirlA* FindTextItem(PlayState* play, uint16_t textId, bool* description) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].first; actor != nullptr; actor = actor->next) {
        if (actor->id != ACTOR_EN_GIRLA) {
            continue;
        }
        auto* item = reinterpret_cast<EnGirlA*>(actor);
        if (item->ootmmState == kVanilla) {
            continue;
        }
        if (actor->textId == textId || item->choiceTextId == textId) {
            *description = actor->textId == textId;
            return item;
        }
    }
    return nullptr;
}

void BuildShopText(u16* textId, bool* loadFromMessageTable) {
    if (gPlayState == nullptr || !OotmmSession_IsActive()) {
        return;
    }
    bool description = false;
    EnGirlA* item = FindTextItem(gPlayState, *textId, &description);
    const auto* placement = item != nullptr ? FindShop(item) : nullptr;
    if (placement == nullptr) {
        return;
    }
    auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
    const uint16_t price = Price(item, gPlayState);
    entry.firstItemCost = price;
    if (item->ootmmState == kSoldOut) {
        entry.msg = "SOLD OUT\x1A";
    } else if (description) {
        const bool needsMushroom =
            item->actor.params == SI_POTION_BLUE && !CHECK_WEEKEVENTREG(WEEKEVENTREG_GAVE_KOTAKE_MUSHROOM);
        entry.msg = OotmmNpcChecks_PlacementName(*placement) + "\x11%r" + std::to_string(price) + " Rupees%w" +
                    (needsMushroom ? "\x11\x11%pRequires Magic Mushroom%w\x1A" : "\x1A");
    } else {
        entry.msg = OotmmNpcChecks_PlacementName(*placement) + "\x11%r" + std::to_string(price) + " Rupees\x11\xC2%gBuy\x11No thanks%w";
    }
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

} // namespace

int OotmmShops_RepeatItem(const Ship::OotmmPlacement& placement, int fallback) {
    if (!placement.CloakItemId.empty() || !placement.IsFor(OotmmSession_GetState().GetPlayerId())) {
        return fallback;
    }
    const auto found = kRenewals.find(placement.ItemId);
    if (found != kRenewals.end()) {
        return found->second;
    }
    const std::string ledger = Ship::OotmmItemGrant::RenewalOf(placement.ItemId);
    if (!ledger.empty()) {
        OotmmIpc_SendRenewalGrant(ledger);
        Ship::GameIpcItemPresentation presentation;
        presentation.ItemId = ledger;
        presentation.ItemName = Ship::OotmmItemCatalog::TrimGameSuffix(placement.ItemName);
        presentation.ItemGame = ledger.starts_with("OOT_") ? "oot" : "mm";
        presentation.DestinationGame = presentation.ItemGame;
        OotmmItemPresentation_Queue(presentation);
        return ITEM_NONE;
    }
    return fallback;
}

extern "C" int OotmmShops_CanSelectKotakeItem(const EnGirlA* item) {
    return item->ootmmState != kSoldOut && !item->ootmmNeedsMushroom;
}

extern "C" void OotmmShops_SetupItem(EnGirlA* item, PlayState* play) {
    if (item == nullptr || play == nullptr || !OotmmSession_IsActive()) {
        return;
    }
    const int id = ShopId(item, play);
    if (id < 0) {
        return;
    }
    item->ootmmShopId = static_cast<s16>(id);
    item->ootmmState = kShuffled;
    const auto* placement = FindShop(item);
    if (placement == nullptr) {
        item->ootmmState = kVanilla;
        return;
    }
    item->ootmmNeedsMushroom =
        item->actor.params == SI_POTION_BLUE && !CHECK_WEEKEVENTREG(WEEKEVENTREG_GAVE_KOTAKE_MUSHROOM);
    item->restockFunc = Restock;
    if (OotmmNpcChecks_IsPlacementCollected(*placement)) {
        ApplyCollected(item, *placement);
        return;
    }
    item->canBuyFunc = CanBuyShuffled;
}

extern "C" int OotmmShops_Draw(EnGirlA* item, PlayState* play) {
    const auto* placement = FindShop(item);
    if (placement == nullptr || item->ootmmState == kSoldOut) {
        return 0;
    }
    const bool cloaked = !placement->CloakItemId.empty();
    return OotmmItemModel_DrawById(play, cloaked ? placement->CloakItemId : placement->ItemId,
                                   cloaked ? placement->CloakItemName : placement->ItemName)
               ? 1
               : 0;
}

extern "C" void OotmmShops_OfferGetItem(Actor* shopkeeper, PlayState* play, EnGirlA* item, float xzRange,
                                        float yRange) {
    const auto* placement = item != nullptr && item->ootmmState == kShuffled ? FindShop(item) : nullptr;
    if (placement == nullptr) {
        sOffering = true;
        Actor_OfferGetItem(shopkeeper, play, static_cast<GetItemId>(item->getItemId), xzRange, yRange);
        sOffering = false;
        return;
    }
    if (!OotmmNpcChecks_IsPlacementCollected(*placement)) {
        OotmmNpcChecks_CollectPurchasedPlacement(*placement);
        return;
    }
    if (OotmmItemPresentation_IsDelivering(placement->CheckId)) {
        shopkeeper->parent = &GET_PLAYER(play)->actor;
    }
}

bool OotmmShops_IsOffering() {
    return sOffering;
}

void OotmmShops_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(BuildShopText);
}
