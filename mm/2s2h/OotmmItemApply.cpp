#include "OotmmItemApply.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmCustomItems.h"
#include "OotmmDungeons.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmAppliedLedger.h>
#include <libultraship/bridge/OotmmItemGrant.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"

extern PlayState* gPlayState;
}

namespace {

const std::unordered_map<std::string, uint8_t> kNativeItems = {
#include "OotmmNativeItems.inc"
};

struct Ladder {
    std::string_view Suffix;
    std::vector<uint8_t> Tiers;
};

const std::vector<Ladder> kLadders = {
    { "SWORD", { ITEM_SWORD_KOKIRI, ITEM_SWORD_RAZOR, ITEM_SWORD_GILDED } },
    { "SHIELD", { ITEM_SHIELD_HERO, ITEM_SHIELD_MIRROR } },
    { "BOMB_BAG", { ITEM_BOMB_BAG_20, ITEM_BOMB_BAG_30, ITEM_BOMB_BAG_40 } },
    { "BOW", { ITEM_QUIVER_30, ITEM_QUIVER_40, ITEM_QUIVER_50 } },
    { "HOOKSHOT", { ITEM_HOOKSHOT, ITEM_LONGSHOT } },
    { "OCARINA", { ITEM_OCARINA_FAIRY, ITEM_OCARINA_OF_TIME } },
    { "NUT_UPGRADE", { ITEM_DEKU_NUT_UPGRADE_30, ITEM_DEKU_NUT_UPGRADE_40 } },
    { "STICK_UPGRADE", { ITEM_DEKU_STICK_UPGRADE_20, ITEM_DEKU_STICK_UPGRADE_30 } },
};

// Ammo pickups name their own size, e.g. MM_ARROWS_10.
const std::vector<std::pair<std::string_view, uint8_t>> kAmmoSlots = {
    { "ARROWS", ITEM_BOW },        { "BOMBS", ITEM_BOMB },
    { "BOMBCHU", ITEM_BOMBCHU },   { "NUTS", ITEM_DEKU_NUT },
    { "STICKS", ITEM_DEKU_STICK },
};

const std::unordered_map<std::string, uint8_t> kQuestFlags = {
    { "MM_SONG_TIME", QUEST_SONG_TIME },
    { "MM_SONG_EPONA", QUEST_SONG_EPONA },
    { "MM_SONG_STORMS", QUEST_SONG_STORMS },
    { "MM_SONG_HEALING", QUEST_SONG_HEALING },
    { "MM_SONG_SOARING", QUEST_SONG_SOARING },
    { "MM_SONG_AWAKENING", QUEST_SONG_SONATA },
    { "MM_SONG_GORON", QUEST_SONG_LULLABY },
    { "MM_SONG_GORON_HALF", QUEST_SONG_LULLABY_INTRO },
    { "MM_SONG_ZORA", QUEST_SONG_BOSSA_NOVA },
    { "MM_SONG_ORDER", QUEST_SONG_OATH },
    { "MM_SONG_EMPTINESS", QUEST_SONG_ELEGY },
    // MM's engine carries a full, dormant Sun's Song; the quest bit switches it on.
    { "MM_SONG_SUN", QUEST_SONG_SUN },
    { "SHARED_SONG_TIME", QUEST_SONG_TIME },
    { "SHARED_SONG_EPONA", QUEST_SONG_EPONA },
    { "SHARED_SONG_STORMS", QUEST_SONG_STORMS },
    { "SHARED_SONG_HEALING", QUEST_SONG_HEALING },
    { "SHARED_SONG_SOARING", QUEST_SONG_SOARING },
    { "SHARED_SONG_AWAKENING", QUEST_SONG_SONATA },
    { "SHARED_SONG_GORON", QUEST_SONG_LULLABY },
    { "SHARED_SONG_GORON_HALF", QUEST_SONG_LULLABY_INTRO },
    { "SHARED_SONG_ZORA", QUEST_SONG_BOSSA_NOVA },
    { "SHARED_SONG_ORDER", QUEST_SONG_OATH },
    { "SHARED_SONG_EMPTINESS", QUEST_SONG_ELEGY },
    { "SHARED_SONG_SUN", QUEST_SONG_SUN },
    { "MM_REMAINS_ODOLWA", QUEST_REMAINS_ODOLWA },
    { "MM_REMAINS_GOHT", QUEST_REMAINS_GOHT },
    { "MM_REMAINS_GYORG", QUEST_REMAINS_GYORG },
    { "MM_REMAINS_TWINMOLD", QUEST_REMAINS_TWINMOLD },
};

struct DungeonSlot {
    std::string_view Code;
    uint8_t Index;
};

const std::vector<DungeonSlot> kDungeons = {
    { "WF", DUNGEON_SCENE_INDEX_WOODFALL_TEMPLE },
    { "SH", DUNGEON_SCENE_INDEX_SNOWHEAD_TEMPLE },
    { "GB", DUNGEON_SCENE_INDEX_GREAT_BAY_TEMPLE },
    { "ST", DUNGEON_SCENE_INDEX_STONE_TOWER_TEMPLE },
};

const DungeonSlot* FindDungeon(std::string_view suffix) {
    const size_t underscore = suffix.rfind('_');
    const std::string_view code =
        underscore == std::string_view::npos ? suffix : suffix.substr(underscore + 1);
    for (const auto& dungeon : kDungeons) {
        if (dungeon.Code == code) {
            return &dungeon;
        }
    }
    return nullptr;
}

Ship::OotmmAppliedLedger sLedger;
std::map<std::string, std::string> sUnhandled;
uint64_t sAppliedRevision = 0;
bool sLedgerLoaded = false;

// Keyed per-game and per-save-tag: the other game's fresh-save reset cannot erase these records,
// and a save recreated under a new tag starts from an empty ledger and re-grants everything.
std::string LedgerPathFor(int fileNum) {
    const auto& state = OotmmSession_GetState();
    return state.GetBootConfig().StatePath + ".applied.mm-" + state.GetNativeSaveTag() + "." +
           std::to_string(fileNum);
}

std::string LedgerPath() {
    return LedgerPathFor(gSaveContext.fileNum);
}

std::string LegacyLedgerPath() {
    return OotmmSession_GetState().GetBootConfig().StatePath + ".applied." +
           std::to_string(gSaveContext.fileNum);
}

void EnsureLedger() {
    if (sLedgerLoaded) {
        return;
    }
    // Older sessions shared one ledger between both games; adopt it once, then stay per-game.
    if (!sLedger.Load(LedgerPath())) {
        sLedger.Load(LegacyLedgerPath());
    }
    sLedgerLoaded = true;
}

const Ladder* FindLadder(std::string_view suffix) {
    for (const auto& ladder : kLadders) {
        if (ladder.Suffix == suffix) {
            return &ladder;
        }
    }
    return nullptr;
}

std::vector<uint8_t> LadderTiers(std::string_view suffix) {
    if (suffix == "WALLET") {
        std::vector<uint8_t> tiers;
        if (OotmmSession_GetState().GetBoolSetting("childWallets", false)) {
            tiers.push_back(ITEM_WALLET_DEFAULT);
        }
        tiers.push_back(ITEM_WALLET_ADULT);
        tiers.push_back(ITEM_WALLET_GIANT);
        return tiers;
    }
    if (const Ladder* ladder = FindLadder(suffix)) {
        return ladder->Tiers;
    }
    return {};
}

uint32_t TrailingNumber(std::string_view suffix, uint32_t fallback) {
    const size_t underscore = suffix.rfind('_');
    if (underscore == std::string_view::npos) {
        return fallback;
    }
    uint32_t value = 0;
    for (const char c : suffix.substr(underscore + 1)) {
        if (c < '0' || c > '9') {
            return fallback;
        }
        value = value * 10 + static_cast<uint32_t>(c - '0');
    }
    return value == 0 ? fallback : value;
}

int16_t RupeeValue(std::string_view suffix) {
    if (suffix.find("BLUE") != std::string_view::npos) return 5;
    if (suffix.find("RED") != std::string_view::npos) return 20;
    if (suffix.find("PURPLE") != std::string_view::npos) return 50;
    if (suffix.find("GOLD") != std::string_view::npos) return 200;
    if (suffix.find("HUGE") != std::string_view::npos) return 200;
    if (suffix.find("RAINBOW") != std::string_view::npos) return 999;
    return 1;
}

void ApplyOnce(PlayState* play, const std::string& key, uint8_t item) {
    sLedger.SetApplied(key, 1);
    if (item == ITEM_NONE) {
        return;
    }
    // gItemSlots only covers inventory items, so SLOT()/INV_CONTENT() are unusable here.
    Item_Give(play, item);
    SPDLOG_INFO("[OoTMM grant] gave {} (item {})", key, static_cast<int>(item));
}

const std::vector<uint8_t> kDeedTradeItems = {
    ITEM_MOONS_TEAR, ITEM_DEED_LAND, ITEM_DEED_SWAMP, ITEM_DEED_MOUNTAIN, ITEM_DEED_OCEAN,
};

const std::vector<uint8_t> kKeyMamaTradeItems = { ITEM_ROOM_KEY, ITEM_LETTER_MAMA };

const std::vector<uint8_t> kCoupleTradeItems = { ITEM_LETTER_TO_KAFEI, ITEM_PENDANT_OF_MEMORIES };

const std::vector<uint8_t>* TradeItemsForSlot(uint8_t slot) {
    switch (slot) {
        case SLOT_TRADE_DEED:
            return &kDeedTradeItems;
        case SLOT_TRADE_KEY_MAMA:
            return &kKeyMamaTradeItems;
        case SLOT_TRADE_COUPLE:
            return &kCoupleTradeItems;
        default:
            return nullptr;
    }
}

std::string NativeTradeKey(uint8_t item) {
    return "MM_TRADE_NATIVE:" + std::to_string(item);
}

bool IsTradeGrantOp(const Ship::OotmmGrantOp& op) {
    switch (op.Kind) {
        case Ship::OotmmGrantKind::Equipment:
        case Ship::OotmmGrantKind::Upgrade:
        case Ship::OotmmGrantKind::Mask:
        case Ship::OotmmGrantKind::Bottle:
            return Ship::OotmmItemGrant::AppliesTo(op, Ship::OotmmGrantGame::Mm) &&
                   LadderTiers(op.Slot).empty();
        default:
            return false;
    }
}

// Swords and shields live in the save rather than the ledger, so a file that was never written
// after its grant would otherwise stay empty for good.
bool EquipmentMissing(uint8_t item) {
    if (item >= ITEM_SWORD_KOKIRI && item <= ITEM_SWORD_GILDED) {
        return GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) <
               static_cast<u16>(item - ITEM_SWORD_KOKIRI + EQUIP_VALUE_SWORD_KOKIRI);
    }
    if (item >= ITEM_SHIELD_HERO && item <= ITEM_SHIELD_MIRROR) {
        return GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) <
               static_cast<u16>(item - ITEM_SHIELD_HERO + EQUIP_VALUE_SHIELD_HERO);
    }
    return false;
}

