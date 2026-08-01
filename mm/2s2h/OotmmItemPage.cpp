#include "OotmmItemPage.h"

#include "OotmmCustomItems.h"
#include "OotmmSession.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
#include "gfxprint.h"
#include "interface/parameter_static/parameter_static.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"

#include "OotmmCustomItemsPlayer.h"

extern PlayState* gPlayState;

extern s16 sEquipState;
extern s16 sEquipMagicArrowSlotHoldTimer;
extern s16 sEquipAnimTimer;

void KaleidoScope_DrawTexQuadRGBA32(GraphicsContext* gfxCtx, TexturePtr texture, u16 width, u16 height, u16 point);
Gfx* Gfx_DrawTexQuadIA8(Gfx* gfx, TexturePtr texture, s16 width, s16 height, u16 point);
}

namespace {

constexpr size_t kSlotsPerPage = ITEM_NUM_SLOTS;
constexpr size_t kColumns = ITEM_GRID_COLS;
constexpr int kEquipStateMoveToCButton = 3;

size_t sPage = 0;
size_t sCursor = 0;

std::vector<uint8_t> OwnedItems() {
    std::vector<uint8_t> owned;
    const int count = OotmmCustomItems_OwnedItemCount();
    owned.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; i++) {
        owned.push_back(OotmmCustomItems_OwnedItemAt(i));
    }
    return owned;
}

size_t FirstOnPage() {
    return sPage == 0 ? 0 : (sPage - 1) * kSlotsPerPage;
}

size_t CustomPageCount() {
    const size_t owned = static_cast<size_t>(OotmmCustomItems_OwnedItemCount());
    return owned == 0 ? 0 : (owned + kSlotsPerPage - 1) / kSlotsPerPage;
}

size_t TotalPages() {
    return 1 + CustomPageCount();
}

bool PageHasItems() {
    return sPage != 0 && static_cast<size_t>(OotmmCustomItems_OwnedItemCount()) > FirstOnPage();
}

void ApplyCursor(PauseContext* pauseCtx, uint8_t item) {
    pauseCtx->cursorItem[PAUSE_ITEM] = item;
    pauseCtx->cursorSlot[PAUSE_ITEM] = static_cast<s16>(sCursor);
    pauseCtx->cursorPoint[PAUSE_ITEM] = static_cast<s16>(sCursor);
    pauseCtx->cursorXIndex[PAUSE_ITEM] = static_cast<s16>(sCursor % kColumns);
    pauseCtx->cursorYIndex[PAUSE_ITEM] = static_cast<s16>(sCursor / kColumns);
}

} // namespace

extern "C" int OotmmItemPage_Active(void) {
    if (!OotmmSession_IsActive()) {
        sPage = 0;
        return 0;
    }
    if (!PageHasItems()) {
        sPage = 0;
        return 0;
    }
    return 1;
}

extern "C" int OotmmItemPage_ConsumePageToggle(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (!OotmmSession_IsActive() || pauseCtx->pageIndex != PAUSE_ITEM) {
        sPage = 0;
        return 0;
    }

    // L also opens the debug inventory editor, so leave it alone while that is up.
    if (pauseCtx->state != PAUSE_STATE_MAIN || pauseCtx->mainState != PAUSE_MAIN_STATE_IDLE ||
        pauseCtx->itemDescriptionOn || pauseCtx->debugEditor != DEBUG_EDITOR_NONE) {
        return 0;
    }

    const size_t pages = TotalPages();
    if (sPage >= pages) {
        sPage = 0;
    }

    if (pages > 1 && CHECK_BTN_ALL(CONTROLLER1(&play->state)->press.button, BTN_L)) {
        sPage = (sPage + 1) % pages;
        sCursor = 0;
        // UpdateCursor skips this frame, so clear the arrow park here or the next toggle leaves the page.
        if (sPage != 0) {
            pauseCtx->cursorSpecialPos = 0;
        }
        Audio_PlaySfx(NA_SE_SY_CURSOR);
        return 1;
    }

    if (sPage != 0 && pauseCtx->cursorSpecialPos != 0) {
        sPage = 0;
    }
    return 0;
}

