#include "OotmmCsmc.h"

// OPEN_DISPS block-declares the FrameInterpolation functions; this include keeps the
// extern "C" declarations visible so those redeclarations inherit C linkage.
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmCsmc.h>

#include <string>
#include <unordered_map>

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

// Test chests placed by the debug menu, shown as a fixed class.
std::unordered_map<Actor*, Ship::OotmmCsmcClass> sChestOverrides;

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
    Ship::OotmmCsmcClass itemClass;
    if (const auto override = sChestOverrides.find(&chest->dyna.actor);
        override != sChestOverrides.end()) {
        itemClass = override->second;
    } else {
        const auto* placement = ChestPlacement(play, chest);
        itemClass = placement != nullptr ? ClassFor(*placement) : Ship::OotmmCsmcClass::Normal;
    }

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

bool Revealing() {
    const auto mode = Ship::OotmmCsmc_Mode(OotmmSession_GetState());
    return mode == Ship::OotmmCsmcMode::Always || (mode == Ship::OotmmCsmcMode::Agony && HasAgonyStone());
}

Ship::OotmmCsmcClass RevealedClass(const Ship::OotmmPlacement& placement) {
    return Ship::OotmmCsmc_Classify(OotmmSession_GetState(), Ship::OotmmGame::Mm, placement, true);
}

Ship::OotmmCsmcClass ContainerClass(const Ship::OotmmPlacement& placement) {
    return Revealing() ? RevealedClass(placement) : Ship::OotmmCsmcClass::Major;
}

void* LoadCsmcDL(const char* family, const char* category) {
    const std::string path = std::string("__OTR__objects/ootmm_csmc/gCsmc") + family + category + "DL";
    return ResourceMgr_LoadGfxByName(path.c_str());
}

const char* ContainerCategory(Ship::OotmmCsmcClass itemClass) {
    switch (itemClass) {
        case Ship::OotmmCsmcClass::BossKey:
            return "BossKey";
        case Ship::OotmmCsmcClass::Major:
            return "Major";
        case Ship::OotmmCsmcClass::Key:
            return "Key";
        case Ship::OotmmCsmcClass::Spider:
            return "Spider";
        case Ship::OotmmCsmcClass::Fairy:
            return "Fairy";
        case Ship::OotmmCsmcClass::Heart:
            return "Heart";
        case Ship::OotmmCsmcClass::Soul:
            return "Soul";
        case Ship::OotmmCsmcClass::MapCompass:
            return "Map";
        default:
            return nullptr;
    }
}

} // namespace

void* OotmmCsmc_ContainerDL(const Ship::OotmmPlacement& placement, const char* family) {
    const char* category = ContainerCategory(ContainerClass(placement));
    return category != nullptr ? LoadCsmcDL(family, category) : nullptr;
}

void* OotmmCsmc_SkulltulaDL(const Ship::OotmmPlacement* placement, const char* part) {
    if (placement == nullptr || !Revealing() || !OotmmSession_GetState().GetBoolSetting("csmcSkulltula", false)) {
        return nullptr;
    }
    const auto itemClass = RevealedClass(*placement);
    if (itemClass == Ship::OotmmCsmcClass::Major) {
        return nullptr;
    }
    const char* category = itemClass == Ship::OotmmCsmcClass::Normal ? "Normal" : ContainerCategory(itemClass);
    return category != nullptr ? LoadCsmcDL(part, category) : nullptr;
}

void* OotmmCsmc_CowRingDL(const Ship::OotmmPlacement* placement, bool collected, void* vanilla) {
    if (placement == nullptr) {
        return nullptr;
    }
    const bool renewable = Ship::OotmmCsmc_IsRenewable(placement->ItemId);
    if (collected && !renewable) {
        return nullptr;
    }
    if (!Revealing() || !OotmmSession_GetState().GetBoolSetting("csmcCow", false)) {
        return vanilla;
    }
    const auto itemClass = RevealedClass(*placement);
    const char* category = itemClass == Ship::OotmmCsmcClass::Normal ? (renewable ? "Renewable" : "Normal")
                                                                      : ContainerCategory(itemClass);
    void* dl = category != nullptr ? LoadCsmcDL("CowRing", category) : nullptr;
    return dl != nullptr ? dl : vanilla;
}

