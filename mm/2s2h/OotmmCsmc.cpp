#include "OotmmCsmc.h"

#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "OotmmIpc.h"
#include "OotmmSession.h"
#include "assets/2s2h_assets.h"

#include <libultraship/bridge/OotmmCsmc.h>

#include <cstring>

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
}

namespace {

// Vanilla chest display lists with their texture loads redirected to segments
// 0x09 (corner) and 0x0A (lock), so each chest can bind its own art per draw.
Gfx sChestBaseDL[42];
Gfx sChestLidDL[48];
Gfx sChestBaseOrnateDL[41];
Gfx sChestLidOrnateDL[38];

struct ChestLook {
    const char* corner;
    const char* lock;
    // The ornate mesh carries the decorated braces the variant art was drawn for.
    bool ornate;
};

ChestLook LookFor(Ship::OotmmCsmcClass itemClass) {
    switch (itemClass) {
        case Ship::OotmmCsmcClass::BossKey:
            return { gBoxChestCornerOrnateTex, gBoxChestLockOrnateTex, true };
        // Souls borrow the major look until they have art of their own.
        case Ship::OotmmCsmcClass::Major:
        case Ship::OotmmCsmcClass::Soul:
            return { gBoxChestCornerMajorTex, gBoxChestLockMajorTex, false };
        case Ship::OotmmCsmcClass::Key:
            return { gBoxChestCornerSmallKeyTex, gBoxChestLockSmallKeyTex, true };
        case Ship::OotmmCsmcClass::Spider:
            return { gBoxChestCornerSkullTokenTex, gBoxChestLockSkullTokenTex, true };
        case Ship::OotmmCsmcClass::Fairy:
            return { gBoxChestCornerStrayFairyTex, gBoxChestLockStrayFairyTex, true };
        case Ship::OotmmCsmcClass::Heart:
            return { gBoxChestCornerHealthTex, gBoxChestLockHealthTex, true };
        case Ship::OotmmCsmcClass::MapCompass:
            return { gBoxChestCornerLesserTex, gBoxChestLockLesserTex, false };
        default:
            return { gBoxChestCornerTex, gBoxChestLockTex, false };
    }
}

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

void CsmcPostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* actor,
                      Gfx** gfx) {
    EnBox* chest = (EnBox*)actor;
    const auto* placement = ChestPlacement(play, chest);
    const ChestLook look =
        LookFor(placement ? ClassFor(*placement) : Ship::OotmmCsmcClass::Normal);

    gSPSegment((*gfx)++, 0x09, (uintptr_t)look.corner);
    gSPSegment((*gfx)++, 0x0A, (uintptr_t)look.lock);
    MATRIX_FINALIZE_AND_LOAD((*gfx)++, play->state.gfxCtx);

    if (limbIndex == OBJECT_BOX_CHEST_LIMB_01) {
        gSPDisplayList((*gfx)++, look.ornate ? sChestBaseOrnateDL : sChestBaseDL);
    } else if (limbIndex == OBJECT_BOX_CHEST_LIMB_03) {
        gSPDisplayList((*gfx)++, look.ornate ? sChestLidOrnateDL : sChestLidDL);
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

namespace {

// Redirects a display list's two texture loads to segments 0x09/0x0A. The command
// indices are fixed offsets into the vanilla chest display lists.
void RedirectTextureLoads(Gfx* dl, size_t cornerIndex, size_t lockIndex) {
    dl[cornerIndex] = gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 1, 0x09000000 | 1);
    dl[cornerIndex + 1] = gsDPNoOp();
    dl[lockIndex] = gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 1, 0x0A000000 | 1);
    dl[lockIndex + 1] = gsDPNoOp();
}

RegisterShipInitFunc sInitChestCopies(
    []() {
        Gfx* baseDL = ResourceMgr_LoadGfxByName(gBoxChestBaseDL);
        Gfx* lidDL = ResourceMgr_LoadGfxByName(gBoxChestLidDL);
        Gfx* baseOrnateDL = ResourceMgr_LoadGfxByName(gBoxChestBaseOrnateDL);
        Gfx* lidOrnateDL = ResourceMgr_LoadGfxByName(gBoxChestLidOrnateDL);
        if (baseDL == nullptr || lidDL == nullptr || baseOrnateDL == nullptr ||
            lidOrnateDL == nullptr) {
            return;
        }

        memcpy(sChestBaseDL, baseDL, sizeof(sChestBaseDL));
        RedirectTextureLoads(sChestBaseDL, 7, 28);
        memcpy(sChestLidDL, lidDL, sizeof(sChestLidDL));
        RedirectTextureLoads(sChestLidDL, 7, 26);
        memcpy(sChestBaseOrnateDL, baseOrnateDL, sizeof(sChestBaseOrnateDL));
        RedirectTextureLoads(sChestBaseOrnateDL, 7, 25);
        memcpy(sChestLidOrnateDL, lidOrnateDL, sizeof(sChestLidOrnateDL));
        sChestLidOrnateDL[7] =
            gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b_LOAD_BLOCK, 1, 0x09000000 | 1);
        sChestLidOrnateDL[8] = gsDPNoOp();
    },
    {});

} // namespace

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
