#include "OotmmCsmc.h"

// OPEN_DISPS block-declares the FrameInterpolation functions; this include keeps the
// extern "C" declarations visible so those redeclarations inherit C linkage.
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmCsmc.h>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
#include "assets/objects/object_box/object_box.h"

extern PlayState* gPlayState;

Gfx* EnBox_SetRenderMode1(GraphicsContext* gfxCtx);
Gfx* EnBox_SetRenderMode2(GraphicsContext* gfxCtx);
Gfx* EnBox_SetRenderMode3(GraphicsContext* gfxCtx);
Gfx* ResourceMgr_LoadGfxByName(const char* path);
uint8_t ResourceMgr_FileExists(const char* resName);
}

namespace {

bool HasAgonyStone() {
    return Ship::OotmmCsmc_MmHasAgony(OotmmSession_GetState(), OotmmIpc_GetInventory());
}

const Ship::OotmmPlacement* ChestPlacement(PlayState* play, EnBox* chest) {
    if (!OotmmSession_IsActive()) {
        return nullptr;
    }
    return OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm,
                                             Play_GetOriginalSceneId(play->sceneId), "chest",
                                             ENBOX_GET_CHEST_FLAG(&chest->dyna.actor));
}

Ship::OotmmCsmcClass ClassFor(const Ship::OotmmPlacement& placement) {
    const auto& state = OotmmSession_GetState();
    const auto mode = Ship::OotmmCsmc_Mode(state);
    const bool revealed = mode == Ship::OotmmCsmcMode::Always || HasAgonyStone();
    return Ship::OotmmCsmc_Classify(state, Ship::OotmmGame::Mm, placement, revealed);
}

// The launcher bakes these from OoTMM's own art into ootmm_assets.o2r.
const char* BakedName(Ship::OotmmCsmcClass itemClass, bool lid) {
    switch (itemClass) {
        case Ship::OotmmCsmcClass::Major:
            return lid ? "__OTR__objects/ootmm_csmc/gCsmcChestMajorLidDL"
                       : "__OTR__objects/ootmm_csmc/gCsmcChestMajorBodyDL";
        case Ship::OotmmCsmcClass::Key:
            return lid ? "__OTR__objects/ootmm_csmc/gCsmcChestKeyLidDL"
                       : "__OTR__objects/ootmm_csmc/gCsmcChestKeyBodyDL";
        case Ship::OotmmCsmcClass::Spider:
            return lid ? "__OTR__objects/ootmm_csmc/gCsmcChestSpiderLidDL"
                       : "__OTR__objects/ootmm_csmc/gCsmcChestSpiderBodyDL";
        case Ship::OotmmCsmcClass::Fairy:
            return lid ? "__OTR__objects/ootmm_csmc/gCsmcChestFairyLidDL"
                       : "__OTR__objects/ootmm_csmc/gCsmcChestFairyBodyDL";
        case Ship::OotmmCsmcClass::Heart:
            return lid ? "__OTR__objects/ootmm_csmc/gCsmcChestHeartLidDL"
                       : "__OTR__objects/ootmm_csmc/gCsmcChestHeartBodyDL";
        case Ship::OotmmCsmcClass::Soul:
            return lid ? "__OTR__objects/ootmm_csmc/gCsmcChestSoulLidDL"
                       : "__OTR__objects/ootmm_csmc/gCsmcChestSoulBodyDL";
        case Ship::OotmmCsmcClass::MapCompass:
            return lid ? "__OTR__objects/ootmm_csmc/gCsmcChestMapLidDL"
                       : "__OTR__objects/ootmm_csmc/gCsmcChestMapBodyDL";
        default:
            // BossKey and Normal use the game's own ornate and plain art.
            return nullptr;
    }
}