extern "C" int OotmmItemPage_EquipOutlineIndex(uint8_t cButtonSlot) {
    if (!OotmmItemPage_Active()) {
        return OotmmCustomItems_IsCustomSlot(cButtonSlot) || cButtonSlot >= kSlotsPerPage
                   ? -1
                   : static_cast<int>(cButtonSlot);
    }
    if (!OotmmCustomItems_IsCustomSlot(cButtonSlot)) {
        return -1;
    }

    const uint8_t item = OotmmCustomItems_ItemInSlot(cButtonSlot);
    const std::vector<uint8_t> owned = OwnedItems();
    const size_t first = FirstOnPage();
    for (size_t cell = 0; cell < kSlotsPerPage && first + cell < owned.size(); cell++) {
        if (owned[first + cell] == item) {
            return static_cast<int>(cell);
        }
    }
    return -1;
}

extern "C" void OotmmItemPage_UpdateCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const std::vector<uint8_t> owned = OwnedItems();
    const size_t first = FirstOnPage();
    const size_t onPage = owned.size() > first ? owned.size() - first : 0;

    pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_YELLOW;
    pauseCtx->nameColorSet = PAUSE_NAME_COLOR_SET_WHITE;

    if (onPage == 0) {
        return;
    }
    if (sCursor >= onPage) {
        sCursor = onPage - 1;
    }

    // An empty vanilla inventory parks the cursor on a page-turn arrow, which blocks ours.
    pauseCtx->cursorSpecialPos = 0;

    const bool idle = pauseCtx->state == PAUSE_STATE_MAIN && pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE &&
                      !pauseCtx->itemDescriptionOn;

    if (idle) {
        const size_t before = sCursor;
        if (pauseCtx->stickAdjX < -30 && sCursor % kColumns != 0) {
            sCursor--;
        } else if (pauseCtx->stickAdjX > 30 && (sCursor % kColumns) != kColumns - 1 && sCursor + 1 < onPage) {
            sCursor++;
        } else if (pauseCtx->stickAdjY > 30 && sCursor >= kColumns) {
            sCursor -= kColumns;
        } else if (pauseCtx->stickAdjY < -30 && sCursor + kColumns < onPage) {
            sCursor += kColumns;
        }
        if (sCursor != before) {
            pauseCtx->cursorShrinkRate = 4.0f;
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        }
    }

    const uint8_t item = owned[first + sCursor];
    ApplyCursor(pauseCtx, item);

    if (gSaveContext.buttonStatus[EQUIP_SLOT_A] != BTN_DISABLED) {
        gSaveContext.buttonStatus[EQUIP_SLOT_A] = BTN_DISABLED;
        gSaveContext.hudVisibility = HUD_VISIBILITY_IDLE;
        Interface_SetHudVisibility(HUD_VISIBILITY_ALL);
    }

    if (!idle || pauseCtx->debugEditor != DEBUG_EDITOR_NONE) {
        return;
    }

    const u16 pressed = CONTROLLER1(&play->state)->press.button;
    s16 targetButton = -1;
    if (CHECK_BTN_ALL(pressed, BTN_CLEFT)) {
        targetButton = PAUSE_EQUIP_C_LEFT;
    } else if (CHECK_BTN_ALL(pressed, BTN_CDOWN)) {
        targetButton = PAUSE_EQUIP_C_DOWN;
    } else if (CHECK_BTN_ALL(pressed, BTN_CRIGHT)) {
        targetButton = PAUSE_EQUIP_C_RIGHT;
    } else if (CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) {
        if (CHECK_BTN_ALL(pressed, BTN_DRIGHT)) {
            targetButton = PAUSE_EQUIP_D_RIGHT;
        } else if (CHECK_BTN_ALL(pressed, BTN_DLEFT)) {
            targetButton = PAUSE_EQUIP_D_LEFT;
        } else if (CHECK_BTN_ALL(pressed, BTN_DDOWN)) {
            targetButton = PAUSE_EQUIP_D_DOWN;
        } else if (CHECK_BTN_ALL(pressed, BTN_DUP)) {
            targetButton = PAUSE_EQUIP_D_UP;
        }
    }

    if (targetButton < 0) {
        return;
    }
    if (!GameInteractor_Should(VB_KALEIDO_EQUIP_ITEM_TO_BUTTON, true, pauseCtx->cursorSlot[PAUSE_ITEM], item)) {
        return;
    }

    const u16 vtxIndex = static_cast<u16>(sCursor * 4);
    pauseCtx->equipTargetCBtn = targetButton;
    pauseCtx->equipTargetItem = item;
    pauseCtx->equipTargetSlot = OotmmCustomItems_SlotOf(item);
    pauseCtx->mainState = PAUSE_MAIN_STATE_EQUIP_ITEM;
    pauseCtx->equipAnimX = pauseCtx->itemVtx[vtxIndex].v.ob[0] * 10;
    pauseCtx->equipAnimY = pauseCtx->itemVtx[vtxIndex].v.ob[1] * 10;
    pauseCtx->equipAnimAlpha = 255;
    sEquipMagicArrowSlotHoldTimer = 0;
    sEquipState = kEquipStateMoveToCButton;
    sEquipAnimTimer = 10;
    Audio_PlaySfx(NA_SE_SY_DECIDE);
}

