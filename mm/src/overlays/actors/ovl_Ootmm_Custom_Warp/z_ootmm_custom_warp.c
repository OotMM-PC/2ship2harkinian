#include "z_ootmm_custom_warp.h"

#include "2s2h/OotmmRegionState.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"

#define FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED)

void OotmmCustomWarp_Init(Actor* thisx, PlayState* play);
void OotmmCustomWarp_Update(Actor* thisx, PlayState* play);
void OotmmCustomWarp_Draw(Actor* thisx, PlayState* play);

ActorProfile Ootmm_Custom_Warp_Profile = {
    /**/ ACTOR_OOTMM_CUSTOM_WARP,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OotmmCustomWarp),
    /**/ OotmmCustomWarp_Init,
    /**/ NULL,
    /**/ OotmmCustomWarp_Update,
    /**/ OotmmCustomWarp_Draw,
};

void OotmmCustomWarp_Init(Actor* thisx, PlayState* play) {
    thisx->room = -1;
}

void OotmmCustomWarp_Update(Actor* thisx, PlayState* play) {
    if (Actor_TalkOfferAccepted(thisx, &play->state)) {
        OotmmRegionState_TriggerWarp(play, thisx->params);
    } else {
        Actor_OfferTalk(thisx, play, 30.0f);
    }
}

void OotmmCustomWarp_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_Scale(0.003f, 0.003f, 0.003f, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                (-play->gameplayFrames & 0x7F) << 2, 0x20, 0x80));
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 0, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 255);
    gSPDisplayList(POLY_XLU_DISP++, gEffFire1DL);

    CLOSE_DISPS(play->state.gfxCtx);
}
