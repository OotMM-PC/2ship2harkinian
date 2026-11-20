#include "OotmmNpcChecks.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "OotmmCheckLedger.h"
#include "OotmmChecks.h"
#include "OotmmCustomItems.h"
#include "OotmmDialog.h"
#include <libultraship/bridge/OotmmDialog.h>
#include "OotmmIpc.h"
#include <libultraship/bridge/OotmmItemGrant.h>
#include <memory>
#include <vector>
#include "OotmmItemApply.h"
#include "OotmmItemModels.h"
#include "OotmmItemPresentation.h"
#include "OotmmSession.h"
#include "OotmmShops.h"
#include <libultraship/libultraship.h>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
#include "functions.h"
#include "z64snap.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_Bg_Dy_Yoseizo/z_bg_dy_yoseizo.h"
#include "src/overlays/actors/ovl_En_Akindonuts/z_en_akindonuts.h"
#include "src/overlays/actors/ovl_En_Elfgrp/z_en_elfgrp.h"
#include "src/overlays/actors/ovl_En_Ginko_Man/z_en_ginko_man.h"
#include "src/overlays/actors/ovl_En_Nb/z_en_nb.h"

extern PlayState* gPlayState;
}

namespace {

struct OfferSite {
    const char* NativeId = nullptr;
    GetItemId Repeat = GI_NONE;
    const char* Companion = nullptr;
    bool Purchase = false;
};

struct PurchaseSite {
    const char* NativeId = nullptr;
    GetItemId Fallback = GI_NONE;
    bool Renew = true;
    uint16_t Price = 0;
};

struct Renewal {
    GetItemId Gi = GI_NONE;
    const char* ItemId = nullptr;
    const char* Name = nullptr;
    bool ViaLedger = false;
};

std::string sPendingLedgerRenewal;
std::string sPendingLedgerName;

const std::unordered_map<std::string, Renewal> kRenewals = {
    { "MM_RECOVERY_HEART", { GI_RECOVERY_HEART, "MM_RECOVERY_HEART", "Recovery Heart" } },
    { "MM_STICK", { GI_DEKU_STICKS_1, "MM_STICK", "Deku Stick" } },
    { "MM_NUT", { GI_DEKU_NUTS_1, "MM_NUT", "Deku Nut" } },
    { "MM_NUTS_5", { GI_DEKU_NUTS_5, "MM_NUTS_5", "5 Deku Nuts" } },
    { "MM_NUTS_10", { GI_DEKU_NUTS_10, "MM_NUTS_10", "10 Deku Nuts" } },
    { "MM_BOMB", { GI_BOMBS_1, "MM_BOMB", "Bomb" } },
    { "MM_BOMBS_5", { GI_BOMBS_5, "MM_BOMBS_5", "5 Bombs" } },
    { "MM_BOMBS_10", { GI_BOMBS_10, "MM_BOMBS_10", "10 Bombs" } },
    { "MM_BOMBS_20", { GI_BOMBS_20, "MM_BOMBS_20", "20 Bombs" } },
    { "MM_BOMBS_30", { GI_BOMBS_30, "MM_BOMBS_30", "30 Bombs" } },
    { "MM_BOMBCHU", { GI_BOMBCHUS_1, "MM_BOMBCHU", "Bombchu" } },
    { "MM_BOMBCHU_5", { GI_BOMBCHUS_5, "MM_BOMBCHU_5", "5 Bombchus" } },
    { "MM_BOMBCHU_10", { GI_BOMBCHUS_10, "MM_BOMBCHU_10", "10 Bombchus" } },
    { "MM_BOMBCHU_20", { GI_BOMBCHUS_20, "MM_BOMBCHU_20", "20 Bombchus" } },
    { "MM_ARROWS_10", { GI_ARROWS_10, "MM_ARROWS_10", "10 Arrows" } },
    { "MM_ARROWS_30", { GI_ARROWS_30, "MM_ARROWS_30", "30 Arrows" } },
    { "MM_ARROWS_40", { GI_ARROWS_40, "MM_ARROWS_40", "40 Arrows" } },
    { "MM_SHIELD_HERO", { GI_SHIELD_HERO, "MM_SHIELD_HERO", "Hero's Shield" } },
    { "MM_MILK", { GI_MILK, "MM_MILK", "Milk" } },
    { "MM_BOTTLE_MILK", { GI_MILK, "MM_MILK", "Milk" } },
    { "MM_CHATEAU", { GI_CHATEAU, "MM_CHATEAU", "Chateau Romani" } },
    { "MM_BOTTLE_CHATEAU", { GI_CHATEAU, "MM_CHATEAU", "Chateau Romani" } },
    { "MM_FISH", { GI_FISH, "MM_FISH", "Fish" } },
    { "MM_BUGS", { GI_BUG, "MM_BUGS", "Bugs" } },
    { "MM_FAIRY", { GI_FAIRY, "MM_FAIRY", "Fairy" } },
    { "MM_POE", { GI_POE, "MM_POE", "Poe" } },
    { "MM_POTION_RED", { GI_POTION_RED, "MM_POTION_RED", "Red Potion" } },
    { "MM_BOTTLE_POTION_RED", { GI_POTION_RED, "MM_POTION_RED", "Red Potion" } },
    { "MM_POTION_GREEN", { GI_POTION_GREEN, "MM_POTION_GREEN", "Green Potion" } },
    { "MM_BOTTLE_POTION_GREEN", { GI_POTION_GREEN, "MM_POTION_GREEN", "Green Potion" } },
    { "MM_POTION_BLUE", { GI_POTION_BLUE, "MM_POTION_BLUE", "Blue Potion" } },
    { "MM_BOTTLE_POTION_BLUE", { GI_POTION_BLUE, "MM_POTION_BLUE", "Blue Potion" } },
    { "MM_MAGIC_BEAN", { GI_MAGIC_BEANS, "MM_MAGIC_BEAN", "Magic Bean" } },
    { "MM_MAGIC_JAR_SMALL", { GI_MAGIC_JAR_SMALL, "MM_MAGIC_JAR_SMALL", "Small Magic Jar" } },
    { "MM_MAGIC_JAR_LARGE", { GI_MAGIC_JAR_BIG, "MM_MAGIC_JAR_LARGE", "Large Magic Jar" } },
};

constexpr PurchaseSite kGormanMilk{ "MM_GORMAN_MILK", GI_RECOVERY_HEART, true, 50 };
constexpr PurchaseSite kMilkBarMilk{ "MM_MILK_BAR_MILK", GI_NONE, true, 20 };
constexpr PurchaseSite kMilkBarChateau{ "MM_MILK_BAR_CHATEAU", GI_NONE, true, 200 };
constexpr PurchaseSite kScrubShops[] = {
    { "MM_SCRUB_SHOP_BEANS", GI_RECOVERY_HEART, true, 10 },
    { "MM_SCRUB_BOMB_BAG", GI_RECOVERY_HEART, false, 200 },
    { "MM_SCRUB_SHOP_POTION_GREEN", GI_RECOVERY_HEART, true, 40 },
    { "MM_SCRUB_SHOP_POTION_BLUE", GI_RECOVERY_HEART, true, 100 },
};
constexpr GetItemId kScrubShopItems[] = { GI_MAGIC_BEANS, GI_BOMB_BAG_40, GI_POTION_GREEN, GI_POTION_BLUE };
bool sMilkBarCantBuy = false;
constexpr const char* kSoldOut = "%rSOLD OUT%w";

std::unordered_map<Actor*, OfferSite> sPendingOffers;
std::unordered_map<Actor*, std::string> sCompletedMessageOffers;
struct TextDoneOffer {
    int32_t Frames;
    bool Read;
};
std::unordered_map<Actor*, TextDoneOffer> sTextDoneOffers;
Actor* sUpdatingActor = nullptr;
bool sCheckingOfferText = false;
int32_t sGrandmaOfferPos = -1;
std::vector<std::string> sDeferredCollects;
constexpr const char* kInitialSong = "MM_SONG_HEALING";
bool sCycleReset = false;
bool sCycleResetArrived = false;
uint32_t sDeferredFrame = 0;
std::string sPendingPurchase;
uint32_t sPurchaseRetryFrame = 0;

void ClearPurchaseStatus() {
    Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGameOverlay()->TextDrawStatus("ootmm-purchase-save", "");
    sPendingPurchase.clear();
    sPurchaseRetryFrame = 0;
}

void ShowPurchaseRetry(const std::string& checkId) {
    sPendingPurchase = checkId;
    Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGameOverlay()->TextDrawStatus(
        "ootmm-purchase-save", "Purchase save failed. Retrying...\nClose programs using the save file.");
}

const Ship::OotmmPlacement* Find(const char* nativeId) {
    return nativeId != nullptr ? OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm, "npc", nativeId) : nullptr;
}

bool IsCollected(const Ship::OotmmPlacement& placement) {
    return OotmmCheckLedger_Contains(placement.CheckId) || OotmmIpc_IsCheckCompleted(placement.CheckId);
}

constexpr const char* kSongSites[] = {
    "MM_SONG_AWAKENING", "MM_GORON_BABY", "MM_SONG_ZORA",    "MM_SONG_EMPTINESS",
    "MM_SONG_ORDER",     nullptr,         nullptr,           "MM_SONG_HEALING",
    "MM_SONG_EPONA",     "MM_SONG_SOARING", "MM_SONG_STORMS", nullptr,
};

