#include "OotmmCustomItems.h"

#include "CustomMessage/CustomMessage.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <array>
#include <string>
#include <string_view>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
#include "2s2h_assets.h"

#include "OotmmCustomItemsPlayer.h"

uint8_t ResourceMgr_FileExists(const char* path);
extern PlayState* gPlayState;
}

namespace {

struct CustomItem {
    uint8_t Item;
    std::string_view OotmmId;
    std::string_view SharedId;
    const char* Setting;
    const char* Icon;
    const char* NameTexture;
    int32_t Action;
};

constexpr std::array<CustomItem, OOTMM_CUSTOM_ITEM_COUNT> kItems = { {
    { ITEM_OOTMM_SPELL_WIND, "MM_SPELL_WIND", "SHARED_SPELL_WIND", "spellWindMm",
      "__OTR__textures/ot_icon_item_static/gItemIconFaroresWindTex",
      "__OTR__textures/ot_item_name_static/gFaroresWindItemNameENGTex", PLAYER_IA_OOTMM_SPELL_WIND },
    { ITEM_OOTMM_SPELL_LOVE, "MM_SPELL_LOVE", "SHARED_SPELL_LOVE", "spellLoveMm",
      "__OTR__textures/ot_icon_item_static/gItemIconNayrusLoveTex",
      "__OTR__textures/ot_item_name_static/gNayrusLoveItemNameENGTex", PLAYER_IA_OOTMM_SPELL_LOVE },
    { ITEM_OOTMM_SPELL_FIRE, "MM_SPELL_FIRE", "SHARED_SPELL_FIRE", "spellFireMm",
      "__OTR__textures/ot_icon_item_static/gItemIconDinsFireTex",
      "__OTR__textures/ot_item_name_static/gDinsFireItemNameENGTex", PLAYER_IA_OOTMM_SPELL_FIRE },
    { ITEM_OOTMM_BOOTS_IRON, "MM_BOOTS_IRON", "SHARED_BOOTS_IRON", "bootsIronMm",
      "__OTR__textures/ot_icon_item_static/gItemIconBootsIronTex",
      "__OTR__textures/ot_item_name_static/gIronBootsItemNameENGTex", PLAYER_IA_OOTMM_BOOTS_IRON },
    { ITEM_OOTMM_BOOTS_HOVER, "MM_BOOTS_HOVER", "SHARED_BOOTS_HOVER", "bootsHoverMm",
      "__OTR__textures/ot_icon_item_static/gItemIconBootsHoverTex",
      "__OTR__textures/ot_item_name_static/gHoverBootsItemNameENGTex", PLAYER_IA_OOTMM_BOOTS_HOVER },
    { ITEM_OOTMM_TUNIC_GORON, "MM_TUNIC_GORON", "SHARED_TUNIC_GORON", "tunicGoronMm",
      "__OTR__textures/ot_icon_item_static/gItemIconTunicGoronTex",
      "__OTR__textures/ot_item_name_static/gGoronTunicItemNameENGTex", PLAYER_IA_OOTMM_TUNIC_GORON },
    { ITEM_OOTMM_TUNIC_ZORA, "MM_TUNIC_ZORA", "SHARED_TUNIC_ZORA", "tunicZoraMm",
      "__OTR__textures/ot_icon_item_static/gItemIconTunicZoraTex",
      "__OTR__textures/ot_item_name_static/gZoraTunicItemNameENGTex", PLAYER_IA_OOTMM_TUNIC_ZORA },
    { ITEM_OOTMM_HAMMER, "MM_HAMMER", "SHARED_HAMMER", "hammerMm",
      "__OTR__textures/ot_icon_item_static/gItemIconHammerTex",
      "__OTR__textures/ot_item_name_static/gMegatonHammerItemNameENGTex", PLAYER_IA_OOTMM_HAMMER },
    { ITEM_OOTMM_BOOMERANG, "MM_BOOMERANG", "SHARED_BOOMERANG", "boomerangMm",
      "__OTR__textures/ot_icon_item_static/gItemIconBoomerangTex",
      "__OTR__textures/ot_item_name_static/gBoomerangItemNameENGTex", PLAYER_IA_OOTMM_BOOMERANG },
    { ITEM_OOTMM_SLINGSHOT, "MM_SLINGSHOT", "SHARED_SLINGSHOT", "slingshotMm",
      "__OTR__textures/ot_icon_item_static/gItemIconSlingshotTex",
      "__OTR__textures/ot_item_name_static/gFairySlingshotItemNameENGTex", PLAYER_IA_OOTMM_SLINGSHOT },
    { ITEM_OOTMM_RUTO_LETTER, "MM_BOTTLE_RUTO_LETTER", "SHARED_BOTTLE_RUTO_LETTER", nullptr,
      "__OTR__textures/ot_icon_item_static/gItemIconBottleRutosLetterTex",
      "__OTR__textures/ot_item_name_static/gRutosLetterItemNameENGTex", PLAYER_IA_OOTMM_BOTTLE_RUTO_LETTER },
    { ITEM_OOTMM_MASK_GERUDO, "MM_MASK_GERUDO", "SHARED_MASK_GERUDO", "gerudoMaskMm",
      "__OTR__textures/ot_icon_item_static/gItemIconMaskGerudoTex",
      "__OTR__textures/ot_item_name_static/gGerudoMaskItemNameENGTex", PLAYER_IA_OOTMM_MASK_GERUDO },
    { ITEM_OOTMM_MASK_SKULL, "MM_MASK_SKULL", "SHARED_MASK_SKULL", "skullMaskMm",
      "__OTR__textures/ot_icon_item_static/gItemIconMaskSkullTex",
      "__OTR__textures/ot_item_name_static/gSkullMaskItemNameENGTex", PLAYER_IA_OOTMM_MASK_SKULL },
    { ITEM_OOTMM_MASK_SPOOKY, "MM_MASK_SPOOKY", "SHARED_MASK_SPOOKY", "spookyMaskMm",
      "__OTR__textures/ot_icon_item_static/gItemIconMaskSpookyTex",
      "__OTR__textures/ot_item_name_static/gSpookyMaskItemNameENGTex", PLAYER_IA_OOTMM_MASK_SPOOKY },
} };

constexpr std::array<int32_t, OOTMM_MAGIC_SPELL_MAX> kSpellCosts = { { 12, 24, 12 } };

const CustomItem* Find(uint8_t item) {
    for (const auto& entry : kItems) {
        if (entry.Item == item) {
            return &entry;
        }
    }
    return nullptr;
}

uint32_t InventoryCount(const CustomItem& entry) {
    const auto& inventory = OotmmIpc_GetInventory();
    const uint32_t own = inventory.Count(std::string(entry.OotmmId));
    if (own != 0) {
        return own;
    }
    return entry.SharedId.empty() ? 0 : inventory.Count(std::string(entry.SharedId));
}

int16_t sSpellActorIds[OOTMM_MAGIC_SPELL_MAX] = {
    ACTOR_OOTMM_MAGIC_WIND,
    ACTOR_OOTMM_MAGIC_DARK,
    ACTOR_OOTMM_MAGIC_FIRE,
};

} // namespace

