/*
 * File: z_ootmm_magic_wind.c
 * Overlay: ovl_Ootmm_Magic_Wind
 * Description: Farore's Wind, ported from Ocarina of Time for OoTMM
 */

#include "z_ootmm_magic_wind.h"

#include "objects/gameplay_keep/gameplay_keep.h"

#include "2s2h/BenPort.h"
#include "2s2h/OotmmSession.h"
#include "2s2h/OotmmCustomItemsPlayer.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA)

#define OOTMM_MAGIC_WIND_VTX "__OTR__overlays/ot_ovl_Magic_Wind/sCylinderVtx"
#define OOTMM_MAGIC_WIND_INNER_DL "__OTR__overlays/ot_ovl_Magic_Wind/sInnerCylinderDL"
#define OOTMM_MAGIC_WIND_OUTER_DL "__OTR__overlays/ot_ovl_Magic_Wind/sOuterCylinderDL"

void OotmmMagicWind_Init(Actor* thisx, PlayState* play);
void OotmmMagicWind_Destroy(Actor* thisx, PlayState* play);
void OotmmMagicWind_Update(Actor* thisx, PlayState* play);
void OotmmMagicWind_Draw(Actor* thisx, PlayState* play);

void OotmmMagicWind_Shrink(OotmmMagicWind* this, PlayState* play);
void OotmmMagicWind_WaitForTimer(OotmmMagicWind* this, PlayState* play);
void OotmmMagicWind_FadeOut(OotmmMagicWind* this, PlayState* play);
void OotmmMagicWind_WaitAtFullSize(OotmmMagicWind* this, PlayState* play);
void OotmmMagicWind_Grow(OotmmMagicWind* this, PlayState* play);
void OotmmMagicWind_Marker(OotmmMagicWind* this, PlayState* play);

#define OOTMM_MAGIC_WIND_PARAMS_MARKER 2

ActorProfile Ootmm_Magic_Wind_Profile = {
    /**/ ACTOR_OOTMM_MAGIC_WIND,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OotmmMagicWind),
    /**/ OotmmMagicWind_Init,
    /**/ OotmmMagicWind_Destroy,
    /**/ OotmmMagicWind_Update,
    /**/ OotmmMagicWind_Draw,
};

static u8 sAnimKnotCounts[27] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static s16 sAnimConstantData[25] = {
    1024, 1024, 1024, 0, 0, 0, 0, 0, 0, 512, 512, 0, 0,
    0,    0,    0,    0, 717, 717, 0, 0, 0, 0, 0, 0,
};

static CurveInterpKnot sAnimInterpolationData[4] = {
    { 0x000C, 0x0001, 1, 1, 0.0f },
    { 0x0014, 0x003C, 0, 0, 1.5f },
    { 0x000C, 0x0001, 1, 1, 0.0f },
    { 0x0014, 0x003C, 0, 0, 1.0f },
};

static CurveAnimationHeader sAnim = {
    sAnimKnotCounts,
    sAnimInterpolationData,
    sAnimConstantData,
    1,
    60,
};

static SkelCurveLimb sRootLimb = { 0x01, LIMB_DONE, { NULL, NULL } };
static SkelCurveLimb sInnerCylinderLimb = { LIMB_DONE, 0x02, { NULL, NULL } };
static SkelCurveLimb sOuterCylinderLimb = { LIMB_DONE, LIMB_DONE, { NULL, NULL } };

static SkelCurveLimb* sSkelLimbs[3] = {
    &sRootLimb,
    &sInnerCylinderLimb,
    &sOuterCylinderLimb,
};

static CurveSkeletonHeader sSkel = { sSkelLimbs, ARRAY_COUNT(sSkelLimbs) };

static u8 sAlphaVtxIndices[] = {
    0x00, 0x03, 0x04, 0x07, 0x09, 0x0A, 0x0D, 0x0F, 0x11,
    0x12, 0x15, 0x16, 0x19, 0x1B, 0x1C, 0x1F, 0x21, 0x23,
};

static Vtx* sCylinderVtx = NULL;

