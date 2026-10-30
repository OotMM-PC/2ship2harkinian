#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#define SLOT_OOTMM_FIRST 0x80
#define SLOT_OOTMM_MAX (SLOT_OOTMM_FIRST + OOTMM_CUSTOM_ITEM_COUNT)

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
void* OotmmCustomItems_ButtonIcon(uint8_t item);
const char* OotmmCustomItems_NameTexture(uint8_t item);

uint8_t OotmmCustomItems_IdForItemId(const char* ootmmItemId);

int OotmmCustomItems_Owned(uint8_t item);
uint32_t OotmmCustomItems_OwnedCount(uint8_t item);
int OotmmCustomItems_OwnedItemCount(void);
uint8_t OotmmCustomItems_OwnedItemAt(int index);
int OotmmCustomItems_UsableNow(uint8_t item);
int OotmmCustomItems_UsableWhileSwimming(uint8_t item);
int OotmmCustomItems_HasMagicUpgrade(void);

int32_t OotmmCustomItems_ItemAction(uint8_t item);

int OotmmCustomItems_WearingDekuShield(void);
const char* OotmmCustomItems_DekuShieldIcon(void);
int32_t OotmmCustomItems_EquippedBoots(void);
int32_t OotmmCustomItems_EquippedTunic(void);
int32_t OotmmCustomItems_EquippedMask(void);
void OotmmCustomItems_SetEquippedBoots(int32_t boots);
void OotmmCustomItems_SetEquippedTunic(int32_t tunic);
void OotmmCustomItems_SetEquippedMask(int32_t mask);
int32_t OotmmCustomItems_ActionToBoots(int32_t itemAction);
int32_t OotmmCustomItems_ActionToTunic(int32_t itemAction);
int32_t OotmmCustomItems_ActionToMask(int32_t itemAction);
const char* OotmmCustomItems_MaskDList(int32_t mask);

int32_t OotmmCustomItems_SlingshotAmmo(void);
void OotmmCustomItems_SetSlingshotAmmo(int32_t ammo);
void OotmmCustomItems_AddSlingshotSeeds(int32_t count);
int32_t OotmmCustomItems_SlingshotTierMax(uint32_t tier);
int32_t OotmmCustomItems_SlingshotMaxAmmo(void);

void OotmmCustomItems_StartRutoLetterTextbox(void);
int32_t OotmmCustomItems_ActionToMagicSpell(int32_t itemAction);
int32_t OotmmCustomItems_MagicSpellCost(int32_t magicSpell);
int32_t OotmmCustomItems_MagicSpellActorId(int32_t magicSpell);

#ifdef __cplusplus
}
#endif
