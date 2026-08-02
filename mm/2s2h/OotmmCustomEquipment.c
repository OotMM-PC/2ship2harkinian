#include "OotmmCustomItems.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
#include "gfx.h"
#include "sys_matrix.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"

#include "OotmmCustomEquipment.h"
#include "OotmmSession.h"

Gfx* ResourceMgr_LoadGfxByName(const char* path);
uint8_t ResourceMgr_FileExists(const char* path);
void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName);

static const ALIGN_ASSET(2) char sOtIronBootTex[] = "__OTR__objects/ot_obj_link_boy/gLinkAdultIronBootTex";
static const ALIGN_ASSET(2) char sOtIronBootTLUT[] = "__OTR__objects/ot_obj_link_boy/object_link_boyTLUT_00CB40";
static const ALIGN_ASSET(2) char sOtHoverHeelTex[] = "__OTR__objects/ot_obj_link_boy/gLinkAdultHoverBootsHeelTex";
static const ALIGN_ASSET(2) char sOtHoverJetTex[] = "__OTR__objects/ot_obj_link_boy/gLinkAdultHoverBootsJetTex";
static const ALIGN_ASSET(2) char sOtHoverFeatherTex[] = "__OTR__objects/ot_obj_link_boy/gLinkAdultHoverBootsFeatherTex";
static const ALIGN_ASSET(2) char sOtHoverCircleTex[] = "__OTR__objects/ot_gameplay_keep/gEffUnknown13Tex";

#define OT_LEFT_HAND_HAMMER_DL "__OTR__objects/ot_obj_link_boy/gLinkAdultLeftHandHoldingHammerNearDL"
#define OT_LEFT_HAND_BOOMERANG_DL "__OTR__objects/ot_obj_link_child/gLinkChildLeftFistAndBoomerangNearDL"
#define OT_RIGHT_HAND_SLINGSHOT_DL "__OTR__objects/ot_obj_link_child/gLinkChildRightHandHoldingSlingshotNearDL"
#define OT_FP_SLINGSHOT_ARM_DL "__OTR__objects/ot_obj_link_child/gLinkChildRightArmStretchedSlingshotDL"
// "Slinghot" is the decomp's own typo in the extracted resource name.
#define OT_SLINGSHOT_STRING_DL "__OTR__objects/ot_obj_link_child/gLinkChildSlinghotStringDL"
#define OT_BOOMERANG_FLIGHT_DL "__OTR__objects/ot_gameplay_keep/gBoomerangDL"

static Gfx* LoadOptionalGfx(const char* path) {
    if (!ResourceMgr_FileExists(path)) {
        return NULL;
    }
    return ResourceMgr_LoadGfxByName(path);
}

static Vtx sIronBootsVtx[87] = {
#include "sIronBootsVtx.vtx.inc"
};

// LUS resolves a Gfx address as segmented only when its low bit is set; even values are
// raw host pointers. The 0x0D matrices are the flex skeleton's right/left foot entries.