extern "C" int OotmmCustomItems_IsCustomItem(uint8_t item) {
    return item >= ITEM_OOTMM_FIRST && item < ITEM_OOTMM_MAX ? 1 : 0;
}

extern "C" int OotmmCustomItems_IsCustomSlot(uint8_t slot) {
    return slot >= SLOT_OOTMM_FIRST && slot < SLOT_OOTMM_MAX ? 1 : 0;
}

extern "C" uint8_t OotmmCustomItems_SlotOf(uint8_t item) {
    if (!OotmmCustomItems_IsCustomItem(item)) {
        return SLOT_NONE;
    }
    return static_cast<uint8_t>(SLOT_OOTMM_FIRST + (item - ITEM_OOTMM_FIRST));
}

extern "C" uint8_t OotmmCustomItems_ItemInSlot(uint8_t slot) {
    if (!OotmmCustomItems_IsCustomSlot(slot)) {
        return ITEM_NONE;
    }
    return static_cast<uint8_t>(ITEM_OOTMM_FIRST + (slot - SLOT_OOTMM_FIRST));
}

extern "C" const char* OotmmCustomItems_IconPath(uint8_t item) {
    const CustomItem* entry = Find(item);
    if (entry == nullptr || !ResourceMgr_FileExists(entry->Icon)) {
        return nullptr;
    }
    return entry->Icon;
}

