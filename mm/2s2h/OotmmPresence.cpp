#include "OotmmPresence.h"

#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/NameTag/NameTag.h"
#include "OotmmSession.h"
#include "OotmmIpc.h"
#include "OotmmCustomItems.h"
#include "OotmmCustomEquipment.h"
#include "OotmmAdultLink.h"
#include "OotmmChildLink.h"

#include <libultraship/bridge/OotmmPresence.h>
#include <ship/Context.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

extern PlayState* gPlayState;
extern FlexSkeletonHeader* gPlayerSkeletons[PLAYER_FORM_MAX];
extern PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX];
extern TexturePtr sPlayerEyesTextures[PLAYER_FORM_MAX][PLAYER_EYES_MAX];
extern TexturePtr sPlayerMouthTextures[PLAYER_FORM_MAX][PLAYER_MOUTH_MAX];
extern const char* D_801C0B20[];
extern Gfx* gPlayerShields[];

Gfx* ResourceMgr_LoadGfxByName(const char* path);
uint8_t ResourceMgr_FileExists(const char* resName);

void Player_DrawImpl(PlayState* play, void** skeleton, Vec3s* jointTable, s32 dListCount, s32 lod,
                     PlayerTransformation playerForm, s32 boots, s32 face, OverrideLimbDrawFlex overrideLimbDraw,
                     PostLimbDrawFlex postLimbDraw, Actor* actor);
}

const char* gOotmmEquipDlCapture[4] = { NULL, NULL, NULL, NULL };
static int32_t sCustomHandCapture[2] = { 0, 0 };

extern "C" void OotmmPresence_CaptureCustomHand(int32_t index, int32_t which) {
    if (index >= 0 && index <= 1) {
        sCustomHandCapture[index] = which;
    }
}

extern "C" void OotmmPresence_CaptureEquipDl(int32_t index, void* dl) {
    if (index < 0 || index > 3) {
        return;
    }
    if (dl == NULL) {
        gOotmmEquipDlCapture[index] = "-";
    } else if (std::strncmp(static_cast<const char*>(dl), "__OTR__", 7) == 0) {
        gOotmmEquipDlCapture[index] = static_cast<const char*>(dl);
    } else {
        gOotmmEquipDlCapture[index] = "";
    }
}