static Gfx sLeftIronBootDL[] = {
    gsSPMatrix(0x0D000181, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPLoadTextureBlock(sOtIronBootTex, G_IM_FMT_CI, G_IM_SIZ_8b, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMIRROR | G_TX_CLAMP, 4, 4, G_TX_NOLOD, G_TX_NOLOD),
    gsDPLoadTLUT_pal256(sOtIronBootTLUT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, 1, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2),
    gsSPClearGeometryMode(G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsSPSetGeometryMode(G_FOG | G_LIGHTING),
    gsDPSetPrimColor(0, 0, 255, 255, 255, 255),
    gsSPVertex(&sIronBootsVtx[0], 32, 0),
    gsSP2Triangles(0, 1, 2, 0, 2, 1, 3, 0),
    gsSP2Triangles(2, 3, 4, 0, 5, 6, 7, 0),
    gsSP2Triangles(5, 7, 8, 0, 4, 9, 10, 0),
    gsSP2Triangles(4, 10, 11, 0, 12, 13, 14, 0),
    gsSP2Triangles(12, 14, 15, 0, 12, 15, 5, 0),
    gsSP2Triangles(12, 5, 8, 0, 16, 17, 18, 0),
    gsSP2Triangles(19, 20, 21, 0, 18, 17, 22, 0),
    gsSP2Triangles(0, 23, 24, 0, 11, 25, 21, 0),
    gsSP2Triangles(23, 22, 26, 0, 26, 22, 17, 0),
    gsSP2Triangles(23, 26, 24, 0, 27, 28, 29, 0),
    gsSP2Triangles(29, 30, 31, 0, 29, 31, 27, 0),
    gsSP1Triangle(25, 19, 21, 0),
    gsSPVertex(&sIronBootsVtx[32], 11, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 5, 6, 0, 3, 7, 0, 0),
    gsSP1Triangle(8, 9, 10, 0),
    gsSPEndDisplayList(),
};

static Gfx sRightIronBootDL[] = {
    gsSPMatrix(0x0D0000C1, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPLoadTextureBlock(sOtIronBootTex, G_IM_FMT_CI, G_IM_SIZ_8b, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMIRROR | G_TX_CLAMP, 4, 4, G_TX_NOLOD, G_TX_NOLOD),
    gsDPLoadTLUT_pal256(sOtIronBootTLUT),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, 1, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2),
    gsSPClearGeometryMode(G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsSPSetGeometryMode(G_FOG | G_LIGHTING),
    gsDPSetPrimColor(0, 0, 255, 255, 255, 255),
    gsSPVertex(&sIronBootsVtx[43], 32, 0),
    gsSP2Triangles(0, 1, 2, 0, 3, 1, 0, 0),
    gsSP2Triangles(4, 3, 0, 0, 5, 6, 7, 0),
    gsSP2Triangles(5, 7, 8, 0, 9, 3, 4, 0),
    gsSP2Triangles(10, 9, 4, 0, 5, 8, 11, 0),
    gsSP2Triangles(5, 11, 12, 0, 5, 12, 13, 0),
    gsSP2Triangles(5, 13, 14, 0, 15, 16, 17, 0),
    gsSP2Triangles(18, 15, 17, 0, 19, 16, 15, 0),
    gsSP2Triangles(20, 21, 2, 0, 18, 22, 10, 0),
    gsSP2Triangles(23, 24, 21, 0, 16, 19, 25, 0),
    gsSP2Triangles(20, 23, 21, 0, 26, 27, 28, 0),
    gsSP2Triangles(28, 29, 30, 0, 28, 30, 26, 0),
    gsSP1Triangle(18, 17, 22, 0),
    gsSPVertex(&sIronBootsVtx[74], 13, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 5, 6, 0, 7, 8, 9, 0),
    gsSP1Triangle(10, 11, 12, 0),
    gsSPEndDisplayList(),
};

static Vtx sHoverBootsVtx[154] = {
#include "sHoverBootsVtx.vtx.inc"
};

static Gfx sLeftHoverBootDL[] = {
    gsSPMatrix(0x0D000181, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_NONE),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPLoadTextureBlock(sOtHoverHeelTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 8, 0, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMIRROR | G_TX_CLAMP, 4, 3, G_TX_NOLOD, G_TX_NOLOD),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, 1, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2),
    gsSPClearGeometryMode(G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsSPSetGeometryMode(G_FOG | G_LIGHTING),
    gsDPSetPrimColor(0, 0, 255, 255, 255, 255),
    gsSPVertex(&sHoverBootsVtx[0], 32, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 5, 6, 0, 7, 8, 9, 0),
    gsSP2Triangles(10, 11, 12, 0, 8, 13, 9, 0),
    gsSP2Triangles(11, 14, 12, 0, 15, 16, 17, 0),
    gsSP2Triangles(18, 19, 20, 0, 21, 22, 23, 0),
    gsSP2Triangles(21, 23, 24, 0, 25, 26, 5, 0),
    gsSP2Triangles(25, 5, 4, 0, 27, 28, 29, 0),
    gsSPVertex(&sHoverBootsVtx[30], 12, 0),
    gsSP2Triangles(0, 1, 2, 0, 3, 4, 5, 0),
    gsSP2Triangles(6, 7, 8, 0, 6, 8, 9, 0),
    gsSP2Triangles(9, 10, 11, 0, 9, 11, 6, 0),
    gsDPPipeSync(),
    gsDPLoadTextureBlock(sOtHoverJetTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                         G_TX_NOMIRROR | G_TX_CLAMP, 5, 5, G_TX_NOLOD, G_TX_NOLOD),
    gsSPVertex(&sHoverBootsVtx[42], 27, 0),
    gsSP2Triangles(0, 1, 2, 0, 3, 4, 5, 0),
    gsSP2Triangles(6, 7, 8, 0, 9, 3, 6, 0),
    gsSP2Triangles(10, 11, 12, 0, 11, 13, 14, 0),
    gsSP2Triangles(15, 16, 17, 0, 18, 19, 20, 0),
    gsSP2Triangles(21, 14, 22, 0, 22, 14, 13, 0),
    gsSP2Triangles(9, 4, 3, 0, 13, 10, 23, 0),
    gsSP2Triangles(20, 19, 24, 0, 0, 20, 24, 0),
    gsSP2Triangles(5, 6, 3, 0, 10, 13, 11, 0),
    gsSP2Triangles(5, 7, 6, 0, 23, 22, 13, 0),
    gsSP2Triangles(24, 1, 0, 0, 8, 25, 9, 0),
    gsSP2Triangles(6, 8, 9, 0, 2, 26, 0, 0),
    gsSP2Triangles(0, 26, 20, 0, 26, 18, 20, 0),
    gsDPPipeSync(),
    gsDPLoadTextureBlock(sOtHoverFeatherTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 16, 0, G_TX_NOMIRROR |
                         G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 5, 4, G_TX_NOLOD, G_TX_NOLOD),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, TEXEL0, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2),
    gsSPClearGeometryMode(G_CULL_BOTH),
    gsSPVertex(&sHoverBootsVtx[69], 8, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 5, 6, 0, 4, 6, 7, 0),
    gsSPEndDisplayList(),
};

static Gfx sRightHoverBootDL[] = {
    gsSPMatrix(0x0D0000C1, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_NONE),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPLoadTextureBlock(sOtHoverHeelTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 8, 0, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMIRROR | G_TX_CLAMP, 4, 3, G_TX_NOLOD, G_TX_NOLOD),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, 1, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2),
    gsSPClearGeometryMode(G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsSPSetGeometryMode(G_FOG | G_LIGHTING),
    gsDPSetPrimColor(0, 0, 255, 255, 255, 255),
    gsSPVertex(&sHoverBootsVtx[77], 32, 0),
    gsSP2Triangles(0, 1, 2, 0, 3, 0, 2, 0),
    gsSP2Triangles(4, 5, 6, 0, 7, 8, 9, 0),
    gsSP2Triangles(10, 11, 12, 0, 7, 13, 8, 0),
    gsSP2Triangles(10, 14, 11, 0, 15, 16, 17, 0),
    gsSP2Triangles(18, 19, 20, 0, 21, 22, 23, 0),
    gsSP2Triangles(24, 21, 23, 0, 5, 25, 26, 0),
    gsSP2Triangles(6, 5, 26, 0, 27, 28, 29, 0),
    gsSPVertex(&sHoverBootsVtx[107], 12, 0),
    gsSP2Triangles(0, 1, 2, 0, 3, 4, 5, 0),
    gsSP2Triangles(6, 7, 8, 0, 9, 6, 8, 0),
    gsSP2Triangles(10, 11, 9, 0, 8, 10, 9, 0),
    gsDPPipeSync(),
    gsDPLoadTextureBlock(sOtHoverJetTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                         G_TX_NOMIRROR | G_TX_CLAMP, 5, 5, G_TX_NOLOD, G_TX_NOLOD),
    gsSPVertex(&sHoverBootsVtx[119], 27, 0),
    gsSP2Triangles(0, 1, 2, 0, 3, 4, 5, 0),
    gsSP2Triangles(6, 7, 8, 0, 8, 5, 9, 0),
    gsSP2Triangles(10, 11, 12, 0, 13, 14, 11, 0),
    gsSP2Triangles(15, 16, 17, 0, 18, 19, 20, 0),
    gsSP2Triangles(21, 13, 22, 0, 14, 13, 21, 0),
    gsSP2Triangles(5, 4, 9, 0, 23, 12, 14, 0),
    gsSP2Triangles(24, 19, 18, 0, 24, 18, 2, 0),
    gsSP2Triangles(5, 8, 3, 0, 11, 14, 12, 0),
    gsSP2Triangles(8, 7, 3, 0, 14, 21, 23, 0),
    gsSP2Triangles(2, 1, 24, 0, 9, 25, 6, 0),
    gsSP2Triangles(9, 6, 8, 0, 2, 26, 0, 0),
    gsSP2Triangles(18, 26, 2, 0, 18, 20, 26, 0),
    gsDPPipeSync(),
    gsDPLoadTextureBlock(sOtHoverFeatherTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 16, 0, G_TX_NOMIRROR |
                         G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 5, 4, G_TX_NOLOD, G_TX_NOLOD),
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, TEXEL0, COMBINED, 0, PRIMITIVE, 0, 0, 0, 0, COMBINED),
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2),
    gsSPClearGeometryMode(G_CULL_BOTH),
    gsSPVertex(&sHoverBootsVtx[146], 8, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
    gsSP2Triangles(4, 5, 6, 0, 4, 6, 7, 0),
    gsSPEndDisplayList(),
};

static Vtx sHoverBootsCircleVtx[] = {
#include "sHoverBootsCircleVtx.vtx.inc"
};

static Gfx sHoverBootsCircleDL[] = {
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_NONE),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPLoadTextureBlock(sOtHoverCircleTex, G_IM_FMT_I, G_IM_SIZ_8b, 16, 32, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                         G_TX_NOMIRROR | G_TX_CLAMP, 4, 5, G_TX_NOLOD, 1),
    gsDPLoadMultiBlock(gameplay_keep_Tex_01AAB0, 0x0100, 1, G_IM_FMT_I, G_IM_SIZ_8b, 16, 32, 0,
                       G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 4, 5, 15, 14),
    gsDPSetCombineLERP(TEXEL1, PRIMITIVE, ENV_ALPHA, TEXEL0, 0, 0, 0, TEXEL0, PRIMITIVE, 0, COMBINED, ENVIRONMENT,
                       COMBINED, 0, PRIMITIVE, 0),
    gsDPSetRenderMode(G_RM_PASS, G_RM_ZB_CLD_SURF2),
    gsSPClearGeometryMode(G_CULL_BACK | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),
    gsSPDisplayList(0x09000001),
    gsSPVertex(sHoverBootsCircleVtx, 13, 0),
    gsSP2Triangles(0, 1, 2, 0, 3, 4, 2, 0),
    gsSP2Triangles(5, 6, 2, 0, 7, 8, 2, 0),
    gsSP2Triangles(9, 10, 2, 0, 11, 12, 2, 0),
    gsSPEndDisplayList(),
};

