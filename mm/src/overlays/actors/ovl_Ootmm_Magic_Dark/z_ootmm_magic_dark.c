/*
 * File: z_ootmm_magic_dark.c
 * Overlay: ovl_Ootmm_Magic_Dark
 * Description: Nayru's Love, ported from Ocarina of Time for OoTMM
 */

#include "z_ootmm_magic_dark.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#include "2s2h/BenPort.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

#define OOTMM_MAGIC_DARK_MATERIAL_DL "__OTR__overlays/ot_ovl_Magic_Dark/sDiamondMaterialDL"
#define OOTMM_MAGIC_DARK_MODEL_DL "__OTR__overlays/ot_ovl_Magic_Dark/sDiamondModelDL"

#define OOTMM_NAYRUS_LOVE_DURATION 1200
#define OOTMM_NAYRUS_LOVE_FLASH_START 1100
#define OOTMM_NAYRUS_LOVE_FADE_START 1180

void OotmmMagicDark_Init(Actor* thisx, PlayState* play);
void OotmmMagicDark_Destroy(Actor* thisx, PlayState* play);
void OotmmMagicDark_OrbUpdate(Actor* thisx, PlayState* play);
void OotmmMagicDark_OrbDraw(Actor* thisx, PlayState* play);
void OotmmMagicDark_DiamondUpdate(Actor* thisx, PlayState* play);
void OotmmMagicDark_DiamondDraw(Actor* thisx, PlayState* play);

ActorProfile Ootmm_Magic_Dark_Profile = {
    /**/ ACTOR_OOTMM_MAGIC_DARK,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OotmmMagicDark),
    /**/ OotmmMagicDark_Init,
    /**/ OotmmMagicDark_Destroy,
    /**/ OotmmMagicDark_OrbUpdate,
    /**/ OotmmMagicDark_OrbDraw,
};

static f32 OotmmMagicDark_GetScale(Player* player) {
    switch (player->transformation) {
        case PLAYER_FORM_FIERCE_DEITY:
            return 0.8f;
        case PLAYER_FORM_GORON:
            return 0.6f;
        case PLAYER_FORM_ZORA:
            return 0.6f;
        case PLAYER_FORM_DEKU:
            return 0.4f;
        case PLAYER_FORM_HUMAN:
            return 0.6f;
    }
    return 0.0f;
}

static void OotmmMagicDark_BecomeDiamond(OotmmMagicDark* this) {
    this->actor.update = OotmmMagicDark_DiamondUpdate;
    this->actor.draw = OotmmMagicDark_DiamondDraw;
    this->actor.scale.x = this->actor.scale.z = this->scale * 1.6f;
    this->actor.scale.y = this->scale * 0.8f;
    this->timer = 0;
    this->primAlpha = 0;
}

void OotmmMagicDark_Init(Actor* thisx, PlayState* play) {
    OotmmMagicDark* this = (OotmmMagicDark*)thisx;
    Player* player = GET_PLAYER(play);

    if (!ResourceMgr_FileExists(OOTMM_MAGIC_DARK_MATERIAL_DL) || !ResourceMgr_FileExists(OOTMM_MAGIC_DARK_MODEL_DL)) {
        Actor_Kill(thisx);
        return;
    }

    this->scale = OotmmMagicDark_GetScale(player);
    this->actor.world.pos = player->actor.world.pos;
    Actor_SetScale(&this->actor, 0.0f);
    this->actor.room = -1;
    this->timer = 0;

    if (gSaveContext.nayrusLoveTimer != 0) {
        OotmmMagicDark_BecomeDiamond(this);
    } else {
        gSaveContext.nayrusLoveTimer = 0;
    }
}

void OotmmMagicDark_Destroy(Actor* thisx, PlayState* play) {
    if (gSaveContext.nayrusLoveTimer == 0) {
        Magic_Reset(play);
    }
}