extern "C" void* OotmmCustomItems_ButtonIcon(uint8_t item) {
    if (const char* icon = OotmmCustomItems_IconPath(item)) {
        return const_cast<char*>(icon);
    }
    if (item < ARRAY_COUNT(gItemIcons)) {
        return gItemIcons[item];
    }
    return const_cast<char*>(gEmptyTexture);
}

extern "C" const char* OotmmCustomItems_NameTexture(uint8_t item) {
    const CustomItem* entry = Find(item);
    if (entry == nullptr || !ResourceMgr_FileExists(entry->NameTexture)) {
        return nullptr;
    }
    return entry->NameTexture;
}

extern "C" uint8_t OotmmCustomItems_IdForItemId(const char* ootmmItemId) {
    if (ootmmItemId == nullptr) {
        return ITEM_NONE;
    }
    const std::string_view id = ootmmItemId;
    for (const auto& entry : kItems) {
        if (entry.OotmmId == id || (!entry.SharedId.empty() && entry.SharedId == id)) {
            return entry.Item;
        }
    }
    return ITEM_NONE;
}

extern "C" uint32_t OotmmCustomItems_OwnedCount(uint8_t item) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    const CustomItem* entry = Find(item);
    if (entry == nullptr ||
        (entry->Setting != nullptr && !OotmmSession_GetState().GetBoolSetting(entry->Setting, false))) {
        return 0;
    }
    return InventoryCount(*entry);
}

extern "C" int OotmmCustomItems_Owned(uint8_t item) {
    return OotmmCustomItems_OwnedCount(item) != 0 ? 1 : 0;
}

extern "C" int OotmmCustomItems_OwnedItemCount(void) {
    int count = 0;
    for (const auto& entry : kItems) {
        if (OotmmCustomItems_Owned(entry.Item)) {
            count++;
        }
    }
    return count;
}

extern "C" uint8_t OotmmCustomItems_OwnedItemAt(int index) {
    int seen = 0;
    for (const auto& entry : kItems) {
        if (!OotmmCustomItems_Owned(entry.Item)) {
            continue;
        }
        if (seen == index) {
            return entry.Item;
        }
        seen++;
    }
    return ITEM_NONE;
}

extern "C" int32_t OotmmCustomItems_ItemAction(uint8_t item) {
    const CustomItem* entry = Find(item);
    return entry != nullptr ? entry->Action : -1;
}

extern "C" int32_t OotmmCustomItems_ActionToMagicSpell(int32_t itemAction) {
    const int32_t spell = itemAction - PLAYER_IA_OOTMM_SPELL_WIND;
    return spell >= OOTMM_MAGIC_SPELL_WIND && spell < OOTMM_MAGIC_SPELL_MAX ? spell : OOTMM_MAGIC_SPELL_NONE;
}

extern "C" int32_t OotmmCustomItems_MagicSpellCost(int32_t magicSpell) {
    if (magicSpell < 0 || magicSpell >= OOTMM_MAGIC_SPELL_MAX) {
        return 0;
    }
    return kSpellCosts[static_cast<size_t>(magicSpell)];
}

extern "C" int32_t OotmmCustomItems_MagicSpellActorId(int32_t magicSpell) {
    if (magicSpell < 0 || magicSpell >= OOTMM_MAGIC_SPELL_MAX) {
        return -1;
    }
    return sSpellActorIds[magicSpell];
}

extern "C" int OotmmCustomItems_HasMagicUpgrade(void) {
    const auto& inventory = OotmmIpc_GetInventory();
    return inventory.Has("MM_MAGIC_UPGRADE") || inventory.Has("SHARED_MAGIC_UPGRADE") ? 1 : 0;
}

