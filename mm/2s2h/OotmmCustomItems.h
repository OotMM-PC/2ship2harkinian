#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Free item ids between ITEM_CC (0xCC) and the player mask ids at ITEM_F0.
typedef enum OotmmCustomItemId {
    ITEM_OOTMM_FIRST = 0xCD,
    ITEM_OOTMM_SPELL_WIND = ITEM_OOTMM_FIRST,
    ITEM_OOTMM_SPELL_LOVE,
    ITEM_OOTMM_SPELL_FIRE,
    ITEM_OOTMM_BOOTS_IRON,
    ITEM_OOTMM_BOOTS_HOVER,
    ITEM_OOTMM_TUNIC_GORON,
    ITEM_OOTMM_TUNIC_ZORA,
    ITEM_OOTMM_HAMMER,
    ITEM_OOTMM_BOOMERANG,
    ITEM_OOTMM_SLINGSHOT,
    ITEM_OOTMM_RUTO_LETTER,
    ITEM_OOTMM_MASK_GERUDO,
    ITEM_OOTMM_MASK_SKULL,
    ITEM_OOTMM_MASK_SPOOKY,
    ITEM_OOTMM_MAX,
} OotmmCustomItemId;

#define OOTMM_CUSTOM_ITEM_COUNT (ITEM_OOTMM_MAX - ITEM_OOTMM_FIRST)

// Above the 48 vanilla inventory slots, below SLOT_NONE (0xFF).
#define SLOT_OOTMM_FIRST 0x80
#define SLOT_OOTMM_MAX (SLOT_OOTMM_FIRST + OOTMM_CUSTOM_ITEM_COUNT)

// Item actions past PLAYER_IA_MAX (0x53); Player::itemAction is s8, so they stay under 0x80.
typedef enum OotmmCustomItemAction {
    PLAYER_IA_OOTMM_FIRST = 0x53,
    PLAYER_IA_OOTMM_SPELL_WIND = PLAYER_IA_OOTMM_FIRST,
    PLAYER_IA_OOTMM_SPELL_LOVE,
    PLAYER_IA_OOTMM_SPELL_FIRE,
    PLAYER_IA_OOTMM_BOOTS_IRON,
    PLAYER_IA_OOTMM_BOOTS_HOVER,
    PLAYER_IA_OOTMM_TUNIC_GORON,
    PLAYER_IA_OOTMM_TUNIC_ZORA,
    PLAYER_IA_OOTMM_HAMMER,
    PLAYER_IA_OOTMM_BOOMERANG,
    PLAYER_IA_OOTMM_SLINGSHOT,
    PLAYER_IA_OOTMM_MASK_GERUDO,
    PLAYER_IA_OOTMM_MASK_SKULL,
    PLAYER_IA_OOTMM_MASK_SPOOKY,
    PLAYER_IA_OOTMM_MAX,
} OotmmCustomItemAction;

// Ruto's Letter rides MM's unused Hylian Loach bottle action, so the vanilla bottle/exchange
// machinery handles it.
#define PLAYER_IA_OOTMM_BOTTLE_RUTO_LETTER 0x1F

typedef enum OotmmCustomBoots {
    OOTMM_BOOTS_NONE,
    OOTMM_BOOTS_IRON,
    OOTMM_BOOTS_HOVER,
} OotmmCustomBoots;

typedef enum OotmmCustomTunic {
    OOTMM_TUNIC_NONE,
    OOTMM_TUNIC_GORON,
    OOTMM_TUNIC_ZORA,
} OotmmCustomTunic;

typedef enum OotmmCustomMask {
    OOTMM_MASK_NONE,
    OOTMM_MASK_GERUDO,
    OOTMM_MASK_SKULL,
    OOTMM_MASK_SPOOKY,
} OotmmCustomMask;

typedef enum OotmmMagicSpell {
    OOTMM_MAGIC_SPELL_NONE = -1,
    OOTMM_MAGIC_SPELL_WIND,
    OOTMM_MAGIC_SPELL_LOVE,
    OOTMM_MAGIC_SPELL_FIRE,
    OOTMM_MAGIC_SPELL_MAX,
} OotmmMagicSpell;

int OotmmCustomItems_IsCustomItem(uint8_t item);
int OotmmCustomItems_IsCustomSlot(uint8_t slot);
uint8_t OotmmCustomItems_SlotOf(uint8_t item);
uint8_t OotmmCustomItems_ItemInSlot(uint8_t slot);

const char* OotmmCustomItems_IconPath(uint8_t item);
/// Icon for a button slot: our texture for custom ids, gItemIcons otherwise.
void* OotmmCustomItems_ButtonIcon(uint8_t item);
const char* OotmmCustomItems_NameTexture(uint8_t item);

/// Item id for an OoTMM inventory id, or ITEM_NONE when it has no custom item.
uint8_t OotmmCustomItems_IdForItemId(const char* ootmmItemId);

int OotmmCustomItems_Owned(uint8_t item);
uint32_t OotmmCustomItems_OwnedCount(uint8_t item);
int OotmmCustomItems_OwnedItemCount(void);
uint8_t OotmmCustomItems_OwnedItemAt(int index);
int OotmmCustomItems_UsableNow(uint8_t item);
int OotmmCustomItems_UsableWhileSwimming(uint8_t item);
/// Whether the seed has granted a magic upgrade; MM's native flag is not consulted.
int OotmmCustomItems_HasMagicUpgrade(void);

/// PlayerItemAction for a custom item, or -1 when the id is not one of ours.
int32_t OotmmCustomItems_ItemAction(uint8_t item);

/// Currently equipped custom boots/tunic/mask; 0 outside an active session.
int32_t OotmmCustomItems_EquippedBoots(void);
int32_t OotmmCustomItems_EquippedTunic(void);
int32_t OotmmCustomItems_EquippedMask(void);
void OotmmCustomItems_SetEquippedBoots(int32_t boots);
void OotmmCustomItems_SetEquippedTunic(int32_t tunic);
void OotmmCustomItems_SetEquippedMask(int32_t mask);
/// OotmmCustomBoots/Tunic/Mask value for an item action, or 0.
int32_t OotmmCustomItems_ActionToBoots(int32_t itemAction);
int32_t OotmmCustomItems_ActionToTunic(int32_t itemAction);
int32_t OotmmCustomItems_ActionToMask(int32_t itemAction);
/// Worn-mask display list OTR path for an OotmmCustomMask, or NULL when unavailable.
const char* OotmmCustomItems_MaskDList(int32_t mask);

/// Deku seed ammo for the cross-game slingshot, clamped to the bag tier.
int32_t OotmmCustomItems_SlingshotAmmo(void);
void OotmmCustomItems_SetSlingshotAmmo(int32_t ammo);
int32_t OotmmCustomItems_SlingshotMaxAmmo(void);

void OotmmCustomItems_StartRutoLetterTextbox(void);
/// OotmmMagicSpell for an item action, or OOTMM_MAGIC_SPELL_NONE.
int32_t OotmmCustomItems_ActionToMagicSpell(int32_t itemAction);
int32_t OotmmCustomItems_MagicSpellCost(int32_t magicSpell);
/// Actor id of the effect a spell spawns, or -1 when the spell is out of range.
int32_t OotmmCustomItems_MagicSpellActorId(int32_t magicSpell);

#ifdef __cplusplus
}
#endif
