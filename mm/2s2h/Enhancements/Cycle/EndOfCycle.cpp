#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/OotmmSession.h"
#include "2s2h/OotmmCustomItems.h"
#include "2s2h/OotmmItemApply.h"

extern "C" {
#include <variables.h>
#include <overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h>
}

#define CVAR_NAME_RUPEES "gEnhancements.Cycle.DoNotResetRupees"
#define CVAR_RUPEES CVarGetInteger(CVAR_NAME_RUPEES, 0)
#define CVAR_NAME_CONSUME "gEnhancements.Cycle.DoNotResetConsumables"
#define CVAR_CONSUME CVarGetInteger(CVAR_NAME_CONSUME, 0)
#define CVAR_NAME_BOTTLE "gEnhancements.Cycle.DoNotResetBottleContent"
#define CVAR_BOTTLE CVarGetInteger(CVAR_NAME_BOTTLE, 0)
#define CVAR_NAME_SWORD "gEnhancements.Cycle.DoNotResetRazorSword"
#define CVAR_SWORD CVarGetInteger(CVAR_NAME_SWORD, 0)
#define CVAR_NAME_TIME "gEnhancements.Cycle.DoNotResetTimeSpeed"
#define CVAR_TIME CVarGetInteger(CVAR_NAME_TIME, 0)

SaveInfo saveInfoCopy;
ShipSaveInfo shipSaveInfoCopy;
s32 timeSpeedOffsetCopy = 0;