void DrawChestPart(PlayState* play, EnBox* chest, bool lid, Gfx** gfx) {
    const auto* placement = ChestPlacement(play, chest);
    const auto itemClass =
        placement != nullptr ? ClassFor(*placement) : Ship::OotmmCsmcClass::Normal;

    MATRIX_FINALIZE_AND_LOAD((*gfx)++, play->state.gfxCtx);
    if (const char* baked = BakedName(itemClass, lid);
        baked != nullptr && ResourceMgr_FileExists(baked)) {
        gSPDisplayList((*gfx)++, ResourceMgr_LoadGfxByName(baked));
    } else if (itemClass == Ship::OotmmCsmcClass::BossKey) {
        gSPDisplayList((*gfx)++, (Gfx*)(lid ? gBoxChestLidOrnateDL : gBoxChestBaseOrnateDL));
    } else {
        gSPDisplayList((*gfx)++, (Gfx*)(lid ? gBoxChestLidDL : gBoxChestBaseDL));
    }
}

void CsmcPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* actor,
                      Gfx** gfx) {
    EnBox* chest = (EnBox*)actor;
    if (limbIndex == OBJECT_BOX_CHEST_LIMB_01) {
        DrawChestPart(play, chest, false, gfx);
    } else if (limbIndex == OBJECT_BOX_CHEST_LIMB_03) {
        DrawChestPart(play, chest, true, gfx);
    }
}

} // namespace

// At global scope so OPEN_DISPS's block-scope FrameInterpolation declarations
// redeclare the extern "C" functions instead of minting namespace-local ones.
static void CsmcChestDraw(Actor* thisx, PlayState* play) {
    EnBox* chest = (EnBox*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    if (chest->unk_1F4.unk_10 != NULL) {
        chest->unk_1F4.unk_10(&chest->unk_1F4, play);
    }
    if (((chest->alpha == 255) && (chest->type != ENBOX_TYPE_BIG_INVISIBLE) &&
         (chest->type != ENBOX_TYPE_SMALL_INVISIBLE)) ||
        (!CHECK_FLAG_ALL(chest->dyna.actor.flags, ACTOR_FLAG_REACT_TO_LENS) &&
         ((chest->type == ENBOX_TYPE_BIG_INVISIBLE) || (chest->type == ENBOX_TYPE_SMALL_INVISIBLE)))) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)EnBox_SetRenderMode1(play->state.gfxCtx));
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        POLY_OPA_DISP = SkelAnime_Draw(play, chest->skelAnime.skeleton, chest->skelAnime.jointTable,
                                       NULL, CsmcPostLimbDraw, &chest->dyna.actor, POLY_OPA_DISP);
    } else if (chest->alpha != 0) {
        gDPPipeSync(POLY_XLU_DISP++);
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, chest->alpha);
        if ((chest->type == ENBOX_TYPE_BIG_INVISIBLE) || (chest->type == ENBOX_TYPE_SMALL_INVISIBLE)) {
            gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)EnBox_SetRenderMode3(play->state.gfxCtx));
        } else {
            gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)EnBox_SetRenderMode2(play->state.gfxCtx));
        }
        POLY_XLU_DISP = SkelAnime_Draw(play, chest->skelAnime.skeleton, chest->skelAnime.jointTable,
                                       NULL, CsmcPostLimbDraw, &chest->dyna.actor, POLY_XLU_DISP);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void OotmmCsmc_Init() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorInit>(
        ACTOR_EN_BOX, [](Actor* actor) {
            // The moon Goron trial keeps its vanilla chests.
            if (gPlayState == nullptr || gPlayState->sceneId == SCENE_LAST_GORON) {
                return;
            }
            EnBox* chest = (EnBox*)actor;
            const auto* placement = ChestPlacement(gPlayState, chest);
            if (placement == nullptr ||
                Ship::OotmmCsmc_Mode(OotmmSession_GetState()) == Ship::OotmmCsmcMode::Never) {
                return;
            }

            actor->draw = CsmcChestDraw;
            const auto itemClass = ClassFor(*placement);
            const bool large = itemClass == Ship::OotmmCsmcClass::Major ||
                               itemClass == Ship::OotmmCsmcClass::BossKey;
            Actor_SetScale(actor, large ? 0.01f : 0.0075f);
            Actor_SetFocus(actor, large ? 40.0f : 20.0f);
        });
}