extern "C" int OotmmCustomItems_UsableNow(uint8_t item) {
    if (item != ITEM_OOTMM_RUTO_LETTER && gSaveContext.save.playerForm != PLAYER_FORM_HUMAN) {
        return 0;
    }
    const int32_t spell = OotmmCustomItems_ActionToMagicSpell(OotmmCustomItems_ItemAction(item));
    if (spell == OOTMM_MAGIC_SPELL_NONE) {
        return 1;
    }
    // Upstream disables Farore's Wind wherever the scene restricts the Song of Soaring.
    if (spell == OOTMM_MAGIC_SPELL_WIND && gPlayState != nullptr &&
        gPlayState->interfaceCtx.restrictions.songOfSoaring != 0) {
        return 0;
    }
    if (spell == OOTMM_MAGIC_SPELL_WIND && OotmmCustomItems_FaroresWindPlaced()) {
        return 1;
    }
    return gSaveContext.save.saveInfo.playerData.magicLevel != 0 &&
                   gSaveContext.save.saveInfo.playerData.magic >= OotmmCustomItems_MagicSpellCost(spell)
               ? 1
               : 0;
}

extern "C" int OotmmCustomItems_UsableWhileSwimming(uint8_t item) {
    if (!OotmmSession_IsActive() || gSaveContext.save.playerForm != PLAYER_FORM_HUMAN) {
        return 0;
    }
    switch (item) {
        case ITEM_OOTMM_BOOTS_IRON:
        case ITEM_OOTMM_BOOTS_HOVER:
        case ITEM_OOTMM_TUNIC_GORON:
        case ITEM_OOTMM_TUNIC_ZORA:
            return 1;
        default:
            return 0;
    }
}

namespace {

int32_t sEquippedBoots = OOTMM_BOOTS_NONE;
int32_t sEquippedTunic = OOTMM_TUNIC_NONE;
int32_t sEquippedMask = OOTMM_MASK_NONE;
int32_t sSlingshotAmmo = -1;

constexpr int32_t kMaxSeeds[4] = { 0, 30, 40, 50 };

} // namespace

extern "C" int32_t OotmmCustomItems_EquippedBoots(void) {
    return OotmmSession_IsActive() ? sEquippedBoots : OOTMM_BOOTS_NONE;
}

extern "C" int32_t OotmmCustomItems_EquippedTunic(void) {
    return OotmmSession_IsActive() ? sEquippedTunic : OOTMM_TUNIC_NONE;
}

extern "C" int32_t OotmmCustomItems_EquippedMask(void) {
    return OotmmSession_IsActive() ? sEquippedMask : OOTMM_MASK_NONE;
}

extern "C" void OotmmCustomItems_SetEquippedBoots(int32_t boots) {
    sEquippedBoots = boots;
}

extern "C" void OotmmCustomItems_SetEquippedTunic(int32_t tunic) {
    sEquippedTunic = tunic;
}

extern "C" void OotmmCustomItems_SetEquippedMask(int32_t mask) {
    sEquippedMask = mask;
}

extern "C" int32_t OotmmCustomItems_ActionToBoots(int32_t itemAction) {
    switch (itemAction) {
        case PLAYER_IA_OOTMM_BOOTS_IRON:
            return OOTMM_BOOTS_IRON;
        case PLAYER_IA_OOTMM_BOOTS_HOVER:
            return OOTMM_BOOTS_HOVER;
        default:
            return OOTMM_BOOTS_NONE;
    }
}

extern "C" int32_t OotmmCustomItems_ActionToTunic(int32_t itemAction) {
    switch (itemAction) {
        case PLAYER_IA_OOTMM_TUNIC_GORON:
            return OOTMM_TUNIC_GORON;
        case PLAYER_IA_OOTMM_TUNIC_ZORA:
            return OOTMM_TUNIC_ZORA;
        default:
            return OOTMM_TUNIC_NONE;
    }
}

extern "C" int32_t OotmmCustomItems_ActionToMask(int32_t itemAction) {
    switch (itemAction) {
        case PLAYER_IA_OOTMM_MASK_GERUDO:
            return OOTMM_MASK_GERUDO;
        case PLAYER_IA_OOTMM_MASK_SKULL:
            return OOTMM_MASK_SKULL;
        case PLAYER_IA_OOTMM_MASK_SPOOKY:
            return OOTMM_MASK_SPOOKY;
        default:
            return OOTMM_MASK_NONE;
    }
}

