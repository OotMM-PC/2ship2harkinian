#include "OotmmAgony.h"

#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmCsmc.h>

#include <algorithm>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"

extern PlayState* gPlayState;
int16_t OTRGetRectDimensionFromLeftEdge(float v);
}

namespace {

constexpr const char* kAgonyIcon = "__OTR__textures/ot_icon_item_24_static/gQuestIconStoneOfAgonyTex";
constexpr float kIconSize = 16.0f;
constexpr float kTextureSize = 24.0f;
s16 sAlpha = 0;
float sOffset = 0.0f;

bool AgonyRumbles() {
    const auto& state = OotmmSession_GetState();
    return !state.GetBoolSetting("stoneAgonyMm", false) ||
           Ship::OotmmCsmc_MmHasAgony(state, OotmmIpc_GetInventory());
}

} // namespace

static void DrawAgonyIcon() {
    if (gPlayState == nullptr || !OotmmSession_IsActive()) {
        return;
    }
    const s16 value = sAlpha;
    sAlpha = std::max(0, sAlpha - 10);
    const int alpha = std::min<int>(value, 255);
    if (alpha <= 0) {
        sOffset = 0.0f;
        return;
    }
    const float target = Math_SinS(static_cast<s16>((gPlayState->state.frames & 7) << 13)) * (value * 0.03f);
    sOffset = sOffset * 0.5f + target * 0.5f;
    const int left = OTRGetRectDimensionFromLeftEdge(26.0f + sOffset);
    const int top = 220;
    const int size = static_cast<int>(kIconSize);
    const int step = static_cast<int>((1 << 10) * kTextureSize / kIconSize);

    OPEN_DISPS(gPlayState->state.gfxCtx);
    Gfx_SetupDL39_Overlay(gPlayState->state.gfxCtx);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0x80, 255, 255, 255, alpha);
    gDPLoadTextureBlock(OVERLAY_DISP++, kAgonyIcon, G_IM_FMT_RGBA, G_IM_SIZ_32b, 24, 24, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, left << 2, top << 2, (left + size) << 2, (top + size) << 2,
                            G_TX_RENDERTILE, 0, 0, step, step);
    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

extern "C" void OotmmAgony_Init(void) {
    REGISTER_VB_SHOULD(VB_RUMBLE_FOR_SECRET, {
        if (!*should || !OotmmSession_IsActive()) {
            return;
        }
        if (!AgonyRumbles()) {
            *should = false;
            return;
        }
        sAlpha = 300;
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawWorldEnd>(DrawAgonyIcon);
}
