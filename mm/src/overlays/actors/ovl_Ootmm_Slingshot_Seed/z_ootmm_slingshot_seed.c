/*
 * File: z_ootmm_slingshot_seed.c
 * Overlay: ovl_Ootmm_Slingshot_Seed
 * Description: Slingshot deku seed projectile, ported from Ocarina of Time for OoTMM
 */

#include "z_ootmm_slingshot_seed.h"

#include "assets/objects/gameplay_keep/gameplay_keep.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void OotmmSlingshotSeed_Init(Actor* thisx, PlayState* play);
void OotmmSlingshotSeed_Destroy(Actor* thisx, PlayState* play);
void OotmmSlingshotSeed_Update(Actor* thisx, PlayState* play);
void OotmmSlingshotSeed_Draw(Actor* thisx, PlayState* play);

void OotmmSlingshotSeed_Shoot(OotmmSlingshotSeed* this, PlayState* play);
void OotmmSlingshotSeed_Fly(OotmmSlingshotSeed* this, PlayState* play);

ActorProfile Ootmm_Slingshot_Seed_Profile = {
    /**/ ACTOR_OOTMM_SLINGSHOT_SEED,
    /**/ ACTORCAT_ITEMACTION,
    /**/ FLAGS,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(OotmmSlingshotSeed),
    /**/ OotmmSlingshotSeed_Init,
    /**/ OotmmSlingshotSeed_Destroy,
    /**/ OotmmSlingshotSeed_Update,
    /**/ OotmmSlingshotSeed_Draw,
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
        { 1 << 0x10, 0x00, 0x01 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NONE,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(terminalVelocity, -150, ICHAIN_STOP),
};

static void OotmmSlingshotSeed_SetProjectileSpeed(Actor* actor, f32 speed) {
    actor->speed = Math_CosS(actor->world.rot.x) * speed;
    actor->velocity.y = -Math_SinS(actor->world.rot.x) * speed;
}

static void OotmmSlingshotSeed_SetupAction(OotmmSlingshotSeed* this, OotmmSlingshotSeedActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void OotmmSlingshotSeed_Init(Actor* thisx, PlayState* play) {
    OotmmSlingshotSeed* this = (OotmmSlingshotSeed*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);

    Collider_InitQuad(play, &this->collider);
    Collider_SetQuad(play, &this->collider, &this->actor, &sQuadInit);

    this->timer = 0;
    this->activeTimer = 0;
    this->touchedPoly = false;

    OotmmSlingshotSeed_SetupAction(this, OotmmSlingshotSeed_Shoot);
}

void OotmmSlingshotSeed_Destroy(Actor* thisx, PlayState* play) {
    OotmmSlingshotSeed* this = (OotmmSlingshotSeed*)thisx;

    Collider_DestroyQuad(play, &this->collider);
}

void OotmmSlingshotSeed_Shoot(OotmmSlingshotSeed* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->actor.parent != NULL) {
        return;
    }

    if (player->unk_D57 == 0) {
        Actor_Kill(&this->actor);
        return;
    }

    Actor_PlaySfx_Flagged(&this->actor, NA_SE_IT_SLING_SHOT);

    OotmmSlingshotSeed_SetProjectileSpeed(&this->actor, 80.0f);

    this->timer = 15;

    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.y = 0;
    this->actor.shape.rot.z = 0;

    OotmmSlingshotSeed_SetupAction(this, OotmmSlingshotSeed_Fly);
}

static void OotmmSlingshotSeed_SpawnSeedPop(PlayState* play, Vec3f* pos) {
    EffectSsStone1_Spawn(play, pos, 0);
    SoundSource_PlaySfxAtFixedWorldPos(play, pos, 20, NA_SE_IT_SLING_REFLECT);
}

void OotmmSlingshotSeed_Fly(OotmmSlingshotSeed* this, PlayState* play) {
    CollisionPoly* hitPoly;
    Vec3f hitPoint;
    s32 bgId;
    s32 collided;

    if (DECR(this->timer) == 0) {
        Actor_Kill(&this->actor);
        return;
    }

    if (this->timer < 8) {
        this->actor.gravity = -0.4f;
    }

    collided = (this->collider.base.atFlags & AT_HIT) != 0;

    if (collided || this->touchedPoly) {
        if (collided) {
            this->actor.world.pos.x = (this->actor.world.pos.x + this->actor.prevPos.x) * 0.5f;
            this->actor.world.pos.y = (this->actor.world.pos.y + this->actor.prevPos.y) * 0.5f;
            this->actor.world.pos.z = (this->actor.world.pos.z + this->actor.prevPos.z) * 0.5f;
        }

        OotmmSlingshotSeed_SpawnSeedPop(play, &this->actor.world.pos);
        Actor_Kill(&this->actor);
        return;
    }

    Actor_MoveWithGravity(&this->actor);

    this->touchedPoly = BgCheck_EntityLineTest1(&play->colCtx, &this->actor.prevPos, &this->actor.world.pos,
                                                &hitPoint, &hitPoly, true, true, true, true, &bgId);

    if (this->touchedPoly) {
        Math_Vec3f_Copy(&this->actor.world.pos, &hitPoint);
    }
}

void OotmmSlingshotSeed_Update(Actor* thisx, PlayState* play) {
    OotmmSlingshotSeed* this = (OotmmSlingshotSeed*)thisx;
    Player* player = GET_PLAYER(play);

    if (!(player->stateFlags1 & PLAYER_STATE1_20000000)) {
        this->actionFunc(this, play);
        Actor_SetFocus(&this->actor, 0.0f);
        this->activeTimer++;
    }
}

static void OotmmSlingshotSeed_UpdateCollider(OotmmSlingshotSeed* this, PlayState* play) {
    static Vec3f sPosAOffset = { 0.0f, 400.0f, 1500.0f };
    static Vec3f sPosBOffset = { 0.0f, -400.0f, 1500.0f };
    Vec3f posA;
    Vec3f posB;

    if (this->actionFunc != OotmmSlingshotSeed_Fly) {
        return;
    }
    Matrix_MultVec3f(&sPosAOffset, &posA);
    Matrix_MultVec3f(&sPosBOffset, &posB);
    func_80126440(play, &this->collider, &this->weaponInfo, &posA, &posB);
}

void OotmmSlingshotSeed_Draw(Actor* thisx, PlayState* play) {
    OotmmSlingshotSeed* this = (OotmmSlingshotSeed*)thisx;
    u8 alpha;
    s32 spin;

    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);

    if (this->actor.speed != 0.0f) {
        alpha = (Math_CosS(this->timer * 5000) * 127.5f) + 127.5f;

        OPEN_DISPS(play->state.gfxCtx);

        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gDPPipeSync(POLY_XLU_DISP++);
        gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BACK);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 255, 255, alpha);

        Matrix_Push();
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
        spin = (play->gameplayFrames & 0xFF) * 4000;
        Matrix_RotateZS(spin, MTXMODE_APPLY);
        Matrix_Scale(50.0f, 50.0f, 50.0f, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, gEffSparklesDL);
        Matrix_Pop();

        Matrix_RotateYS(this->actor.world.rot.y, MTXMODE_APPLY);

        CLOSE_DISPS(play->state.gfxCtx);
    }

    OotmmSlingshotSeed_UpdateCollider(this, play);
}