extern "C" const char* OotmmCustomItems_MaskDList(int32_t mask) {
    const char* path;
    switch (mask) {
        case OOTMM_MASK_GERUDO:
            path = "__OTR__objects/ot_obj_link_child/gLinkChildGerudoMaskDL";
            break;
        case OOTMM_MASK_SKULL:
            path = "__OTR__objects/ot_obj_link_child/gLinkChildSkullMaskDL";
            break;
        case OOTMM_MASK_SPOOKY:
            path = "__OTR__objects/ot_obj_link_child/gLinkChildSpookyMaskDL";
            break;
        default:
            return nullptr;
    }
    return ResourceMgr_FileExists(path) ? path : nullptr;
}

extern "C" int32_t OotmmCustomItems_SlingshotMaxAmmo(void) {
    uint32_t tier = OotmmCustomItems_OwnedCount(ITEM_OOTMM_SLINGSHOT);
    if (tier > 3) {
        tier = 3;
    }
    return kMaxSeeds[tier];
}

extern "C" int32_t OotmmCustomItems_SlingshotAmmo(void) {
    const int32_t max = OotmmCustomItems_SlingshotMaxAmmo();
    if (sSlingshotAmmo < 0) {
        sSlingshotAmmo = max;
    }
    if (sSlingshotAmmo > max) {
        sSlingshotAmmo = max;
    }
    return sSlingshotAmmo;
}

namespace {

int32_t sMaskEffectiveSpoof = 0;

bool IsNightTime() {
    const uint16_t time = gSaveContext.save.time;
    return time >= CLOCK_TIME(18, 0) || time < CLOCK_TIME(6, 0);
}

int32_t EffectiveSpoofedMask() {
    if (OotmmCustomItems_EquippedMask() == OOTMM_MASK_SPOOKY && IsNightTime()) {
        return PLAYER_MASK_GIBDO;
    }
    return 0;
}

} // namespace

extern "C" void OotmmCustomItems_MaskBeforeUpdate(Player* player) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    if (sMaskEffectiveSpoof > 0 && gSaveContext.save.equippedMask == PLAYER_MASK_NONE &&
        player->currentMask == sMaskEffectiveSpoof) {
        player->currentMask = PLAYER_MASK_NONE;
    }
}

extern "C" void OotmmCustomItems_MaskAfterUpdate(Player* player) {
    if (!OotmmSession_IsActive()) {
        return;
    }

    const int32_t prevSpoof = sMaskEffectiveSpoof;

    if (OotmmCustomItems_EquippedMask() == OOTMM_MASK_NONE) {
        if (prevSpoof > 0 && player->currentMask == prevSpoof) {
            player->currentMask = PLAYER_MASK_NONE;
        }
        sMaskEffectiveSpoof = 0;
        return;
    }

    if (player->transformation != PLAYER_FORM_HUMAN) {
        OotmmCustomItems_SetEquippedMask(OOTMM_MASK_NONE);
        if (prevSpoof > 0 && player->currentMask == prevSpoof) {
            player->currentMask = PLAYER_MASK_NONE;
        }
        sMaskEffectiveSpoof = 0;
        return;
    }

    const int32_t spoof = EffectiveSpoofedMask();
    if (prevSpoof > 0 && player->currentMask == prevSpoof) {
        player->currentMask = PLAYER_MASK_NONE;
    }

    sMaskEffectiveSpoof = spoof;
    if (spoof > 0) {
        player->currentMask = static_cast<uint8_t>(spoof);
    }
}

extern "C" void OotmmCustomItems_OnNativeMaskUse(Player* player, int32_t itemAction) {
    if (!OotmmSession_IsActive() || OotmmCustomItems_EquippedMask() == OOTMM_MASK_NONE) {
        return;
    }
    if (OotmmCustomItems_EquippedMask() == OOTMM_MASK_SPOOKY && IsNightTime() &&
        itemAction == GET_IA_FROM_MASK(PLAYER_MASK_GIBDO)) {
        return;
    }
    OotmmCustomItems_SetEquippedMask(OOTMM_MASK_NONE);
    sMaskEffectiveSpoof = -1;
}

extern "C" int OotmmCustomItems_SuppressVanillaMaskDraw(Player* player) {
    return OotmmSession_IsActive() && player->transformation == PLAYER_FORM_HUMAN &&
                   OotmmCustomItems_EquippedMask() == OOTMM_MASK_SPOOKY && IsNightTime() &&
                   player->currentMask == PLAYER_MASK_GIBDO
               ? 1
               : 0;
}