namespace {

constexpr int kLimbBuf = 24;

int32_t gPuppetForm = -1; // puppet form during a puppet draw, -1 otherwise
int32_t gPuppetMoveFlags = 0;
const char* gPuppetEquipDl[4] = { NULL, NULL, NULL, NULL };
uint8_t gPuppetMask = 0;
s32 gPuppetItemAction = -1;
s32 gPuppetCustomMask = 0;
u8 gPuppetShield = 0;
u8 gPuppetSheathType = 0;
u8 gPuppetRightHandType = 0;
bool gPuppetDekuShield = false;
s32 gPuppetCustomHand[2] = { 0, 0 };
int32_t gPuppetPlayerId = 0;   // drawing puppet's owner during a puppet draw, 0 otherwise
int32_t gPuppetHumanModel = 0; // that puppet's human-form model source (pose "hm")
int32_t gPuppetTunic = 0;      // that puppet's synced tunic (colors ported cloth via ENV)

struct PuppetState {
    SkelAnime skelAnime{};
    Vec3s jointTable[kLimbBuf]{};
    Vec3s morphTable[kLimbBuf]{};
    Vec3s netJoints[kLimbBuf]{};
    Vec3f targetPos{};
    s16 targetYaw = 0;
    u8 moveFlags = 0;
    u8 form = PLAYER_FORM_HUMAN;
    u8 humanModel = 0;
    uint16_t playerId = 0;
    bool hasPose = false;
    bool skeletonReady = false;
    // Backs the FlexSkeletonHeader* handed to SkelAnime whenever the puppet wears a
    // namespaced skeleton; the string must outlive the SkelAnime that points into it.
    std::string skelPath;
    std::string dlLeftHand;
    std::string dlRightHand;
    std::string dlSheath;
    std::string dlWaist;
    u8 boots = 0;
    u8 mask = 0;
    s32 itemAction = -1;
    s32 customMask = 0;
    u8 shield = 0;
    u8 sheathType = 0;
    u8 rightHandType = 0;
    bool dekuShield = false;
    s32 customHand[2] = { 0, 0 };
    s32 customBoots = 0;
    s32 tunic = 0;
    ColliderCylinder collider{};
    bool colliderReady = false;
    u8 pvpCooldown = 0;
};

struct PuppetSlot {
    Actor* actor = nullptr;
    std::unique_ptr<PuppetState> state;
    int staleTicks = 0;
    uint32_t lastSeq = 0;
    std::string taggedName;
};

std::unordered_map<uint16_t, PuppetSlot> gPuppets;
std::unordered_map<Actor*, PuppetState*> gStateByActor;
uint32_t gLocalSeq = 0;
bool gPvpEnabled = false;

struct PendingPvpHit {
    uint16_t targetPlayer;
    int damage;
    s16 yaw;
};
std::vector<PendingPvpHit> gPendingPvpHits;

ColliderCylinderInit sPuppetCylinderInit = {
    {
        COL_MATERIAL_HIT5,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 20, 60, 0, { 0, 0, 0 } },
};

// Composes "__OTR__pNNobjs/…" for a canonical objects/ path when player N's synced
// namespace provides it; the caller's buffer holds the result.
const char* PuppetNamespaced(char* buf, size_t cap, uint16_t playerId, const char* canonical) {
    if (playerId == 0 || canonical == nullptr || std::strncmp(canonical, "objects/", 8) != 0) {
        return nullptr;
    }
    std::snprintf(buf, cap, "__OTR__p%02xobjs/%s", playerId & 0xFF, canonical + 8);
    return ResourceMgr_FileExists(buf + 7) ? buf : nullptr;
}

Gfx* PuppetLoadGfx(const char* name) {
    char ns[128];
    if (PuppetNamespaced(ns, sizeof(ns), static_cast<uint16_t>(gPuppetPlayerId), name) != nullptr) {
        return ResourceMgr_LoadGfxByName(ns + 7);
    }
    return ResourceMgr_LoadGfxByName(name);
}

// The human form draws whichever model the sender reported; the sender's synced pNN
// namespace overrides any form, and an unparseable resource falls back before SkelAnime.
FlexSkeletonHeader* PuppetSkeleton(PuppetState* state, u8 form) {
    const char* canonical = nullptr;
    if (form == PLAYER_FORM_HUMAN) {
        canonical = state->humanModel == 2   ? "objects/ot_obj_link_boy/gLinkAdultSkel"
                    : state->humanModel == 1 ? "objects/ot_obj_link_child/gLinkChildSkel"
                                             : "objects/object_link_child/gLinkHumanSkel";
    } else if (const char* live = reinterpret_cast<const char*>(gPlayerSkeletons[form]);
               live != nullptr && std::strncmp(live, "__OTR__", 7) == 0) {
        canonical = live + 7;
    }

    char ns[96];
    const char* path = PuppetNamespaced(ns, sizeof(ns), state->playerId, canonical);
    if (path == nullptr && form == PLAYER_FORM_HUMAN && state->humanModel != 0) {
        // Nothing synced: the canonical ported object (ootmm_assets' vanilla OoT Link)
        // still matches the sender better than this machine's human model.
        std::snprintf(ns, sizeof(ns), "__OTR__%s", canonical);
        path = ResourceMgr_FileExists(ns + 7) ? ns : nullptr;
    }
    if (path != nullptr &&
        Ship::Context::GetInstance()->GetResourceManager()->LoadResource(path + 7) != nullptr) {
        state->skelPath = path;
        return reinterpret_cast<FlexSkeletonHeader*>(const_cast<char*>(state->skelPath.c_str()));
    }
    return gPlayerSkeletons[form];
}

void PuppetInitSkeleton(PuppetState* state, PlayState* play, u8 form) {
    if (form >= PLAYER_FORM_MAX) {
        form = PLAYER_FORM_HUMAN;
    }
    SkelAnime_InitPlayer(play, &state->skelAnime, PuppetSkeleton(state, form),
                         (PlayerAnimationHeader*)gPlayerAnim_link_normal_wait, 1 | 8, state->jointTable,
                         state->morphTable, PLAYER_LIMB_MAX);
    state->form = form;
    state->skeletonReady = state->skelAnime.skeleton != nullptr;
}

} // namespace

