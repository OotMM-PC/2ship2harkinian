#include "OotmmItemApply.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "OotmmCustomItems.h"
#include "OotmmDungeons.h"
#include "OotmmFaroresWind.h"
#include "OotmmIpc.h"
#include "OotmmItemPresentation.h"
#include "OotmmRegionState.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmAppliedLedger.h>
#include <libultraship/bridge/OotmmItemGrant.h>
#include <libultraship/bridge/OotmmHealth.h>
#include <libultraship/bridge/OotmmMagic.h>
#include <libultraship/bridge/OotmmBombchu.h>
#include <libultraship/bridge/OotmmSharedFamilies.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
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
void func_80833B18(PlayState* play, Player* player, s32 arg2, f32 speed, f32 velocityY, s16 arg5,
                   s32 invincibilityTimer);
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
    { "HOOKSHOT_SHORT", { ITEM_LONGSHOT, ITEM_HOOKSHOT } },
    { "OCARINA_FAIRY", { ITEM_OCARINA_FAIRY, ITEM_OCARINA_OF_TIME } },
    { "NUT_UPGRADE", { ITEM_DEKU_NUT_UPGRADE_30, ITEM_DEKU_NUT_UPGRADE_40 } },
    { "STICK_UPGRADE", { ITEM_DEKU_STICK_UPGRADE_20, ITEM_DEKU_STICK_UPGRADE_30 } },
};