bool IsMmBombchuAmmo(const std::string& itemId) {
    return itemId == "MM_BOMBCHU" || itemId == "MM_BOMBCHU_5" ||
           itemId == "MM_BOMBCHU_10" || itemId == "MM_BOMBCHU_20";
}

constexpr int32_t kUnlimitedRoom = 999;

int32_t VitalFree(std::string_view family) {
    const auto& info = gSaveContext.save.saveInfo;
    if (family == "health") {
        return info.playerData.healthCapacity - info.playerData.health;
    }
    if (family == "magic") {
        return info.playerData.isMagicAcquired
                   ? (info.playerData.isDoubleMagicAcquired ? 0x60 : 0x30) - info.playerData.magic
                   : 0;
    }
    if (family == "fairy") {
        return VitalFree("health") > 0 || VitalFree("magic") > 0;
    }
    if (family == "sticks") {
        return CUR_UPG_VALUE(UPG_DEKU_STICKS) == 0
                   ? kUnlimitedRoom
                   : CUR_CAPACITY(UPG_DEKU_STICKS) - info.inventory.ammo[SLOT_DEKU_STICK];
    }
    if (family == "nuts") {
        return CUR_UPG_VALUE(UPG_DEKU_NUTS) == 0 ? kUnlimitedRoom
                                                 : CUR_CAPACITY(UPG_DEKU_NUTS) - info.inventory.ammo[SLOT_DEKU_NUT];
    }
    if (family == "bombs") {
        return CUR_UPG_VALUE(UPG_BOMB_BAG) > 0 ? CUR_CAPACITY(UPG_BOMB_BAG) - info.inventory.ammo[SLOT_BOMB] : 0;
    }
    if (family == "bombchu") {
        return OotmmItemApply_BombchuCapacity() - info.inventory.ammo[SLOT_BOMBCHU];
    }
    if (family == "arrows") {
        return CUR_UPG_VALUE(UPG_QUIVER) > 0 ? CUR_CAPACITY(UPG_QUIVER) - info.inventory.ammo[SLOT_BOW] : 0;
    }
    if (family == "seeds") {
        const auto capacity = OotmmCustomItems_SlingshotMaxAmmo();
        return capacity > 0 ? capacity - OotmmCustomItems_SlingshotAmmo() : 0;
    }
    if (family == "rupees") {
        return CUR_CAPACITY(UPG_WALLET) - info.playerData.rupees;
    }
    if (family == "bottles") {
        int32_t empty = 0;
        for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; ++slot) {
            empty += info.inventory.items[slot] == ITEM_BOTTLE;
        }
        return empty;
    }
    if (family == "beans") {
        return info.inventory.items[SLOT_MAGIC_BEANS] < 20 ? kUnlimitedRoom : 0;
    }
    const auto shield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
    if (family == "SHIELD_DEKU") {
        return shield == PLAYER_SHIELD_NONE;
    }
    if (family == "SHIELD_HERO") {
        return shield == PLAYER_SHIELD_NONE ||
               (shield == PLAYER_SHIELD_HEROS_SHIELD && OotmmCustomItems_WearingDekuShield());
    }
    return 1;
}

std::string OotVitalsPath() {
    return OotmmSession_GetState().GetBootConfig().StatePath + ".vitals.oot";
}

bool CanPurchaseMmCapacity(const std::string& itemId) {
    const bool sharedShields = OotmmSession_GetState().GetBoolSetting("sharedShields", false);
    const std::string family = Ship::OotmmItemGrant::VitalFamily(itemId);
    if (itemId.starts_with("OOT_")) {
        if (sharedShields && ((family == "SHIELD_DEKU" && VitalFree("SHIELD_DEKU") > 0) ||
                              (family == "SHIELD_HYLIAN" && VitalFree("SHIELD_HERO") > 0))) {
            return true;
        }
        return Ship::OotmmItemGrant::CanHoldRemote(itemId, OotmmIpc_GetInventory(), OotVitalsPath());
    }
    if (VitalFree(family) > 0) {
        return true;
    }
    if (sharedShields && (family == "SHIELD_DEKU" || family == "SHIELD_HERO")) {
        return Ship::OotmmItemGrant::CanHoldRemote(family == "SHIELD_DEKU" ? "OOT_SHIELD_DEKU" : "OOT_SHIELD_HYLIAN",
                                                   OotmmIpc_GetInventory(), OotVitalsPath());
    }
    return false;
}

constexpr const char* kTingleSites[] = {
    "MM_TINGLE_MAP_CLOCK_TOWN", "MM_TINGLE_MAP_WOODFALL", "MM_TINGLE_MAP_SNOWHEAD",
    "MM_TINGLE_MAP_ROMANI_RANCH", "MM_TINGLE_MAP_GREAT_BAY", "MM_TINGLE_MAP_STONE_TOWER",
};

std::string TingleOfferName(uint16_t map, const char* fallback) {
    const auto* placement = Find(kTingleSites[map]);
    if (placement == nullptr) {
        return fallback;
    }
    if (IsCollected(*placement)) {
        return "Sold out";
    }
    auto name = Ship::OotmmItemCatalog::TrimGameSuffix(
        placement->CloakItemId.empty() ? placement->ItemName : placement->CloakItemName);
    if (!placement->IsFor(OotmmSession_GetState().GetPlayerId())) {
        name += " (Player " + std::to_string(placement->OwnerPlayer) + ")";
    }
    return name;
}

void RegisterTinglePrices() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0x1D0B, [](uint16_t* textId, bool* loadFromMessageTable) {
            if (!OotmmSession_IsActive()) {
                return;
            }
            auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
            entry.msg = "Thank you! Here you go.\x1A";
            CustomMessage::LoadCustomMessageIntoFont(entry);
            *loadFromMessageTable = false;
        });
    static constexpr const char* names[] = {
        "Clock Town", "Woodfall", "Snowhead", "Romani Ranch", "Great Bay", "Stone Tower",
    };
    for (uint16_t location = 0; location < 6; ++location) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            0x1D11 + location, [location](uint16_t* textId, bool* loadFromMessageTable) {
                const auto& state = OotmmSession_GetState();
                const auto first = state.GetPrice(129 + location * 2);
                const auto second = state.GetPrice(130 + location * 2);
                if (!state.IsActive()) {
                    return;
                }
                auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
                entry.firstItemCost = first.value_or(entry.firstItemCost);
                entry.secondItemCost = second.value_or(entry.secondItemCost);
                const uint16_t next = (location + 1) % 6;
                OotmmDialog::Load(entry, "",
                                  { TingleOfferName(location, names[location]) + " - %r" +
                                        Ship::OotmmDialogPrice(entry.firstItemCost),
                                    TingleOfferName(next, names[next]) + " - %r" +
                                        Ship::OotmmDialogPrice(entry.secondItemCost),
                                    "No thanks" });
                *loadFromMessageTable = false;
            });
    }
}

struct RemainsSite {
    const char* NativeId;
    int32_t Index;
};

RemainsSite RemainsFor(int32_t sceneId) {
    switch (sceneId) {
        case SCENE_MITURIN_BS:
            return { "MM_REMAINS_ODOLWA", 1 };
        case SCENE_HAKUGIN_BS:
            return { "MM_REMAINS_GOHT", 2 };
        case SCENE_SEA_BS:
            return { "MM_REMAINS_GYORG", 3 };
        case SCENE_INISIE_BS:
            return { "MM_REMAINS_TWINMOLD", 4 };
        default:
            return { nullptr, 0 };
    }
}

bool DeferCollect(const char* nativeId) {
    if (nativeId == nullptr || Find(nativeId) == nullptr) {
        return false;
    }
    if (std::find(sDeferredCollects.begin(), sDeferredCollects.end(), nativeId) == sDeferredCollects.end()) {
        sDeferredCollects.emplace_back(nativeId);
    }
    sDeferredFrame = gPlayState != nullptr ? gPlayState->gameplayFrames : 0;
    return true;
}

void OfferInitialSong() {
    if (static_cast<uint16_t>(gSaveContext.save.entrance) != ENTRANCE(SOUTH_CLOCK_TOWN, 0)) {
        if (sCycleResetArrived) {
            sCycleReset = false;
            sCycleResetArrived = false;
        }
        return;
    }
    if (sCycleReset) {
        sCycleResetArrived = true;
        return;
    }
    if (OotmmSession_IsActive() && !OotmmNpcChecks_IsCollected(kInitialSong)) {
        DeferCollect(kInitialSong);
    }
}

void FlushDeferredCollects() {
    if (sDeferredCollects.empty() || gPlayState == nullptr || gPlayState->gameplayFrames - sDeferredFrame < 2 ||
        Play_InCsMode(gPlayState) || CutsceneManager_GetCurrentCsId() != CS_ID_NONE ||
        Message_GetState(&gPlayState->msgCtx) != TEXT_STATE_NONE) {
        return;
    }
    const Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || player->csAction != PLAYER_CSACTION_NONE || (player->stateFlags1 & PLAYER_STATE1_400)) {
        return;
    }
    for (const auto& nativeId : sDeferredCollects) {
        OotmmNpcChecks_Collect(nativeId.c_str());
    }
    sDeferredCollects.clear();
}