// The joint table omits the root-limb scaling the draw applies per form and movement flag, and the
// four equipment limbs are always substituted, so both are replayed from the sender.
extern "C" s32 OotmmPuppet_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            Actor* actor) {
    (void)play;
    (void)rot;
    (void)actor;
    if (limbIndex == PLAYER_LIMB_ROOT && gPuppetForm >= 0 && gPuppetForm < PLAYER_FORM_MAX &&
        gPuppetForm != PLAYER_FORM_FIERCE_DEITY) {
        // The sender's human height depends on THEIR mapper state, never this client's
        // tables — locally the human row holds adult metrics while our own mapper is on.
        const f32 scale =
            gPuppetForm == PLAYER_FORM_HUMAN
                ? (gPuppetHumanModel == 2 ? sPlayerAgeProperties[PLAYER_FORM_ZORA].unk_08
                                          : OotmmAdultLink_VanillaHumanRootScale())
                : sPlayerAgeProperties[gPuppetForm].unk_08;
        const int32_t mf = gPuppetMoveFlags;
        if (!(mf & 4) || (mf & 1)) {
            pos->x *= scale;
            pos->z *= scale;
        }
        if (!(mf & 4) || (mf & 2)) {
            pos->y *= scale;
        }
    }
    if (dList != NULL) {
        const char* name = NULL;
        if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
            name = gPuppetEquipDl[0];
        } else if (limbIndex == PLAYER_LIMB_RIGHT_HAND) {
            name = gPuppetEquipDl[1];
        } else if (limbIndex == PLAYER_LIMB_SHEATH) {
            name = gPuppetEquipDl[2];
        } else if (limbIndex == PLAYER_LIMB_WAIST) {
            name = gPuppetEquipDl[3];
        }
        if (name != NULL && name[0] != '\0') {
            *dList = (name[0] == '-') ? NULL : PuppetLoadGfx(name);
        }
        const s32 which = limbIndex == PLAYER_LIMB_LEFT_HAND    ? gPuppetCustomHand[0]
                          : limbIndex == PLAYER_LIMB_RIGHT_HAND ? gPuppetCustomHand[1]
                                                                : OOTMM_CUSTOM_HAND_NONE;
        Gfx* custom = NULL;
        switch (which) {
            case OOTMM_CUSTOM_HAND_HAMMER:
                custom = OotmmEquipment_LeftHandHammerDList();
                break;
            case OOTMM_CUSTOM_HAND_BOOMERANG:
                custom = OotmmEquipment_LeftHandBoomerangDList();
                break;
            case OOTMM_CUSTOM_HAND_SLINGSHOT:
                custom = OotmmEquipment_RightHandSlingshotDList();
                break;
            case OOTMM_CUSTOM_HAND_DEKU_SHIELD:
                custom = OotmmEquipment_DekuShieldHandDList(0);
                break;
        }
        if (custom != NULL) {
            *dList = custom;
        }
    }
    return false;
}