// Ammo pickups name their own size, e.g. MM_ARROWS_10.
const std::vector<std::pair<std::string_view, uint8_t>> kAmmoSlots = {
    { "ARROW", ITEM_BOW },         { "BOMBCHU", ITEM_BOMBCHU },
    { "BOMB", ITEM_BOMB },         { "NUT", ITEM_DEKU_NUT },
    { "STICK", ITEM_DEKU_STICK },
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
Ship::OotmmSharedResourceState sSharedResources;

uint32_t CollectedSmallKeys(const DungeonSlot& dungeon) {
    const uint32_t max = static_cast<uint32_t>(OotmmDungeons_MaxSmallKeys(dungeon.Index));
    const std::string code(dungeon.Code);
    if (sLedger.Applied("MM_SKELETON_KEY") != 0 || sLedger.Applied("SHARED_SKELETON_KEY") != 0 ||
        sLedger.Applied("MM_KEY_RING_" + code) != 0) {
        return max;
    }
    return std::min(max, sLedger.Applied("MM_SMALL_KEY_" + code));
}

void ApplySmallKeyItem(const std::string& itemId, uint32_t count) {
    std::vector<uint32_t> before;
    for (const auto& dungeon : kDungeons) {
        before.push_back(CollectedSmallKeys(dungeon));
    }
    sLedger.SetApplied(itemId, count);
    for (size_t i = 0; i < kDungeons.size(); ++i) {
        const uint32_t after = CollectedSmallKeys(kDungeons[i]);
        if (after > before[i]) {
            s8& held = gSaveContext.save.saveInfo.inventory.dungeonKeys[kDungeons[i].Index];
            held = static_cast<s8>((held < 0 ? 0 : held) + (after - before[i]));
        }
    }
}

bool FamilyEnabled(const Ship::OotmmSharedFamily& family) {
    if (!family.EnabledValue.empty()) {
        return OotmmSession_GetState().GetStringSetting(std::string(family.Setting), "") == family.EnabledValue;
    }
    return OotmmSession_GetState().GetBoolSetting(std::string(family.Setting), false);
}

bool AnySharedFamily() {
    for (const auto& family : Ship::OotmmSharedFamilies()) {
        if (FamilyEnabled(family)) {
            return true;
        }
    }
    return false;
}

bool SharedReward(const std::string& id) {
    for (const auto& family : Ship::OotmmSharedFamilies()) {
        if (FamilyEnabled(family) && family.IsReward(id)) {
            return true;
        }
    }
    return false;
}

bool LoadSharedResources(const nlohmann::json& saved) {
    if (!AnySharedFamily()) {
        return true;
    }
    const auto& boot = OotmmSession_GetState().GetBootConfig();
    return sSharedResources.Load(boot.SharedResources, saved, boot.SaveId, gSaveContext.fileNum);
}
std::map<std::string, std::string> sUnhandled;
uint64_t sAppliedRevision = 0;
bool sRetryMagicApplication = false;
bool sLedgerLoaded = false;
bool sLedgerValid = true;
std::chrono::steady_clock::time_point sLedgerRetryAt{};
std::unordered_map<std::string, bool> sReadSnapshots;

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
    if (sLedgerLoaded || std::chrono::steady_clock::now() < sLedgerRetryAt) {
        return;
    }
    sLedgerValid = false;
    sLedgerRetryAt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    // Older sessions shared one ledger between both games; adopt it once, then stay per-game.
    const bool legacyValid = sLedger.LoadLegacy(LedgerPath(), LegacyLedgerPath());
    nlohmann::json save;
    const auto file = SaveManager_GetFileName(gSaveContext.fileNum + 1);
    if (SaveManager_ReadSaveFile(file, save) == 0) {
        const auto selected = sReadSnapshots.find(file);
        const bool owlSave = selected == sReadSnapshots.end() ? gSaveContext.save.isOwlSave : selected->second;
        const char* section = owlSave ? "owlSave" : "newCycleSave";
        if (save.contains(section) && save[section].is_object()) {
            auto& snapshot = save[section];
            sLedgerValid = LoadSharedResources(snapshot.value("ootmmSharedResources", nlohmann::json(nullptr)));
            if (snapshot.contains("ootmmApplied")) {
                if (!sLedger.RestoreSnapshot(snapshot["ootmmApplied"])) {
                    sLedgerValid = false;
                    SPDLOG_ERROR("[OoTMM] Invalid applied-item snapshot in {}", section);
                }
            } else {
                if (!legacyValid) {
                    sLedgerValid = false;
                    SPDLOG_ERROR("[OoTMM] Invalid legacy applied ledger; delivery is suspended");
                    return;
                }
                snapshot["ootmmApplied"] = sLedger.GetValues();
                if (!SaveManager_WriteSaveFile(file, save)) {
                    sLedgerValid = false;
                    SPDLOG_ERROR("[OoTMM] Failed to initialize applied-item snapshot");
                    return;
                }
            }
        } else {
            SPDLOG_ERROR("[OoTMM] Missing applied-item save section {}", section);
        }
    } else {
        return;
    }
    if (sLedgerValid) {
        auto& player = gSaveContext.save.saveInfo.playerData;
        auto& inventory = gSaveContext.save.saveInfo.inventory;
        const auto health = Ship::ResolveOotmmSavedHealth(
            player.healthCapacity, player.health, inventory.questItems >> 28,
            uint64_t(sLedger.Applied("MM_HEART_CONTAINER")) + sLedger.Applied("SHARED_HEART_CONTAINER"),
            uint64_t(sLedger.Applied("MM_HEART_PIECE")) + sLedger.Applied("SHARED_HEART_PIECE"));
        if (!health) {
            sLedgerValid = false;
            SPDLOG_ERROR("[OoTMM] Applied health rewards exceed native save bounds; delivery is suspended");
        } else {
            player.healthCapacity = health->Capacity;
            player.health += health->Healing;
            inventory.questItems = (inventory.questItems & 0x0fffffffU) | (uint32_t(health->Pieces) << 28);
        }
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

std::vector<uint8_t> LadderTiers(const Ship::OotmmGrantOp& op) {
    const std::string_view suffix = op.Slot;
    const auto& state = OotmmSession_GetState();
    const bool shared = op.Game == Ship::OotmmGrantGame::Shared;
    if (suffix == "OCARINA") {
        if (state.GetBoolSetting("fairyOcarinaMm", false)) {
            return FindLadder("OCARINA_FAIRY")->Tiers;
        }
        if (shared) {
            return { ITEM_NONE, ITEM_OCARINA_OF_TIME };
        }
        return { ITEM_OCARINA_OF_TIME };
    }
    if (suffix == "HOOKSHOT") {
        if (state.GetBoolSetting("shortHookshotMm", false)) {
            return FindLadder("HOOKSHOT_SHORT")->Tiers;
        }
        return { ITEM_HOOKSHOT };
    }
    if (suffix == "WALLET") {
        std::vector<uint8_t> tiers;
        if (state.GetBoolSetting("childWallets", false)) {
            tiers.push_back(ITEM_WALLET_DEFAULT);
        }
        tiers.push_back(ITEM_WALLET_ADULT);
        tiers.push_back(ITEM_WALLET_GIANT);
        return tiers;
    }
    if (suffix == "SWORD" && !shared && state.GetStringSetting("progressiveGFS", "separate") == "progressive") {
        return { ITEM_SWORD_KOKIRI, ITEM_SWORD_RAZOR, ITEM_SWORD_GILDED, ITEM_SWORD_GREAT_FAIRY };
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
    if (suffix.find("SILVER") != std::string_view::npos) return 100;
    if (suffix.find("GOLD") != std::string_view::npos) return 200;
    if (suffix.find("HUGE") != std::string_view::npos) return 200;
    if (suffix.find("RAINBOW") != std::string_view::npos) return 999;
    return 1;
}

void RefreshTradeButtons(uint8_t slot, uint8_t removed = ITEM_NONE);

int ProgressiveSlot(uint8_t item) {
    switch (item) {
        case ITEM_OCARINA_FAIRY:
        case ITEM_OCARINA_OF_TIME:
            return SLOT_OCARINA;
        case ITEM_LONGSHOT:
        case ITEM_HOOKSHOT:
            return SLOT_HOOKSHOT;
        default:
            return -1;
    }
}

bool Downgrades(uint8_t item) {
    const auto& items = gSaveContext.save.saveInfo.inventory.items;
    return (item == ITEM_OCARINA_FAIRY && items[SLOT_OCARINA] == ITEM_OCARINA_OF_TIME) ||
           (item == ITEM_LONGSHOT && items[SLOT_HOOKSHOT] == ITEM_HOOKSHOT);
}

void ApplyOnce(PlayState* play, const std::string& key, uint8_t item) {
    sLedger.SetApplied(key, 1);
    if (item == ITEM_NONE || Downgrades(item)) {
        return;
    }
    if (const int slot = ProgressiveSlot(item); slot >= 0) {
        gSaveContext.save.saveInfo.inventory.items[slot] = item;
        RefreshTradeButtons(slot);
        SPDLOG_INFO("[OoTMM grant] gave {} (item {})", key, static_cast<int>(item));
        return;
    }
    // gItemSlots only covers inventory items, so SLOT()/INV_CONTENT() are unusable here.
    Item_Give(play, item);
    SPDLOG_INFO("[OoTMM grant] gave {} (item {})", key, static_cast<int>(item));
}

bool IsConsumable(uint8_t item) {
    return item == ITEM_DEKU_STICK || item == ITEM_DEKU_NUT || item == ITEM_BOMB;
}

void GiveUnits(PlayState* play, const std::string& itemId, uint8_t item, uint32_t already, uint32_t count) {
    for (uint32_t unit = already; unit < count && unit - already < 255; unit++) {
        Item_Give(play, item);
    }
    sLedger.SetApplied(itemId, count);
    SPDLOG_INFO("[OoTMM grant] gave {} x{} (item {})", itemId, count - already, static_cast<int>(item));
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

std::string ConsumedTradeKey(uint8_t item) {
    return "MM_TRADE_CONSUMED:" + std::to_string(item);
}

bool IsTradeGrantOp(const Ship::OotmmGrantOp& op) {
    switch (op.Kind) {
        case Ship::OotmmGrantKind::Equipment:
        case Ship::OotmmGrantKind::Upgrade:
        case Ship::OotmmGrantKind::Mask:
        case Ship::OotmmGrantKind::Bottle:
            return Ship::OotmmItemGrant::AppliesTo(op, Ship::OotmmGrantGame::Mm) &&
                   LadderTiers(op).empty();
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
    if (!sLedgerValid) {
        return {};
    }
    const uint8_t current = gSaveContext.save.saveInfo.inventory.items[slot];
    // Grants overwrite the slot, so remember natively obtained occupants before they vanish.
    if (std::find(candidates->begin(), candidates->end(), current) != candidates->end() &&
        !sLedger.Has(NativeTradeKey(current)) && !sLedger.Has(ConsumedTradeKey(current))) {
        sLedger.SetApplied(NativeTradeKey(current), 1);
        sLedger.Save(LedgerPath());
    }

    std::vector<uint8_t> owned;
    for (const uint8_t item : *candidates) {
        if (sLedger.Has(ConsumedTradeKey(item))) {
            continue;
        }
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

void RefreshTradeButtons(uint8_t slot, uint8_t removed) {
    const auto item = gSaveContext.save.saveInfo.inventory.items[slot];
    auto& equips = gSaveContext.save.saveInfo.equips;
    auto& dpad = gSaveContext.save.shipSaveInfo.dpadEquips;
    for (int form = 0; form < 4; ++form) {
        for (int button = EQUIP_SLOT_C_LEFT; button <= EQUIP_SLOT_C_RIGHT; ++button) {
            if (removed != ITEM_NONE && equips.buttonItems[form][button] == removed) {
                equips.buttonItems[form][button] = ITEM_NONE;
                equips.cButtonSlots[form][button] = SLOT_NONE;
            } else if (equips.cButtonSlots[form][button] == slot) {
                equips.buttonItems[form][button] = item;
            }
        }
        for (int button = 0; button < 4; ++button) {
            if (removed != ITEM_NONE && dpad.dpadItems[form][button] == removed) {
                dpad.dpadItems[form][button] = ITEM_NONE;
                dpad.dpadSlots[form][button] = SLOT_NONE;
            } else if (dpad.dpadSlots[form][button] == slot) {
                dpad.dpadItems[form][button] = item;
            }
        }
    }
    if (gPlayState != nullptr) {
        for (int button = EQUIP_SLOT_C_LEFT; button <= EQUIP_SLOT_C_RIGHT; ++button) {
            Interface_LoadItemIconImpl(gPlayState, button);
        }
        for (int button = EQUIP_SLOT_D_RIGHT; button <= EQUIP_SLOT_D_UP; ++button) {
            Interface_Dpad_LoadItemIconImpl(gPlayState, button);
        }
    }
}

uint32_t BombchuBagTier() {
    const uint64_t bags = uint64_t(sLedger.Applied("MM_BOMBCHU_BAG")) +
                          sLedger.Applied("SHARED_BOMBCHU_BAG");
    return static_cast<uint32_t>(std::min<uint64_t>(3,
        std::max<uint64_t>(bags, sLedger.Applied("native:bombchuBagFirst"))));
}

int32_t BombchuCapacity() {
    const auto tier = BombchuBagTier();
    return Ship::ResolveOotmmBombchuCapacity(
        OotmmSession_GetState().GetStringSetting("bombchuBehaviorMm", "bombBag"),
        CUR_UPG_VALUE(UPG_BOMB_BAG), tier).value_or(0);
}

constexpr int kFirstMmDungeon = 8;
constexpr int kLastBossDungeon = 12;

void ApplyBossClears(uint32_t mask) {
    uint32_t applied = sLedger.Applied("native:bossClearsApplied");
    for (int dungeon = kFirstMmDungeon; dungeon <= kLastBossDungeon; ++dungeon) {
        const uint32_t bit = 1u << dungeon;
        if ((mask & bit) && !(applied & bit)) {
            OotmmRegionState_ApplyBossClear(dungeon);
            applied |= bit;
        }
    }
    sLedger.SetApplied("native:bossClearsApplied", applied);
}

uint32_t SharedValueLimit(std::string_view key) {
    if (OotmmFaroresWind_IsSharedKey(key)) {
        return 2;
    }
    if (!Ship::OotmmTrapForSetting(key).empty()) {
        return 65535;
    }
    if (key == "bossClears") {
        return 0xFFFF;
    }
    if (key == "bombchuOwned" || key == "bombchuBagFirst" || key == "keg") {
        return 1;
    }
    if (key == "rupees") {
        return 9999;
    }
    if (key == "health" || key == "healthCapacity") {
        return 0x140;
    }
    if (key == "heartPieces") {
        return 3;
    }
    if (key == "magic") {
        return 0x60;
    }
    return 99;
}

void WriteSharedValue(std::string_view key, uint32_t value) {
    auto& playerData = gSaveContext.save.saveInfo.playerData;
    auto& inventory = gSaveContext.save.saveInfo.inventory;
    if (OotmmFaroresWind_IsSharedKey(key)) {
        OotmmFaroresWind_WriteShared(key, value);
    } else if (const auto trap = Ship::OotmmTrapForSetting(key); !trap.empty()) {
        sLedger.SetApplied(std::string(trap), std::max(sLedger.Applied(std::string(trap)), value));
    } else if (key == "bossClears") {
        const uint32_t mask = sLedger.Applied("native:bossClears") | value;
        sLedger.SetApplied("native:bossClears", mask);
        ApplyBossClears(mask);
    } else if (key == "bombchu") {
        inventory.ammo[SLOT_BOMBCHU] = static_cast<int8_t>(value);
    } else if (key == "bombchuOwned") {
        inventory.items[SLOT_BOMBCHU] = value ? ITEM_BOMBCHU : ITEM_NONE;
    } else if (key == "bombchuBagFirst") {
        sLedger.SetApplied("native:bombchuBagFirst", value);
    } else if (key == "rupees") {
        playerData.rupees = static_cast<s16>(value);
        gSaveContext.rupeeAccumulator = 0;
    } else if (key == "health") {
        playerData.health = static_cast<s16>(value);
        gSaveContext.healthAccumulator = 0;
    } else if (key == "healthCapacity") {
        playerData.healthCapacity = static_cast<s16>(value);
    } else if (key == "heartPieces") {
        inventory.questItems = (inventory.questItems & 0x0FFFFFFF) | (value << 28);
    } else if (key == "magic") {
        playerData.magic = static_cast<s8>(value);
    } else if (key == "arrows") {
        inventory.ammo[SLOT_BOW] = static_cast<int8_t>(value);
    } else if (key == "bombs") {
        inventory.ammo[SLOT_BOMB] = static_cast<int8_t>(value);
    } else if (key == "nuts") {
        inventory.ammo[SLOT_DEKU_NUT] = static_cast<int8_t>(value);
    } else if (key == "sticks") {
        inventory.ammo[SLOT_DEKU_STICK] = static_cast<int8_t>(value);
    } else if (key == "seeds") {
        OotmmCustomItems_SetSlingshotAmmo(static_cast<int32_t>(value));
    } else if (key == "keg") {
        inventory.ammo[SLOT_POWDER_KEG] = static_cast<int8_t>(value);
    }
}

bool RestoreSharedResources() {
    if (!AnySharedFamily() || !sSharedResources.NeedsRestore()) {
        return true;
    }
    const auto& snapshot = *sSharedResources.Current();
    for (const auto& [key, value] : snapshot.Values) {
        if (value > SharedValueLimit(key)) {
            return false;
        }
    }
    for (const auto& [id, count] : snapshot.Applied) {
        if (!SharedReward(id) || OotmmIpc_GetInventory().Count(id) < count) {
            return false;
        }
    }
    std::vector<std::string> previous;
    for (const auto& [id, count] : sLedger.GetValues()) {
        if (SharedReward(id)) {
            previous.push_back(id);
        }
    }
    for (const auto& id : previous) {
        sLedger.Erase(id);
    }
    for (const auto& [id, count] : snapshot.Applied) {
        sLedger.SetApplied(id, count);
    }
    for (const auto& family : Ship::OotmmSharedFamilies()) {
        if (!FamilyEnabled(family)) {
            continue;
        }
        for (const auto key : family.Values) {
            if (const auto value = snapshot.Values.find(std::string(key)); value != snapshot.Values.end()) {
                WriteSharedValue(key, value->second);
            }
        }
    }
    sSharedResources.Restored();
    return true;
}

struct WalletState {
    bool HasWallet = true;
    uint8_t Level = 0;
    bool Bottomless = false;
    uint32_t Owned = 0;
};

WalletState sWallet;

WalletState ComputeWallet() {
    const auto& state = OotmmSession_GetState();
    const auto& inventory = OotmmIpc_GetInventory();
    WalletState wallet;
    wallet.Owned = inventory.Count("MM_WALLET") + inventory.Count("SHARED_WALLET");
    uint32_t upgrades = wallet.Owned;
    if (state.GetBoolSetting("childWallets", false)) {
        wallet.HasWallet = upgrades > 0;
        upgrades = upgrades > 0 ? upgrades - 1 : 0;
    }
    const bool colossal = state.GetBoolSetting("colossalWallets", false);
    const uint32_t maxLevel = colossal ? 3 : 2;
    wallet.Level = static_cast<uint8_t>(std::min(upgrades, maxLevel));
    wallet.Bottomless = colossal && state.GetBoolSetting("bottomlessWallets", false) && upgrades > maxLevel;
    return wallet;
}

void RefreshWallet() {
    sWallet = ComputeWallet();
    gUpgradeCapacities[UPG_WALLET][0] = sWallet.HasWallet ? 99 : 0;
    gUpgradeCapacities[UPG_WALLET][3] = sWallet.Bottomless ? 9999 : 999;
    if (CUR_UPG_VALUE(UPG_WALLET) != sWallet.Level) {
        Inventory_ChangeUpgrade(UPG_WALLET, sWallet.Level);
    }
    auto& rupees = gSaveContext.save.saveInfo.playerData.rupees;
    if (rupees > CUR_CAPACITY(UPG_WALLET)) {
        rupees = CUR_CAPACITY(UPG_WALLET);
    }
}

void ApplyWallet() {
    RefreshWallet();
    const bool fill = OotmmSession_GetState().GetBoolSetting("fillWallets", false);
    for (uint32_t tier = 0; tier < sWallet.Owned; tier++) {
        const std::string key = "WALLET_TIER:" + std::to_string(tier);
        if (sLedger.Has(key)) {
            continue;
        }
        sLedger.SetApplied(key, 1);
        if (fill) {
            Rupees_ChangeBy(static_cast<s16>(CUR_CAPACITY(UPG_WALLET)));
        }
    }
}

void ApplyOne(PlayState* play, const std::string& itemId, uint32_t count) {
    if (count == 0) {
        return;
    }
    if (OotmmCustomItems_IdForItemId(itemId.c_str()) == ITEM_OOTMM_SLINGSHOT &&
        OotmmCustomItems_OwnedCount(ITEM_OOTMM_SLINGSHOT) != 0) {
        const uint32_t already = sLedger.Applied(itemId);
        for (uint32_t tier = already + 1; tier <= count; ++tier) {
            OotmmCustomItems_AddSlingshotSeeds(OotmmCustomItems_SlingshotTierMax(tier));
        }
        if (count > already) {
            sLedger.SetApplied(itemId, count);
        }
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
                if (op.Slot == "WALLET") {
                    ApplyWallet();
                    break;
                }
                if (op.Slot == "BOMBCHU_BAG") {
                    gSaveContext.save.saveInfo.inventory.items[SLOT_BOMBCHU] = ITEM_BOMBCHU;
                    if (count > already) {
                        sLedger.SetApplied(itemId, count);
                        gSaveContext.save.saveInfo.inventory.ammo[SLOT_BOMBCHU] = BombchuCapacity();
                    }
                    break;
                }
                if (op.Slot == "DEFENSE_UPGRADE") {
                    if (count > already) {
                        const auto health = Ship::ResolveOotmmHealth(
                            gSaveContext.save.saveInfo.playerData.healthCapacity, gSaveContext.save.saveInfo.playerData.health, gSaveContext.save.saveInfo.inventory.questItems >> 28, 0, 0, 20);
                        if (!health) {
                            sUnhandled[itemId] = "Invalid native health for defense reward";
                            break;
                        }
                        gSaveContext.save.saveInfo.playerData.doubleDefense = 1;
                        gSaveContext.save.saveInfo.inventory.defenseHearts = 20;
                        Health_ChangeBy(play, health->Healing);
                        sLedger.SetApplied(itemId, count);
                    }
                    break;
                }
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
                if (op.Slot == "STRENGTH") {
                    const size_t tier = std::min<size_t>(op.Amount, 3);
                    if (tier == 0) {
                        break;
                    }
                    const std::string key = itemId + ":" + std::to_string(tier - 1);
                    if (!sLedger.Has(key)) {
                        sLedger.SetApplied(key, 1);
                        if (tier > CUR_UPG_VALUE(UPG_STRENGTH)) {
                            Inventory_ChangeUpgrade(UPG_STRENGTH, static_cast<s16>(tier));
                        }
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
                const std::vector<uint8_t> tiers = LadderTiers(op);
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
                    } else if (found->second >= ITEM_MOONS_TEAR && found->second <= ITEM_PENDANT_OF_MEMORIES) {
                        if (count > already) {
                            OotmmItemApply_RecordTradeGrant(found->second);
                            auto& current = gSaveContext.save.saveInfo.inventory.items[SLOT(found->second)];
                            if (current == ITEM_NONE) {
                                current = found->second;
                            }
                            sLedger.SetApplied(itemId, count);
                        }
                    } else if (IsConsumable(found->second)) {
                        if (count > already) {
                            GiveUnits(play, itemId, found->second, already, count);
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
                if (op.Slot == "BOMBCHU" || op.Slot.starts_with("BOMBCHU_")) {
                    if (OotmmSession_GetState().GetStringSetting("bombchuBehaviorMm", "bombBag") == "bagFirst") {
                        sLedger.SetApplied("native:bombchuBagFirst", 1);
                    }
                    const auto capacity = BombchuCapacity();
                    if (capacity > 0) {
                        gSaveContext.save.saveInfo.inventory.items[SLOT_BOMBCHU] = ITEM_BOMBCHU;
                        const uint64_t amount = uint64_t(count - already) * TrailingNumber(op.Slot, 1);
                        gSaveContext.save.saveInfo.inventory.ammo[SLOT_BOMBCHU] = static_cast<int8_t>(std::min<uint64_t>(
                            capacity, std::max<int32_t>(0, gSaveContext.save.saveInfo.inventory.ammo[SLOT_BOMBCHU]) + amount));
                    }
                    sLedger.SetApplied(itemId, count);
                    break;
                }
                if (op.Slot.rfind("DEKU_SEEDS", 0) == 0) {
                    OotmmCustomItems_AddSlingshotSeeds(
                        static_cast<int32_t>((count - already) * TrailingNumber(op.Slot, 1)));
                    sLedger.SetApplied(itemId, count);
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
                    const uint32_t delta = count - already;
                    const bool container = op.Slot == "HEART_CONTAINER";
                    const bool piece = op.Slot == "HEART_PIECE";
                    const auto result = Ship::ResolveOotmmHealth(
                        gSaveContext.save.saveInfo.playerData.healthCapacity, gSaveContext.save.saveInfo.playerData.health, gSaveContext.save.saveInfo.inventory.questItems >> 28,
                        container ? delta : 0, piece ? delta : 0, (!container && !piece) ? delta : 0);
                    if (!result) {
                        sUnhandled[itemId] = "Health reward exceeds native save bounds";
                        break;
                    }
                    gSaveContext.save.saveInfo.playerData.healthCapacity = result->Capacity;
                    gSaveContext.save.saveInfo.inventory.questItems = (gSaveContext.save.saveInfo.inventory.questItems & 0x0fffffffU) |
                        (uint32_t(result->Pieces) << 28);
                    Health_ChangeBy(play, result->Healing);
                    sLedger.SetApplied(itemId, count);
                }
                break;
            }
            case Ship::OotmmGrantKind::Magic: {
                if (op.Slot == "MAGIC_UPGRADE") {
                    const auto& inventory = OotmmIpc_GetInventory();
                    auto& playerData = gSaveContext.save.saveInfo.playerData;
                    const auto upgrade = Ship::ResolveOotmmMagicUpgrade(
                        inventory.Count("MM_MAGIC_UPGRADE"), inventory.Count("SHARED_MAGIC_UPGRADE"),
                        count, already, playerData.isMagicAcquired, playerData.isDoubleMagicAcquired);
                    if (upgrade.Refill) {
                        playerData.isMagicAcquired = true;
                        playerData.isDoubleMagicAcquired = upgrade.Tier >= 2;
                        playerData.magicLevel = 0;
                        playerData.magic = upgrade.Tier * MAGIC_NORMAL_METER;
                        gSaveContext.magicFillTarget = playerData.magic;
                        sLedger.SetApplied(itemId, count);
                    }
                    break;
                }
                if (count <= already) {
                    break;
                }
                const auto& inventory = OotmmIpc_GetInventory();
                const auto& playerData = gSaveContext.save.saveInfo.playerData;
                const bool ownsMagic = inventory.Count("MM_MAGIC_UPGRADE") != 0 ||
                                       inventory.Count("SHARED_MAGIC_UPGRADE") != 0;
                if ((!playerData.isMagicAcquired && ownsMagic) ||
                    (playerData.isMagicAcquired &&
                     (playerData.magicLevel == 0 ||
                      gSaveContext.magicCapacity != (playerData.isDoubleMagicAcquired ?
                                                         MAGIC_DOUBLE_METER : MAGIC_NORMAL_METER) ||
                      gSaveContext.magicState == MAGIC_STATE_STEP_CAPACITY ||
                      gSaveContext.magicState == MAGIC_STATE_FILL))) {
                    sRetryMagicApplication = true;
                    break;
                }
                Magic_Add(play, Ship::ResolveOotmmMagicJarAmount(
                    Ship::OotmmGrantGame::Mm, OotmmSession_GetState().GetBoolSetting("sharedMagic", false),
                    op.Slot == "MAGIC_JAR_LARGE", count - already));
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
                    if (count > already) {
                        ApplySmallKeyItem(itemId, count);
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
            case Ship::OotmmGrantKind::SmallKeyRing:
            case Ship::OotmmGrantKind::SkeletonKey: {
                if (already == 0) {
                    ApplySmallKeyItem(itemId, 1);
                }
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
                if ((itemId == "MM_SONG_GORON_HALF" || itemId == "SHARED_SONG_GORON_HALF") &&
                    OotmmSession_GetState().GetStringSetting("progressiveGoronLullabyMm", "progressive") ==
                        "progressive") {
                    for (uint32_t unit = already; unit < count; unit++) {
                        SET_QUEST_ITEM(CHECK_QUEST_ITEM(QUEST_SONG_LULLABY_INTRO) ? QUEST_SONG_LULLABY
                                                                                  : QUEST_SONG_LULLABY_INTRO);
                    }
                    sLedger.SetApplied(itemId, count);
                    break;
                }
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

constexpr u32 kTrapBlockingStates = PLAYER_STATE1_4 | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD | PLAYER_STATE1_200 |
                                    PLAYER_STATE1_400 | PLAYER_STATE1_800000;

bool InShop(Player* player) {
    const Actor* actor = player->interactRangeActor;
    return player->getItemId != GI_NONE && actor != nullptr &&
           (actor->id == ACTOR_EN_OSSAN || actor->id == ACTOR_EN_TRT || actor->id == ACTOR_EN_SOB1);
}

void HurtForTrap(Player* player, s32 response, f32 speed, f32 yVelocity, s16 yaw) {
    player->actor.colChkInfo.damage = 0;
    func_80833B18(gPlayState, player, response, speed, yVelocity, yaw, 20);
}

bool ApplyTrap(size_t type, Player* player) {
    auto& playerData = gSaveContext.save.saveInfo.playerData;
    switch (type) {
        case 0:
            HurtForTrap(player, 3, 0, 0, 0);
            return true;
        case 1:
            if (player->transformation == PLAYER_FORM_GORON ||
                OotmmCustomItems_EquippedTunic() == OOTMM_TUNIC_GORON) {
                return true;
            }
            HurtForTrap(player, 0, 0, 0, 0);
            player->bodyIsBurning = true;
            for (int part = 0; part < PLAYER_BODYPART_MAX; part++) {
                player->bodyFlameTimers[part] = Rand_S16Offset(0, 200);
            }
            return true;
        case 2:
            HurtForTrap(player, 4, 0, 0, 0);
            return true;
        case 3:
            playerData.health = 1;
            HurtForTrap(player, 0, 0, 0, 0);
            return true;
        case 4:
            playerData.magic = 0;
            CLEAR_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
            HurtForTrap(player, 0, 0, 0, 0);
            return true;
        case 5:
            HurtForTrap(player, 1, Rand_ZeroFloat(20.0f), Rand_ZeroFloat(15.0f), static_cast<s16>(Rand_Next() & 0xFFFF));
            return true;
        case 6:
            Rupees_ChangeBy(-10);
            return true;
        default:
            return false;
    }
}

void UpdateTraps() {
    if (gPlayState == nullptr || !OotmmSession_IsActive() || !sLedgerValid ||
        gSaveContext.gameMode != GAMEMODE_NORMAL || gPlayState->pauseCtx.state != 0 ||
        gPlayState->msgCtx.msgMode != MSGMODE_NONE || OotmmItemPresentation_IsPending()) {
        return;
    }
    const auto& inventory = OotmmIpc_GetInventory();
    if (inventory.GetRevision() != sAppliedRevision) {
        return;
    }
    for (size_t type = 0; type < Ship::kOotmmTraps.size(); type++) {
        const std::string id(Ship::kOotmmTraps[type].first);
        const uint32_t applied = sLedger.Applied(id);
        if (inventory.Count(id) <= applied) {
            continue;
        }
        Player* player = GET_PLAYER(gPlayState);
        if (player == nullptr || (player->stateFlags1 & kTrapBlockingStates) || player->invincibilityTimer != 0 ||
            InShop(player)) {
            return;
        }
        if (ApplyTrap(type, player)) {
            sLedger.SetApplied(id, applied + 1);
            sLedger.Save(LedgerPath());
        }
        return;
    }
}

} // namespace

extern "C" int32_t OotmmItemApply_BombchuCapacity(void) {
    if (!OotmmSession_IsActive()) {
        return CUR_CAPACITY(UPG_BOMB_BAG);
    }
    EnsureLedger();
    return sLedgerValid ? BombchuCapacity() : 0;
}

extern "C" int32_t OotmmItemApply_SelectBombDrop(int32_t bombDrop, int32_t bombchuDrop) {
    if (!OotmmSession_IsActive()) {
        return bombDrop;
    }
    EnsureLedger();
    if (!sLedgerValid) {
        return -1;
    }
    switch (Ship::ResolveOotmmBombDrop(CUR_UPG_VALUE(UPG_BOMB_BAG) > 0, BombchuBagTier() > 0,
        std::max<int32_t>(0, gSaveContext.save.saveInfo.inventory.ammo[SLOT_BOMB]),
        std::max<int32_t>(0, gSaveContext.save.saveInfo.inventory.ammo[SLOT_BOMBCHU]))) {
        case Ship::OotmmBombDrop::None: return -1;
        case Ship::OotmmBombDrop::Bomb: return bombDrop;
        case Ship::OotmmBombDrop::Bombchu: return bombchuDrop;
        case Ship::OotmmBombDrop::Random: return Rand_ZeroOne() < 0.5f ? bombchuDrop : bombDrop;
    }
    return -1;
}

extern "C" void OotmmItemApply_AddBombchuAmmo(int32_t amount) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    const int32_t capacity = OotmmItemApply_BombchuCapacity();
    if (capacity == 0 || !sLedgerValid) {
        return;
    }
    if (amount > 0) {
        gSaveContext.save.saveInfo.inventory.items[SLOT_BOMBCHU] = ITEM_BOMBCHU;
    }
    gSaveContext.save.saveInfo.inventory.ammo[SLOT_BOMBCHU] = static_cast<int8_t>(std::clamp<int64_t>(
        int64_t(gSaveContext.save.saveInfo.inventory.ammo[SLOT_BOMBCHU]) + amount, 0, capacity));
}

void OotmmItemApply_ResetLedgerForNewSave() {
    if (!OotmmSession_IsActive()) {
        return;
    }
    sLedger.Reset();
    sLedgerRetryAt = {};
    sReadSnapshots[SaveManager_GetFileName(gSaveContext.fileNum + 1)] = false;
    sLedgerValid = LoadSharedResources(nullptr);
    sLedger.Save(LedgerPath());
    sLedgerLoaded = true;
    sAppliedRevision = 0;
}

int16_t OotmmWallet_ScaleRupees(int16_t amount) {
    if (amount <= 0 || !OotmmSession_IsActive() || !OotmmSession_GetState().GetBoolSetting("rupeeScaling", false)) {
        return amount;
    }
    const int extra = (CUR_UPG_VALUE(UPG_WALLET) == 3 ? 1 : 0) + (sWallet.Bottomless ? 1 : 0);
    int32_t value = amount;
    if (extra >= 1) {
        value *= 2;
    }
    if (extra >= 2) {
        value *= 10;
    }
    return static_cast<int16_t>(std::min(value, 9999));
}

int32_t OotmmWallet_HasWallet(void) {
    return !OotmmSession_IsActive() || sWallet.HasWallet;
}

int32_t OotmmWallet_Bottomless(void) {
    return OotmmSession_IsActive() && sWallet.Bottomless;
}

void OotmmItemApply_Reconcile() {
    if (gPlayState == nullptr || !OotmmSession_IsActive() || OotmmSession_HasPendingInitialSave()) {
        return;
    }
    const auto& inventory = OotmmIpc_GetInventory();
    if (inventory.GetRevision() == sAppliedRevision) {
        return;
    }
    EnsureLedger();
    if (!sLedgerValid) {
        return;
    }
    sRetryMagicApplication = false;
    RefreshWallet();
    for (const auto& [itemId, count] : inventory.GetValues()) {
        if (Ship::OotmmItemGrant::SuffixOf(itemId) == "BOMB_BAG" ||
            (Ship::OotmmItemGrant::GameOf(itemId) == Ship::OotmmGrantGame::Shared && !SharedReward(itemId))) {
            ApplyOne(gPlayState, itemId, count);
        }
    }
    if (!RestoreSharedResources()) {
        return;
    }
    size_t owned = 0;
    for (const auto& [itemId, count] : inventory.GetValues()) {
        if (count > 0) {
            owned++;
        }
        ApplyOne(gPlayState, itemId, count);
    }
    if (!sRetryMagicApplication) {
        sAppliedRevision = inventory.GetRevision();
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
        sLedger.Erase(ConsumedTradeKey(item));
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

extern "C" void OotmmItemApply_RecordTradeGrant(uint8_t item) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    for (const uint8_t slot : { SLOT_TRADE_DEED, SLOT_TRADE_KEY_MAMA, SLOT_TRADE_COUPLE }) {
        const auto* candidates = TradeItemsForSlot(slot);
        if (std::find(candidates->begin(), candidates->end(), item) == candidates->end()) {
            continue;
        }
        OwnedTradeItems(slot);
        if (sLedgerValid) {
            sLedger.SetApplied(NativeTradeKey(item), 1);
            sLedger.Erase(ConsumedTradeKey(item));
            sLedger.Save(LedgerPath());
        }
        return;
    }
}

extern "C" int32_t OotmmItemApply_ConsumeTradeItem(uint8_t item, uint8_t slot) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    const auto* candidates = TradeItemsForSlot(slot);
    if (candidates == nullptr || std::find(candidates->begin(), candidates->end(), item) == candidates->end()) {
        return 0;
    }
    const auto owned = OwnedTradeItems(slot);
    if (!sLedgerValid || std::find(owned.begin(), owned.end(), item) == owned.end()) {
        return 1;
    }
    sLedger.SetApplied(ConsumedTradeKey(item), 1);
    const auto remaining = OwnedTradeItems(slot);
    auto& current = gSaveContext.save.saveInfo.inventory.items[slot];
    if (current == item) {
        current = remaining.empty() ? ITEM_NONE : remaining.front();
    }
    RefreshTradeButtons(slot, item);
    sLedger.Save(LedgerPath());
    return 1;
}

extern "C" int32_t OotmmItemApply_ToggleTradeItem(uint8_t slot) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    const auto owned = OwnedTradeItems(slot);
    if (owned.size() < 2) {
        return 0;
    }
    auto& current = gSaveContext.save.saveInfo.inventory.items[slot];
    const auto found = std::find(owned.begin(), owned.end(), current);
    current = found == owned.end() || found + 1 == owned.end() ? owned.front() : *(found + 1);
    RefreshTradeButtons(slot);
    return 1;
}

extern "C" void OotmmItemApply_RememberTradeItems(void) {
    if (OotmmSession_IsActive()) {
        for (const uint8_t slot : { SLOT_TRADE_DEED, SLOT_TRADE_KEY_MAMA, SLOT_TRADE_COUPLE }) {
            OwnedTradeItems(slot);
        }
    }
}

extern "C" void OotmmItemApply_ResetTradeCycle(void) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    EnsureLedger();
    if (!sLedgerValid) {
        return;
    }
    for (const uint8_t slot : { SLOT_TRADE_DEED, SLOT_TRADE_KEY_MAMA, SLOT_TRADE_COUPLE }) {
        for (const uint8_t item : *TradeItemsForSlot(slot)) {
            sLedger.Erase(ConsumedTradeKey(item));
        }
        const auto owned = OwnedTradeItems(slot);
        gSaveContext.save.saveInfo.inventory.items[slot] = owned.empty() ? ITEM_NONE : owned.front();
        RefreshTradeButtons(slot);
    }
    sLedger.Save(LedgerPath());
}

void OotmmItemApply_RefreshSaveSnapshot() {
    sLedgerLoaded = false;
    sLedgerRetryAt = {};
    sAppliedRevision = 0;
    EnsureLedger();
}

nlohmann::json OotmmItemApply_LegacySnapshot(int fileNum) {
    Ship::OotmmAppliedLedger ledger;
    if (!ledger.LoadLegacy(LedgerPathFor(fileNum),
            OotmmSession_GetState().GetBootConfig().StatePath + ".applied." + std::to_string(fileNum))) {
        return nullptr;
    }
    return ledger.GetValues();
}

void OotmmItemApply_NoteSaveRead(const std::string& fileName, bool owlSave) {
    sReadSnapshots[fileName] = owlSave;
}


uint32_t ReadSharedValue(const nlohmann::json& saveInfo, std::string_view key) {
    if (OotmmFaroresWind_IsSharedKey(key)) {
        return OotmmFaroresWind_ReadShared(key);
    }
    if (const auto trap = Ship::OotmmTrapForSetting(key); !trap.empty()) {
        return sLedger.Applied(std::string(trap));
    }
    if (key == "bossClears") {
        return sLedger.Applied("native:bossClears");
    }
    const auto& inventory = saveInfo.at("inventory");
    const auto& playerData = saveInfo.at("playerData");
    const auto ammo = [&](int slot) { return static_cast<uint32_t>(std::max(0, inventory.at("ammo").at(slot).get<int>())); };
    if (key == "bombchu") {
        return ammo(SLOT_BOMBCHU);
    }
    if (key == "bombchuOwned") {
        return inventory.at("items").at(SLOT_BOMBCHU) == ITEM_BOMBCHU ? 1u : 0u;
    }
    if (key == "bombchuBagFirst") {
        return sLedger.Applied("native:bombchuBagFirst");
    }
    if (key == "rupees" || key == "health" || key == "healthCapacity" || key == "magic") {
        return static_cast<uint32_t>(std::max(0, playerData.at(std::string(key)).get<int>()));
    }
    if (key == "heartPieces") {
        return (inventory.at("questItems").get<uint32_t>() >> 28) & 0xF;
    }
    if (key == "arrows") {
        return ammo(SLOT_BOW);
    }
    if (key == "bombs") {
        return ammo(SLOT_BOMB);
    }
    if (key == "nuts") {
        return ammo(SLOT_DEKU_NUT);
    }
    if (key == "sticks") {
        return ammo(SLOT_DEKU_STICK);
    }
    if (key == "seeds") {
        return static_cast<uint32_t>(OotmmCustomItems_SlingshotAmmo());
    }
    if (key == "keg") {
        return ammo(SLOT_POWDER_KEG);
    }
    return 0;
}

bool OotmmItemApply_CaptureSharedSnapshot(const nlohmann::json& saveInfo, nlohmann::json& snapshot) {
    snapshot = nullptr;
    if (!OotmmSession_IsActive() || !AnySharedFamily()) {
        return true;
    }
    EnsureLedger();
    if (!sLedgerValid) {
        return false;
    }
    const auto& boot = OotmmSession_GetState().GetBootConfig();
    std::map<std::string, uint32_t> applied;
    for (const auto& [id, count] : sLedger.GetValues()) {
        if (SharedReward(id)) {
            applied.emplace(id, count);
        }
    }
    std::map<std::string, uint32_t> values;
    for (const auto& family : Ship::OotmmSharedFamilies()) {
        if (FamilyEnabled(family)) {
            for (const auto key : family.Values) {
                values.emplace(std::string(key), ReadSharedValue(saveInfo, key));
            }
        }
    }
    return sSharedResources.Capture(boot.SaveId, gSaveContext.fileNum, "mm", values, applied, snapshot);
}

bool OotmmItemApply_Embed(nlohmann::json& save) {
    if (OotmmSession_HasPendingInitialSave()) {
        return false;
    }
    if (OotmmSession_IsActive() && gPlayState != nullptr) {
        EnsureLedger();
        if (!sLedgerValid) {
            return false;
        }
        nlohmann::json shared;
        if (!OotmmItemApply_CaptureSharedSnapshot(save.at("save").at("saveInfo"), shared)) {
            return false;
        }
        if (!shared.is_null()) {
            save["ootmmSharedResources"] = std::move(shared);
        }
        const auto& playerData = gSaveContext.save.saveInfo.playerData;
        auto& magic = save.at("save").at("saveInfo").at("playerData").at("magic");
        if (gSaveContext.magicState == MAGIC_STATE_STEP_CAPACITY ||
            gSaveContext.magicState == MAGIC_STATE_FILL) {
            magic = Ship::ResolveOotmmMagicSaveValue(magic.get<int16_t>(),
                gSaveContext.magicFillTarget, playerData.isMagicAcquired,
                playerData.isDoubleMagicAcquired, true);
        } else if (gSaveContext.isMagicRequested && gSaveContext.magicToAdd > 0 &&
            gSaveContext.magicCapacity == (playerData.isDoubleMagicAcquired ?
                                              MAGIC_DOUBLE_METER : MAGIC_NORMAL_METER)) {
            magic = Ship::ResolveOotmmMagicSaveValue(magic.get<int16_t>(),
                magic.get<int16_t>() + gSaveContext.magicToAdd, playerData.isMagicAcquired,
                playerData.isDoubleMagicAcquired, true);
        }
        save["ootmmApplied"] = sLedger.GetValues();
    }
    return true;
}

void OotmmItemApply_RecordBossClear(int dungeonId) {
    if (dungeonId < 0 || dungeonId > kLastBossDungeon) {
        return;
    }
    EnsureLedger();
    const uint32_t bit = 1u << dungeonId;
    sLedger.SetApplied("native:bossClears", sLedger.Applied("native:bossClears") | bit);
    if (dungeonId >= kFirstMmDungeon) {
        sLedger.SetApplied("native:bossClearsApplied", sLedger.Applied("native:bossClearsApplied") | bit);
    }
}

void OotmmItemApply_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(
        OotmmItemApply_Reconcile);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(UpdateTraps);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        sAppliedRevision = 0;
        sLedgerLoaded = false;
        sLedgerRetryAt = {};
        if (OotmmSession_IsActive()) {
            EnsureLedger();
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>([](s16 fileNum) {
        if (!OotmmSession_IsActive()) {
            return;
        }
        sLedger.Reset();
        sLedgerValid = LoadSharedResources(nullptr);
        sLedger.Save(LedgerPathFor(fileNum));
        sLedgerLoaded = true;
        sAppliedRevision = 0;
    });
}