const PurchaseSite* PurchaseFor(const Actor* actor, int32_t item) {
    if (actor == nullptr) {
        return nullptr;
    }
    switch (actor->id) {
        case ACTOR_EN_IN:
            return item == GI_MILK ? &kGormanMilk : nullptr;
        case ACTOR_EN_TAB:
            if (item == GI_MILK) {
                return &kMilkBarMilk;
            }
            return item == GI_CHATEAU ? &kMilkBarChateau : nullptr;
        case ACTOR_EN_AKINDONUTS:
            return item == kScrubShopItems[ENAKINDONUTS_GET_3(actor)] ? &kScrubShops[ENAKINDONUTS_GET_3(actor)]
                                                                       : nullptr;
        default:
            return nullptr;
    }
}

Renewal RepeatFor(const PurchaseSite& site, const Ship::OotmmPlacement& placement) {
    const auto self = OotmmSession_GetState().GetPlayerId();
    if (site.Renew && placement.CloakItemId.empty() && placement.IsFor(self)) {
        if (const auto found = kRenewals.find(placement.ItemId); found != kRenewals.end()) {
            return found->second;
        }
        static std::unordered_map<std::string, Renewal> sLedgerRenewals;
        static std::vector<std::unique_ptr<std::string>> sStrings;
        if (const auto cached = sLedgerRenewals.find(placement.ItemId); cached != sLedgerRenewals.end()) {
            return cached->second;
        }
        std::string renewal = Ship::OotmmItemGrant::RenewalOf(placement.ItemId);
        if (!renewal.empty()) {
            sStrings.push_back(std::make_unique<std::string>(std::move(renewal)));
            const char* id = sStrings.back()->c_str();
            sStrings.push_back(
                std::make_unique<std::string>(Ship::OotmmItemCatalog::TrimGameSuffix(placement.ItemName)));
            const char* name = sStrings.back()->c_str();
            return sLedgerRenewals[placement.ItemId] = Renewal{ GI_RECOVERY_HEART, id, name, true };
        }
    }
    return site.Fallback == GI_RECOVERY_HEART ? kRenewals.at("MM_RECOVERY_HEART") : Renewal{};
}

int PurchaseState(const PurchaseSite& site) {
    const auto* placement = Find(site.NativeId);
    if (placement == nullptr) {
        return -1;
    }
    if (!IsCollected(*placement)) {
        return OotmmNpcChecks_CanPurchasePlacement(*placement);
    }
    const auto repeat = RepeatFor(site, *placement);
    if (repeat.Gi == GI_NONE) {
        return 0;
    }
    const bool allowed = OotmmSession_GetState().GetStringSetting("mode", "single") != "single" ||
                         CanPurchaseMmCapacity(repeat.ItemId);
    sPendingLedgerRenewal = allowed && repeat.ViaLedger ? repeat.ItemId : "";
    sPendingLedgerName = allowed && repeat.ViaLedger ? repeat.Name : "";
    return allowed;
}

std::string CompactPlacementName(const Ship::OotmmPlacement& placement) {
    const bool cloaked = !placement.CloakItemId.empty();
    std::string name =
        "%g" + Ship::OotmmItemCatalog::TrimGameSuffix(cloaked ? placement.CloakItemName : placement.ItemName);
    const std::string game = cloaked ? (placement.CloakItemId.starts_with("OOT_") ? "oot" : "mm") : placement.ItemGame;
    if (!placement.IsFor(OotmmSession_GetState().GetPlayerId())) {
        name += " (Player " + std::to_string(placement.OwnerPlayer) + ")";
    } else if (!game.empty() && game != "mm") {
        name += " (OoT)";
    }
    return name + "%w";
}

std::string PurchaseName(const PurchaseSite& site, bool compact = false) {
    const auto* placement = Find(site.NativeId);
    if (placement == nullptr) {
        return {};
    }
    if (!IsCollected(*placement)) {
        return compact ? CompactPlacementName(*placement) : OotmmNpcChecks_PlacementName(*placement);
    }
    const auto repeat = RepeatFor(site, *placement);
    return repeat.Gi == GI_NONE ? kSoldOut : std::string("%g") + repeat.Name + "%w";
}

OfferSite PurchaseOffer(const PurchaseSite& site) {
    const auto* placement = Find(site.NativeId);
    const GetItemId repeat = placement != nullptr ? RepeatFor(site, *placement).Gi : GI_NONE;
    return { site.NativeId, repeat, nullptr, true };
}

const PurchaseSite* TalkingScrubShop() {
    if (gPlayState == nullptr) {
        return nullptr;
    }
    const Actor* actor = gPlayState->msgCtx.talkActor;
    if (actor == nullptr || actor->id != ACTOR_EN_AKINDONUTS) {
        return nullptr;
    }
    const auto* site = &kScrubShops[ENAKINDONUTS_GET_3(actor)];
    return Find(site->NativeId) != nullptr ? site : nullptr;
}

void LoadPurchaseText(u16* textId, bool* loadFromMessageTable, const std::string& body, bool format) {
    auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
    const auto end = entry.msg.rfind('\xBF');
    const char last = end != std::string::npos && end > 0 ? entry.msg[end - 1] : '\0';
    entry.msg = body;
    if (last == '\x19' || last == '\xE0') {
        entry.msg.push_back(last);
    }
    entry.autoFormat = format;
    if (!format) {
        CustomMessage::ReplaceColorChars(&entry.msg);
        CustomMessage::EnsureMessageEnd(&entry.msg);
    }
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

void LoadPurchaseChoice(u16* textId, bool* loadFromMessageTable, std::string body, std::vector<std::string> options) {
    OotmmDialog::Load(CustomMessage::LoadVanillaMessageTableEntry(*textId), std::move(body), std::move(options));
    *loadFromMessageTable = false;
}

void RegisterBankText() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0x44C, [](u16* textId, bool* loadFromMessageTable) {
            if (!OotmmSession_IsActive()) {
                return;
            }
            const u32 bank = HIGH_SCORE(HS_BANK_RUPEES);
            const u32 fee = gSaveContext.save.isNight ? 4 : 0;
            auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
            entry.msg = "You have %p" + std::to_string(bank) + " Rupee" + (bank != 1 ? "s" : "") +
                        "%w in the bank.\x11\xC3" + (bank >= 99999 ? "%r" : "%g") + " Deposit\x11" +
                        (bank <= fee ? "%r" : "%g") + " Withdraw\x11%g Cancel";
            entry.autoFormat = false;
            CustomMessage::ReplaceColorChars(&entry.msg);
            CustomMessage::EnsureMessageEnd(&entry.msg);
            CustomMessage::LoadCustomMessageIntoFont(entry);
            *loadFromMessageTable = false;
        });
}

const char* PictoSubject() {
    const u32 flags = gSaveContext.save.saveInfo.pictoFlags0;
    const u32 lulu = (1u << PICTO_VALID_LULU_HEAD) | (1u << PICTO_VALID_LULU_RIGHT_ARM) | (1u << PICTO_VALID_LULU_LEFT_ARM);
    if (flags & (1u << PICTO_VALID_MONKEY)) {
        return "picture of a monkey";
    }
    if (flags & (1u << PICTO_VALID_BIG_OCTO)) {
        return "picture of a big octo";
    }
    if (flags & (1u << PICTO_VALID_DEKU_KING)) {
        return "picture of the Deku King";
    }
    if ((flags & lulu) == lulu) {
        return "good picture of Lulu";
    }
    if (flags & (1u << PICTO_VALID_LULU_HEAD)) {
        return "bad picture of Lulu";
    }
    if (flags & (1u << PICTO_VALID_SCARECROW)) {
        return "picture of a scarecrow";
    }
    if (flags & (1u << PICTO_VALID_TINGLE)) {
        return "picture of Tingle";
    }
    if (flags & (1u << PICTO_VALID_PIRATE_GOOD)) {
        return "good picture of a pirate";
    }
    if (flags & (1u << PICTO_VALID_PIRATE_TOO_FAR)) {
        return "bad picture of a pirate";
    }
    if (flags & (1u << PICTO_VALID_IN_SWAMP)) {
        return "picture of the swamp";
    }
    return "picture";
}

void RegisterPictoText() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0xF8, [](u16* textId, bool* loadFromMessageTable) {
            if (!OotmmSession_IsActive()) {
                return;
            }
            auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
            entry.msg = std::string("Keep this %r") + PictoSubject() + "%w?%g\xC2YesNo";
            entry.autoFormat = false;
            CustomMessage::ReplaceColorChars(&entry.msg);
            CustomMessage::EnsureMessageEnd(&entry.msg);
            CustomMessage::LoadCustomMessageIntoFont(entry);
            *loadFromMessageTable = false;
        });
}