void OotmmMagicDark_DiamondUpdate(Actor* thisx, PlayState* play) {
    OotmmMagicDark* this = (OotmmMagicDark*)thisx;
    Player* player = GET_PLAYER(play);
    s16 nayrusLoveTimer = gSaveContext.nayrusLoveTimer;
    f32 scaleTarget;
    u8 alphaCap;

    if (nayrusLoveTimer >= OOTMM_NAYRUS_LOVE_DURATION) {
        player->invincibilityTimer = 0;
        gSaveContext.nayrusLoveTimer = 0;
        Actor_Kill(thisx);
        return;
    }

    scaleTarget = OotmmMagicDark_GetScale(player);
    Math_StepToF(&this->scale, scaleTarget, 0.01f);

    player->invincibilityTimer = -100;

    if (this->timer < 20) {
        thisx->scale.x = thisx->scale.z = (1.6f - (this->timer * 0.03f)) * this->scale;
        thisx->scale.y = ((this->timer * 0.01f) + 0.8f) * this->scale;
    } else {
        thisx->scale.x = thisx->scale.y = thisx->scale.z = this->scale;
    }

    thisx->scale.x *= 1.3f;
    thisx->scale.z *= 1.3f;

    alphaCap = (this->timer < 20) ? (this->timer * 12) : 255;

    if (nayrusLoveTimer >= OOTMM_NAYRUS_LOVE_FADE_START) {
        this->primAlpha = 15595 - (nayrusLoveTimer * 13);
        if (nayrusLoveTimer & 1) {
            this->primAlpha = this->primAlpha >> 1;
        }
    } else if (nayrusLoveTimer >= OOTMM_NAYRUS_LOVE_FLASH_START) {
        this->primAlpha = (u8)(nayrusLoveTimer << 7) + 127;
    } else {
        this->primAlpha = 255;
    }

    if (this->primAlpha > alphaCap) {
        this->primAlpha = alphaCap;
    }

    thisx->world.rot.y += 0x3E8;
    thisx->shape.rot.y = thisx->world.rot.y + Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    this->timer++;
    gSaveContext.nayrusLoveTimer = nayrusLoveTimer + 1;

    if (nayrusLoveTimer < OOTMM_NAYRUS_LOVE_FLASH_START) {
        Actor_PlaySfx_Flagged(thisx, NA_SE_PL_MAGIC_SOUL_NORMAL - SFX_FLAG);
    } else {
        Actor_PlaySfx_Flagged(thisx, NA_SE_PL_MAGIC_SOUL_FLASH - SFX_FLAG);
    }
}

void OotmmMagicDark_DimLighting(PlayState* play, f32 intensity) {
    s32 i;
    f32 colorScale;
    f32 fogScale;

    if (play->roomCtx.curRoom.type == ROOM_TYPE_BOSS) {
        return;
    }

    intensity = CLAMP(intensity, 0.0f, 1.0f);
    fogScale = intensity - 0.2f;
    if (intensity < 0.2f) {
        fogScale = 0.0f;
    }

    play->envCtx.adjLightSettings.fogNear = (850.0f - play->envCtx.lightSettings.fogNear) * fogScale;

    if (intensity == 0.0f) {
        for (i = 0; i < ARRAY_COUNT(play->envCtx.adjLightSettings.fogColor); i++) {
            play->envCtx.adjLightSettings.fogColor[i] = 0;
        }
        return;
    }

    colorScale = intensity * 5.0f;
    if (colorScale > 1.0f) {
        colorScale = 1.0f;
    }
    for (i = 0; i < ARRAY_COUNT(play->envCtx.adjLightSettings.fogColor); i++) {
        play->envCtx.adjLightSettings.fogColor[i] = -(s16)(play->envCtx.lightSettings.fogColor[i] * colorScale);
    }
}

void OotmmMagicDark_OrbUpdate(Actor* thisx, PlayState* play) {
    OotmmMagicDark* this = (OotmmMagicDark*)thisx;
    Player* player = GET_PLAYER(play);

    Actor_PlaySfx_Flagged(&this->actor, NA_SE_PL_MAGIC_SOUL_BALL - SFX_FLAG);

    if (this->timer < 35) {
        OotmmMagicDark_DimLighting(play, this->timer * (1 / 45.0f));
        Math_SmoothStepToF(&thisx->scale.x, this->scale * (1 / 12.000001f), 0.05f, 0.01f, 0.0001f);
        Actor_SetScale(&this->actor, thisx->scale.x);
    } else if (this->timer < 55) {
        Actor_SetScale(&this->actor, thisx->scale.x * 0.9f);
        Math_SmoothStepToF(&this->orbOffset.y, player->bodyPartsPos[PLAYER_BODYPART_WAIST].y, 0.5f, 3.0f, 1.0f);
        if (this->timer > 48) {
            OotmmMagicDark_DimLighting(play, (54 - this->timer) * 0.2f);
        }
    } else {
        OotmmMagicDark_BecomeDiamond(this);
    }

    this->timer++;
}

