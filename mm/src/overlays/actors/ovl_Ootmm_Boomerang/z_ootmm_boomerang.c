/*
 * File: z_ootmm_boomerang.c
 * Overlay: ovl_Ootmm_Boomerang
 * Description: Human Link's boomerang, ported from Ocarina of Time for OoTMM
 */

#include "z_ootmm_boomerang.h"

#include "2s2h/OotmmCustomEquipment.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void OotmmBoomerang_Init(Actor* thisx, PlayState* play);
void OotmmBoomerang_Destroy(Actor* thisx, PlayState* play);
void OotmmBoomerang_Update(Actor* thisx, PlayState* play);
void OotmmBoomerang_Draw(Actor* thisx, PlayState* play);

void OotmmBoomerang_Fly(OotmmBoomerang* this, PlayState* play);

ActorProfile Ootmm_Boomerang_Profile = {
    /**/ ACTOR_OOTMM_BOOMERANG,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OotmmBoomerang),
    /**/ OotmmBoomerang_Init,
    /**/ OotmmBoomerang_Destroy,
    /**/ OotmmBoomerang_Update,
    /**/ OotmmBoomerang_Draw,
};

static ColliderQuadInit sQuadInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_QUAD,
    },
    {
        ELEM_MATERIAL_UNK2,
        { 0x00000010, 0x00, 0x01 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_S8(attentionRangeType, ATTENTION_RANGE_5, ICHAIN_CONTINUE),
    ICHAIN_VEC3S(shape.rot, 0, ICHAIN_STOP),
};

static void OotmmBoomerang_SetProjectileSpeed(Actor* actor, f32 speed) {
    actor->speed = Math_CosS(actor->world.rot.x) * speed;
    actor->velocity.y = -Math_SinS(actor->world.rot.x) * speed;
}

static void OotmmBoomerang_SetupAction(OotmmBoomerang* this, OotmmBoomerangActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OotmmBoomerang_Init(Actor* thisx, PlayState* play) {
    static u8 sP1StartColor[4] = { 255, 255, 100, 255 };
    static u8 sP2StartColor[4] = { 255, 255, 100, 64 };
    static u8 sP1EndColor[4] = { 255, 255, 100, 0 };
    static u8 sP2EndColor[4] = { 255, 255, 100, 0 };
    OotmmBoomerang* this = (OotmmBoomerang*)thisx;
    EffectBlureInit1 blureInit;
    s32 i;

    this->actor.room = -1;
    Actor_ProcessInitChain(&this->actor, sInitChain);

    memset(&blureInit, 0, sizeof(blureInit));

    for (i = 0; i < 4; i++) {
        blureInit.p1StartColor[i] = sP1StartColor[i];
        blureInit.p2StartColor[i] = sP2StartColor[i];
        blureInit.p1EndColor[i] = sP1EndColor[i];
        blureInit.p2EndColor[i] = sP2EndColor[i];
    }

    blureInit.elemDuration = 8;
    blureInit.unkFlag = 0;
    blureInit.calcMode = 0;

    this->effectIndex = -1;
    Effect_Add(play, &this->effectIndex, EFFECT_BLURE1, 0, 0, &blureInit);

    Collider_InitQuad(play, &this->collider);
    Collider_SetQuad(play, &this->collider, &this->actor, &sQuadInit);

    OotmmBoomerang_SetupAction(this, OotmmBoomerang_Fly);
}

void OotmmBoomerang_Destroy(Actor* thisx, PlayState* play) {
    OotmmBoomerang* this = (OotmmBoomerang*)thisx;

    if (this->effectIndex >= 0) {
        Effect_Destroy(play, this->effectIndex);
    }

    Collider_DestroyQuad(play, &this->collider);
}

void OotmmBoomerang_Fly(OotmmBoomerang* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Actor* target = this->moveTo;
    s32 collided;
    s16 yawTarget;
    s16 yawDiff;
    s16 pitchTarget;
    s16 pitchDiff;
    f32 distScale;
    f32 distFromLink;
    s32 hitBgId;
    Vec3f hitPoint;
    CollisionPoly* hitPoly;

    if (target != NULL) {
        yawTarget = Actor_WorldYawTowardPoint(&this->actor, &target->focus.pos);
        yawDiff = this->actor.world.rot.y - yawTarget;

        pitchTarget = Actor_WorldPitchTowardPoint(&this->actor, &target->focus.pos);
        pitchDiff = this->actor.world.rot.x - pitchTarget;

        distScale = (200.0f - Math_Vec3f_DistXYZ(&this->actor.world.pos, &target->focus.pos)) * 0.005f;
        if (distScale < 0.12f) {
            distScale = 0.12f;
        }

        if ((target != &player->actor) && ((target->update == NULL) || (ABS_ALT(yawDiff) > 0x4000))) {
            this->moveTo = NULL;
        } else {
            Math_ScaledStepToS(&this->actor.world.rot.y, yawTarget, (s16)(ABS_ALT(yawDiff) * distScale));
            Math_ScaledStepToS(&this->actor.world.rot.x, pitchTarget, (s16)(ABS_ALT(pitchDiff) * distScale));
        }
    }

    OotmmBoomerang_SetProjectileSpeed(&this->actor, 12.0f);
    Actor_MoveWithGravity(&this->actor);

    Actor_PlaySfx_Flagged(&this->actor, NA_SE_IT_BOOMERANG_FLY - SFX_FLAG);

    if (this->collider.base.atFlags & AT_HIT) {
        if ((this->collider.base.at != NULL) &&
            ((this->collider.base.at->id == ACTOR_EN_ITEM00) || (this->collider.base.at->id == ACTOR_EN_SI))) {
            this->grabbed = this->collider.base.at;

            if (this->collider.base.at->id == ACTOR_EN_SI) {
                this->collider.base.at->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
            } else {
                this->collider.base.at->gravity = 0.0f;
            }
        }
    }

    if (DECR(this->returnTimer) == 0) {
        distFromLink = Math_Vec3f_DistXYZ(&this->actor.world.pos, &player->actor.focus.pos);
        this->moveTo = &player->actor;

        if (distFromLink < 40.0f) {
            target = this->grabbed;

            if (target != NULL) {
                Math_Vec3f_Copy(&target->world.pos, &player->actor.world.pos);

                if (target->id == ACTOR_EN_ITEM00) {
                    target->gravity = -0.9f;
                    target->bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);
                } else {
                    target->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
                }
            }

            player->stateFlags1 &= ~PLAYER_STATE1_ZORA_BOOMERANG_THROWN;
            player->zoraBoomerangActor = NULL;
            Actor_Kill(&this->actor);
        }
    } else {
        collided = BgCheck_EntityLineTest1(&play->colCtx, &this->actor.prevPos, &this->actor.world.pos, &hitPoint,
                                           &hitPoly, true, true, true, true, &hitBgId);

        if (collided) {
            if (func_800B90AC(play, &this->actor, hitPoly, hitBgId, &hitPoint)) {
                collided = false;
            } else {
                CollisionCheck_SpawnShieldParticlesMetal(play, &hitPoint);
            }
        }

        if (collided) {
            this->actor.world.rot.x = -this->actor.world.rot.x;
            this->actor.world.rot.y += 0x8000;
            this->moveTo = &player->actor;
            this->returnTimer = 0;
        }
    }

    target = this->grabbed;

    if (target != NULL) {
        if (target->update == NULL) {
            this->grabbed = NULL;
        } else {
            Math_Vec3f_Copy(&target->world.pos, &this->actor.world.pos);
        }
    }
}

void OotmmBoomerang_Update(Actor* thisx, PlayState* play) {
    OotmmBoomerang* this = (OotmmBoomerang*)thisx;
    Player* player = GET_PLAYER(play);

    if (!(player->stateFlags1 & PLAYER_STATE1_20000000)) {
        this->actionFunc(this, play);
        Actor_SetFocus(&this->actor, 0.0f);
        this->activeTimer++;
    }
}

void OotmmBoomerang_Draw(Actor* thisx, PlayState* play) {
    static Vec3f sPosAOffset = { -960.0f, 0.0f, 0.0f };
    static Vec3f sPosBOffset = { 960.0f, 0.0f, 0.0f };
    OotmmBoomerang* this = (OotmmBoomerang*)thisx;
    Gfx* model = OotmmEquipment_BoomerangFlightDList();
    Vec3f posA;
    Vec3f posB;
    void* blure;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    Matrix_RotateYS(this->actor.world.rot.y, MTXMODE_APPLY);
    Matrix_RotateZS(0x1F40, MTXMODE_APPLY);
    Matrix_RotateXS(this->actor.world.rot.x, MTXMODE_APPLY);

    Matrix_MultVec3f(&sPosAOffset, &posA);
    Matrix_MultVec3f(&sPosBOffset, &posB);

    if (func_80126440(play, &this->collider, &this->weaponInfo, &posA, &posB)) {
        blure = Effect_GetByIndex(this->effectIndex);
        if (blure != NULL) {
            EffectBlure_AddVertex(blure, &posA, &posB);
        }
    }

    if (model != NULL) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        Matrix_RotateYS(this->activeTimer * 0x2EE0, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, model);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