void RegisterPurchaseTexts() {
    static constexpr const char* kCantBuy = "You can't buy that right now!";
    for (const uint16_t id : { 0x3466, 0x3490 }) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            id, [](u16* textId, bool* loadFromMessageTable) {
                if (!OotmmSession_IsActive() || Find(kGormanMilk.NativeId) == nullptr) {
                    return;
                }
                LoadPurchaseChoice(textId, loadFromMessageTable,
                                   "%r" + Ship::OotmmDialogPrice(kGormanMilk.Price) + "%w will do ya for " +
                                       PurchaseName(kGormanMilk) + "!",
                                   { "I'll buy it", "No thanks" });
            });
    }
    for (const uint16_t id : { 0x3469, 0x347F }) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            id, [](u16* textId, bool* loadFromMessageTable) {
                if (OotmmSession_IsActive() && Find(kGormanMilk.NativeId) != nullptr) {
                    LoadPurchaseText(textId, loadFromMessageTable, kCantBuy, true);
                }
            });
    }
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0x2B0B, [](u16* textId, bool* loadFromMessageTable) {
            if (!OotmmSession_IsActive() ||
                (Find(kMilkBarMilk.NativeId) == nullptr && Find(kMilkBarChateau.NativeId) == nullptr)) {
                return;
            }
            const auto line = [](const PurchaseSite& site, const char* vanilla) {
                const auto name = Find(site.NativeId) != nullptr ? PurchaseName(site, true) : std::string(vanilla);
                return name == kSoldOut ? name : name + ": %r" + Ship::OotmmDialogPrice(site.Price);
            };
            LoadPurchaseChoice(textId, loadFromMessageTable, "",
                               { line(kMilkBarMilk, "Regular Milk"), line(kMilkBarChateau, "Chateau"), "No Thanks" });
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0x2B0C, [](u16* textId, bool* loadFromMessageTable) {
            if (std::exchange(sMilkBarCantBuy, false) && OotmmSession_IsActive()) {
                LoadPurchaseText(textId, loadFromMessageTable, kCantBuy, true);
            }
        });
    static constexpr std::pair<uint16_t, int> kIntros[] = { { 0x15E1, 0 }, { 0x15F5, 1 }, { 0x1608, 2 }, { 0x161C, 3 } };
    for (const auto& [id, shop] : kIntros) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            id, [shop](u16* textId, bool* loadFromMessageTable) {
                static constexpr const char* kLines[][2] = {
                    { "I sell ", " to Deku Scrubs, but I'd really like to leave my hometown.\x10I'm hoping to find "
                                 "some success in a livelier place!" },
                    { "I sell ", " but I'm focusing my marketing efforts on Gorons...\x10What I'd really like to "
                                 "do is go back home and do business where I'm surrounded by trees and grass." },
                    { "I'm selling ", ", but I'm focusing my marketing efforts on Zoras.\x10"
                                      "Actually, I'd like to do business someplace where it's cooler and the air is "
                                      "clean." },
                    { "I'm here to sell ", ".\x10" "Actually, I want to do business in the sea breeze while "
                                           "listening to the sound of the waves." },
                };
                const auto& site = kScrubShops[shop];
                if (!OotmmSession_IsActive() || Find(site.NativeId) == nullptr) {
                    return;
                }
                LoadPurchaseText(textId, loadFromMessageTable,
                                 std::string(kLines[shop][0]) + PurchaseName(site) + kLines[shop][1], true);
            });
    }
    static constexpr std::pair<uint16_t, int> kOffers[] = { { 0x15E9, 0 }, { 0x15F3, 0 }, { 0x1600, 1 }, { 0x1606, 1 },
                                                            { 0x1612, 2 }, { 0x1617, 2 }, { 0x1626, 3 }, { 0x162D, 3 } };
    for (const auto& [id, shop] : kOffers) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            id, [shop](u16* textId, bool* loadFromMessageTable) {
                const auto& site = kScrubShops[shop];
                if (!OotmmSession_IsActive() || Find(site.NativeId) == nullptr) {
                    return;
                }
                LoadPurchaseText(textId, loadFromMessageTable,
                                 "I'll give you " + PurchaseName(site) + " for %r" + std::to_string(site.Price) +
                                     " Rupees%w!",
                                 true);
            });
    }
    for (const uint16_t id : { 0x15ED, 0x1603 }) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            id, [](u16* textId, bool* loadFromMessageTable) {
                if (OotmmSession_IsActive() && TalkingScrubShop() != nullptr) {
                    LoadPurchaseText(textId, loadFromMessageTable, "Not enough Rupees!", true);
                }
            });
    }
    for (const uint16_t id : { 0x15EC, 0x15EE, 0x1601, 0x1602, 0x1613 }) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            id, [](u16* textId, bool* loadFromMessageTable) {
                if (OotmmSession_IsActive() && TalkingScrubShop() != nullptr) {
                    LoadPurchaseText(textId, loadFromMessageTable, kCantBuy, true);
                }
            });
    }
}

bool IsCollectedId(const char* nativeId) {
    const auto* placement = Find(nativeId);
    return placement != nullptr && IsCollected(*placement);
}

const char* UnlessCollected(const char* nativeId) {
    return IsCollectedId(nativeId) ? nullptr : nativeId;
}

const char* LotterySite() {
    switch (CURRENT_DAY) {
        case 1:
            return UnlessCollected("MM_LOTTERY_NIGHT_1");
        case 2:
            return UnlessCollected("MM_LOTTERY_NIGHT_2");
        case 3:
            return UnlessCollected("MM_LOTTERY_NIGHT_3");
        default:
            return nullptr;
    }
}

const char* BankSite(const Actor* actor) {
    switch (reinterpret_cast<const EnGinkoMan*>(actor)->curTextId) {
        case 0x45B:
            return "MM_BANK_1";
        case 0x45C:
            return "MM_BANK_2";
        case 0x45D:
            return "MM_BANK_3";
        default:
            return nullptr;
    }
}

bool IsQuiver(GetItemId item) {
    return item == GI_QUIVER_30 || item == GI_QUIVER_40 || item == GI_QUIVER_50;
}

