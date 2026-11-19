#include "OotmmItemCycle.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
#include "gfx.h"
#include "sys_matrix.h"
#include "archives/icon_item_static/icon_item_static_yar.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"

#include "2s2h/BenGui/CosmeticEditor.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmItemApply.h"

#define CYCLE_ANIM_FRAMES 5

typedef struct CycleSlot {
    u8 slot;
    u8 showLeft;
    u8 showRight;
    u8 left;
    u8 right;
    u8 animTimer;
} CycleSlot;

static Vtx sNeighborVtx[] = {
    VTX(-48, 16, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-16, 16, 0, 32 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-48, -16, 0, 0 << 5, 32 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-16, -16, 0, 32 << 5, 32 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(16, 16, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(48, 16, 0, 32 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(16, -16, 0, 0 << 5, 32 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(48, -16, 0, 32 << 5, 32 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
};

static Vtx sCircleVtx[] = {
    VTX(-56, 24, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-8, 24, 0, 48 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-56, -24, 0, 0 << 5, 48 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-8, -24, 0, 48 << 5, 48 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(8, 24, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(56, 24, 0, 48 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(8, -24, 0, 0 << 5, 48 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(56, -24, 0, 48 << 5, 48 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
};

static Vtx sAButtonVtx[] = {
    VTX(-18, 12, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(18, 12, 0, 24 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-18, -12, 0, 0 << 5, 16 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(18, -12, 0, 24 << 5, 16 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
};

static CycleSlot sSlots[] = { { SLOT_TRADE_DEED }, { SLOT_TRADE_KEY_MAMA }, { SLOT_TRADE_COUPLE } };
static s32 sNeighborsValid = false;
static u32 sNeighborsFrame;
static s32 sOpenSlot = -1;
static u32 sUpdateFrame;

static void RefreshNeighbors(PlayState* play) {
    s32 i;

    if (sNeighborsValid && (sNeighborsFrame == play->state.frames)) {
        return;
    }
    for (i = 0; i < ARRAY_COUNT(sSlots); i++) {
        CycleSlot* cycle = &sSlots[i];
        u8 item = gSaveContext.save.saveInfo.inventory.items[cycle->slot];

        cycle->showLeft = cycle->showRight = false;
        if ((item != ITEM_NONE) && OotmmItemApply_TradeCycleNeighbors(cycle->slot, &cycle->left, &cycle->right)) {
            cycle->showLeft = (cycle->left != ITEM_NONE) && (cycle->left != item);
            cycle->showRight = (cycle->right != ITEM_NONE) && (cycle->right != item) && (cycle->right != cycle->left);
        }
    }
    sNeighborsValid = true;
    sNeighborsFrame = play->state.frames;
}

static CycleSlot* FindSlot(PlayState* play, u8 slot) {
    s32 i;

    RefreshNeighbors(play);
    for (i = 0; i < ARRAY_COUNT(sSlots); i++) {
        if (sSlots[i].slot == slot) {
            return &sSlots[i];
        }
    }
    return NULL;
}

static void DrawNeighbor(PlayState* play, u8 item, u16 point) {
    ItemId itemId = item;
    u8 restricted =
        GameInteractor_Should(VB_ITEM_BE_RESTRICTED, !gPlayerFormItemRestrictions[GET_PLAYER_FORM][item], &itemId);

    OPEN_DISPS(play->state.gfxCtx);

    if (restricted) {
        gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
        gSPGrayscale(POLY_OPA_DISP++, true);
    }
    KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx, gItemIcons[item], 32, 32, point);
    if (restricted) {
        gSPGrayscale(POLY_OPA_DISP++, false);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSlot(PlayState* play, CycleSlot* cycle) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 open = OotmmItemCycle_Active(play) && (sOpenSlot == cycle->slot);
    Vtx* itemVtx = &pauseCtx->itemVtx[cycle->slot * 4];
    f32 animScale;

    if (open) {
        if (cycle->animTimer < CYCLE_ANIM_FRAMES) {
            cycle->animTimer++;
        }
    } else if (cycle->animTimer > 0) {
        cycle->animTimer--;
    }

    if (!cycle->showLeft && !cycle->showRight) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Translate(itemVtx[0].v.ob[0] + ((itemVtx[3].v.ob[0] - itemVtx[0].v.ob[0]) / 2),
                     itemVtx[0].v.ob[1] + ((itemVtx[3].v.ob[1] - itemVtx[0].v.ob[1]) / 2), 0.0f, MTXMODE_APPLY);

    animScale = (f32)(CYCLE_ANIM_FRAMES - cycle->animTimer) / CYCLE_ANIM_FRAMES;
    if (!open || (cycle->animTimer < CYCLE_ANIM_FRAMES)) {
        f32 scale = 1.0f - (0.675f * animScale);

        Matrix_Translate(0.0f, -15.0f * animScale, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(scale, scale, 1.0f, MTXMODE_APPLY);
    }

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);

    if (!open && (cycle->animTimer == 0) && (pauseCtx->cursorSlot[PAUSE_ITEM] == cycle->slot) &&
        (pauseCtx->cursorSpecialPos == 0)) {
        gSPVertex(POLY_OPA_DISP++, sAButtonVtx, 4, 0);
        gDPSetPrimColorOverride(POLY_OPA_DISP++, 0, 0, 100, 200, 255, pauseCtx->alpha, COSMETIC_ELEMENT_A_BUTTON);
        gDPLoadTextureBlock(POLY_OPA_DISP++, gABtnSymbolTex, G_IM_FMT_IA, G_IM_SIZ_8b, 24, 16, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    }

    if (open) {
        gSPVertex(POLY_OPA_DISP++, sCircleVtx, 8, 0);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, pauseCtx->alpha * (1.0f - animScale));
        gDPLoadTextureBlock_4b(POLY_OPA_DISP++, gPausePromptCursorTex, G_IM_FMT_I, 48, 48, 0,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);
        if (cycle->showLeft) {
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
        }
        if (cycle->showRight) {
            gSP1Quadrangle(POLY_OPA_DISP++, 4, 6, 7, 5, 0);
        }
    }

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gSPVertex(POLY_OPA_DISP++, sNeighborVtx, 8, 0);
    if (cycle->showLeft) {
        DrawNeighbor(play, cycle->left, 0);
    }
    if (cycle->showRight) {
        DrawNeighbor(play, cycle->right, 4);
    }

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

int OotmmItemCycle_Active(PlayState* play) {
    if ((sOpenSlot >= 0) && (((play->state.frames - sUpdateFrame) > 1) ||
                             (play->pauseCtx.cursorSlot[PAUSE_ITEM] != sOpenSlot))) {
        sOpenSlot = -1;
    }
    return sOpenSlot >= 0;
}

int OotmmItemCycle_CanCycle(PlayState* play, uint8_t slot) {
    CycleSlot* cycle = FindSlot(play, slot);

    return (cycle != NULL) && (cycle->showLeft || cycle->showRight);
}

void OotmmItemCycle_Update(PlayState* play, uint8_t slot) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 open = OotmmItemCycle_Active(play) && (sOpenSlot == slot);
    s32 direction = 0;

    sUpdateFrame = play->state.frames;
    if (!OotmmItemCycle_CanCycle(play, slot)) {
        sOpenSlot = -1;
        return;
    }

    if ((play->msgCtx.msgLength == 0) && CHECK_BTN_ALL(CONTROLLER1(&play->state)->press.button, BTN_A)) {
        sOpenSlot = open ? -1 : slot;
        Audio_PlaySfx(NA_SE_SY_DECIDE);
        return;
    }
    if (!open) {
        return;
    }

    pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_BLUE;
    if ((pauseCtx->stickAdjX > 30) || (pauseCtx->stickAdjY > 30)) {
        direction = 1;
    } else if ((pauseCtx->stickAdjX < -30) || (pauseCtx->stickAdjY < -30)) {
        direction = -1;
    }
    if ((direction != 0) && OotmmItemApply_StepTradeItem(slot, direction)) {
        sNeighborsValid = false;
        Audio_PlaySfx(NA_SE_SY_CURSOR);
    }
}

void OotmmItemCycle_Draw(PlayState* play) {
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    CLOSE_DISPS(play->state.gfxCtx);

    RefreshNeighbors(play);
    for (i = 0; i < ARRAY_COUNT(sSlots); i++) {
        DrawSlot(play, &sSlots[i]);
    }
}
