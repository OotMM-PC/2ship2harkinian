#include "OotmmBottles.h"

#include "OotmmCustomItems.h"
#include "OotmmItemApply.h"
#include "OotmmSession.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"

#include <libultraship/bridge/OotmmBottles.h>

#include <array>
#include <utility>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

using Content = Ship::OotmmBottleContent;

constexpr std::array<std::pair<uint8_t, Content>, 22> kContents = { {
    { ITEM_BOTTLE, Content::Empty },
    { ITEM_POTION_RED, Content::PotionRed },
    { ITEM_POTION_GREEN, Content::PotionGreen },
    { ITEM_POTION_BLUE, Content::PotionBlue },
    { ITEM_FAIRY, Content::Fairy },
    { ITEM_FISH, Content::Fish },
    { ITEM_MILK_BOTTLE, Content::Milk },
    { ITEM_MILK_HALF, Content::MilkHalf },
    { ITEM_OOTMM_RUTO_LETTER, Content::RutoLetter },
    { ITEM_BLUE_FIRE, Content::BlueFire },
    { ITEM_BUG, Content::Bugs },
    { ITEM_POE, Content::Poe },
    { ITEM_BIG_POE, Content::BigPoe },
    { ITEM_MUSHROOM, Content::Mushroom },
    { ITEM_CHATEAU, Content::Chateau },
    { ITEM_GOLD_DUST, Content::GoldDust },
    { ITEM_SEAHORSE, Content::Seahorse },
    { ITEM_DEKU_PRINCESS, Content::DekuPrincess },
    { ITEM_SPRING_WATER, Content::SpringWater },
    { ITEM_HOT_SPRING_WATER, Content::SpringWaterHot },
    { ITEM_ZORA_EGG, Content::ZoraEgg },
    { ITEM_OBABA_DRINK, Content::KotakeDrink },
} };

void RefreshButtons(uint8_t slot, uint8_t item) {
    for (int form = 0; form < 4; form++) {
        for (int button = EQUIP_SLOT_C_LEFT; button <= EQUIP_SLOT_C_RIGHT; button++) {
            if (C_SLOT_EQUIP(form, button) == slot) {
                BUTTON_ITEM_EQUIP(form, button) = item;
            }
        }
        for (int button = 0; button < 4; button++) {
            auto& equips = gSaveContext.save.shipSaveInfo.dpadEquips;
            if (equips.dpadSlots[form][button] == slot) {
                equips.dpadItems[form][button] = item;
            }
        }
    }
}

bool sLetterReturned = false;

void BuildLetterReturnText(u16* textId, bool* loadFromMessageTable) {
    if (!sLetterReturned) {
        return;
    }
    sLetterReturned = false;
    auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
    entry.autoFormat = false;
    CustomMessage::Replace(&entry.msg, "you.\xe0", "you.\x11You can also have the letter back...\xe0");
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

} // namespace

extern "C" void OotmmBottles_Init(void) {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(0x294C, BuildLetterReturnText);
}

extern "C" int32_t OotmmBottles_Shared(void) {
    return OotmmSession_IsActive() && OotmmSession_GetState().GetBoolSetting("sharedBottles", false) ? 1 : 0;
}

extern "C" uint32_t OotmmBottles_ContentOf(uint8_t item) {
    for (const auto& [native, content] : kContents) {
        if (native == item) {
            return static_cast<uint32_t>(content);
        }
    }
    return static_cast<uint32_t>(Content::None);
}

extern "C" uint8_t OotmmBottles_ItemFor(uint32_t content) {
    for (const auto& [native, known] : kContents) {
        if (static_cast<uint32_t>(known) == content) {
            return native;
        }
    }
    return ITEM_NONE;
}

extern "C" void OotmmBottles_SetSlot(int32_t bottle, uint32_t content) {
    if (bottle < 0 || bottle >= Ship::kOotmmBottleCount) {
        return;
    }
    const uint8_t slot = static_cast<uint8_t>(SLOT_BOTTLE_1 + bottle);
    const uint8_t item = OotmmBottles_ItemFor(content);
    gSaveContext.save.saveInfo.inventory.items[slot] = item;
    RefreshButtons(slot, item);
}

extern "C" int32_t OotmmBottles_RutoLetterHeld(void) {
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
        if (gSaveContext.save.saveInfo.inventory.items[slot] == ITEM_OOTMM_RUTO_LETTER) {
            return 1;
        }
    }
    return 0;
}

extern "C" void OotmmBottles_UseRutoLetter(PlayState* play) {
    const uint32_t used = OotmmItemApply_LedgerGet("native:rutoLetterUsed") | Ship::kOotmmRutoLetterUsedMm;
    OotmmItemApply_LedgerSet("native:rutoLetterUsed", used);
    const bool kingZoraOpen = OotmmSession_GetState().GetStringSetting("zoraKing", "vanilla") == "open";
    const uint32_t both = Ship::kOotmmRutoLetterUsedOot | Ship::kOotmmRutoLetterUsedMm;
    if ((used & both) != both && !kingZoraOpen) {
        sLetterReturned = true;
        return;
    }
    Player_UpdateBottleHeld(play, GET_PLAYER(play), ITEM_BOTTLE, PLAYER_IA_BOTTLE_EMPTY);
}
