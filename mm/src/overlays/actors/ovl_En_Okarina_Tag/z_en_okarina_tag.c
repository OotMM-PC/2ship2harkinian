/*
 * File: z_en_okarina_tag.c
 * Overlay: ovl_En_Okarina_Tag
 * Description:
 */

#include "z_en_okarina_tag.h"
#include "align_asset_macro.h"
#include "2s2h/OotmmSongEvents.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA | ACTOR_FLAG_LOCK_ON_DISABLED)

#define OOTMM_SONG_TAG 0x4000

void EnOkarinaTag_Init(Actor* thisx, PlayState* play);
void EnOkarinaTag_Destroy(Actor* thisx, PlayState* play);
void EnOkarinaTag_Update(Actor* thisx, PlayState* play);
void EnOkarinaTag_DrawOotmm(Actor* thisx, PlayState* play);

void func_8093E518(EnOkarinaTag* this, PlayState* play);
void func_8093E68C(EnOkarinaTag* this, PlayState* play);

typedef struct {
    const char* texture;
    u8 width;
    u8 height;
    Color_RGB8 color;
} OotmmSongTagStyle;

static const ALIGN_ASSET(2) char sSongTagLullabyTex[] = "__OTR__objects/ootmm_song_tags/gSongTagLullabyTex";
static const ALIGN_ASSET(2) char sSongTagEponaTex[] = "__OTR__objects/ootmm_song_tags/gSongTagEponaTex";
static const ALIGN_ASSET(2) char sSongTagSariaTex[] = "__OTR__objects/ootmm_song_tags/gSongTagSariaTex";
static const ALIGN_ASSET(2) char sSongTagStormsTex[] = "__OTR__objects/ootmm_song_tags/gSongTagStormsTex";
static const ALIGN_ASSET(2) char sSongTagSunTex[] = "__OTR__objects/ootmm_song_tags/gSongTagSunTex";
static const ALIGN_ASSET(2) char sSongTagTimeTex[] = "__OTR__objects/ootmm_song_tags/gSongTagTimeTex";
static const ALIGN_ASSET(2) char sSongTagMinuetTex[] = "__OTR__objects/ootmm_song_tags/gSongTagMinuetTex";
static const ALIGN_ASSET(2) char sSongTagBoleroTex[] = "__OTR__objects/ootmm_song_tags/gSongTagBoleroTex";
static const ALIGN_ASSET(2) char sSongTagSerenadeTex[] = "__OTR__objects/ootmm_song_tags/gSongTagSerenadeTex";
static const ALIGN_ASSET(2) char sSongTagRequiemTex[] = "__OTR__objects/ootmm_song_tags/gSongTagRequiemTex";
static const ALIGN_ASSET(2) char sSongTagNocturneTex[] = "__OTR__objects/ootmm_song_tags/gSongTagNocturneTex";
static const ALIGN_ASSET(2) char sSongTagPreludeTex[] = "__OTR__objects/ootmm_song_tags/gSongTagPreludeTex";
static const ALIGN_ASSET(2) char sSongTagHealingTex[] = "__OTR__objects/ootmm_song_tags/gSongTagHealingTex";
static const ALIGN_ASSET(2) char sSongTagSoaringTex[] = "__OTR__objects/ootmm_song_tags/gSongTagSoaringTex";
static const ALIGN_ASSET(2) char sSongTagAwakeningTex[] = "__OTR__objects/ootmm_song_tags/gSongTagAwakeningTex";
static const ALIGN_ASSET(2) char sSongTagGoronTex[] = "__OTR__objects/ootmm_song_tags/gSongTagGoronTex";
static const ALIGN_ASSET(2) char sSongTagGoronHalfTex[] = "__OTR__objects/ootmm_song_tags/gSongTagGoronHalfTex";
static const ALIGN_ASSET(2) char sSongTagZoraTex[] = "__OTR__objects/ootmm_song_tags/gSongTagZoraTex";
static const ALIGN_ASSET(2) char sSongTagEmptinessTex[] = "__OTR__objects/ootmm_song_tags/gSongTagEmptinessTex";
static const ALIGN_ASSET(2) char sSongTagOrderTex[] = "__OTR__objects/ootmm_song_tags/gSongTagOrderTex";