OfferSite ResolveOffer(Actor* actor, GetItemId item) {
    const int16_t scene = gPlayState != nullptr ? gPlayState->sceneId : -1;
    const GetItemId heart = GI_RECOVERY_HEART;
    switch (actor->id) {
        case ACTOR_EN_SELLNUTS:
            return item == GI_DEED_LAND ? OfferSite{ "MM_SCRUB_LAND", heart } : OfferSite{};
        case ACTOR_EN_AKINDONUTS:
            if (const auto* purchase = PurchaseFor(actor, item)) {
                return PurchaseOffer(*purchase);
            }
            if (item == GI_DEED_SWAMP) {
                return { "MM_SCRUB_SWAMP", heart };
            }
            if (item == GI_DEED_MOUNTAIN) {
                return { "MM_SCRUB_MOUNTAIN", heart };
            }
            if (item == GI_DEED_OCEAN) {
                return { "MM_SCRUB_OCEAN", heart };
            }
            return item == GI_RUPEE_HUGE && (actor->params & 3) == 3 ? OfferSite{ "MM_SCRUB_VALLEY", heart }
                                                                       : OfferSite{};
        case ACTOR_EN_BJT:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_TOILET_HEART_PIECE" } : OfferSite{};
        case ACTOR_EN_GURUGURU:
            return item == GI_MASK_BREMEN ? OfferSite{ "MM_MASK_BREMEN", heart } : OfferSite{};
        case ACTOR_EN_AL:
            if (item == GI_MASK_KAFEIS_MASK) {
                return { "MM_MASK_KAFEI" };
            }
            return item == GI_CHATEAU_BOTTLE ? OfferSite{ "MM_CHATEAU_ROMANI", heart } : OfferSite{};
        case ACTOR_EN_YB:
            return item == GI_MASK_KAMARO ? OfferSite{ "MM_MASK_KAMARO" } : OfferSite{};
        case ACTOR_EN_HS:
            return item == GI_MASK_BUNNY || item == GI_RUPEE_RED ? OfferSite{ "MM_MASK_BUNNY", GI_RUPEE_RED }
                                                                 : OfferSite{};
        case ACTOR_EN_STONE_HEISHI:
            return item == GI_MASK_STONE || item == GI_RUPEE_BLUE ? OfferSite{ "MM_MASK_STONE", GI_RUPEE_BLUE }
                                                                  : OfferSite{};
        case ACTOR_EN_BABA:
            return item == GI_MASK_BLAST ? OfferSite{ "MM_MASK_BLAST", heart } : OfferSite{};
        case ACTOR_EN_AZ:
            if (item == GI_BOTTLE) {
                return { "MM_BEAVER_1", heart };
            }
            return item == GI_HEART_PIECE ? OfferSite{ "MM_BEAVER_2", heart } : OfferSite{};
        case ACTOR_EN_BOM_BOWL_MAN:
            return { "MM_BOMBER_NOTEBOOK", heart };
        case ACTOR_EN_MA_YTO:
            return item == GI_MASK_ROMANI ? OfferSite{ "MM_CREMIA_ESCORT", GI_RUPEE_HUGE } : OfferSite{};
        case ACTOR_EN_LIFT_NUTS:
            if (item == GI_HEART_PIECE) {
                return { "MM_DEKU_PLAYGROUND_2", heart };
            }
            return { UnlessCollected("MM_DEKU_PLAYGROUND_1"), item };
        case ACTOR_EN_AOB_01:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_DOG_LADY_HEART_PIECE", heart } : OfferSite{};
        case ACTOR_EN_JGAME_TSN:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_FISHERMAN_HEART_PIECE", heart } : OfferSite{};
        case ACTOR_EN_MINIFROG:
            return { "MM_FROG_HEART_PIECE", heart };
        case ACTOR_EN_GB2:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_GHOST_HUT_HEART_PIECE", heart } : OfferSite{};
        case ACTOR_EN_IN:
            if (const auto* purchase = PurchaseFor(actor, item)) {
                return PurchaseOffer(*purchase);
            }
            return item == GI_MASK_GARO ? OfferSite{ "MM_MASK_GARO", GI_MILK } : OfferSite{};
        case ACTOR_EN_TAB:
            if (const auto* purchase = PurchaseFor(actor, item)) {
                return PurchaseOffer(*purchase);
            }
            return {};
        case ACTOR_EN_KGY:
            if (item != GI_SWORD_RAZOR && item != GI_SWORD_GILDED) {
                return {};
            }
            return { IsCollectedId("MM_SWORD_RAZOR") ? "MM_SWORD_GILDED" : "MM_SWORD_RAZOR", heart };
        case ACTOR_EN_GK:
            return item == GI_GOLD_DUST || item == GI_GOLD_DUST_2 ? OfferSite{ "MM_GORON_RACE", heart } : OfferSite{};
        case ACTOR_EN_GS:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_GOSSIP_HEART_PIECE", heart } : OfferSite{};
        case ACTOR_EN_KITAN:
            return { "MM_KEATON_HEART_PIECE", heart };
        case ACTOR_EN_TRT:
        case ACTOR_EN_TRT2:
            return !OotmmShops_IsOffering() && (item == GI_POTION_RED || item == GI_POTION_RED_BOTTLE)
                       ? OfferSite{ "MM_KOTAKE_RED_POTION", heart }
                       : OfferSite{};
        case ACTOR_EN_COL_MAN:
            return { "MM_LAB_FISH_HEART_PIECE", heart };
        case ACTOR_EN_GEG:
            return { "MM_MASK_DON_GERO", GI_RUPEE_PURPLE };
        case ACTOR_EN_DNO:
            return { "MM_MASK_SCENTS", GI_RUPEE_RED };
        case ACTOR_EN_TOTO:
            return item == GI_MASK_CIRCUS_LEADER ? OfferSite{ "MM_MASK_TROUPE_LEADER", heart } : OfferSite{};
        case ACTOR_EN_DT:
            return { "MM_MAYOR_HEART_PIECE", heart };
        case ACTOR_OBJ_MOON_STONE:
            return { "MM_MOON_TEAR", heart };
        case ACTOR_EN_MM3:
            return { "MM_POSTMAN_HEART_PIECE", heart };
        case ACTOR_EN_INVADEPOH:
            return { "MM_ROMANI_BOTTLE", heart };
        case ACTOR_EN_RZ:
            return { "MM_ROSA_HEART_PIECE", heart };
        case ACTOR_EN_SCOPENUTS:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_SCRUB_TELESCOPE", heart } : OfferSite{};
        case ACTOR_EN_OT:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_SEAHORSE_HEART_PIECE", heart } : OfferSite{};
        case ACTOR_EN_SYATEKI_MAN:
            if (IsQuiver(item)) {
                return { scene == SCENE_SYATEKI_MIZU ? "MM_SHOOTING_GAME_TOWN_1" : "MM_SHOOTING_GAME_SWAMP_1", heart };
            }
            return item == GI_HEART_PIECE ? OfferSite{ scene == SCENE_SYATEKI_MIZU ? "MM_SHOOTING_GAME_TOWN_2"
                                                                                  : "MM_SHOOTING_GAME_SWAMP_2",
                                                       heart }
                                          : OfferSite{};
        case ACTOR_EN_STH:
            if (item == GI_WALLET_ADULT || item == GI_WALLET_GIANT || item == GI_RUPEE_SILVER) {
                return { "MM_SPIDER_HOUSE_OCEAN", heart };
            }
            return item == GI_MASK_TRUTH ? OfferSite{ "MM_SPIDER_HOUSE_SWAMP", heart } : OfferSite{};
        case ACTOR_EN_KENDO_JS:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_SWORDSMAN_HEART_PIECE", heart } : OfferSite{};
        case ACTOR_EN_ZOS:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_ZORA_EVAN", heart } : OfferSite{};
        case ACTOR_EN_ZOT:
            return scene == SCENE_33ZORACITY && actor->params == -1015
                       ? OfferSite{ UnlessCollected("MM_ZORA_HALL_LIGHTS"), item }
                       : OfferSite{};
        case ACTOR_EN_FU:
            if (item == GI_HEART_PIECE) {
                return { "MM_HONEY_DARLING_2", heart };
            }
            return item == GI_RUPEE_PURPLE ? OfferSite{ UnlessCollected("MM_HONEY_DARLING_1"), item } : OfferSite{};
        case ACTOR_EN_KUJIYA:
            return item == GI_RUPEE_PURPLE ? OfferSite{ LotterySite(), item } : OfferSite{};
        case ACTOR_EN_FSN:
            if (OotmmShops_IsOffering()) {
                return {};
            }
            if (item == GI_MASK_KEATON) {
                return { "MM_MASK_KEATON", heart };
            }
            return item == GI_LETTER_TO_MAMA ? OfferSite{ "MM_LETTER_TO_MAMA", heart } : OfferSite{};
        case ACTOR_EN_GINKO_MAN:
            return { BankSite(actor), heart };
        case ACTOR_EN_AN:
            if (item == GI_ROOM_KEY) {
                return { "MM_ROOM_KEY", heart };
            }
            return item == GI_LETTER_TO_KAFEI ? OfferSite{ "MM_LETTER_TO_KAFEI", heart } : OfferSite{};
        case ACTOR_EN_DNH:
            if (item == GI_PICTOGRAPH_BOX) {
                return { "MM_KOUME_PICTOGRAPH_BOX", heart };
            }
            return item == GI_HEART_PIECE ? OfferSite{ "MM_KOUME_HEART_PIECE", heart } : OfferSite{};
        case ACTOR_EN_PM:
            return item == GI_MASK_POSTMAN ? OfferSite{ "MM_MASK_POSTMAN", heart } : OfferSite{};
        case ACTOR_EN_PST:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_POSTBOX", heart } : OfferSite{};
        case ACTOR_EN_SHN:
            return item == GI_HEART_PIECE ? OfferSite{ "MM_TOUR_GUIDE_HEART_PIECE", heart } : OfferSite{};
        case ACTOR_EN_NB:
            if (item != GI_HEART_PIECE) {
                return {};
            }
            return { sGrandmaOfferPos == 0xB3 ? "MM_GRANDMA_HEART_PIECE_2" : "MM_GRANDMA_HEART_PIECE_1", heart };
        case ACTOR_EN_JS:
            return item == GI_MASK_FIERCE_DEITY ? OfferSite{ "MM_MASK_FIERCE_DEITY", heart } : OfferSite{};
        case ACTOR_DM_CHAR02:
            return item == GI_OCARINA_OF_TIME ? OfferSite{ "MM_SKULL_KID_OCARINA", heart, "MM_SKULL_KID_SONG" }
                                              : OfferSite{};
        case ACTOR_EN_GO:
            return item == GI_POWDER_KEG
                       ? OfferSite{ "MM_GORON_KEG",
                                    INV_CONTENT(ITEM_POWDER_KEG) == ITEM_POWDER_KEG ? GI_POWDER_KEG : heart }
                       : OfferSite{};
        default:
            return {};
    }
}

const char* CutsceneMaskSite(int32_t item) {
    switch (item) {
        case ITEM_MASK_GORON:
        case GID_MASK_GORON:
            return "MM_MASK_GORON";
        case ITEM_MASK_ZORA:
        case GID_MASK_ZORA:
            return "MM_MASK_ZORA";
        case ITEM_MASK_GIBDO:
        case GID_MASK_GIBDO:
            return "MM_MASK_GIBDO";
        case ITEM_MASK_COUPLE:
        case GID_MASK_COUPLE:
            return "MM_MASK_COUPLE";
        default:
            return nullptr;
    }
}

const char* CutsceneTextSite(uint16_t textId) {
    switch (textId) {
        case 0x79:
            return "MM_MASK_GORON";
        case 0x7A:
            return "MM_MASK_ZORA";
        case 0x87:
            return "MM_MASK_GIBDO";
        case 0x85:
            return "MM_MASK_COUPLE";
        default:
            return nullptr;
    }
}

const char* GreatFairySite(int32_t type) {
    switch (type) {
        case ENELFGRP_TYPE_POWER:
            return "MM_GREAT_FAIRY_SWAMP";
        case ENELFGRP_TYPE_WISDOM:
            return "MM_GREAT_FAIRY_MOUNTAIN";
        case ENELFGRP_TYPE_COURAGE:
            return "MM_GREAT_FAIRY_OCEAN";
        case ENELFGRP_TYPE_KINDNESS:
            return "MM_GREAT_FAIRY_VALLEY";
        default:
            return gSaveContext.save.playerForm == PLAYER_FORM_HUMAN ? "MM_GREAT_FAIRY_TOWN" : "MM_GREAT_FAIRY_TOWN_ALT";
    }
}

bool BankRewardDue(const char* nativeId, int32_t threshold, const char* previous) {
    return HIGH_SCORE(HS_BANK_RUPEES) >= static_cast<u32>(threshold) && !IsCollectedId(nativeId) &&
           (previous == nullptr || IsCollectedId(previous));
}

} // namespace

extern "C" int OotmmNpcChecks_SuppressCutsceneText(uint16_t textId) {
    const char* site = CutsceneTextSite(textId);
    return site != nullptr && OotmmNpcChecks_IsShuffled(site);
}

extern "C" int OotmmNpcChecks_GreatFairyHuman(int vanillaHuman) {
    return OotmmNpcChecks_IsShuffled("MM_GREAT_FAIRY_TOWN") ? gSaveContext.save.playerForm == PLAYER_FORM_HUMAN
                                                            : vanillaHuman;
}