void OotmmMagicWind_SetupAction(OotmmMagicWind* this, OotmmMagicWindActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OotmmMagicWind_UpdateAlpha(f32 alpha) {
    s32 i;

    if (sCylinderVtx == NULL) {
        return;
    }
    for (i = 0; i < ARRAY_COUNT(sAlphaVtxIndices); i++) {
        sCylinderVtx[sAlphaVtxIndices[i]].n.a = alpha * 255.0f;
    }
}

void OotmmMagicWind_Init(Actor* thisx, PlayState* play) {
    OotmmMagicWind* this = (OotmmMagicWind*)thisx;
    Player* player = GET_PLAYER(play);

    this->actor.room = -1;

    if (this->actor.params == OOTMM_MAGIC_WIND_PARAMS_MARKER) {
        Math_Vec3f_Copy(&this->actor.world.pos, &gSaveContext.respawn[RESPAWN_MODE_HUMAN].pos);
        this->markerRatio = 1.0f;
        this->markerAlpha = 255;
        this->markerYOffset = 60.0f;
        Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y + 60.0f,
                                  this->actor.world.pos.z, 255, 255, 255, -1);
        this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
        OotmmMagicWind_SetupAction(this, OotmmMagicWind_Marker);
        return;
    }

    if (!ResourceMgr_FileExists(OOTMM_MAGIC_WIND_INNER_DL) || !ResourceMgr_FileExists(OOTMM_MAGIC_WIND_OUTER_DL) ||
        !ResourceMgr_FileExists(OOTMM_MAGIC_WIND_VTX)) {
        Actor_Kill(thisx);
        return;
    }

    sInnerCylinderLimb.dList[1] = ResourceMgr_LoadGfxByName(OOTMM_MAGIC_WIND_INNER_DL);
    sOuterCylinderLimb.dList[1] = ResourceMgr_LoadGfxByName(OOTMM_MAGIC_WIND_OUTER_DL);
    sCylinderVtx = ResourceMgr_LoadVtxByName(OOTMM_MAGIC_WIND_VTX);

    SkelCurve_Init(play, &this->skelCurve, &sSkel, &sAnim);

    switch (this->actor.params) {
        case 0:
            SkelCurve_SetAnim(&this->skelCurve, &sAnim, 0.0f, 60.0f, 0.0f, 1.0f);
            this->timer = 29;
            OotmmMagicWind_SetupAction(this, OotmmMagicWind_WaitForTimer);
            break;

        case 1:
            SkelCurve_SetAnim(&this->skelCurve, &sAnim, 60.0f, 0.0f, 60.0f, -1.0f);
            OotmmMagicWind_SetupAction(this, OotmmMagicWind_Shrink);
            Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_WARP);
            break;
    }
}

void OotmmMagicWind_Destroy(Actor* thisx, PlayState* play) {
    OotmmMagicWind* this = (OotmmMagicWind*)thisx;

    if (this->actor.params == OOTMM_MAGIC_WIND_PARAMS_MARKER) {
        LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
        return;
    }

    SkelCurve_Destroy(play, &this->skelCurve);
    Magic_Reset(play);
}