bool IsGrantedTradeItem(const std::string& itemId, uint32_t count, uint8_t item) {
    if (count == 0 || OotmmCustomItems_IdForItemId(itemId.c_str()) != ITEM_NONE) {
        return false;
    }
    const auto found = kNativeItems.find(itemId);
    if (found == kNativeItems.end() || found->second != item) {
        return false;
    }
    for (const auto& op : Ship::OotmmItemGrant::Resolve(itemId, count)) {
        if (IsTradeGrantOp(op)) {
            return true;
        }
    }
    return false;
}

std::vector<uint8_t> OwnedTradeItems(uint8_t slot) {
    const std::vector<uint8_t>* candidates = TradeItemsForSlot(slot);
    if (candidates == nullptr) {
        return {};
    }
    EnsureLedger();
    const uint8_t current = gSaveContext.save.saveInfo.inventory.items[slot];
    // Grants overwrite the slot, so remember natively obtained occupants before they vanish.
    if (std::find(candidates->begin(), candidates->end(), current) != candidates->end() &&
        !sLedger.Has(NativeTradeKey(current))) {
        sLedger.SetApplied(NativeTradeKey(current), 1);
        sLedger.Save(LedgerPath());
    }

    std::vector<uint8_t> owned;
    for (const uint8_t item : *candidates) {
        bool granted = false;
        for (const auto& [itemId, count] : OotmmIpc_GetInventory().GetValues()) {
            if (IsGrantedTradeItem(itemId, count, item)) {
                granted = true;
                break;
            }
        }
        if (item == current || granted || sLedger.Has(NativeTradeKey(item))) {
            owned.push_back(item);
        }
    }
    return owned;
}