void OotmmEquipment_GetTunicColor(uint8_t* r, uint8_t* g, uint8_t* b) {
    switch (OotmmCustomItems_EquippedTunic()) {
        case OOTMM_TUNIC_GORON:
            *r = 100;
            *g = 20;
            *b = 0;
            break;
        case OOTMM_TUNIC_ZORA:
            *r = 0;
            *g = 60;
            *b = 100;
            break;
        default:
            *r = 30;
            *g = 105;
            *b = 27;
            break;
    }
}

static Gfx sTunicTintDL[] = {
    gsDPSetPrimColor(0, 0, 30, 105, 27, 255),
    gsDPPipeSync(),
    gsSPEndDisplayList(),
};

typedef struct {
    const char* path;
    const char* patchName;
    int index;
} TunicPatchSlot;

// Human Link's cloth materials bake the tunic green as an in-DL prim color; these are those slots.
static const TunicPatchSlot sTunicPatchSlots[] = {
    { "objects/object_link_child/gLinkHumanWaistDL", "ootmmTunic", 5 },
    { "objects/object_link_child/gLinkHumanRightThighDL", "ootmmTunic", 10 },
    { "objects/object_link_child/gLinkHumanLeftThighDL", "ootmmTunic", 10 },
    { "objects/object_link_child/gLinkHumanHeadDL", "ootmmTunic", 92 },
    { "objects/object_link_child/gLinkHumanHatDL", "ootmmTunic", 10 },
    { "objects/object_link_child/gLinkHumanCollarDL", "ootmmTunic", 5 },
    { "objects/object_link_child/gLinkHumanLeftShoulderDL", "ootmmTunic1", 10 },
    { "objects/object_link_child/gLinkHumanLeftShoulderDL", "ootmmTunic2", 65 },
    { "objects/object_link_child/gLinkHumanRightShoulderDL", "ootmmTunic1", 10 },
    { "objects/object_link_child/gLinkHumanRightShoulderDL", "ootmmTunic2", 65 },
    { "objects/object_link_child/gLinkHumanTorsoDL", "ootmmTunic", 5 },
};