extern "C" int OotmmNpcChecks_DrawReward(Actor* actor, PlayState* play, const char* nativeId) {
    const auto* placement = Find(nativeId);
    if (actor == nullptr || play == nullptr || placement == nullptr) {
        return 0;
    }
    Matrix_Push();
    if (actor->scale.x > 0.0001f) {
        const float scale = 0.35f / actor->scale.x;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }
    if (IsCollected(*placement)) {
        GetItem_Draw(play, GID_RECOVERY_HEART);
    } else {
        const std::string& itemId = placement->CloakItemId.empty() ? placement->ItemId : placement->CloakItemId;
        const std::string& itemName =
            placement->CloakItemId.empty() ? placement->ItemName : placement->CloakItemName;
        if (!OotmmItemModel_DrawById(play, itemId, itemName)) {
            GetItem_Draw(play, GID_RUPEE_GREEN);
        }
    }
    Matrix_Pop();
    return 1;
}

extern "C" const char* OotmmNpcChecks_TingleSite(int map) {
    return map >= 0 && map < 6 ? kTingleSites[map] : nullptr;
}

extern "C" int OotmmNpcChecks_IsShuffled(const char* nativeId) {
    return Find(nativeId) != nullptr;
}

extern "C" int OotmmNpcChecks_IsCollected(const char* nativeId) {
    const auto* placement = Find(nativeId);
    return placement != nullptr && IsCollected(*placement);
}

extern "C" int OotmmNpcChecks_Collect(const char* nativeId) {
    const auto* placement = Find(nativeId);
    if (placement == nullptr) {
        return 0;
    }
    OotmmIpc_SendCheckCollected(placement->CheckId, !IsCollected(*placement));
    OotmmCheckLedger_Record(placement->CheckId);
    return 1;
}

std::string OotmmNpcChecks_PlacementName(const Ship::OotmmPlacement& placement) {
    const std::string& raw = placement.CloakItemId.empty() ? placement.ItemName : placement.CloakItemName;
    std::string name = "%g" + raw + "%w";
    for (const auto& [suffix, tag] : { std::pair<std::string_view, std::string_view>{ " (OoT)", "\x05OoT" },
                                       { " (OOT)", "\x05OoT" },
                                       { " (MM)", "%pMM" } }) {
        if (raw.ends_with(suffix)) {
            name = "%g" + raw.substr(0, raw.size() - suffix.size()) + "%w (" + std::string(tag) + "%w)";
            break;
        }
    }
    if (!placement.IsFor(OotmmSession_GetState().GetPlayerId())) {
        name += " for %yPlayer " + std::to_string(placement.OwnerPlayer) + "%w";
    }
    return name;
}

extern "C" int OotmmNpcChecks_SmithyStage(void) {
    if (!OotmmNpcChecks_IsShuffled("MM_SWORD_RAZOR")) {
        return -1;
    }
    if (!OotmmNpcChecks_IsCollected("MM_SWORD_RAZOR")) {
        return 0;
    }
    return OotmmNpcChecks_IsCollected("MM_SWORD_GILDED") ? 2 : 1;
}

extern "C" int OotmmNpcChecks_PurchaseAllowed(Actor* actor, int getItemId, int vanillaAllowed) {
    const auto* site = OotmmSession_IsActive() ? PurchaseFor(actor, getItemId) : nullptr;
    const int state = site != nullptr ? PurchaseState(*site) : -1;
    return state < 0 ? vanillaAllowed : state;
}

bool OotmmNpcChecks_IsPlacementCollected(const Ship::OotmmPlacement& placement) {
    return IsCollected(placement);
}

bool OotmmNpcChecks_CanPurchaseRepeat(const std::string& itemId) {
    const auto found = kRenewals.find(itemId);
    return OotmmSession_GetState().GetStringSetting("mode", "single") != "single" ||
           CanPurchaseMmCapacity(found != kRenewals.end() ? found->second.ItemId : itemId);
}

bool OotmmNpcChecks_CanPurchasePlacement(const Ship::OotmmPlacement& placement) {
    if (IsCollected(placement) || Ship::OotmmItemCatalog::IsNothing(placement.ItemId)) {
        return false;
    }
    const auto& state = OotmmSession_GetState();
    if (!placement.IsFor(state.GetPlayerId())) {
        return true;
    }
    if (IsMmBombchuAmmo(placement.ItemId)) {
        const auto behavior = state.GetStringSetting("bombchuBehaviorMm", "bombBag");
        if ((behavior == "bagFirst" || behavior == "bagSeparate") &&
            gSaveContext.save.saveInfo.inventory.items[SLOT_BOMBCHU] != ITEM_BOMBCHU) {
            return false;
        }
    }
    if (placement.ItemId == "MM_MAGIC_BEAN" &&
        gSaveContext.save.saveInfo.inventory.items[SLOT_MAGIC_BEANS] != ITEM_MAGIC_BEANS) {
        return false;
    }
    return state.GetStringSetting("mode", "single") != "single" || CanPurchaseMmCapacity(placement.ItemId);
}

bool OotmmNpcChecks_CollectPurchasedPlacement(const Ship::OotmmPlacement& placement) {
    if (gPlayState == nullptr || gSaveContext.rupeeAccumulator != 0) {
        return false;
    }
    if (IsCollected(placement)) {
        if (sPendingPurchase == placement.CheckId) {
            ClearPurchaseStatus();
        }
        return true;
    }
    const uint32_t frame = gPlayState->gameplayFrames;
    if (sPurchaseRetryFrame != 0 && frame - sPurchaseRetryFrame < 60) {
        return false;
    }
    OotmmCheckLedger_Record(placement.CheckId);
    if (!OotmmSession_SaveProgress()) {
        OotmmCheckLedger_Forget(placement.CheckId);
        sPurchaseRetryFrame = frame;
        ShowPurchaseRetry(placement.CheckId);
        return false;
    }
    sPurchaseRetryFrame = 0;
    if (sPendingPurchase == placement.CheckId) {
        ClearPurchaseStatus();
    }
    OotmmIpc_SendCheckCollected(placement.CheckId, true);
    return true;
}

extern "C" int OotmmNpcChecks_CanPurchase(const char* nativeId) {
    const auto* placement = Find(nativeId);
    return placement == nullptr || OotmmNpcChecks_CanPurchasePlacement(*placement);
}

extern "C" int OotmmNpcChecks_CollectPurchased(const char* nativeId) {
    const auto* placement = Find(nativeId);
    return placement != nullptr && OotmmNpcChecks_CollectPurchasedPlacement(*placement);
}

extern "C" int OotmmNpcChecks_Done(const char* nativeId, int vanillaDone) {
    return OotmmNpcChecks_IsShuffled(nativeId) ? OotmmNpcChecks_IsCollected(nativeId) : vanillaDone;
}

extern "C" void OotmmNpcChecks_SongTaught(int songItemId) {
    const char* nativeId = nullptr;
    if (songItemId == ITEM_SONG_LULLABY_INTRO) {
        nativeId = "MM_GORON_ELDER";
    } else if (songItemId >= ITEM_SONG_SONATA && songItemId <= ITEM_SONG_SUN) {
        nativeId = kSongSites[songItemId - ITEM_SONG_SONATA];
    }
    if (nativeId != nullptr) {
        OotmmNpcChecks_Collect(nativeId);
    }
}

extern "C" int OotmmNpcChecks_CollectRemains(int sceneId) {
    const RemainsSite site = RemainsFor(sceneId);
    return site.NativeId != nullptr && OotmmNpcChecks_Collect(site.NativeId);
}

extern "C" int OotmmNpcChecks_RemainsIsShuffled(int sceneId) {
    const RemainsSite site = RemainsFor(sceneId);
    return site.NativeId != nullptr && OotmmNpcChecks_IsShuffled(site.NativeId);
}

extern "C" int OotmmNpcChecks_RemainsReady(PlayState* play) {
    if (play == nullptr) {
        return 1;
    }
    const RemainsSite site = RemainsFor(play->sceneId);
    if (site.NativeId == nullptr) {
        return 1;
    }
    const auto* placement = Find(site.NativeId);
    if (placement == nullptr) {
        return 1;
    }
    Player* player = GET_PLAYER(play);
    const auto idle = [&](const Ship::OotmmPlacement& shown) {
        return player != nullptr && !OotmmIpc_IsCheckPresentationPending(shown.CheckId) &&
               Message_GetState(&play->msgCtx) == TEXT_STATE_NONE && !Play_InCsMode(play) &&
               !(player->stateFlags1 & (PLAYER_STATE1_400 | PLAYER_STATE1_20000000));
    };
    if (const auto* oath = Find("MM_SONG_ORDER"); oath != nullptr) {
        if (!IsCollected(*oath)) {
            OotmmNpcChecks_Collect("MM_SONG_ORDER");
            return 0;
        }
        if (!IsCollected(*placement) && !idle(*oath)) {
            return 0;
        }
    }
    if (!IsCollected(*placement)) {
        OotmmNpcChecks_Collect(site.NativeId);
        return 0;
    }
    return idle(*placement);
}