void ApplyOne(PlayState* play, const std::string& itemId, uint32_t count) {
    if (count == 0) {
        return;
    }
    // Custom items live in the launcher inventory only; nothing is written to MM's save.
    if (OotmmCustomItems_IdForItemId(itemId.c_str()) != ITEM_NONE) {
        return;
    }
    for (const auto& op : Ship::OotmmItemGrant::Resolve(itemId, count)) {
        if (!Ship::OotmmItemGrant::AppliesTo(op, Ship::OotmmGrantGame::Mm)) {
            continue;
        }
        const uint32_t already = sLedger.Applied(itemId);
        switch (op.Kind) {
            case Ship::OotmmGrantKind::Equipment:
            case Ship::OotmmGrantKind::Upgrade:
            case Ship::OotmmGrantKind::Mask:
            case Ship::OotmmGrantKind::Bottle: {
                if (op.Slot == "TRANSCENDENT_FAIRY") {
                    if (already == 0) {
                        for (const auto& dungeon : kDungeons) {
                            gSaveContext.save.saveInfo.inventory.strayFairies[dungeon.Index] =
                                STRAY_FAIRY_SCATTERED_TOTAL;
                        }
                        SET_WEEKEVENTREG(WEEKEVENTREG_08_80);
                        Health_ChangeBy(play, static_cast<int16_t>(20 * 16));
                        sLedger.SetApplied(itemId, 1);
                    }
                    break;
                }
                if (op.Slot == "SCALE") {
                    // MM has no scale item ids; upstream item_add.c writes upgrades.scale
                    // directly (silver = 1, golden = 2), with an optional bronze tier first.
                    std::vector<uint8_t> scaleTiers;
                    if (OotmmSession_GetState().GetBoolSetting("bronzeScale", false)) {
                        scaleTiers.push_back(0);
                    }
                    scaleTiers.push_back(1);
                    scaleTiers.push_back(2);
                    const size_t tier = std::min<size_t>(op.Amount, scaleTiers.size()) - 1;
                    const std::string key = itemId + ":" + std::to_string(tier);
                    if (!sLedger.Has(key)) {
                        sLedger.SetApplied(key, 1);
                        if (scaleTiers[tier] > CUR_UPG_VALUE(UPG_SCALE)) {
                            Inventory_ChangeUpgrade(UPG_SCALE, scaleTiers[tier]);
                        }
                        SPDLOG_INFO("[OoTMM grant] gave {} (scale {})", key,
                                    static_cast<int>(scaleTiers[tier]));
                    }
                    break;
                }
                const std::vector<uint8_t> tiers = LadderTiers(op.Slot);
                if (!tiers.empty()) {
                    const size_t tier = std::min<size_t>(op.Amount, tiers.size()) - 1;
                    const std::string key = itemId + ":" + std::to_string(tier);
                    if (!sLedger.Has(key)) {
                        ApplyOnce(play, key, tiers[tier]);
                    }
                    break;
                }
                {
                    const auto found = kNativeItems.find(itemId);
                    if (found == kNativeItems.end()) {
                        if (already == 0) {
                            sUnhandled[itemId] = op.Slot + " (no native item)";
                        }
                    } else if (already == 0 || EquipmentMissing(found->second)) {
                        ApplyOnce(play, itemId, found->second);
                    }
                }
                break;
            }
            case Ship::OotmmGrantKind::Rupees: {
                if (count > already) {
                    Rupees_ChangeBy(static_cast<int16_t>((count - already) * RupeeValue(op.Slot)));
                    sLedger.SetApplied(itemId, count);
                }
                break;
            }
            case Ship::OotmmGrantKind::Ammo: {
                if (count <= already) {
                    break;
                }
                for (const auto& [name, slot] : kAmmoSlots) {
                    if (op.Slot.rfind(name, 0) == 0) {
                        const uint32_t per = TrailingNumber(op.Slot, 1);
                        Inventory_ChangeAmmo(slot, static_cast<int16_t>((count - already) * per));
                        sLedger.SetApplied(itemId, count);
                        break;
                    }
                }
                break;
            }
            case Ship::OotmmGrantKind::Health: {
                if (count > already) {
                    Health_ChangeBy(play, static_cast<int16_t>((count - already) * 16));
                    sLedger.SetApplied(itemId, count);
                }
                break;
            }
            case Ship::OotmmGrantKind::Magic: {
                if (count <= already) {
                    break;
                }
                if (op.Slot.rfind("MAGIC_UPGRADE", 0) == 0) {
                    gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
                    if (count >= 2) {
                        gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired = true;
                    }
                    gSaveContext.save.saveInfo.playerData.magicLevel = 0;
                } else {
                    Magic_Add(play, static_cast<s16>(MAGIC_NORMAL_METER));
                }
                sLedger.SetApplied(itemId, count);
                break;
            }
            case Ship::OotmmGrantKind::DungeonItem: {
                if (op.Slot.rfind("STRAY_FAIRY", 0) == 0) {
                    if (count <= already) {
                        break;
                    }
                    if (op.Slot == "STRAY_FAIRY_TOWN") {
                        SET_WEEKEVENTREG(WEEKEVENTREG_08_80);
                    } else {
                        const DungeonSlot* fairyDungeon = FindDungeon(op.Slot);
                        if (fairyDungeon == nullptr ||
                            fairyDungeon->Index >=
                                ARRAY_COUNT(gSaveContext.save.saveInfo.inventory.strayFairies)) {
                            break;
                        }
                        s8& fairies =
                            gSaveContext.save.saveInfo.inventory.strayFairies[fairyDungeon->Index];
                        fairies = static_cast<s8>(std::min<int>(
                            STRAY_FAIRY_SCATTERED_TOTAL,
                            fairies + static_cast<int>(count - already)));
                    }
                    Health_ChangeBy(play, static_cast<int16_t>((count - already) * 3 * 16));
                    sLedger.SetApplied(itemId, count);
                    break;
                }
                const DungeonSlot* dungeon = FindDungeon(op.Slot);
                if (dungeon == nullptr ||
                    dungeon->Index >=
                        ARRAY_COUNT(gSaveContext.save.saveInfo.inventory.dungeonItems)) {
                    break;
                }
                if (op.Slot.rfind("SMALL_KEY", 0) == 0) {
                    if (count > already &&
                        dungeon->Index <
                            ARRAY_COUNT(gSaveContext.save.saveInfo.inventory.dungeonKeys)) {
                        const s8 held = gSaveContext.save.saveInfo.inventory.dungeonKeys[dungeon->Index];
                        gSaveContext.save.saveInfo.inventory.dungeonKeys[dungeon->Index] =
                            static_cast<s8>((held < 0 ? 0 : held) + (count - already));
                        sLedger.SetApplied(itemId, count);
                    }
                    break;
                }
                if (already != 0) {
                    break;
                }
                const uint8_t bit = op.Slot.rfind("MAP", 0) == 0        ? DUNGEON_MAP
                                    : op.Slot.rfind("COMPASS", 0) == 0  ? DUNGEON_COMPASS
                                    : op.Slot.rfind("BOSS_KEY", 0) == 0 ? DUNGEON_BOSS_KEY
                                                                        : 0xFF;
                if (bit != 0xFF) {
                    SET_DUNGEON_ITEM(bit, dungeon->Index);
                    sLedger.SetApplied(itemId, 1);
                }
                break;
            }
            case Ship::OotmmGrantKind::SmallKeyRing: {
                const DungeonSlot* dungeon = FindDungeon(op.Slot);
                if (already != 0 || dungeon == nullptr ||
                    dungeon->Index >=
                        ARRAY_COUNT(gSaveContext.save.saveInfo.inventory.dungeonKeys)) {
                    break;
                }
                const int keys = OotmmDungeons_MaxSmallKeys(dungeon->Index);
                if (keys == 0) {
                    break;
                }
                gSaveContext.save.saveInfo.inventory.dungeonKeys[dungeon->Index] =
                    static_cast<s8>(keys);
                sLedger.SetApplied(itemId, 1);
                break;
            }
            case Ship::OotmmGrantKind::Token: {
                if (count <= already) {
                    break;
                }
                uint32_t swamp = (gSaveContext.save.saveInfo.skullTokenCount >> 16) & 0xFFFF;
                uint32_t ocean = gSaveContext.save.saveInfo.skullTokenCount & 0xFFFF;
                if (op.Slot == "PLATINUM_TOKEN") {
                    swamp = 30;
                    ocean = 30;
                } else if (op.Slot == "GS_TOKEN_SWAMP") {
                    swamp = std::min<uint32_t>(30, swamp + (count - already));
                } else if (op.Slot == "GS_TOKEN_OCEAN") {
                    ocean = std::min<uint32_t>(30, ocean + (count - already));
                } else {
                    sUnhandled[itemId] = op.Slot;
                    break;
                }
                gSaveContext.save.saveInfo.skullTokenCount = (swamp << 16) | ocean;
                sLedger.SetApplied(itemId, count);
                break;
            }
            case Ship::OotmmGrantKind::Song:
            case Ship::OotmmGrantKind::QuestFlag: {
                if (already != 0) {
                    break;
                }
                const auto found = kQuestFlags.find(itemId);
                if (found != kQuestFlags.end()) {
                    SET_QUEST_ITEM(found->second);
                    sLedger.SetApplied(itemId, 1);
                }
                break;
            }
            default:
                if (!Ship::OotmmItemGrant::IsInventoryBacked(op.Kind)) {
                    sUnhandled[itemId] = op.Slot;
                }
                break;
        }
    }
}

} // namespace

