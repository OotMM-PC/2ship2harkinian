#include "OotmmItemModels.h"

#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

#include <iterator>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "z64.h"

Gfx* ResourceMgr_LoadGfxByName(const char* path);
}

namespace {

constexpr const char* kOcarinaButtonModels[] = {
    "__OTR__objects/object_ocarina_a_button/gOcarinaAButtonDL",
    "__OTR__objects/object_ocarina_c_up_button/gOcarinaCUpButtonDL",
    "__OTR__objects/object_ocarina_c_down_button/gOcarinaCDownButtonDL",
    "__OTR__objects/object_ocarina_c_left_button/gOcarinaCLeftButtonDL",
    "__OTR__objects/object_ocarina_c_right_button/gOcarinaCRightButtonDL",
};

bool DrawOcarinaButton(PlayState* play, uint32_t variant) {
    if (variant >= std::size(kOcarinaButtonModels)) {
        return false;
    }
    Gfx* displayList = ResourceMgr_LoadGfxByName(kOcarinaButtonModels[variant]);
    if (displayList == nullptr) {
        return false;
    }

    GraphicsContext* gfxCtx = play->state.gfxCtx;
    FrameInterpolation_RecordOpenChild(kOcarinaButtonModels[variant], 0);
    Gfx_SetupDL25_Opa(gfxCtx);
    Matrix_Push();
    Matrix_Scale(0.6f, 0.6f, 0.6f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(gfxCtx->polyOpa.p++, gfxCtx);
    Matrix_Pop();
    gSPDisplayList(gfxCtx->polyOpa.p++, displayList);
    FrameInterpolation_RecordCloseChild();
    return true;
}

} // namespace

bool OotmmItemModel_Draw(PlayState* play, const Ship::OotmmItemDefinition& item) {
    if (play == nullptr) {
        return false;
    }
    switch (item.ModelKind) {
        case Ship::OotmmItemModelKind::OcarinaButton:
            return DrawOcarinaButton(play, item.ModelVariant);
        default:
            return false;
    }
}