void OotmmMagicWind_Marker(OotmmMagicWind* this, PlayState* play) {
    RespawnData* point = &gSaveContext.respawn[RESPAWN_MODE_HUMAN];
    s32 params = point->data;
    f32 yOffset = 60.0f;
    f32 ratio = 1.0f;
    s32 alpha = 255;
    s32 temp = params - 40;

    if (params == 0) {
        Actor_Kill(&this->actor);
        return;
    }

    if (temp < 0) {
        point->data = ++params;
        if (params == 0) {
            Actor_Kill(&this->actor);
            return;
        }
        ratio = ABS_ALT(params) * 0.025f;
        this->timer = 60;
        this->markerDrift = 1.0f;
    } else if (this->timer != 0) {
        this->timer--;
    } else if (this->markerDrift > 0.0f) {
        static Vec3f sSparkleVel = { 0.0f, -0.05f, 0.0f };
        static Vec3f sSparkleAccel = { 0.0f, -0.025f, 0.0f };
        static Color_RGBA8 sSparklePrimColor = { 255, 255, 255, 0 };
        static Color_RGBA8 sSparkleEnvColor = { 100, 200, 0, 0 };
        Vec3f* curPos = &point->pos;
        Vec3f* nextPos = &gSaveContext.respawn[RESPAWN_MODE_DOWN].pos;
        f32 prevDrift = this->markerDrift;
        Vec3f dist;
        f32 diff = Math_Vec3f_DistXYZAndStoreDiff(nextPos, curPos, &dist);
        Vec3f sparklePos;
        f32 factor;
        f32 length;
        f32 dx;
        f32 speed;

        if (diff < 20.0f) {
            this->markerDrift = 0.0f;
            Math_Vec3f_Copy(curPos, nextPos);
        } else {
            length = diff * (1.0f / this->markerDrift);
            speed = CLAMP_MIN(20.0f / length, 0.05f);
            Math_StepToF(&this->markerDrift, 0.0f, speed);
            factor = this->markerDrift / prevDrift;
            curPos->x = nextPos->x + (dist.x * factor);
            curPos->y = nextPos->y + (dist.y * factor);
            curPos->z = nextPos->z + (dist.z * factor);
            length *= 0.5f;
            dx = diff - length;
            yOffset += sqrtf(SQ(length) - SQ(dx)) * 0.2f;
        }

        sparklePos.x = curPos->x + Rand_CenteredFloat(6.0f);
        sparklePos.y = curPos->y + 80.0f + (6.0f * Rand_ZeroOne());
        sparklePos.z = curPos->z + Rand_CenteredFloat(6.0f);
        EffectSsKirakira_SpawnDispersed(play, &sparklePos, &sSparkleVel, &sSparkleAccel, &sSparklePrimColor,
                                        &sSparkleEnvColor, 1000, 16);

        if (this->markerDrift == 0.0f) {
            *point = gSaveContext.respawn[RESPAWN_MODE_DOWN];
            point->playerParams = PLAYER_PARAMS(0xFF, PLAYER_START_MODE_D);
            point->data = 40;
        }
    } else if (temp > 0) {
        Vec3f* curPos = &point->pos;
        f32 nextRatio = 1.0f - (temp * 0.1f);
        f32 curRatio = 1.0f - ((temp - 1) * 0.1f);
        Vec3f eye;
        Vec3f dist;

        if (nextRatio > 0.0f) {
            f32 step;

            eye.x = play->view.eye.x;
            eye.y = play->view.eye.y - yOffset;
            eye.z = play->view.eye.z;
            Math_Vec3f_DistXYZAndStoreDiff(&eye, curPos, &dist);
            step = nextRatio / curRatio;
            curPos->x = eye.x + (dist.x * step);
            curPos->y = eye.y + (dist.y * step);
            curPos->z = eye.z + (dist.z * step);
        }

        alpha = 255 - (temp * 30);
        if (alpha < 0) {
            point->data = 0;
            OotmmCustomItems_ClearFaroresWind();
            Actor_Kill(&this->actor);
            return;
        }
        point->data = ++params;
        ratio = 1.0f + (temp * 0.2f);
    }

    Math_Vec3f_Copy(&this->actor.world.pos, &point->pos);
    this->markerRatio = ratio;
    this->markerAlpha = alpha;
    this->markerYOffset = yOffset;
    Lights_PointNoGlowSetInfo(&this->lightInfo, point->pos.x, point->pos.y + yOffset, point->pos.z, 255, 255, 255,
                              (s32)(500.0f * ratio));
}

void OotmmMagicWind_WaitForTimer(OotmmMagicWind* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->timer > 0) {
        this->timer--;
        return;
    }

    Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_NORMAL);
    OotmmMagicWind_UpdateAlpha(1.0f);
    OotmmMagicWind_SetupAction(this, OotmmMagicWind_Grow);
    SkelCurve_Update(play, &this->skelCurve);
}

void OotmmMagicWind_Grow(OotmmMagicWind* this, PlayState* play) {
    if (SkelCurve_Update(play, &this->skelCurve)) {
        OotmmMagicWind_SetupAction(this, OotmmMagicWind_WaitAtFullSize);
        this->timer = 50;
    }
}

void OotmmMagicWind_WaitAtFullSize(OotmmMagicWind* this, PlayState* play) {
    if (this->timer > 0) {
        this->timer--;
    } else {
        OotmmMagicWind_SetupAction(this, OotmmMagicWind_FadeOut);
        this->timer = 30;
    }
}