extern "C" void OotmmCustomItems_SetSlingshotAmmo(int32_t ammo) {
    const int32_t max = OotmmCustomItems_SlingshotMaxAmmo();
    if (ammo < 0) {
        ammo = 0;
    }
    if (ammo > max) {
        ammo = max;
    }
    sSlingshotAmmo = ammo;
}

namespace {

Actor* SpawnSpell(PlayState* play, Player* player, int32_t magicSpell, int32_t params) {
    const int32_t actorId = OotmmCustomItems_MagicSpellActorId(magicSpell);
    if (play == nullptr || player == nullptr || actorId < 0) {
        return nullptr;
    }
    return Actor_Spawn(&play->actorCtx, play, static_cast<s16>(actorId), player->actor.world.pos.x,
                       player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, params);
}

} // namespace

extern "C" Actor* OotmmCustomItems_SpawnMagicSpellActor(PlayState* play, Player* player, int32_t magicSpell) {
    return SpawnSpell(play, player, magicSpell, 0);
}

namespace {

constexpr int32_t kFaroresWindMarkerParams = 2;

// Vanilla persists the point in SaveContext.fw; MM has no such field, so the
// authoritative copy lives here for the session and mirrors into respawn[HUMAN].
RespawnData sFwPoint;
RespawnData sFwRespawnTop;
bool sFwRespawnTopValid;

Actor* FindFaroresWindMarker(PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor != nullptr; actor = actor->next) {
        if (actor->id == ACTOR_OOTMM_MAGIC_WIND && actor->params == kFaroresWindMarkerParams) {
            return actor;
        }
    }
    return nullptr;
}

void SpawnFaroresWindMarker(PlayState* play) {
    if (FindFaroresWindMarker(play) != nullptr) {
        return;
    }
    const Vec3f& pos = gSaveContext.respawn[RESPAWN_MODE_HUMAN].pos;
    Actor_Spawn(&play->actorCtx, play, ACTOR_OOTMM_MAGIC_WIND, pos.x, pos.y, pos.z, 0, 0, 0,
                kFaroresWindMarkerParams);
}

} // namespace

extern "C" int OotmmCustomItems_FaroresWindPlaced(void) {
    return gSaveContext.respawn[RESPAWN_MODE_HUMAN].data > 0 ? 1 : 0;
}

extern "C" void OotmmCustomItems_PlaceFaroresWind(PlayState* play) {
    if (play == nullptr) {
        return;
    }
    Play_SetupRespawnPoint(play, RESPAWN_MODE_HUMAN, PLAYER_PARAMS(0xFF, PLAYER_START_MODE_D));
    gSaveContext.respawn[RESPAWN_MODE_HUMAN].data = 1;

    sFwPoint = gSaveContext.respawn[RESPAWN_MODE_DOWN];
    sFwPoint.playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_D);
    sFwPoint.data = 40;

    sFwRespawnTop = gSaveContext.respawn[RESPAWN_MODE_TOP];
    sFwRespawnTopValid = true;

    SpawnFaroresWindMarker(play);
}

extern "C" void OotmmCustomItems_DispelFaroresWind(void) {
    Audio_PlaySfx_AtPos(&gSaveContext.respawn[RESPAWN_MODE_HUMAN].pos, NA_SE_PL_MAGIC_WIND_VANISH);
    gSaveContext.respawn[RESPAWN_MODE_HUMAN].data =
        static_cast<int8_t>(-gSaveContext.respawn[RESPAWN_MODE_HUMAN].data);
    sFwPoint.data = 0;
}

extern "C" void OotmmCustomItems_ClearFaroresWind(void) {
    sFwPoint.data = 0;
}

extern "C" void OotmmCustomItems_WarpToFaroresWind(PlayState* play) {
    if (play == nullptr || sFwPoint.data <= 0) {
        return;
    }
    gSaveContext.respawn[RESPAWN_MODE_HUMAN] = sFwPoint;
    gSaveContext.respawnFlag = 8;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->nextEntrance = sFwPoint.entrance;
    play->transitionType = TRANS_TYPE_FADE_WHITE_FAST;

    if (sFwRespawnTopValid) {
        gSaveContext.respawn[RESPAWN_MODE_TOP] = sFwRespawnTop;
        sFwRespawnTopValid = false;
    }
}