void OotmmItemApply_ResetLedgerForNewSave() {
    if (!OotmmSession_IsActive()) {
        return;
    }
    sLedger.Reset();
    sLedger.Save(LedgerPath());
    sLedgerLoaded = true;
    sAppliedRevision = 0;
}

void OotmmItemApply_Reconcile() {
    if (gPlayState == nullptr || !OotmmSession_IsActive()) {
        return;
    }
    const auto& inventory = OotmmIpc_GetInventory();
    if (inventory.GetRevision() == sAppliedRevision) {
        return;
    }
    EnsureLedger();
    sAppliedRevision = inventory.GetRevision();
    size_t owned = 0;
    for (const auto& [itemId, count] : inventory.GetValues()) {
        if (count > 0) {
            owned++;
        }
        ApplyOne(gPlayState, itemId, count);
    }
    // Sun's Song is the one native quest song with note-shuffle pieces (6 notes).
    if (inventory.Count("MM_SONG_NOTE_SUN") + inventory.Count("SHARED_SONG_NOTE_SUN") >= 6) {
        SET_QUEST_ITEM(QUEST_SONG_SUN);
    }
    SPDLOG_INFO("[OoTMM grant] revision {}: {} entries, {} owned", inventory.GetRevision(),
                inventory.GetValues().size(), owned);
    sLedger.Save(LedgerPath());
    if (!sUnhandled.empty()) {
        SPDLOG_WARN("[OoTMM grant] {} owned items have no grant path:", sUnhandled.size());
        for (const auto& [id, slot] : sUnhandled) {
            SPDLOG_WARN("[OoTMM grant]   {} ({})", id, slot);
        }
        sUnhandled.clear();
    }
}

