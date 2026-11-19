#include "OotmmSongPage.h"

#include "OotmmItemPage.h"
#include "OotmmSession.h"
#include "OotmmSongs.h"

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"

extern PlayState* gPlayState;

uint8_t ResourceMgr_FileExists(const char* resName);
}

namespace {

constexpr int kSongSlots = QUEST_SONG_SUN - QUEST_SONG_SONATA + 1;
constexpr int kIndicatorColumn = 6;
constexpr int kIndicatorRow = 6;

constexpr int32_t kSlotSongs[kSongSlots] = {
    OOTMM_SONG_MINUET,   OOTMM_SONG_BOLERO, OOTMM_SONG_SERENADE, OOTMM_SONG_REQUIEM,
    OOTMM_SONG_NOCTURNE, OOTMM_SONG_NONE,   OOTMM_SONG_ZELDA,    OOTMM_SONG_NONE,
    OOTMM_SONG_SARIA,    OOTMM_SONG_NONE,   OOTMM_SONG_PRELUDE,  OOTMM_SONG_NONE,
};

constexpr int kSlotColors[kSongSlots] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 5, 11 };

constexpr const char* kNameTextures[OOTMM_SONG_COUNT] = {
    "__OTR__textures/ot_item_name_static/gMinuetOfForestItemNameENGTex",
    "__OTR__textures/ot_item_name_static/gBoleroOfFireItemNameENGTex",
    "__OTR__textures/ot_item_name_static/gSerenadeOfWaterItemNameENGTex",
    "__OTR__textures/ot_item_name_static/gRequiemOfSpiritItemNameENGTex",
    "__OTR__textures/ot_item_name_static/gNocturneOfShadowItemNameENGTex",
    "__OTR__textures/ot_item_name_static/gPreludeOfLightItemNameENGTex",
    "__OTR__textures/ot_item_name_static/gZeldasLullabyItemNameENGTex",
    "__OTR__textures/ot_item_name_static/gSariasSongItemNameENGTex",
};

bool sSecondPage = false;

bool IsSongPoint(int point) {
    return point >= QUEST_SONG_SONATA && point <= QUEST_SONG_SUN;
}

bool Browsing(const PauseContext* pauseCtx) {
    return OotmmSession_IsActive() && pauseCtx->pageIndex == PAUSE_QUEST && pauseCtx->state == PAUSE_STATE_MAIN &&
           (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE ||
            pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG) &&
           !pauseCtx->itemDescriptionOn && pauseCtx->debugEditor == DEBUG_EDITOR_NONE;
}

bool CursorOnSong(const PauseContext* pauseCtx) {
    return pauseCtx->cursorSpecialPos == 0 && IsSongPoint(pauseCtx->cursorSlot[PAUSE_QUEST]);
}

void ShowPage(PauseContext* pauseCtx, bool secondPage) {
    sSecondPage = secondPage;
    if (pauseCtx->pageIndex == PAUSE_QUEST) {
        pauseCtx->namedItem = PAUSE_ITEM_NONE;
    }
    if (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG) {
        pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
    }
}

const char* NameTexture(int32_t song) {
    if (song == OOTMM_SONG_NONE || !ResourceMgr_FileExists(kNameTextures[song - 1])) {
        return nullptr;
    }
    return kNameTextures[song - 1];
}

} // namespace

extern "C" int OotmmSongPage_Active(void) {
    if (!sSecondPage) {
        return 0;
    }
    if (gPlayState == nullptr) {
        sSecondPage = false;
    } else if (!OotmmSession_IsActive() || gPlayState->pauseCtx.state != PAUSE_STATE_MAIN ||
               gPlayState->pauseCtx.pageIndex != PAUSE_QUEST) {
        ShowPage(&gPlayState->pauseCtx, false);
    }
    return sSecondPage;
}

extern "C" void OotmmSongPage_ConsumePageToggle(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const bool secondPage = OotmmSongPage_Active();

    if (!Browsing(pauseCtx) || !CursorOnSong(pauseCtx) ||
        !CHECK_BTN_ALL(CONTROLLER1(&play->state)->press.button, BTN_L)) {
        return;
    }
    if (!secondPage && OotmmSongs_OwnedMask() == 0) {
        return;
    }

    ShowPage(pauseCtx, !secondPage);
    Audio_PlaySfx(NA_SE_SY_CURSOR);
}

extern "C" int32_t OotmmSongPage_SlotSong(int slot) {
    if (slot < 0 || slot >= kSongSlots) {
        return OOTMM_SONG_NONE;
    }
    const int32_t song = kSlotSongs[slot];
    return song != OOTMM_SONG_NONE && (OotmmSongs_OwnedMask() & (1 << song)) ? song : OOTMM_SONG_NONE;
}

extern "C" int OotmmSongPage_ColorSlot(int slot) {
    return OotmmSongPage_Active() && slot >= 0 && slot < kSongSlots ? kSlotColors[slot] : slot;
}

extern "C" int32_t OotmmSongPage_HoverSong(PlayState* play) {
    const PauseContext* pauseCtx = &play->pauseCtx;

    if (!OotmmSongPage_Active() || pauseCtx->cursorSpecialPos != 0) {
        return OOTMM_SONG_NONE;
    }
    return OotmmSongPage_SlotSong(pauseCtx->cursorSlot[PAUSE_QUEST] - QUEST_SONG_SONATA);
}

extern "C" uint16_t OotmmSongPage_CursorItem(PlayState* play, uint16_t nativeItem) {
    const int point = play->pauseCtx.cursorPoint[PAUSE_QUEST];

    if (!OotmmSongPage_Active() || !IsSongPoint(point)) {
        return nativeItem;
    }
    return NameTexture(OotmmSongPage_SlotSong(point - QUEST_SONG_SONATA)) != nullptr
               ? ITEM_SONG_SONATA + (point - QUEST_SONG_SONATA)
               : PAUSE_ITEM_NONE;
}

extern "C" void OotmmSongPage_EnterHover(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (OotmmSongPage_HoverSong(play) == OOTMM_SONG_NONE || play->msgCtx.msgLength != 0) {
        return;
    }

    pauseCtx->ocarinaButtonsY[OCARINA_BTN_A] = -62;
    pauseCtx->ocarinaButtonsY[OCARINA_BTN_C_DOWN] = -56;
    pauseCtx->ocarinaButtonsY[OCARINA_BTN_C_RIGHT] = -49;
    pauseCtx->ocarinaButtonsY[OCARINA_BTN_C_LEFT] = -46;
    pauseCtx->ocarinaButtonsY[OCARINA_BTN_C_UP] = -41;
    pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG;
}

extern "C" const char* OotmmSongPage_NameTexture(void) {
    return gPlayState != nullptr ? NameTexture(OotmmSongPage_HoverSong(gPlayState)) : nullptr;
}

extern "C" void OotmmSongPage_DrawPageIndicator(PlayState* play) {
    const PauseContext* pauseCtx = &play->pauseCtx;

    if (!Browsing(pauseCtx)) {
        return;
    }

    const bool secondPage = OotmmSongPage_Active();
    if (!secondPage && (!CursorOnSong(pauseCtx) || OotmmSongs_OwnedMask() == 0)) {
        return;
    }
    OotmmItemPage_DrawPageNumber(play, secondPage ? 2 : 1, 2, kIndicatorColumn, kIndicatorRow);
}