extern "C" int OotmmCustomItems_FaroresWindSceneMatch(PlayState* play) {
    if (play == nullptr || !OotmmSession_IsActive()) {
        return 0;
    }
    const s32 fwScene = Entrance_GetSceneIdAbsolute(gSaveContext.respawn[RESPAWN_MODE_HUMAN].entrance);
    if (fwScene == play->sceneId) {
        return 1;
    }
    switch (play->sceneId) {
        case SCENE_17SETUGEN:
            return fwScene == SCENE_17SETUGEN2 ? 1 : 0;
        case SCENE_17SETUGEN2:
            return fwScene == SCENE_17SETUGEN ? 1 : 0;
        case SCENE_11GORONNOSATO:
            return fwScene == SCENE_11GORONNOSATO2 ? 1 : 0;
        case SCENE_11GORONNOSATO2:
            return fwScene == SCENE_11GORONNOSATO ? 1 : 0;
        case SCENE_10YUKIYAMANOMURA:
            return fwScene == SCENE_10YUKIYAMANOMURA2 ? 1 : 0;
        case SCENE_10YUKIYAMANOMURA2:
            return fwScene == SCENE_10YUKIYAMANOMURA ? 1 : 0;
        case SCENE_20SICHITAI:
            return fwScene == SCENE_20SICHITAI2 ? 1 : 0;
        case SCENE_20SICHITAI2:
            return fwScene == SCENE_20SICHITAI ? 1 : 0;
        case SCENE_INISIE_N:
            return fwScene == SCENE_INISIE_R ? 2 : 0;
        case SCENE_INISIE_R:
            return fwScene == SCENE_INISIE_N ? 2 : 0;
        default:
            return 0;
    }
}

extern "C" void OotmmCustomItems_StartFaroresWindPrompt(void) {
    CustomMessage::Entry options;
    options.autoFormat = false;

    std::string msg;
    msg += "\x17";
    msg += "You cast Farore's Wind!";
    msg += "\x11";
    msg += "\x02";
    msg += "\xC3";
    msg += "     Return to the Warp Point";
    msg += "\x11";
    msg += "     Dispel the Warp Point";
    msg += "\x11";
    msg += "     Exit";
    msg += "\xBF";

    CustomMessage::StartTextbox(msg, options);
}

// TODO: handing the letter over cannot consume it; the launcher inventory is monotone and there is
// no check system yet to record the trade.
extern "C" void OotmmCustomItems_StartRutoLetterTextbox(void) {
    std::string msg;
    msg += "...Huh?";
    msg += "\x10";
    msg += "It looks like there is something already inside this bottle. It's a letter:";
    msg += "\x10";
    msg += "\"Help me. I'm waiting for you inside Lord Jabu-Jabu's belly. --Ruto";
    msg += "\x10";
    msg += "PS: Don't tell my father! PPS: Don't flush this letter!\"";

    CustomMessage::StartTextbox(msg);
}

extern "C" void OotmmCustomItems_AfterPlayerInit(PlayState* play) {
    if (play == nullptr || !OotmmSession_IsActive()) {
        return;
    }
    if (gSaveContext.nayrusLoveTimer != 0) {
        gSaveContext.magicState = MAGIC_STATE_METER_FLASH_1;
        OotmmCustomItems_SpawnMagicSpellActor(play, GET_PLAYER(play), OOTMM_MAGIC_SPELL_LOVE);
    }
    if (sFwPoint.data != 0) {
        gSaveContext.respawn[RESPAWN_MODE_HUMAN] = sFwPoint;
        if (OotmmCustomItems_FaroresWindSceneMatch(play) != 0) {
            SpawnFaroresWindMarker(play);
        }
    } else {
        gSaveContext.respawn[RESPAWN_MODE_HUMAN].data = 0;
        gSaveContext.respawn[RESPAWN_MODE_HUMAN].pos.x = 0.0f;
        gSaveContext.respawn[RESPAWN_MODE_HUMAN].pos.y = 0.0f;
        gSaveContext.respawn[RESPAWN_MODE_HUMAN].pos.z = 0.0f;
    }
}