static const OotmmSongTagStyle sOotmmSongTagStyles[20] = {
    { sSongTagLullabyTex, 32, 64, { 255, 255, 0 } },
    { sSongTagEponaTex, 64, 64, { 255, 136, 0 } },
    { sSongTagSariaTex, 64, 64, { 61, 168, 29 } },
    { sSongTagStormsTex, 64, 64, { 163, 163, 0 } },
    { sSongTagSunTex, 32, 32, { 238, 34, 34 } },
    { sSongTagTimeTex, 64, 64, { 136, 136, 255 } },
    { sSongTagMinuetTex, 64, 64, { 0, 255, 0 } },
    { sSongTagBoleroTex, 64, 64, { 255, 0, 0 } },
    { sSongTagSerenadeTex, 64, 64, { 0, 0, 255 } },
    { sSongTagRequiemTex, 64, 64, { 255, 128, 0 } },
    { sSongTagNocturneTex, 64, 64, { 128, 0, 255 } },
    { sSongTagPreludeTex, 64, 64, { 255, 255, 0 } },
    { sSongTagHealingTex, 32, 64, { 255, 0, 255 } },
    { sSongTagSoaringTex, 64, 64, { 225, 208, 7 } },
    { sSongTagAwakeningTex, 64, 64, { 0, 255, 0 } },
    { sSongTagGoronTex, 64, 64, { 156, 132, 41 } },
    { sSongTagGoronHalfTex, 64, 64, { 156, 132, 41 } },
    { sSongTagZoraTex, 64, 64, { 132, 198, 198 } },
    { sSongTagEmptinessTex, 64, 64, { 255, 128, 0 } },
    { sSongTagOrderTex, 64, 64, { 255, 255, 255 } },
};

static Vtx sOotmmSongTagQuad[4] = {
    VTX(-26, 0, -26, 0, 0, 255, 255, 255, 255),
    VTX(-26, 0, 26, 0, 2048, 255, 255, 255, 255),
    VTX(26, 0, 26, 2048, 2048, 255, 255, 255, 255),
    VTX(26, 0, -26, 2048, 0, 255, 255, 255, 255),
};

ActorProfile En_Okarina_Tag_Profile = {
    /**/ ACTOR_EN_OKARINA_TAG,
    /**/ ACTORCAT_SWITCH,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(EnOkarinaTag),
    /**/ EnOkarinaTag_Init,
    /**/ EnOkarinaTag_Destroy,
    /**/ EnOkarinaTag_Update,
    /**/ NULL,
};

void EnOkarinaTag_Destroy(Actor* thisx, PlayState* play) {
}

void EnOkarinaTag_Init(Actor* thisx, PlayState* play) {
    EnOkarinaTag* this = (EnOkarinaTag*)thisx;
    f32 zRot = 0.0f;
    s32 i = 0;

    if ((thisx->params & OOTMM_SONG_TAG) != 0) {
        CollisionPoly* floorPoly = NULL;
        s32 bgId;
        Vec3f rayPos = thisx->world.pos;
        f32 floorY;

        this->unk148 = thisx->params & 0xF;
        this->unk14A = ((thisx->params >> 4) & 0xF) - 1;
        thisx->room = -1;
        thisx->draw = EnOkarinaTag_DrawOotmm;
        Actor_SetScale(thisx, 1.375f);
        rayPos.y += 20.0f;
        floorY = BgCheck_EntityRaycastFloor5(&play->colCtx, &floorPoly, &bgId, thisx, &rayPos);
        if (floorY > BGCHECK_Y_MIN) {
            thisx->world.pos.y = floorY;
        }
        if (floorPoly != NULL) {
            thisx->floorPoly = floorPoly;
            func_800BE3D0(thisx, thisx->shape.rot.y, &thisx->shape.rot);
        }
        return;
    }

    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->unk148 = ENOKARINATAG_GET_F800(thisx);
    this->unk14A = ENOKARINATAG_GET_780(thisx);
    this->switchFlag = ENOKARINATAG_GET_SWITCH_FLAG(thisx);
    if (this->actor.world.rot.z > 0) {
        zRot = this->actor.world.rot.z;
        while (zRot > 10.0f) {
            zRot -= 10.0f;
            i++;
        }
    }
    this->unk154 = zRot * 50.0f;
    this->unk158 = i * 50.0f;
    if (this->switchFlag == ENOKARINATAG_SWITCH_FLAG_NONE) {
        this->switchFlag = SWITCH_FLAG_NONE;
    }
    if (this->unk14A == 0xF) {
        this->unk14A = -1;
    }
    this->actor.attentionRangeType = ATTENTION_RANGE_1;
    this->actionFunc = func_8093E518;
}