extern "C" int32_t OotmmItemApply_TradeSlotCandidates(uint8_t slot, uint8_t* outItems,
                                                      int32_t capacity) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    const std::vector<uint8_t>* candidates = TradeItemsForSlot(slot);
    if (candidates == nullptr) {
        return 0;
    }
    int32_t count = 0;
    for (const uint8_t item : *candidates) {
        if (count >= capacity) {
            break;
        }
        outItems[count++] = item;
    }
    return count;
}

extern "C" int32_t OotmmItemApply_OwnedTradeItems(uint8_t slot, uint8_t* outItems,
                                                  int32_t capacity) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    const std::vector<uint8_t> owned = OwnedTradeItems(slot);
    int32_t count = 0;
    for (const uint8_t item : owned) {
        if (count >= capacity) {
            break;
        }
        outItems[count++] = item;
    }
    return count;
}

extern "C" void OotmmItemApply_SetTradeItemOwned(uint8_t slot, uint8_t item, int32_t owned) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    const std::vector<uint8_t>* candidates = TradeItemsForSlot(slot);
    if (candidates == nullptr ||
        std::find(candidates->begin(), candidates->end(), item) == candidates->end()) {
        return;
    }
    EnsureLedger();
    if (owned) {
        sLedger.SetApplied(NativeTradeKey(item), 1);
        sLedger.Save(LedgerPath());
        const uint8_t current = gSaveContext.save.saveInfo.inventory.items[slot];
        if (std::find(candidates->begin(), candidates->end(), current) == candidates->end()) {
            gSaveContext.save.saveInfo.inventory.items[slot] = item;
        }
        return;
    }

    sLedger.Erase(NativeTradeKey(item));
    sLedger.Save(LedgerPath());
    for (const auto& [itemId, count] : OotmmIpc_GetInventory().GetValues()) {
        if (IsGrantedTradeItem(itemId, count, item)) {
            OotmmIpc_SetDebugItemValue(itemId, 0);
        }
    }
    if (gSaveContext.save.saveInfo.inventory.items[slot] == item) {
        gSaveContext.save.saveInfo.inventory.items[slot] = ITEM_NONE;
        // The debug-clear is asynchronous, so the removed item may still look granted.
        for (const uint8_t replacement : OwnedTradeItems(slot)) {
            if (replacement != item) {
                gSaveContext.save.saveInfo.inventory.items[slot] = replacement;
                break;
            }
        }
    }
}

void OotmmItemApply_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(
        OotmmItemApply_Reconcile);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        sAppliedRevision = 0;
        sLedgerLoaded = false;
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>([](s16 fileNum) {
        if (!OotmmSession_IsActive()) {
            return;
        }
        sLedger.Reset();
        sLedger.Save(LedgerPathFor(fileNum));
        sLedgerLoaded = true;
        sAppliedRevision = 0;
    });
}
