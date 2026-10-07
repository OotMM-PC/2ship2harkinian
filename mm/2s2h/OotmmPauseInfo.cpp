#include "OotmmPauseInfo.h"

#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"
#include "OotmmTriforce.h"

#include <libultraship/bridge/OotmmInfoMenu.h>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

Ship::OotmmInfoMenu sMenu;

Vtx* QuadVertices(GraphicsContext* gfxCtx, float x, float y, float width, float height) {
    Vtx* v = static_cast<Vtx*>(GRAPH_ALLOC(gfxCtx, 4 * sizeof(Vtx)));
    const s16 left = static_cast<s16>(x);
    const s16 right = static_cast<s16>(x + width);
    const s16 top = static_cast<s16>(y);
    const s16 bottom = static_cast<s16>(y - height);
    const s16 s = static_cast<s16>(width * 32.0f);
    const s16 t = static_cast<s16>(height * 32.0f);
    const s16 xs[4] = { left, right, right, left };
    const s16 ys[4] = { top, top, bottom, bottom };
    const s16 ss[4] = { 0, s, s, 0 };
    const s16 ts[4] = { 0, 0, t, t };
    for (int i = 0; i < 4; i++) {
        v[i].v.ob[0] = xs[i];
        v[i].v.ob[1] = ys[i];
        v[i].v.ob[2] = 0;
        v[i].v.flag = 0;
        v[i].v.tc[0] = ss[i];
        v[i].v.tc[1] = ts[i];
        v[i].v.cn[0] = 0xFF;
        v[i].v.cn[1] = 0xFF;
        v[i].v.cn[2] = 0xFF;
        v[i].v.cn[3] = 0xFF;
    }
    return v;
}

} // namespace

static void DrawInfoMenu(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    const Ship::OotmmMenuTriforce triforce{ static_cast<uint32_t>(OotmmTriforce_Count()),
                                            static_cast<uint32_t>(OotmmTriforce_DisplayMax()) };
    const auto quads = sMenu.Layout(OotmmSession_GetState(), OotmmIpc_GetInventory(), triforce);

    OPEN_DISPS(gfxCtx);
    Gfx_SetupDL42_Opa(gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, 255);
    Vtx* background = QuadVertices(gfxCtx, Ship::OotmmInfoMenu::kLeft, Ship::OotmmInfoMenu::kTop,
                                   Ship::OotmmInfoMenu::kWidth, Ship::OotmmInfoMenu::kHeight);
    gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(background), 4, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 2, 1, 0, 0, 3, 2, 0);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (const Ship::OotmmMenuQuad& quad : quads) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, (quad.Color >> 24) & 0xFF, (quad.Color >> 16) & 0xFF,
                        (quad.Color >> 8) & 0xFF, quad.Color & 0xFF);
        float width = Ship::OotmmInfoMenu::kIconSize;
        float height = Ship::OotmmInfoMenu::kIconSize;
        if (quad.IsIcon) {
            gDPLoadTextureBlock(POLY_OPA_DISP++, Ship::OotmmInfoMenu::IconTexture(quad.Icon), G_IM_FMT_RGBA,
                                G_IM_SIZ_16b, 12, 12, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                                G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        } else {
            width = Ship::OotmmInfoMenu::kGlyphWidth;
            height = Ship::OotmmInfoMenu::kGlyphHeight;
            gDPLoadTextureBlock(POLY_OPA_DISP++, Ship::OotmmInfoMenu::GlyphTexture(quad.Glyph), G_IM_FMT_IA,
                                G_IM_SIZ_8b, 8, 12, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                                G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        }
        Vtx* vertices = QuadVertices(gfxCtx, quad.X, quad.Y, width, height);
        gSPVertex(POLY_OPA_DISP++, reinterpret_cast<uintptr_t>(vertices), 4, 0);
        gSP2Triangles(POLY_OPA_DISP++, 0, 2, 1, 0, 0, 3, 2, 0);
    }
    CLOSE_DISPS(gfxCtx);
}

extern "C" int OotmmPauseInfo_Update(PlayState* play) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    PauseContext* pauseCtx = &play->pauseCtx;
    const auto& state = OotmmSession_GetState();
    if (pauseCtx->state == PAUSE_STATE_MAIN && pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE &&
        CHECK_BTN_ANY(CONTROLLER1(&play->state)->press.button, BTN_L | BTN_CUP)) {
        Audio_PlaySfx(sMenu.Next(state, OotmmIpc_GetInventory()) ? NA_SE_SY_CURSOR : NA_SE_SY_ERROR);
    }
    if (!sMenu.IsOpen()) {
        return 0;
    }
    if (sMenu.Scroll(state, CONTROLLER1(&play->state)->cur.stick_y / 128.0f)) {
        Audio_PlaySfx(NA_SE_SY_CURSOR);
    }
    return 1;
}

extern "C" int OotmmPauseInfo_Draw(PlayState* play) {
    if (!OotmmSession_IsActive() || !sMenu.IsOpen()) {
        return 0;
    }
    DrawInfoMenu(play);
    return 1;
}

extern "C" int OotmmPauseInfo_Screen(void) {
    return sMenu.Screen();
}

extern "C" int OotmmPauseInfo_Cursor(void) {
    return sMenu.Cursor();
}