void OotmmMagicWind_FadeOut(OotmmMagicWind* this, PlayState* play) {
    if (this->timer > 0) {
        OotmmMagicWind_UpdateAlpha((f32)this->timer * (1.0f / 30.0f));
        this->timer--;
    } else {
        Actor_Kill(&this->actor);
    }
}

void OotmmMagicWind_Shrink(OotmmMagicWind* this, PlayState* play) {
    if (SkelCurve_Update(play, &this->skelCurve)) {
        Actor_Kill(&this->actor);
    }
}

void OotmmMagicWind_Update(Actor* thisx, PlayState* play) {
    OotmmMagicWind* this = (OotmmMagicWind*)thisx;

    this->actionFunc(this, play);
}

s32 OotmmMagicWind_OverrideLimbDraw(PlayState* play, SkelCurve* skelCurve, s32 limbIndex, Actor* thisx) {
    OPEN_DISPS(play->state.gfxCtx);

    if (limbIndex == 1) {
        gSPSegment(POLY_XLU_DISP++, 8,
                   Gfx_TwoTexScroll(play->state.gfxCtx, G_TX_RENDERTILE, (play->state.frames * 9) & 0xFF,
                                    0xFF - ((play->state.frames * 0xF) & 0xFF), 0x40, 0x40, 1,
                                    (play->state.frames * 0xF) & 0xFF, 0xFF - ((play->state.frames * 0x1E) & 0xFF),
                                    0x40, 0x40));
    } else if (limbIndex == 2) {
        gSPSegment(POLY_XLU_DISP++, 9,
                   Gfx_TwoTexScroll(play->state.gfxCtx, G_TX_RENDERTILE, (play->state.frames * 3) & 0xFF,
                                    0xFF - ((play->state.frames * 5) & 0xFF), 0x40, 0x40, 1,
                                    (play->state.frames * 6) & 0xFF, 0xFF - ((play->state.frames * 0xA) & 0xFF), 0x40,
                                    0x40));
    }

    CLOSE_DISPS(play->state.gfxCtx);

    return true;
}

void OotmmMagicWind_DrawMarker(OotmmMagicWind* this, PlayState* play) {
    RespawnData* point = &gSaveContext.respawn[RESPAWN_MODE_HUMAN];
    s32 sceneMatch = OotmmCustomItems_FaroresWindSceneMatch(play);
    f32 scale = 0.025f * this->markerRatio;
    f32 yPos;

    if ((play->csCtx.state != CS_STATE_IDLE) || (sceneMatch == 0)) {
        return;
    }
    if ((this->markerDrift == 0.0f) && (point->roomIndex != play->roomCtx.curRoom.num) &&
        (point->roomIndex != play->roomCtx.prevRoom.num)) {
        return;
    }

    yPos = point->pos.y + this->markerYOffset;
    if (sceneMatch == 2) {
        yPos = -yPos;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Translate(point->pos.x, yPos, point->pos.z, MTXMODE_NEW);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
    Matrix_Push();

    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetPrimColor(POLY_XLU_DISP++, 128, 128, 255, 255, 200, this->markerAlpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 100, 200, 0, 255);

    Matrix_RotateZF(BINANG_TO_RAD_ALT2((play->gameplayFrames * 1500) & 0xFFFF), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, gEffFlash1DL);

    Matrix_Pop();
    Matrix_RotateZF(BINANG_TO_RAD_ALT2(~((play->gameplayFrames * 1200) & 0xFFFF)), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, gEffFlash1DL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void OotmmMagicWind_Draw(Actor* thisx, PlayState* play) {
    OotmmMagicWind* this = (OotmmMagicWind*)thisx;

    if (this->actor.params == OOTMM_MAGIC_WIND_PARAMS_MARKER) {
        OotmmMagicWind_DrawMarker(this, play);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    if (this->actionFunc != OotmmMagicWind_WaitForTimer) {
        POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 25);
        SkelCurve_Draw(&this->actor, play, &this->skelCurve, OotmmMagicWind_OverrideLimbDraw, NULL, 1, NULL);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