extern "C" void OotmmNpcChecks_DrawRemainsReward(Actor* actor, PlayState* play) {
    if (actor == nullptr || play == nullptr) {
        return;
    }
    const RemainsSite site = RemainsFor(play->sceneId);
    if (site.NativeId == nullptr) {
        return;
    }
    const auto* placement = Find(site.NativeId);
    if (placement == nullptr || IsCollected(*placement)) {
        return;
    }
    const std::string& itemId = placement->CloakItemId.empty() ? placement->ItemId : placement->CloakItemId;
    const std::string& itemName = placement->CloakItemId.empty() ? placement->ItemName : placement->CloakItemName;
    Matrix_Push();
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y + 35.0f, actor->world.pos.z, MTXMODE_NEW);
    Matrix_Scale(0.35f, 0.35f, 0.35f, MTXMODE_APPLY);
    Matrix_RotateYS((play->state.frames % 100) * (0x10000 / 100), MTXMODE_APPLY);
    if (!OotmmItemModel_DrawById(play, itemId, itemName)) {
        GetItem_Draw(play, GID_RUPEE_GREEN);
    }
    Matrix_Pop();
}

extern "C" int OotmmNpcChecks_CanFinishSilentOffer(const char* nativeId) {
    const bool wasChecking = sCheckingOfferText;
    sCheckingOfferText = true;
    const int32_t state = gPlayState != nullptr ? Message_GetState(&gPlayState->msgCtx) : TEXT_STATE_NONE;
    sCheckingOfferText = wasChecking;
    const auto* placement = Find(nativeId);
    if (gPlayState == nullptr || placement == nullptr || !OotmmNpcChecks_IsCollected(nativeId) ||
        OotmmIpc_IsCheckPresentationPending(placement->CheckId) || state != TEXT_STATE_NONE) {
        return 0;
    }
    const Player* player = GET_PLAYER(gPlayState);
    return player->csAction == PLAYER_CSACTION_NONE && !(player->stateFlags1 & PLAYER_STATE1_400);
}

extern "C" int OotmmNpcChecks_CanFinishMessageOffer(Actor* actor) {
    const auto offer = sCompletedMessageOffers.find(actor);
    return offer != sCompletedMessageOffers.end() && OotmmNpcChecks_CanFinishSilentOffer(offer->second.c_str());
}

extern "C" void OotmmNpcChecks_FinishMessageOffer(Actor* actor) {
    sCompletedMessageOffers.erase(actor);
    sTextDoneOffers.erase(actor);
}

extern "C" void OotmmNpcChecks_ResumeAfterOffer(Actor* actor) {
    if (OotmmNpcChecks_CanFinishMessageOffer(actor)) {
        OotmmNpcChecks_FinishMessageOffer(actor);
        actor->flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
    }
}

extern "C" void OotmmNpcChecks_SetUpdatingActor(Actor* actor) {
    sUpdatingActor = actor;
    const auto offer = sTextDoneOffers.find(actor);
    if (actor != nullptr && offer != sTextDoneOffers.end() && (offer->second.Read || --offer->second.Frames <= 0)) {
        sTextDoneOffers.erase(offer);
    }
}

extern "C" int OotmmNpcChecks_OfferTextDone(void) {
    if (sUpdatingActor == nullptr || sCheckingOfferText) {
        return 0;
    }
    const auto offer = sTextDoneOffers.find(sUpdatingActor);
    if (offer == sTextDoneOffers.end()) {
        return 0;
    }
    const bool ready = OotmmNpcChecks_CanFinishMessageOffer(sUpdatingActor) != 0;
    if (ready) {
        offer->second.Read = true;
    }
    return ready ? 1 : 0;
}