void func_8093E518(EnOkarinaTag* this, PlayState* play) {
    f32 xzRange;
    f32 yRange;
    s16 yDiff;
    u16 var_v1;

    if (this->switchFlag > SWITCH_FLAG_NONE) {
        if (this->unk148 == 0) {
            if (Flags_GetSwitch(play, this->switchFlag)) {
                return;
            }
        }
        if (this->unk148 == 1) {
            if (!Flags_GetSwitch(play, this->switchFlag)) {
                return;
            }
        }
    }
    var_v1 = this->unk14A;
    if (var_v1 == 6) {
        var_v1 = 0xA;
        if (!gSaveContext.save.saveInfo.scarecrowSpawnSongSet) {
            return;
        }
    }
    if (this->unk14A == -1) {
        var_v1 = 0;
    }
    if (Actor_OcarinaInteractionAccepted(&this->actor, &play->state)) {
        Message_DisplayOcarinaStaff(play, OCARINA_ACTION_CHECK_HEALING + var_v1);
        this->actionFunc = func_8093E68C;
    } else {
        yDiff = ABS_ALT((s16)(this->actor.yawTowardsPlayer - this->actor.world.rot.y));
        if (yDiff >= 0x4300) {
            this->unk150 = 0;
            return;
        }
        xzRange = this->unk154;
        this->unk150 = 1;
        if (xzRange == 0.0f) {
            xzRange = 50000.0f;
        }
        yRange = this->unk158;
        if (yRange == 0.0f) {
            yRange = 50000.0f;
        }
        Actor_OfferOcarinaInteraction(&this->actor, play, xzRange, yRange);
    }
}

void func_8093E68C(EnOkarinaTag* this, PlayState* play) {
    if (play->msgCtx.ocarinaMode == OCARINA_MODE_END) {
        this->actionFunc = func_8093E518;
    } else {
        if ((play->msgCtx.ocarinaMode == OCARINA_MODE_EVENT) ||
            ((this->unk14A == -1) && ((play->msgCtx.ocarinaMode == OCARINA_MODE_PLAYED_TIME) ||
                                      (play->msgCtx.ocarinaMode == OCARINA_MODE_PLAYED_HEALING) ||
                                      (play->msgCtx.ocarinaMode == OCARINA_MODE_PLAYED_EPONAS) ||
                                      (play->msgCtx.ocarinaMode == OCARINA_MODE_PLAYED_SOARING) ||
                                      (play->msgCtx.ocarinaMode == OCARINA_MODE_PLAYED_SUNS) ||
                                      (play->msgCtx.ocarinaMode == OCARINA_MODE_PLAYED_STORMS) ||
                                      (play->msgCtx.ocarinaMode == OCARINA_MODE_F)))) {
            if (this->switchFlag > SWITCH_FLAG_NONE) {
                switch (this->unk148) {
                    case 0:
                        Flags_SetSwitch(play, this->switchFlag);
                        break;

                    case 1:
                        Flags_UnsetSwitch(play, this->switchFlag);
                        break;

                    case 2:
                        if (Flags_GetSwitch(play, this->switchFlag)) {
                            Flags_UnsetSwitch(play, this->switchFlag);
                        } else {
                            Flags_SetSwitch(play, this->switchFlag);
                        }
                        break;

                    default:
                        break;
                }
            }
            play->msgCtx.ocarinaMode = OCARINA_MODE_END;
            Audio_PlaySfx(NA_SE_SY_CORRECT_CHIME);
            this->actionFunc = func_8093E518;
        }
    }
}

void EnOkarinaTag_Update(Actor* thisx, PlayState* play) {
    EnOkarinaTag* this = (EnOkarinaTag*)thisx;

    if ((thisx->params & OOTMM_SONG_TAG) != 0) {
        return;
    }

    this->actionFunc(this, play);
}

void EnOkarinaTag_DrawOotmm(Actor* thisx, PlayState* play) {
    EnOkarinaTag* this = (EnOkarinaTag*)thisx;

    if (this->unk14A >= 0 && this->unk14A != play->roomCtx.curRoom.num &&
        this->unk14A != play->roomCtx.prevRoom.num) {
        return;
    }
    const s32 song = OotmmSongEvents_Assigned(this->unk148);
    if (song < 0 || song >= ARRAY_COUNT(sOotmmSongTagStyles)) {
        return;
    }
    const OotmmSongTagStyle* style = &sOotmmSongTagStyles[song];
    const u32 wrapS = style->width == 32 ? G_TX_MIRROR | G_TX_WRAP : G_TX_NOMIRROR | G_TX_WRAP;
    const u32 wrapT = style->height == 32 ? G_TX_MIRROR | G_TX_WRAP : G_TX_NOMIRROR | G_TX_WRAP;
    const u32 maskS = style->width == 32 ? 5 : 6;
    const u32 maskT = style->height == 32 ? 5 : 6;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPLoadTextureBlock(POLY_XLU_DISP++, style->texture, G_IM_FMT_IA, G_IM_SIZ_8b, style->width, style->height, 0,
                        wrapS, wrapT, maskS, maskT, G_TX_NOLOD, G_TX_NOLOD);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIDECALA_PRIM, G_CC_PASS2);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_DECAL2);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BACK | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, style->color.r, style->color.g, style->color.b, 255);
    gSPVertex(POLY_XLU_DISP++, sOotmmSongTagQuad, 4, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 2, 1, 0, 0, 3, 2, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}