void OotmmMagicDark_DiamondDraw(Actor* thisx, PlayState* play) {
    OotmmMagicDark* this = (OotmmMagicDark*)thisx;
    Player* player = GET_PLAYER(play);
    u16 gameplayFrames = play->gameplayFrames;
    f32 waistY;
    f32 heightDiff;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);

    this->actor.world.pos.x = player->bodyPartsPos[PLAYER_BODYPART_WAIST].x;
    this->actor.world.pos.z = player->bodyPartsPos[PLAYER_BODYPART_WAIST].z;

    if (player->transformation == PLAYER_FORM_GORON) {
        if (player->stateFlags3 & PLAYER_STATE3_1000) {
            waistY = player->actor.world.pos.y + Player_GetHeight(player) * 0.5f;
        } else {
            // The Goron's waist sits far lower than the other forms'.
            waistY = player->bodyPartsPos[PLAYER_BODYPART_WAIST].y + 10.0f;
        }
    } else {
        waistY = player->bodyPartsPos[PLAYER_BODYPART_WAIST].y;
    }

    heightDiff = waistY - this->actor.world.pos.y;
    if (heightDiff < -2.0f) {
        this->actor.world.pos.y = waistY + 2.0f;
    } else if (heightDiff > 2.0f) {
        this->actor.world.pos.y = waistY - 2.0f;
    }

    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    Matrix_RotateYS(this->actor.shape.rot.y, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 170, 255, 255, (s32)(this->primAlpha * 0.6f) & 0xFF);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
    gSPDisplayList(POLY_XLU_DISP++, ResourceMgr_LoadGfxByName(OOTMM_MAGIC_DARK_MATERIAL_DL));
    gSPDisplayList(POLY_XLU_DISP++,
                   Gfx_TwoTexScroll(play->state.gfxCtx, G_TX_RENDERTILE, gameplayFrames * 2, gameplayFrames * -4, 32,
                                    32, 1, 0, gameplayFrames * -16, 64, 32));
    gSPDisplayList(POLY_XLU_DISP++, ResourceMgr_LoadGfxByName(OOTMM_MAGIC_DARK_MODEL_DL));

    CLOSE_DISPS(play->state.gfxCtx);
}

void OotmmMagicDark_OrbDraw(Actor* thisx, PlayState* play) {
    OotmmMagicDark* this = (OotmmMagicDark*)thisx;
    Player* player = GET_PLAYER(play);
    f32 spin = play->state.frames & 0x1F;
    Vec3f pos;

    if (this->timer < 32) {
        pos.x = (player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND].x +
                 player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND].x) *
                0.5f;
        pos.y = (player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND].y +
                 player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND].y) *
                0.5f;
        pos.z = (player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND].z +
                 player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND].z) *
                0.5f;
        if (this->timer > 20) {
            pos.y += (this->timer - 20) * 1.4f;
        }
        this->orbOffset = pos;
    } else if (this->timer < 130) {
        pos = this->orbOffset;
    } else {
        return;
    }

    pos.x -= this->actor.scale.x * 300.0f * Math_SinS(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play))) *
             Math_CosS(Camera_GetCamDirPitch(GET_ACTIVE_CAM(play)));
    pos.y -= this->actor.scale.x * 300.0f * Math_SinS(Camera_GetCamDirPitch(GET_ACTIVE_CAM(play)));
    pos.z -= this->actor.scale.x * 300.0f * Math_CosS(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play))) *
             Math_CosS(Camera_GetCamDirPitch(GET_ACTIVE_CAM(play)));

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 150, 255, 255);
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    Matrix_Push();
    Matrix_RotateZF(spin * (M_PIf / 32), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, gEffFlash1DL);
    Matrix_Pop();
    Matrix_RotateZF(-spin * (M_PIf / 32), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, gEffFlash1DL);

    CLOSE_DISPS(play->state.gfxCtx);
}