void OotmmNpcChecks_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        static int sFrames = 0;
        static std::string sLast;
        if (gPlayState == nullptr || !OotmmSession_IsActive() || ++sFrames < 20) {
            return;
        }
        sFrames = 0;
        std::string body = Ship::OotmmItemGrant::BuildVitals(Ship::OotmmGrantGame::Mm, VitalFree);
        if (body != sLast) {
            Ship::OotmmItemGrant::WriteVitals(OotmmSession_GetState().GetBootConfig().StatePath + ".vitals.mm", body);
            sLast = std::move(body);
        }
    });
    RegisterTinglePrices();
    RegisterPurchaseTexts();
    RegisterBankText();
    RegisterPictoText();
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorDestroy>(
        [](Actor* actor) {
            sPendingOffers.erase(actor);
            sCompletedMessageOffers.erase(actor);
            sTextDoneOffers.erase(actor);
            if (sUpdatingActor == actor) {
                sUpdatingActor = nullptr;
            }
        });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](s16, s8) {
            sPendingOffers.clear();
            sCompletedMessageOffers.clear();
            sTextDoneOffers.clear();
            sUpdatingActor = nullptr;
            ClearPurchaseStatus();
            OfferInitialSong();
        });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        ClearPurchaseStatus();
        sDeferredCollects.clear();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::AfterEndOfCycleSave>([]() { sCycleReset = true; });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(FlushDeferredCollects);
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* actor = va_arg(args, Actor*);
        const auto pending = sPendingOffers.find(actor);
        const OfferSite offer = pending != sPendingOffers.end() ? pending->second : ResolveOffer(actor, *item);
        const char* site = offer.NativeId;
        if (site == nullptr || !OotmmNpcChecks_IsShuffled(site)) {
            return;
        }
        if (actor->id == ACTOR_EN_SELLNUTS && gPlayState != nullptr) {
            Player* player = GET_PLAYER(gPlayState);
            player->stateFlags1 &= ~PLAYER_STATE1_20000000;
            if (player->csAction == PLAYER_CSACTION_19) {
                Player_SetCsActionWithHaltedActors(gPlayState, nullptr, PLAYER_CSACTION_END);
            }
        }
        if (pending == sPendingOffers.end() && OotmmNpcChecks_IsCollected(site) && offer.Repeat != GI_NONE) {
            if (!sPendingLedgerRenewal.empty() && gPlayState != nullptr) {
                OotmmIpc_SendRenewalGrant(sPendingLedgerRenewal);
                Ship::GameIpcItemPresentation presentation;
                presentation.ItemId = sPendingLedgerRenewal;
                presentation.ItemName = sPendingLedgerName;
                presentation.ItemGame = sPendingLedgerRenewal.starts_with("OOT_") ? "oot" : "mm";
                presentation.DestinationGame = presentation.ItemGame;
                OotmmItemPresentation_Queue(presentation);
                sPendingLedgerRenewal.clear();
                *should = false;
                actor->parent = &GET_PLAYER(gPlayState)->actor;
                sCompletedMessageOffers[actor] = site;
                sTextDoneOffers[actor].Frames = 30;
                sTextDoneOffers[actor].Read = false;
                return;
            }
            if (offer.Repeat == GI_RECOVERY_HEART) {
                sCompletedMessageOffers[actor] = site;
                sTextDoneOffers[actor].Frames = 30;
                sTextDoneOffers[actor].Read = false;
            }
            *item = offer.Repeat;
            return;
        }
        *should = false;
        if (pending == sPendingOffers.end()) {
            if (offer.Purchase) {
                if (!OotmmNpcChecks_CollectPurchasedPlacement(*Find(site))) {
                    return;
                }
            } else {
                OotmmNpcChecks_Collect(site);
            }
            if (offer.Companion != nullptr) {
                OotmmNpcChecks_Collect(offer.Companion);
            }
            sPendingOffers.emplace(actor, offer);
        }
        if (!OotmmIpc_IsCheckPresentationPending(Find(site)->CheckId) && gPlayState != nullptr) {
            actor->parent = &GET_PLAYER(gPlayState)->actor;
            sPendingOffers.erase(actor);
            sCompletedMessageOffers[actor] = site;
            sTextDoneOffers[actor].Frames = 30;
            sTextDoneOffers[actor].Read = false;
        }
    });
    REGISTER_VB_SHOULD(VB_SMITHY_START_UPGRADING_SWORD, {
        if (OotmmNpcChecks_SmithyStage() >= 0) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_EXEC_MSG_EVENT, {
        const uint32_t cmdId = va_arg(args, uint32_t);
        Actor* actor = va_arg(args, Actor*);
        uint8_t* script = va_arg(args, uint8_t*);
        if (actor == nullptr) {
            return;
        }
        if (actor->id == ACTOR_EN_NB && cmdId == MSCRIPT_CMD_ID_OFFER_ITEM) {
            sGrandmaOfferPos = static_cast<int32_t>(script - reinterpret_cast<uint8_t*>(
                                                                  reinterpret_cast<EnNb*>(actor)->msgScript));
            return;
        }
        if (cmdId == MSCRIPT_CMD_ID_CHECK_RUPEES && actor->id == ACTOR_EN_TAB) {
            const auto* cmd = reinterpret_cast<const MsgScriptCmdCheckRupees*>(script);
            const int16_t price = SCRIPT_PACK_16(cmd->rupeesH, cmd->rupeesL);
            if (!OotmmNpcChecks_PurchaseAllowed(actor, price == kMilkBarChateau.Price ? GI_CHATEAU : GI_MILK, true)) {
                sMilkBarCantBuy = true;
                *should = false;
            }
            return;
        }
        if (actor->id == ACTOR_EN_DNH && cmdId == MSCRIPT_CMD_ID_CHECK_WEEK_EVENT_REG && OotmmNpcChecks_IsShuffled("MM_KOUME_HEART_PIECE")) {
            const auto* cmd = reinterpret_cast<const MsgScriptCmdCheckWeekEventReg*>(script);
            if (PACK_WEEKEVENTREG_FLAG(cmd->flagByte, cmd->flagMask) == WEEKEVENTREG_26_40 &&
                script[sizeof(MsgScriptCmdCheckWeekEventReg)] == MSCRIPT_CMD_ID_CHECK_EVENT_INF) {
                *should = false;
            }
            return;
        }
        if (cmdId != MSCRIPT_CMD_ID_CHECK_ITEM) {
            return;
        }
        const auto* cmd = reinterpret_cast<const MsgScriptCmdCheckItem*>(script);
        const ItemId checked = static_cast<ItemId>(SCRIPT_PACK_16(cmd->itemH, cmd->itemL));
        if ((actor->id == ACTOR_EN_PM && checked == ITEM_MASK_POSTMAN &&
             OotmmNpcChecks_IsShuffled("MM_MASK_POSTMAN")) ||
            (actor->id == ACTOR_EN_DNH && checked == ITEM_PICTOGRAPH_BOX &&
             OotmmNpcChecks_IsShuffled("MM_KOUME_PICTOGRAPH_BOX")) ||
            (actor->id == ACTOR_EN_GO && checked == ITEM_POWDER_KEG && OotmmNpcChecks_IsShuffled("MM_GORON_KEG"))) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_DMCHAR05, {
        if (DeferCollect(CutsceneMaskSite(va_arg(args, int)))) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_DRAW_ITEM_FROM_DMCHAR05, {
        const char* site = CutsceneMaskSite(va_arg(args, int));
        const auto* placement = Find(site);
        if (placement == nullptr || gPlayState == nullptr) {
            return;
        }
        const std::string& itemId = placement->CloakItemId.empty() ? placement->ItemId : placement->CloakItemId;
        const std::string& itemName =
            placement->CloakItemId.empty() ? placement->ItemName : placement->CloakItemName;
        if (!OotmmItemModel_DrawById(gPlayState, itemId, itemName)) {
            GetItem_Draw(gPlayState, GID_RUPEE_GREEN);
        }
        *should = false;
    });
    REGISTER_VB_SHOULD(VB_HAVE_HEALED_PAMELAS_FATHER, {
        if (OotmmNpcChecks_IsShuffled("MM_MASK_GIBDO")) {
            *should = OotmmNpcChecks_IsCollected("MM_MASK_GIBDO");
        }
    });
    REGISTER_VB_SHOULD(VB_CONSIDER_DARMANI_HEALED, {
        if (OotmmNpcChecks_IsShuffled("MM_MASK_GORON")) {
            *should = OotmmNpcChecks_IsCollected("MM_MASK_GORON");
        }
    });
    REGISTER_VB_SHOULD(VB_CHECK_FOR_ROOM_KEY, {
        uint8_t owned[16];
        const int32_t count =
            OotmmItemApply_OwnedTradeItems(SLOT_TRADE_KEY_MAMA, owned, static_cast<int32_t>(std::size(owned)));
        *should = *should || (std::find(owned, owned + count, static_cast<uint8_t>(ITEM_ROOM_KEY)) != owned + count);
    });
    REGISTER_VB_SHOULD(VB_CONSIDER_MIKAU_HEALED, {
        const int whenGone = va_arg(args, int);
        if (OotmmNpcChecks_IsShuffled("MM_MASK_ZORA")) {
            const bool gone = OotmmNpcChecks_IsCollected("MM_MASK_ZORA");
            *should = whenGone ? gone : !gone;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_PENDANT_OF_MEMORIES_FROM_KAFEI, {
        if (DeferCollect("MM_PENDANT_OF_MEMORIES")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_STK_HAVE_OCARINA, {
        if (OotmmNpcChecks_IsShuffled("MM_SKULL_KID_OCARINA")) {
            *should = !OotmmNpcChecks_IsCollected("MM_SKULL_KID_OCARINA");
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_STRAY_FAIRY_MANAGER, {
        const auto* group = va_arg(args, EnElfgrp*);
        if (DeferCollect(GreatFairySite(group->type))) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_GREAT_FAIRY, {
        const auto* fairy = va_arg(args, BgDyYoseizo*);
        if (OotmmNpcChecks_IsShuffled(GreatFairySite(GREAT_FAIRY_GET_TYPE(&fairy->actor)))) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GREAT_FAIRY_GIVE_DOUBLE_DEFENSE_HEARTS, {
        if (OotmmNpcChecks_IsShuffled("MM_GREAT_FAIRY_OCEAN")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_BE_ELIGIBLE_FOR_BOMBERS_NOTEBOOK, {
        if (OotmmNpcChecks_IsShuffled("MM_BOMBER_NOTEBOOK")) {
            *should = CHECK_WEEKEVENTREG(WEEKEVENTREG_73_80) && !OotmmNpcChecks_IsCollected("MM_BOMBER_NOTEBOOK");
        }
    });
    REGISTER_VB_SHOULD(VB_HAVE_ROMANI_MASK, {
        if (OotmmNpcChecks_IsShuffled("MM_CREMIA_ESCORT")) {
            *should = OotmmNpcChecks_IsCollected("MM_CREMIA_ESCORT");
        }
    });
    REGISTER_VB_SHOULD(VB_HAVE_GARO_MASK, {
        if (OotmmNpcChecks_IsShuffled("MM_MASK_GARO")) {
            *should = OotmmNpcChecks_IsCollected("MM_MASK_GARO");
        }
    });
    REGISTER_VB_SHOULD(VB_PASS_FIRST_BANK_THRESHOLD, {
        if (OotmmNpcChecks_IsShuffled("MM_BANK_1")) {
            *should = BankRewardDue("MM_BANK_1", 200, nullptr);
        }
    });
    REGISTER_VB_SHOULD(VB_PASS_INTEREST_BANK_THRESHOLD, {
        if (OotmmNpcChecks_IsShuffled("MM_BANK_2")) {
            *should = BankRewardDue("MM_BANK_2", 500, "MM_BANK_1");
        }
    });
    REGISTER_VB_SHOULD(VB_PASS_SECOND_BANK_THRESHOLD, {
        if (OotmmNpcChecks_IsShuffled("MM_BANK_3")) {
            *should = BankRewardDue("MM_BANK_3", 1000, "MM_BANK_2");
        }
    });
    REGISTER_VB_SHOULD(VB_PASS_SECOND_BANK_THRESHOLD_ALT, {
        if (OotmmNpcChecks_IsShuffled("MM_BANK_3")) {
            *should = true;
        }
    });
    REGISTER_VB_SHOULD(VB_MADAME_AROMA_ASK_FOR_HELP, {
        if (OotmmNpcChecks_IsShuffled("MM_MASK_KAFEI")) {
            *should = !OotmmNpcChecks_IsCollected("MM_MASK_KAFEI");
        }
    });
    REGISTER_VB_SHOULD(VB_HAVE_KAMAROS_MASK, {
        if (OotmmNpcChecks_IsShuffled("MM_MASK_KAMARO")) {
            *should = OotmmNpcChecks_IsCollected("MM_MASK_KAMARO");
        }
    });
    REGISTER_VB_SHOULD(VB_HAVE_BLAST_MASK, {
        if (OotmmNpcChecks_IsShuffled("MM_MASK_BLAST")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_MNK, {
        if (DeferCollect("MM_SONG_AWAKENING")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_GK_LULLABY, {
        if (DeferCollect("MM_GORON_BABY")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_JG, {
        if (DeferCollect("MM_GORON_ELDER")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_OSN, {
        if (OotmmNpcChecks_IsShuffled(kInitialSong)) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_ROMANI, {
        if (DeferCollect("MM_SONG_EPONA")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_KNIGHT, {
        if (DeferCollect("MM_SONG_EMPTINESS")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_ROSA_SISTERS_HEART_PIECE, {
        if (*should && DeferCollect("MM_ROSA_HEART_PIECE")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_SONG_OF_SOARING, {
        if (OotmmNpcChecks_IsShuffled("MM_SONG_SOARING")) {
            if (!OotmmNpcChecks_IsCollected("MM_SONG_SOARING")) {
                DeferCollect("MM_SONG_SOARING");
            }
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_SONG_OF_STORMS, {
        if (DeferCollect("MM_SONG_STORMS")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_OATH_TO_ORDER, {
        if (*should && DeferCollect("MM_SONG_ORDER")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GIVE_NEW_WAVE_BOSSA_NOVA, {
        if (OotmmNpcChecks_Collect("MM_SONG_ZORA")) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_JG_THINK_YOU_KNOW_LULLABY, {
        if (OotmmNpcChecks_IsShuffled("MM_GORON_ELDER") && OotmmNpcChecks_IsCollected("MM_GORON_ELDER")) {
            *should = true;
        }
    });
    REGISTER_VB_SHOULD(VB_ROMANI_CONSIDER_EPONA_SONG_GIVEN, {
        if (OotmmNpcChecks_IsShuffled("MM_SONG_EPONA")) {
            *should = OotmmNpcChecks_IsCollected("MM_SONG_EPONA");
        }
    });
    REGISTER_VB_SHOULD(VB_OSN_CONSIDER_ELIGIBLE_FOR_SONG_OF_HEALING, {
        if (OotmmNpcChecks_IsShuffled(kInitialSong)) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_SPAWN_BOSS_REMAINS, {
        s32* remains = va_arg(args, s32*);
        if (gPlayState != nullptr) {
            const RemainsSite site = RemainsFor(gPlayState->sceneId);
            if (site.NativeId != nullptr && OotmmNpcChecks_IsShuffled(site.NativeId)) {
                *remains = OotmmNpcChecks_IsCollected(site.NativeId) ? 0 : site.Index;
                *should = false;
            }
        }
    });
}