extern "C" void OotmmItemPage_Draw(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const std::vector<uint8_t> owned = OwnedItems();
    const size_t first = FirstOnPage();

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL42_Opa(play->state.gfxCtx);

    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    for (size_t button = EQUIP_SLOT_C_LEFT; button <= EQUIP_SLOT_C_RIGHT; button++) {
        const int cell = OotmmItemPage_EquipOutlineIndex(GET_CUR_FORM_BTN_SLOT(button));
        if (cell < 0) {
            continue;
        }
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)&pauseCtx->itemVtx[cell * 4], 4, 0);
        POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, (TexturePtr)gEquippedItemOutlineTex, 32, 32, 0);
    }

    if (CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) {
        for (size_t button = EQUIP_SLOT_D_RIGHT; button <= EQUIP_SLOT_D_UP; button++) {
            const int cell = OotmmItemPage_EquipOutlineIndex(DPAD_GET_CUR_FORM_BTN_SLOT(button));
            if (cell < 0) {
                continue;
            }
            gSPVertex(POLY_OPA_DISP++, (uintptr_t)&pauseCtx->itemVtx[cell * 4], 4, 0);
            POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, (TexturePtr)gEquippedItemOutlineTex, 32, 32, 0);
        }
    }

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    for (size_t cell = 0; cell < kSlotsPerPage && first + cell < owned.size(); cell++) {
        const uint8_t item = owned[first + cell];
        const char* texture = OotmmCustomItems_IconPath(item);
        if (texture == nullptr) {
            continue;
        }

        Vtx* vtx = &pauseCtx->itemVtx[cell * 4];
        if (cell == sCursor && pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) {
            vtx[0].v.ob[0] = vtx[2].v.ob[0] = vtx[0].v.ob[0] - 2;
            vtx[1].v.ob[0] = vtx[3].v.ob[0] = vtx[0].v.ob[0] + 32;
            vtx[0].v.ob[1] = vtx[1].v.ob[1] = vtx[0].v.ob[1] + 2;
            vtx[2].v.ob[1] = vtx[3].v.ob[1] = vtx[0].v.ob[1] - 32;
        }

        const u8 shade = OotmmCustomItems_UsableNow(item) ? 255 : 100;
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, shade, shade, shade, pauseCtx->alpha);
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)vtx, 4, 0);
        KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx, (TexturePtr)texture, 32, 32, 0);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void OotmmItemPage_DrawPageIndicator(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (!OotmmSession_IsActive() || pauseCtx->pageIndex != PAUSE_ITEM || pauseCtx->state != PAUSE_STATE_MAIN ||
        pauseCtx->mainState != PAUSE_MAIN_STATE_IDLE) {
        return;
    }

    const size_t pages = TotalPages();
    if (pages < 2) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx* polyOpa = POLY_OPA_DISP;
    Gfx* gfx = Graph_GfxPlusOne(polyOpa);
    gSPDisplayList(OVERLAY_DISP++, gfx);

    GfxPrint printer;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    GfxPrint_SetColor(&printer, 255, 255, 255, pauseCtx->alpha);
    GfxPrint_SetPos(&printer, 27, 6);
    GfxPrint_Printf(&printer, "%d/%d (L)", static_cast<int>(sPage + 1), static_cast<int>(pages));

    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);

    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(polyOpa, gfx);
    POLY_OPA_DISP = gfx;

    CLOSE_DISPS(play->state.gfxCtx);
}