void OotmmEquipment_UpdateTunicTint(void) {
    static int sPatched = 0;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    size_t i;

    if (!OotmmSession_IsActive()) {
        if (sPatched) {
            for (i = 0; i < ARRAY_COUNT(sTunicPatchSlots); i++) {
                ResourceMgr_UnpatchGfxByName(sTunicPatchSlots[i].path, sTunicPatchSlots[i].patchName);
            }
            sPatched = 0;
        }
        return;
    }

    OotmmEquipment_GetTunicColor(&r, &g, &b);
    {
        Gfx tint[] = { gsDPSetPrimColor(0, 0, r, g, b, 255) };
        sTunicTintDL[0] = tint[0];
    }

    if (!sPatched) {
        Gfx call[] = { gsSPDisplayList(sTunicTintDL) };

        for (i = 0; i < ARRAY_COUNT(sTunicPatchSlots); i++) {
            ResourceMgr_PatchGfxByName(sTunicPatchSlots[i].path, sTunicPatchSlots[i].patchName,
                                       sTunicPatchSlots[i].index, call[0]);
        }
        sPatched = 1;
    }
}

void OotmmEquipment_DrawBoots(PlayState* play, Player* player) {
    OPEN_DISPS(play->state.gfxCtx);

    switch (OotmmCustomItems_EquippedBoots()) {
        case OOTMM_BOOTS_IRON:
            gSPDisplayList(POLY_OPA_DISP++, sLeftIronBootDL);
            gSPDisplayList(POLY_OPA_DISP++, sRightIronBootDL);
            break;
        case OOTMM_BOOTS_HOVER:
            gSPDisplayList(POLY_OPA_DISP++, sLeftHoverBootDL);
            gSPDisplayList(POLY_OPA_DISP++, sRightHoverBootDL);
            break;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void OotmmEquipment_DrawHoverCircle(PlayState* play, Player* player) {
    static Vec3s sCircleRot = { 0, 0, 0 };
    s32 alpha = 255;
    s32 timer = player->hoverBootsTimer;
    s32 clamped;

    if (OotmmCustomItems_EquippedBoots() != OOTMM_BOOTS_HOVER || (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (player->stateFlags1 & PLAYER_STATE1_800000) || timer == 0 || timer >= 19) {
        return;
    }

    if (timer >= 15) {
        alpha = (19 - timer) * 51.0f;
    } else {
        clamped = timer;
        if (clamped > 9) {
            clamped = 9;
        }
        alpha = (-clamped * 4) + 36;
        alpha = SQ(alpha);
        alpha = (s32)((Math_CosS(alpha) * 100.0f) + 100.0f) + 55.0f;
        alpha *= clamped * (1.0f / 9.0f);
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_SetTranslateRotateYXZ(player->actor.world.pos.x, player->actor.world.pos.y + 2.0f,
                                 player->actor.world.pos.z, &sCircleRot);
    Matrix_Scale(4.0f, 4.0f, 4.0f, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x09,
               Gfx_TwoTexScroll(play->state.gfxCtx, G_TX_RENDERTILE, 0, 0, 16, 32, 1, 0,
                                (play->gameplayFrames * -15) % 128, 16, 32));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 120, 90, 30, 128);
    gSPDisplayList(POLY_XLU_DISP++, sHoverBootsCircleDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void OotmmEquipment_DrawWornMask(PlayState* play, Player* player) {
    const char* path = OotmmCustomItems_MaskDList(OotmmCustomItems_EquippedMask());

    if (path == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, ResourceMgr_LoadGfxByName(path));

    CLOSE_DISPS(play->state.gfxCtx);
}

void OotmmEquipment_DrawSlingshotString(PlayState* play, Player* player) {
    static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
    Gfx* string = LoadOptionalGfx(OT_SLINGSHOT_STRING_DL);
    Vec3f stringOrigin;
    f32 distXYZ;

    if (string == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Translate(606.0f, 236.0f, 0.0f, MTXMODE_APPLY);

    if ((player->stateFlags3 & PLAYER_STATE3_40) && (player->unk_B28 >= 0) && (player->unk_ACC < 0xB)) {
        Matrix_MultVec3f(&sZeroVec, &stringOrigin);
        distXYZ = Math_Vec3f_DistXYZ(&player->leftHandWorld.pos, &stringOrigin);

        player->unk_B08 = distXYZ - 3.0f;
        if (distXYZ < 3.0f) {
            player->unk_B08 = 0.0f;
        } else {
            player->unk_B08 *= 1.6f;
            if (player->unk_B08 > 1.0f) {
                player->unk_B08 = 1.0f;
            }
        }

        player->unk_B0C = -0.5f;
    }

    Matrix_Scale(1.0f, player->unk_B08, 1.0f, MTXMODE_APPLY);
    Matrix_RotateZF(player->unk_B08 * -0.2f, MTXMODE_APPLY);

    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, string);

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// OoT authored these in child-Link limb space, which MM's human Link shares; the back list even
// carries its own offset matrix, so neither needs any transform here.
Gfx* OotmmEquipment_DekuShieldBackDList(void) {
    return LoadOptionalGfx("__OTR__objects/ot_obj_link_child/gLinkChildDekuShieldWithMatrixDL");
}

Gfx* OotmmEquipment_DekuShieldHandDList(int lod) {
    return LoadOptionalGfx(lod != 0
                               ? "__OTR__objects/ot_obj_link_child/gLinkChildRightFistAndDekuShieldFarDL"
                               : "__OTR__objects/ot_obj_link_child/gLinkChildRightFistAndDekuShieldNearDL");
}

Gfx* OotmmEquipment_LeftHandHammerDList(void) {
    return LoadOptionalGfx(OT_LEFT_HAND_HAMMER_DL);
}

Gfx* OotmmEquipment_LeftHandBoomerangDList(void) {
    return LoadOptionalGfx(OT_LEFT_HAND_BOOMERANG_DL);
}

Gfx* OotmmEquipment_RightHandSlingshotDList(void) {
    return LoadOptionalGfx(OT_RIGHT_HAND_SLINGSHOT_DL);
}

Gfx* OotmmEquipment_FirstPersonSlingshotDList(void) {
    return LoadOptionalGfx(OT_FP_SLINGSHOT_ARM_DL);
}

Gfx* OotmmEquipment_BoomerangFlightDList(void) {
    return LoadOptionalGfx(OT_BOOMERANG_FLIGHT_DL);
}