// A transformed sender keeps currentMask set to its own transformation mask, which the real draw
// skips because the face already is that mask; only other masks are held up.
extern "C" void OotmmPuppet_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList1, Gfx** dList2, Vec3s* rot,
                                         Actor* actor) {
    (void)dList2;
    (void)rot;
    (void)actor;
    // The shield on the back is pushed here rather than through *dList, so it needs its own
    // replay alongside the four synced equipment limbs.
    if (limbIndex == PLAYER_LIMB_SHEATH && dList1 != NULL && *dList1 != NULL &&
        gPuppetForm == PLAYER_FORM_HUMAN && gPuppetShield != PLAYER_SHIELD_NONE &&
        (gPuppetSheathType == PLAYER_MODELTYPE_SHEATH_14 || gPuppetSheathType == PLAYER_MODELTYPE_SHEATH_15)) {
        Gfx* shield = gPuppetDekuShield ? OotmmEquipment_DekuShieldBackDList() : NULL;
        OPEN_DISPS(play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++,
                       shield != NULL ? shield : gPlayerShields[2 * ((gPuppetShield - 1) ^ 0)]);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    // Cross-game masks come from their own resource path, not MM's mask table.
    if (limbIndex == PLAYER_LIMB_HEAD && gPuppetCustomMask != 0) {
        const char* path = OotmmCustomItems_MaskDList(gPuppetCustomMask);
        if (path != NULL) {
            OPEN_DISPS(play->state.gfxCtx);
            MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, ResourceMgr_LoadGfxByName(path));
            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
    if (limbIndex != PLAYER_LIMB_HEAD || gPuppetMask < PLAYER_MASK_TRUTH || gPuppetMask > PLAYER_MASK_DEKU ||
        dList1 == NULL || *dList1 == NULL) {
        return;
    }
    if (gPuppetForm != PLAYER_FORM_HUMAN &&
        (gPuppetMask < PLAYER_MASK_FIERCE_DEITY || gPuppetMask == gPuppetForm + PLAYER_MASK_FIERCE_DEITY)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)D_801C0B20[gPuppetMask - 1]);
    CLOSE_DISPS(play->state.gfxCtx);
}

namespace {

// Face symbol families per human-form model, mirroring the mapper tables; the human table
// rows cannot be read live because the local mappers overwrite them.
const char* const kPuppetHumanEyes[PLAYER_EYES_MAX] = {
    "gLinkHumanEyesOpenTex", "gLinkHumanEyesHalfTex", "gLinkHumanEyesClosedTex",
    "gLinkHumanEyesRightTex", "gLinkHumanEyesLeftTex", "gLinkHumanEyesUpTex",
    "gLinkHumanEyesDownTex", "gLinkHumanEyesWincingTex",
};
const char* const kPuppetHumanMouths[PLAYER_MOUTH_MAX] = {
    "gLinkHumanMouthClosedTex", "gLinkHumanMouthHalfTex", "gLinkHumanMouthOpenTex",
    "gLinkHumanMouthSmileTex",
};
const char* const kPuppetChildEyes[PLAYER_EYES_MAX] = {
    "gLinkChildEyesOpenTex", "gLinkChildEyesHalfTex", "gLinkChildEyesClosedfTex",
    "gLinkChildEyesRollRightTex", "gLinkChildEyesRollLeftTex", "gLinkChildEyesUnk1Tex",
    "gLinkChildEyesUnk2Tex", "gLinkChildEyesShockTex",
};
const char* const kPuppetChildMouths[PLAYER_MOUTH_MAX] = {
    "gLinkChildMouth1Tex", "gLinkChildMouth2Tex", "gLinkChildMouth3Tex", "gLinkChildMouth4Tex",
};
const char* const kPuppetAdultEyes[PLAYER_EYES_MAX] = {
    "gLinkAdultEyesOpenTex", "gLinkAdultEyesHalfTex", "gLinkAdultEyesClosedfTex",
    "gLinkAdultEyesRollRightTex", "gLinkAdultEyesRollLeftTex", "gLinkAdultEyesUnk1Tex",
    "gLinkAdultEyesUnk2Tex", "gLinkAdultEyesShockTex",
};
const char* const kPuppetAdultMouths[PLAYER_MOUTH_MAX] = {
    "gLinkAdultMouth1Tex", "gLinkAdultMouth2Tex", "gLinkAdultMouth3Tex", "gLinkAdultMouth4Tex",
};

void* PuppetFaceTexture(char* buf, size_t cap, bool eyes, int32_t playerForm, int32_t index,
                        void* fallback) {
    if (gPuppetForm < 0 || gPuppetPlayerId == 0 || index < 0 ||
        index >= (eyes ? PLAYER_EYES_MAX : PLAYER_MOUTH_MAX)) {
        return fallback;
    }
    char canonical[96];
    if (playerForm == PLAYER_FORM_HUMAN) {
        const char* dir = gPuppetHumanModel == 2   ? "ot_obj_link_boy"
                          : gPuppetHumanModel == 1 ? "ot_obj_link_child"
                                                   : "object_link_child";
        const char* name =
            gPuppetHumanModel == 2   ? (eyes ? kPuppetAdultEyes[index] : kPuppetAdultMouths[index])
            : gPuppetHumanModel == 1 ? (eyes ? kPuppetChildEyes[index] : kPuppetChildMouths[index])
                                     : (eyes ? kPuppetHumanEyes[index] : kPuppetHumanMouths[index]);
        std::snprintf(canonical, sizeof(canonical), "objects/%s/%s", dir, name);
    } else {
        // Non-human rows never get remapped locally, so the live entry names the canonical
        // texture; a null row (a form without that texture) stays as-is.
        const char* live = static_cast<const char*>(fallback);
        if (live == nullptr || std::strncmp(live, "__OTR__", 7) != 0) {
            return fallback;
        }
        std::snprintf(canonical, sizeof(canonical), "%s", live + 7);
    }

    if (PuppetNamespaced(buf, cap, static_cast<uint16_t>(gPuppetPlayerId), canonical) != nullptr) {
        return buf;
    }
    if (playerForm == PLAYER_FORM_HUMAN && gPuppetHumanModel != 0) {
        std::snprintf(buf, cap, "__OTR__%s", canonical);
        if (ResourceMgr_FileExists(buf + 7)) {
            return buf;
        }
    }
    return fallback;
}

} // namespace

extern "C" void OotmmPuppet_SetTunicColor(PlayState* play) {
    // Ported cloth reads the tunic from ENV color; emit the SENDER's synced tunic, not ours.
    if (gPuppetForm != PLAYER_FORM_HUMAN || gPuppetHumanModel == 0) {
        return;
    }
    uint8_t r;
    uint8_t g;
    uint8_t b;
    OotmmEquipment_TunicColorOf(gPuppetTunic, &r, &g, &b);
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)OotmmAdultLink_CullSegmentDL());
    gDPSetEnvColor(POLY_OPA_DISP++, r, g, b, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void* OotmmPuppet_EyeTexture(int32_t playerForm, int32_t eyeIndex) {
    void* fallback = playerForm >= 0 && playerForm < PLAYER_FORM_MAX && eyeIndex >= 0 &&
                             eyeIndex < PLAYER_EYES_MAX
                         ? sPlayerEyesTextures[playerForm][eyeIndex]
                         : NULL;
    static char sPath[128];
    return PuppetFaceTexture(sPath, sizeof(sPath), true, playerForm, eyeIndex, fallback);
}

extern "C" void* OotmmPuppet_MouthTexture(int32_t playerForm, int32_t mouthIndex) {
    void* fallback = playerForm >= 0 && playerForm < PLAYER_FORM_MAX && mouthIndex >= 0 &&
                             mouthIndex < PLAYER_MOUTH_MAX
                         ? sPlayerMouthTextures[playerForm][mouthIndex]
                         : NULL;
    static char sPath[128];
    return PuppetFaceTexture(sPath, sizeof(sPath), false, playerForm, mouthIndex, fallback);
}

namespace {

void PuppetActionFunc(Actor* actor, PlayState* play) {
    PuppetState* state = gStateByActor.count(actor) ? gStateByActor[actor] : nullptr;
    if (state == nullptr || !state->hasPose) {
        return;
    }
    const float dx = state->targetPos.x - actor->world.pos.x;
    const float dy = state->targetPos.y - actor->world.pos.y;
    const float dz = state->targetPos.z - actor->world.pos.z;
    if (dx * dx + dy * dy + dz * dz > 90000.0f) {
        actor->world.pos = state->targetPos;
    } else {
        actor->world.pos.x += dx * 0.4f;
        actor->world.pos.y += dy * 0.4f;
        actor->world.pos.z += dz * 0.4f;
    }
    Math_ScaledStepToS(&actor->shape.rot.y, state->targetYaw, 0x1800);
    actor->world.rot.y = actor->shape.rot.y;
    actor->shape.yOffset = 0.0f; // EnItem00 logic re-applies the display hover; keep grounded
    std::memcpy(state->skelAnime.jointTable, state->netJoints, sizeof(state->netJoints));

    // The hit is attacker-authoritative: the victim's game applies it when the relay delivers it.
    if (gPvpEnabled && state->colliderReady) {
        if (state->pvpCooldown > 0) {
            state->pvpCooldown--;
        }
        if ((state->collider.base.acFlags & AC_HIT) && state->pvpCooldown == 0) {
            state->pvpCooldown = 8; // one hit per swing, not one per contact frame
            int damage = 4;
            if (state->collider.elem.acHitElem != NULL && state->collider.elem.acHitElem->atDmgInfo.damage > 0) {
                damage = state->collider.elem.acHitElem->atDmgInfo.damage;
            }
            Player* attacker = GET_PLAYER(play);
            const s16 yaw = attacker != NULL ? Math_Vec3f_Yaw(&attacker->actor.world.pos, &actor->world.pos)
                                             : actor->shape.rot.y;
            gPendingPvpHits.push_back(PendingPvpHit{ state->playerId, damage, yaw });
        }
        state->collider.base.acFlags &= ~AC_HIT;
        Collider_UpdateCylinder(actor, &state->collider);
        CollisionCheck_SetAC(play, &play->colChkCtx, &state->collider.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &state->collider.base);
    }
}

void PuppetDrawFunc(Actor* actor, PlayState* play) {
    PuppetState* state = gStateByActor.count(actor) ? gStateByActor[actor] : nullptr;
    if (state == nullptr || !state->hasPose || !state->skeletonReady) {
        return;
    }
    // Rebuilt from scratch because the EnItem00 shell's matrix carries an item hover, and yOffset
    // is deliberately not replayed: MM's player draw compensates most of it back out.
    Matrix_Translate(actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, MTXMODE_NEW);
    Matrix_RotateYS(actor->shape.rot.y, MTXMODE_APPLY);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    gPuppetForm = state->form;
    gPuppetPlayerId = state->playerId;
    gPuppetHumanModel = state->humanModel;
    gPuppetTunic = state->tunic;
    gPuppetMoveFlags = state->moveFlags;
    gPuppetEquipDl[0] = state->dlLeftHand.c_str();
    gPuppetEquipDl[1] = state->dlRightHand.c_str();
    gPuppetEquipDl[2] = state->dlSheath.c_str();
    gPuppetEquipDl[3] = state->dlWaist.c_str();
    gPuppetMask = state->mask;
    gPuppetItemAction = state->itemAction;
    gPuppetCustomMask = state->customMask;
    gPuppetShield = state->shield;
    gPuppetSheathType = state->sheathType;
    gPuppetRightHandType = state->rightHandType;
    gPuppetDekuShield = state->dekuShield;
    gPuppetCustomHand[0] = state->customHand[0];
    gPuppetCustomHand[1] = state->customHand[1];
    Player_DrawImpl(play, state->skelAnime.skeleton, state->skelAnime.jointTable, state->skelAnime.dListCount, 0,
                    (PlayerTransformation)state->form, state->boots, 0, OotmmPuppet_OverrideLimbDraw,
                    OotmmPuppet_PostLimbDraw, actor);
    gPuppetEquipDl[0] = gPuppetEquipDl[1] = gPuppetEquipDl[2] = gPuppetEquipDl[3] = NULL;
    gPuppetMask = 0;
    gPuppetCustomMask = 0;
    gPuppetItemAction = -1;
    gPuppetCustomHand[0] = gPuppetCustomHand[1] = 0;
    if (state->form == PLAYER_FORM_HUMAN) {
        OotmmEquipment_DrawBootsOf(play, state->customBoots);
    }
    gPuppetForm = -1;
    gPuppetPlayerId = 0;
    gPuppetHumanModel = 0;
    gPuppetTunic = 0;
}

void ApplyPoseToPuppet(PuppetSlot& slot, PlayState* play, const Ship::OotmmPlayerPose& pose) {
    PuppetState* state = slot.state.get();
    const u8 form = static_cast<u8>(pose.Form);
    const u8 humanModel = static_cast<u8>(pose.HumanModel & 0xFF);
    state->playerId = pose.PlayerId;
    if (!state->skeletonReady || form != state->form || humanModel != state->humanModel) {
        state->humanModel = humanModel;
        PuppetInitSkeleton(state, play, form);
    }
    const size_t count = std::min(pose.JointTable.size() / 3, static_cast<size_t>(kLimbBuf));
    for (size_t i = 0; i < count; ++i) {
        state->netJoints[i].x = pose.JointTable[i * 3 + 0];
        state->netJoints[i].y = pose.JointTable[i * 3 + 1];
        state->netJoints[i].z = pose.JointTable[i * 3 + 2];
    }
    state->targetPos.x = pose.Pos[0];
    state->targetPos.y = pose.Pos[1];
    state->targetPos.z = pose.Pos[2];
    state->targetYaw = pose.Yaw;
    state->moveFlags = static_cast<u8>(pose.MoveFlags & 0xFF);
    state->dlLeftHand = pose.DlLeftHand;
    state->dlRightHand = pose.DlRightHand;
    state->dlSheath = pose.DlSheath;
    state->dlWaist = pose.DlWaist;
    state->boots = static_cast<u8>(pose.Boots & 0xFF);
    state->mask = static_cast<u8>(pose.Mask & 0xFF);
    state->itemAction = pose.ItemAction;
    state->customMask = pose.CustomMask;
    state->shield = static_cast<u8>(pose.Shield & 0xFF);
    state->sheathType = static_cast<u8>(pose.SheathType & 0xFF);
    state->rightHandType = static_cast<u8>(pose.RightHandType & 0xFF);
    state->dekuShield = pose.DekuShield;
    state->customHand[0] = pose.CustomLeftHand;
    state->customHand[1] = pose.CustomRightHand;
    state->customBoots = pose.CustomBoots;
    state->tunic = pose.Tunic;
    if (!state->hasPose && slot.actor != nullptr) {
        slot.actor->world.pos = state->targetPos;
        slot.actor->shape.rot.y = state->targetYaw;
    }
    state->hasPose = true;
}

void DropPuppet(PuppetSlot& slot) {
    if (slot.actor != nullptr) {
        if (slot.state != nullptr && slot.state->colliderReady && gPlayState != nullptr) {
            Collider_DestroyCylinder(gPlayState, &slot.state->collider);
            slot.state->colliderReady = false;
        }
        gStateByActor.erase(slot.actor);
        Actor_Kill(slot.actor);
        slot.actor = nullptr;
    }
}

void PresenceTick() {
    if (gPlayState == nullptr || !OotmmSession_IsActive()) {
        return;
    }
    // Solo sessions never get a roster, so presence costs them nothing.
    if (!OotmmIpc_PresenceActive()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    const uint16_t localPlayer = static_cast<uint16_t>(OotmmSession_GetState().GetPlayerId());

    Ship::OotmmPlayerPose pose;
    pose.PlayerId = localPlayer;
    pose.Game = "mm";
    pose.SceneId = gPlayState->sceneId;
    pose.RoomId = gPlayState->roomCtx.curRoom.num;
    pose.Pos[0] = player->actor.world.pos.x;
    pose.Pos[1] = player->actor.world.pos.y;
    pose.Pos[2] = player->actor.world.pos.z;
    pose.Yaw = player->actor.shape.rot.y;
    pose.YOffset = player->actor.shape.yOffset;
    pose.MoveFlags = player->skelAnime.movementFlags;
    pose.Form = player->transformation;
    pose.HumanModel = OotmmAdultLink_IsAdult() ? 2 : (OotmmChildLink_Active() ? 1 : 0);
    pose.Seq = ++gLocalSeq;
    const auto canonical = [](const char* name) -> std::string {
        if (name == nullptr) {
            return {};
        }
        if (name[0] == '-') {
            return "-";
        }
        if (std::strncmp(name, "__OTR__", 7) == 0) {
            name += 7;
        }
        return name;
    };
    pose.DlLeftHand = canonical(gOotmmEquipDlCapture[0]);
    pose.DlRightHand = canonical(gOotmmEquipDlCapture[1]);
    pose.DlSheath = canonical(gOotmmEquipDlCapture[2]);
    pose.DlWaist = canonical(gOotmmEquipDlCapture[3]);
    pose.Boots = player->currentBoots;
    pose.Mask = player->currentMask;
    pose.ItemAction = player->itemAction;
    pose.CustomMask = OotmmCustomItems_EquippedMask();
    pose.Shield = player->currentShield;
    pose.SheathType = player->sheathType;
    pose.RightHandType = player->rightHandType;
    pose.DekuShield = OotmmCustomItems_WearingDekuShield() && !OotmmAdultLink_IsAdult();
    pose.CustomLeftHand = sCustomHandCapture[0];
    pose.CustomRightHand = sCustomHandCapture[1];
    pose.CustomBoots = OotmmCustomItems_EquippedBoots();
    pose.Tunic = OotmmCustomItems_EquippedTunic();
    if (player->skelAnime.jointTable != nullptr) {
        pose.JointTable.resize(kLimbBuf * 3);
        for (int i = 0; i < kLimbBuf && i < PLAYER_LIMB_MAX; ++i) {
            pose.JointTable[i * 3 + 0] = player->skelAnime.jointTable[i].x;
            pose.JointTable[i * 3 + 1] = player->skelAnime.jointTable[i].y;
            pose.JointTable[i * 3 + 2] = player->skelAnime.jointTable[i].z;
        }
    }
    OotmmIpc_SendPlayerPose(pose);

    for (auto& [id, slot] : gPuppets) {
        slot.staleTicks++;
    }
    std::vector<Ship::OotmmPlayerPose> roster;
    std::vector<Ship::OotmmPvpHit> pvpHits;
    const bool freshRoster = OotmmIpc_TakeRemotePresence(roster, pvpHits);
    gPvpEnabled = gOotmmPvpEnabled != 0;

    if (!gPendingPvpHits.empty()) {
        static uint32_t pvpHitSeq = 0;
        for (const PendingPvpHit& pending : gPendingPvpHits) {
            Ship::OotmmPvpHit hit;
            hit.SourcePlayer = localPlayer;
            hit.TargetPlayer = pending.targetPlayer;
            hit.Damage = pending.damage;
            hit.Yaw = pending.yaw;
            hit.Seq = ++pvpHitSeq;
            OotmmIpc_SendPvpHit(hit);
        }
        gPendingPvpHits.clear();
    }

    {
        for (const Ship::OotmmPvpHit& hit : pvpHits) {
            if (hit.TargetPlayer != localPlayer || !gPvpEnabled) {
                continue;
            }
            Player* self = GET_PLAYER(gPlayState);
            if (self != nullptr && self->invincibilityTimer == 0 && !(self->stateFlags1 & PLAYER_STATE1_DEAD)) {
                Actor* source = &self->actor;
                if (const auto it = gPuppets.find(hit.SourcePlayer); it != gPuppets.end() && it->second.actor != nullptr) {
                    source = it->second.actor;
                }
                func_800B8D50(gPlayState, source, 6.0f, hit.Yaw, 5.0f, static_cast<u32>(std::max(1, hit.Damage)));
            }
        }
    }

    // Without a fresh roster the puppets simply keep their last pose; only a roster retires them.
    if (!freshRoster) {
        return;
    }

    for (const Ship::OotmmPlayerPose& remote : roster) {
        if (remote.PlayerId == localPlayer || remote.Game != "mm" || remote.SceneId != gPlayState->sceneId) {
            continue;
        }
        PuppetSlot& slot = gPuppets[remote.PlayerId];
        if (slot.state == nullptr) {
            slot.state = std::make_unique<PuppetState>();
        }
        if (slot.actor != nullptr && remote.Seq != 0 && remote.Seq == slot.lastSeq) {
            slot.staleTicks--; // unchanged sample; keep alive but don't reset freshness fully
        }
        if (slot.actor == nullptr) {
            EnItem00* shell = CustomItem::Spawn(remote.Pos[0], remote.Pos[1], remote.Pos[2], remote.Yaw, 0, 0,
                                                (ActorFunc)PuppetActionFunc, (ActorFunc)PuppetDrawFunc);
            if (shell != nullptr) {
                slot.actor = &shell->actor;
                // Take the update over completely: the item logic spins the actor, re-applies the
                // display hover, and with no pickup flags never calls the actionFunc.
                slot.actor->update = (ActorFunc)PuppetActionFunc;
                slot.actor->shape.yOffset = 0.0f;
                gStateByActor[slot.actor] = slot.state.get();
                Collider_InitCylinder(gPlayState, &slot.state->collider);
                Collider_SetCylinder(gPlayState, &slot.state->collider, slot.actor, &sPuppetCylinderInit);
                slot.state->colliderReady = true;
                slot.state->pvpCooldown = 0;
            }
        }
        if (slot.actor != nullptr) {
            ApplyPoseToPuppet(slot, gPlayState, remote);
            slot.staleTicks = 0;
            slot.lastSeq = remote.Seq;
            const std::string wanted = gOotmmShowNames != 0
                                           ? (!remote.ClientName.empty() ? remote.ClientName
                                                                         : "Player " + std::to_string(remote.PlayerId))
                                           : "";
            if (wanted != slot.taggedName) {
                NameTag_RemoveAllForActor(slot.actor);
                if (!wanted.empty()) {
                    // Above the head; world.pos is at the feet.
                    NameTagOptions options = {};
                    options.tag = "ootmm_coop";
                    options.yOffset = 44;
                    NameTag_RegisterForActorWithOptions(slot.actor, wanted.c_str(), options);
                }
                slot.taggedName = wanted;
            }
        }
    }
    for (auto it = gPuppets.begin(); it != gPuppets.end();) {
        if (it->second.staleTicks > 60) {
            DropPuppet(it->second);
            it = gPuppets.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace

extern "C" void OotmmPresence_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() { PresenceTick(); });
    // Scene loads destroy every actor; drop the now-dangling puppet pointers with them.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s16, s8) {
        gStateByActor.clear();
        gPuppets.clear();
    });
}