void RegisterEndOfCycleSaveHooks() {
    COND_HOOK(BeforeEndOfCycleSave, true, []() {
        OotmmItemApply_RememberTradeItems();
        memcpy(&saveInfoCopy, &gSaveContext.save.saveInfo, sizeof(SaveInfo));
        memcpy(&shipSaveInfoCopy, &gSaveContext.save.shipSaveInfo, sizeof(ShipSaveInfo));
        timeSpeedOffsetCopy = gSaveContext.save.timeSpeedOffset;
    });

    COND_HOOK(AfterEndOfCycleSave, true, []() { OotmmItemApply_ResetTradeCycle(); });

    COND_HOOK(AfterEndOfCycleSave, true, []() {
        if (!(OotmmSession_IsActive() ? OotmmSession_GetState().GetBoolSetting("keepItemsReset", false) : CVAR_RUPEES)) {
            return;
        }
        gSaveContext.save.saveInfo.playerData.rupees = saveInfoCopy.playerData.rupees;
        CLEAR_EVENTINF(EVENTINF_THREEDAYRESET_LOST_RUPEES);
    });

    COND_HOOK(AfterEndOfCycleSave, true, []() {
        if (OotmmSession_IsActive()) {
            if (OotmmSession_GetState().GetBoolSetting("keepItemsReset", false)) {
                memcpy(gSaveContext.save.saveInfo.inventory.ammo, saveInfoCopy.inventory.ammo,
                       sizeof(gSaveContext.save.saveInfo.inventory.ammo));
                gSaveContext.eventInf[7] &= 0xE0;
            } else {
                memset(gSaveContext.save.saveInfo.inventory.ammo, 0, sizeof(gSaveContext.save.saveInfo.inventory.ammo));
            }
            AMMO(ITEM_PICTOGRAPH_BOX) = 0;
            return;
        }
        if (!CVAR_CONSUME) {
            return;
        }
        if (INV_CONTENT(ITEM_BOMB) == ITEM_BOMB) {
            INV_CONTENT(ITEM_BOMB) = saveInfoCopy.inventory.items[ITEM_BOMB];
            AMMO(ITEM_BOMB) = saveInfoCopy.inventory.ammo[ITEM_BOMB];
        }

        if (INV_CONTENT(ITEM_DEKU_NUT) == ITEM_DEKU_NUT) {
            INV_CONTENT(ITEM_DEKU_NUT) = saveInfoCopy.inventory.items[ITEM_DEKU_NUT];
            AMMO(ITEM_DEKU_NUT) = saveInfoCopy.inventory.ammo[ITEM_DEKU_NUT];
        }

        if (INV_CONTENT(ITEM_DEKU_STICK) == ITEM_DEKU_STICK) {
            INV_CONTENT(ITEM_DEKU_STICK) = saveInfoCopy.inventory.items[ITEM_DEKU_STICK];
            AMMO(ITEM_DEKU_STICK) = saveInfoCopy.inventory.ammo[ITEM_DEKU_STICK];
        }

        if (INV_CONTENT(ITEM_BOW) == ITEM_BOW) {
            INV_CONTENT(ITEM_BOW) = saveInfoCopy.inventory.items[ITEM_BOW];
            AMMO(ITEM_BOW) = saveInfoCopy.inventory.ammo[ITEM_BOW];
        }

        for (int i = 0; i < ITEM_NUM_SLOTS; i++) {
            if (gAmmoItems[i] != ITEM_NONE) {
                if ((gSaveContext.save.saveInfo.inventory.items[i] != ITEM_NONE) && (i != SLOT_PICTOGRAPH_BOX)) {
                    gSaveContext.save.saveInfo.inventory.items[i] = saveInfoCopy.inventory.items[i];
                    gSaveContext.save.saveInfo.inventory.ammo[i] = saveInfoCopy.inventory.ammo[i];
                }
            }
        }

        CLEAR_EVENTINF(EVENTINF_THREEDAYRESET_LOST_BOMB_AMMO);
        CLEAR_EVENTINF(EVENTINF_THREEDAYRESET_LOST_NUT_AMMO);
        CLEAR_EVENTINF(EVENTINF_THREEDAYRESET_LOST_STICK_AMMO);
        CLEAR_EVENTINF(EVENTINF_THREEDAYRESET_LOST_ARROW_AMMO);
    });

    COND_HOOK(AfterEndOfCycleSave, true, []() {
        if (OotmmSession_IsActive()) {
            const bool keep = OotmmSession_GetState().GetBoolSetting("keepBottlesReset", false);
            if (keep && (saveInfoCopy.weekEventReg[WEEKEVENTREG_DRANK_CHATEAU_ROMANI >> 8] &
                         (WEEKEVENTREG_DRANK_CHATEAU_ROMANI & 0xFF))) {
                SET_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
            } else {
                CLEAR_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
            }
            auto& inventory = gSaveContext.save.saveInfo.inventory;
            for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; ++slot) {
                const auto before = saveInfoCopy.inventory.items[slot];
                if (before == ITEM_NONE) {
                    continue;
                }
                auto after = before;
                switch (before) {
                    case ITEM_GOLD_DUST:
                    case ITEM_OOTMM_RUTO_LETTER:
                        break;
                    case ITEM_HOT_SPRING_WATER:
                        after = keep ? ITEM_SPRING_WATER : ITEM_BOTTLE;
                        break;
                    case ITEM_ZORA_EGG:
                    case ITEM_SEAHORSE:
                    case ITEM_DEKU_PRINCESS:
                    case ITEM_MUSHROOM:
                    case ITEM_BIG_POE:
                        after = ITEM_BOTTLE;
                        break;
                    default:
                        if (!keep) {
                            after = ITEM_BOTTLE;
                        }
                        break;
                }
                inventory.items[slot] = after;
            }
            for (int form = 0; form < 4; ++form) {
                auto& equips = gSaveContext.save.saveInfo.equips;
                for (int button = EQUIP_SLOT_C_LEFT; button <= EQUIP_SLOT_C_RIGHT; ++button) {
                    const auto slot = equips.cButtonSlots[form][button];
                    if (slot >= SLOT_BOTTLE_1 && slot <= SLOT_BOTTLE_6) {
                        equips.buttonItems[form][button] = inventory.items[slot];
                    }
                }
                auto& dpad = gSaveContext.save.shipSaveInfo.dpadEquips;
                for (int button = 0; button < 4; ++button) {
                    const auto slot = dpad.dpadSlots[form][button];
                    if (slot >= SLOT_BOTTLE_1 && slot <= SLOT_BOTTLE_6) {
                        dpad.dpadItems[form][button] = inventory.items[slot];
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
            return;
        }
        if (!CVAR_BOTTLE) {
            return;
        }
        int stolenBottles = (((saveInfoCopy.stolenItems & 0xFF000000) >> 0x18) == ITEM_BOTTLE) +
                            (((saveInfoCopy.stolenItems & 0x00FF0000) >> 0x10) == ITEM_BOTTLE);

        // Replace bottles back, accounting for any stolen bottles
        for (int i = SLOT_BOTTLE_1; i <= SLOT_BOTTLE_6; i++) {
            gSaveContext.save.saveInfo.inventory.items[i] = saveInfoCopy.inventory.items[i];

            if (stolenBottles > 0 && saveInfoCopy.inventory.items[i] == ITEM_NONE) {
                stolenBottles--;
                gSaveContext.save.saveInfo.inventory.items[i] = ITEM_BOTTLE;
            }
        }

        // Set back button equips to the correct bottle type
        for (int j = EQUIP_SLOT_C_LEFT; j <= EQUIP_SLOT_C_RIGHT; j++) {
            if (GET_CUR_FORM_BTN_ITEM(j) == ITEM_BOTTLE) {
                SET_CUR_FORM_BTN_ITEM(j, saveInfoCopy.equips.buttonItems[0][j]);
            }
        }
        for (int j = EQUIP_SLOT_D_RIGHT; j <= EQUIP_SLOT_D_UP; j++) {
            if (DPAD_GET_CUR_FORM_BTN_ITEM(j) == ITEM_BOTTLE) {
                DPAD_SET_CUR_FORM_BTN_ITEM(j, shipSaveInfoCopy.dpadEquips.dpadItems[0][j]);
            }
        }
    });

    COND_HOOK(AfterEndOfCycleSave, CVAR_SWORD, []() {
        u8 curSword = (saveInfoCopy.equips.equipment & gEquipMasks[EQUIP_TYPE_SWORD]) >> gEquipShifts[EQUIP_TYPE_SWORD];

        // Check for razor sword equipped, stolen, or turned into the smithy
        if (curSword == EQUIP_VALUE_SWORD_RAZOR ||
            (curSword == EQUIP_VALUE_SWORD_NONE &&
             ((saveInfoCopy.permanentSceneFlags[SCENE_KAJIYA].unk_14 & 4) ||
              (((saveInfoCopy.stolenItems & 0xFF000000) >> 0x18) == ITEM_SWORD_RAZOR) ||
              (((saveInfoCopy.stolenItems & 0x00FF0000) >> 0x10) == ITEM_SWORD_RAZOR)))) {

            SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_RAZOR);
            BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_SWORD_RAZOR;
        }
    });

    COND_HOOK(AfterEndOfCycleSave, CVAR_TIME, []() { gSaveContext.save.timeSpeedOffset = timeSpeedOffsetCopy; });
}

static RegisterShipInitFunc initFunc(RegisterEndOfCycleSaveHooks,
                                     { CVAR_NAME_RUPEES, CVAR_NAME_CONSUME, CVAR_NAME_BOTTLE, CVAR_NAME_SWORD,
                                       CVAR_NAME_TIME });