uint32_t OotmmCsmc_TreeColor(const Ship::OotmmPlacement* placement) {
    if (placement == nullptr) {
        return 0x32AA46;
    }
    switch (ContainerClass(*placement)) {
        case Ship::OotmmCsmcClass::BossKey:
            return 0x0000FF;
        case Ship::OotmmCsmcClass::Major:
            return 0xFFFF00;
        case Ship::OotmmCsmcClass::Key:
            return 0x444444;
        case Ship::OotmmCsmcClass::Spider:
            return 0xFFFFFF;
        case Ship::OotmmCsmcClass::Fairy:
            return 0xFF7AFB;
        case Ship::OotmmCsmcClass::Heart:
            return 0xFF0000;
        case Ship::OotmmCsmcClass::Soul:
            return 0x340B9C;
        case Ship::OotmmCsmcClass::MapCompass:
            return 0xC75000;
        default:
            return 0x32AA46;
    }
}

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

void OotmmCsmc_SetChestOverride(Actor* chest, Ship::OotmmCsmcClass itemClass) {
    sChestOverrides[chest] = itemClass;
    chest->draw = CsmcChestDraw;
    const bool large = itemClass == Ship::OotmmCsmcClass::Major ||
                       itemClass == Ship::OotmmCsmcClass::BossKey;
    Actor_SetScale(chest, large ? 0.01f : 0.0075f);
    Actor_SetFocus(chest, large ? 40.0f : 20.0f);
}

void OotmmCsmc_ClearChestOverrides() {
    sChestOverrides.clear();
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

void OotmmCsmc_DrawGlitter(PlayState* play, Actor* actor, const Ship::OotmmPlacement& placement) {
    static Vtx sVtx[] = {
        VTX(-10, -10, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF),
        VTX(10, -10, 0, 0x400, 0, 0xFF, 0xFF, 0xFF, 0xFF),
        VTX(10, 10, 0, 0x400, 0x400, 0xFF, 0xFF, 0xFF, 0xFF),
        VTX(-10, 10, 0, 0, 0x400, 0xFF, 0xFF, 0xFF, 0xFF),
    };
    const float distance = actor->xzDistToPlayer;
    if (distance > 1000.0f) {
        return;
    }
    const float alpha = distance <= 300.0f ? 1.0f : (1000.0f - distance) / 700.0f;
    const float alphaNoZ = distance > 600.0f ? 0.0f : distance <= 300.0f ? 1.0f : (600.0f - distance) / 300.0f;

    const auto& state = OotmmSession_GetState();
    const auto mode = Ship::OotmmCsmc_Mode(state);
    const bool enabled = mode == Ship::OotmmCsmcMode::Always ||
                         (mode == Ship::OotmmCsmcMode::Agony && HasAgonyStone());
    const uint32_t color = Ship::OotmmCsmc_GlitterColor(
        Ship::OotmmCsmc_Classify(state, Ship::OotmmGame::Mm, placement, true), enabled);
    const u8 r = (color >> 16) & 0xFF;
    const u8 g = (color >> 8) & 0xFF;
    const u8 b = color & 0xFF;

    Matrix_Translate(actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Translate(0.0f, 20.0f, 0.0f, MTXMODE_APPLY);

    OPEN_DISPS(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_2CYCLE);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock_4b(POLY_XLU_DISP++, Ship::OotmmCsmc_GlitterFrame(play->gameplayFrames), G_IM_FMT_I, 32, 32, 0,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED, 0, 0, 0,
                      COMBINED);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, r, g, b, static_cast<u8>(alpha * 0.9f * 255));
    gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(sVtx), 4, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 2, 1, 0, 0, 3, 2, 0);
    if (alphaNoZ > 0.0f) {
        gSPClearGeometryMode(POLY_XLU_DISP++, G_ZBUFFER);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, r, g, b, static_cast<u8>(alphaNoZ * 0.25f * 255));
        gSP2Triangles(POLY_XLU_DISP++, 0, 2, 1, 0, 0, 3, 2, 0);
        gSPSetGeometryMode(POLY_XLU_DISP++, G_ZBUFFER);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
