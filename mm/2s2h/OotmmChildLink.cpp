#include "OotmmChildLink.h"

#include "OotmmSession.h"

#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <cstring>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"

extern Gfx* gPlayerHandHoldingShields[2 * (PLAYER_SHIELD_MAX - 1)];
extern Gfx* gPlayerSheath12DLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerSheath13DLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerSheath14DLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerShields[];
extern Gfx* gPlayerSheathedSwords[];
extern Gfx* gPlayerSwordSheaths[];
extern Gfx* gPlayerLeftHandTwoHandSwordDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandOpenDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandClosedDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandOneHandSwordDLs[2 * PLAYER_FORM_MAX];
extern Gfx* D_801C018C[];
extern Gfx* gPlayerRightHandOpenDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandClosedDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandBowDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandInstrumentDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerRightHandHookshotDLs[2 * PLAYER_FORM_MAX];
extern Gfx* gPlayerLeftHandBottleDLs[2 * PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonLeftForearmDLs[PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonLeftHandDLs[PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonRightShoulderDLs[PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonRightHandDLs[PLAYER_FORM_MAX];
extern Gfx* sPlayerFirstPersonRightHandHookshotDLs[PLAYER_FORM_MAX];
extern TexturePtr sPlayerEyesTextures[PLAYER_FORM_MAX][PLAYER_EYES_MAX];
extern TexturePtr sPlayerMouthTextures[PLAYER_FORM_MAX][PLAYER_MOUTH_MAX];

uint8_t ResourceMgr_FileExists(const char* resName);
}

namespace {

#define CHILD_SYM(name) \
    alignas(4) constexpr char name[] = "__OTR__objects/ot_obj_link_child/" #name
CHILD_SYM(gLinkChildSkel);
CHILD_SYM(gLinkChildRightFistAndDekuShieldNearDL);
CHILD_SYM(gLinkChildHylianShieldAndSheathNearDL);
CHILD_SYM(gLinkChildSwordAndSheathNearDL);
CHILD_SYM(gLinkChildSheathNearDL);
CHILD_SYM(gLinkChildLeftHandNearDL);
CHILD_SYM(gLinkChildLeftFistNearDL);
CHILD_SYM(gLinkChildLeftFistAndKokiriSwordNearDL);
CHILD_SYM(gLinkChildLeftHandUpNearDL);
CHILD_SYM(gLinkChildRightHandNearDL);
CHILD_SYM(gLinkChildRightHandClosedNearDL);
CHILD_SYM(gLinkChildRightHandHoldingSlingshotNearDL);
CHILD_SYM(gLinkChildRightHandHoldingFairyOcarinaNearDL);
CHILD_SYM(gLinkChildRightShoulderNearDL);
CHILD_SYM(gLinkChildRightArmStretchedSlingshotDL);
CHILD_SYM(gLinkChildEyesOpenTex);
CHILD_SYM(gLinkChildEyesHalfTex);
CHILD_SYM(gLinkChildEyesClosedfTex);
CHILD_SYM(gLinkChildEyesRollRightTex);
CHILD_SYM(gLinkChildEyesRollLeftTex);
CHILD_SYM(gLinkChildEyesUnk1Tex);
CHILD_SYM(gLinkChildEyesUnk2Tex);
CHILD_SYM(gLinkChildEyesShockTex);
CHILD_SYM(gLinkChildMouth1Tex);
CHILD_SYM(gLinkChildMouth2Tex);
CHILD_SYM(gLinkChildMouth3Tex);
CHILD_SYM(gLinkChildMouth4Tex);
#undef CHILD_SYM

#define CHILD_DL(name) ((Gfx*)(name))

// The launcher stamps this into the generated archive; its presence is the opt-in.
constexpr char kMarkerPath[] = "objects/ot_obj_link_child/ootmm_localmodel_marker";

alignas(8) Gfx sChildEmptyDL[] = {
    gsSPEndDisplayList(),
};

constexpr int kHuman = PLAYER_FORM_HUMAN;
constexpr int kHumanNear = 2 * PLAYER_FORM_HUMAN;
constexpr int kHumanFar = 2 * PLAYER_FORM_HUMAN + 1;

bool sApplied = false;
bool sCustomModel = false;

Gfx* sBackupDLs[64];
int sBackupCount = 0;
FlexSkeletonHeader* sBackupSkel;
TexturePtr sBackupEyes[PLAYER_EYES_MAX];
TexturePtr sBackupMouth[PLAYER_MOUTH_MAX];

struct DlPatch {
    Gfx** Slot;
    Gfx* Value;
};

// OoT child has no in-hand shield, hookshot or two-hander (fists stand in), and no waist DL at
// all: that geometry lives in the skeleton limb, which the z_player_lib carve-out keeps.
const DlPatch kDlPatches[] = {
    { &gPlayerHandHoldingShields[0], CHILD_DL(gLinkChildRightFistAndDekuShieldNearDL) },
    { &gPlayerHandHoldingShields[1], CHILD_DL(gLinkChildRightFistAndDekuShieldNearDL) },
    { &gPlayerHandHoldingShields[2], CHILD_DL(gLinkChildRightFistAndDekuShieldNearDL) },
    { &gPlayerHandHoldingShields[3], CHILD_DL(gLinkChildRightFistAndDekuShieldNearDL) },
    { &gPlayerSheath12DLs[kHumanNear], sChildEmptyDL },
    { &gPlayerSheath12DLs[kHumanFar], sChildEmptyDL },
    { &gPlayerSheath13DLs[kHumanNear], sChildEmptyDL },
    { &gPlayerSheath13DLs[kHumanFar], sChildEmptyDL },
    { &gPlayerSheath14DLs[kHumanNear], sChildEmptyDL },
    { &gPlayerSheath14DLs[kHumanFar], sChildEmptyDL },
    { &gPlayerShields[0], CHILD_DL(gLinkChildHylianShieldAndSheathNearDL) },
    { &gPlayerShields[1], CHILD_DL(gLinkChildHylianShieldAndSheathNearDL) },
    { &gPlayerShields[2], CHILD_DL(gLinkChildHylianShieldAndSheathNearDL) },
    { &gPlayerShields[3], CHILD_DL(gLinkChildHylianShieldAndSheathNearDL) },
    { &gPlayerSheathedSwords[0], CHILD_DL(gLinkChildSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[1], CHILD_DL(gLinkChildSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[2], CHILD_DL(gLinkChildSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[3], CHILD_DL(gLinkChildSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[4], CHILD_DL(gLinkChildSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[5], CHILD_DL(gLinkChildSwordAndSheathNearDL) },
    { &gPlayerSwordSheaths[0], CHILD_DL(gLinkChildSheathNearDL) },
    { &gPlayerSwordSheaths[1], CHILD_DL(gLinkChildSheathNearDL) },
    { &gPlayerSwordSheaths[2], CHILD_DL(gLinkChildSheathNearDL) },
    { &gPlayerSwordSheaths[3], CHILD_DL(gLinkChildSheathNearDL) },
    { &gPlayerSwordSheaths[4], CHILD_DL(gLinkChildSheathNearDL) },
    { &gPlayerSwordSheaths[5], CHILD_DL(gLinkChildSheathNearDL) },
    { &gPlayerLeftHandTwoHandSwordDLs[kHumanNear], CHILD_DL(gLinkChildLeftFistNearDL) },
    { &gPlayerLeftHandTwoHandSwordDLs[kHumanFar], CHILD_DL(gLinkChildLeftFistNearDL) },
    { &gPlayerLeftHandOpenDLs[kHumanNear], CHILD_DL(gLinkChildLeftHandNearDL) },
    { &gPlayerLeftHandOpenDLs[kHumanFar], CHILD_DL(gLinkChildLeftHandNearDL) },
    { &gPlayerLeftHandClosedDLs[kHumanNear], CHILD_DL(gLinkChildLeftFistNearDL) },
    { &gPlayerLeftHandClosedDLs[kHumanFar], CHILD_DL(gLinkChildLeftFistNearDL) },
    { &gPlayerLeftHandOneHandSwordDLs[kHumanNear], CHILD_DL(gLinkChildLeftFistAndKokiriSwordNearDL) },
    { &gPlayerLeftHandOneHandSwordDLs[kHumanFar], CHILD_DL(gLinkChildLeftFistAndKokiriSwordNearDL) },
    { &D_801C018C[0], CHILD_DL(gLinkChildLeftFistAndKokiriSwordNearDL) },
    { &D_801C018C[1], CHILD_DL(gLinkChildLeftFistAndKokiriSwordNearDL) },
    { &D_801C018C[2], CHILD_DL(gLinkChildLeftFistAndKokiriSwordNearDL) },
    { &D_801C018C[3], CHILD_DL(gLinkChildLeftFistAndKokiriSwordNearDL) },
    { &D_801C018C[4], CHILD_DL(gLinkChildLeftFistAndKokiriSwordNearDL) },
    { &D_801C018C[5], CHILD_DL(gLinkChildLeftFistAndKokiriSwordNearDL) },
    { &gPlayerRightHandOpenDLs[kHumanNear], CHILD_DL(gLinkChildRightHandNearDL) },
    { &gPlayerRightHandOpenDLs[kHumanFar], CHILD_DL(gLinkChildRightHandNearDL) },
    { &gPlayerRightHandClosedDLs[kHumanNear], CHILD_DL(gLinkChildRightHandClosedNearDL) },
    { &gPlayerRightHandClosedDLs[kHumanFar], CHILD_DL(gLinkChildRightHandClosedNearDL) },
    { &gPlayerRightHandBowDLs[kHumanNear], CHILD_DL(gLinkChildRightHandHoldingSlingshotNearDL) },
    { &gPlayerRightHandBowDLs[kHumanFar], CHILD_DL(gLinkChildRightHandHoldingSlingshotNearDL) },
    { &gPlayerRightHandInstrumentDLs[kHumanNear], CHILD_DL(gLinkChildRightHandHoldingFairyOcarinaNearDL) },
    { &gPlayerRightHandInstrumentDLs[kHumanFar], CHILD_DL(gLinkChildRightHandHoldingFairyOcarinaNearDL) },
    { &gPlayerRightHandHookshotDLs[kHumanNear], CHILD_DL(gLinkChildRightHandClosedNearDL) },
    { &gPlayerRightHandHookshotDLs[kHumanFar], CHILD_DL(gLinkChildRightHandClosedNearDL) },
    // OoT child has no hand-holding-bottle mesh; the open palm carries the pose, bottle unseen.
    { &gPlayerLeftHandBottleDLs[kHumanNear], CHILD_DL(gLinkChildLeftHandUpNearDL) },
    { &gPlayerLeftHandBottleDLs[kHumanFar], CHILD_DL(gLinkChildLeftHandUpNearDL) },
    { &sPlayerFirstPersonLeftForearmDLs[kHuman], sChildEmptyDL },
    { &sPlayerFirstPersonLeftHandDLs[kHuman], CHILD_DL(gLinkChildLeftFistNearDL) },
    { &sPlayerFirstPersonRightShoulderDLs[kHuman], CHILD_DL(gLinkChildRightShoulderNearDL) },
    { &sPlayerFirstPersonRightHandDLs[kHuman], CHILD_DL(gLinkChildRightArmStretchedSlingshotDL) },
    { &sPlayerFirstPersonRightHandHookshotDLs[kHuman], CHILD_DL(gLinkChildRightHandClosedNearDL) },
};

const TexturePtr kChildEyes[PLAYER_EYES_MAX] = {
    (TexturePtr)gLinkChildEyesOpenTex,      (TexturePtr)gLinkChildEyesHalfTex,
    (TexturePtr)gLinkChildEyesClosedfTex,   (TexturePtr)gLinkChildEyesRollRightTex,
    (TexturePtr)gLinkChildEyesRollLeftTex,  (TexturePtr)gLinkChildEyesUnk1Tex,
    (TexturePtr)gLinkChildEyesUnk2Tex,      (TexturePtr)gLinkChildEyesShockTex,
};

const TexturePtr kChildMouth[PLAYER_MOUTH_MAX] = {
    (TexturePtr)gLinkChildMouth1Tex,
    (TexturePtr)gLinkChildMouth2Tex,
    (TexturePtr)gLinkChildMouth3Tex,
    (TexturePtr)gLinkChildMouth4Tex,
};

} // namespace

namespace OotmmChildLink {

bool Wanted() {
    return OotmmSession_IsActive() && ResourceMgr_FileExists(kMarkerPath);
}

void Apply() {
    if (sApplied) {
        return;
    }

    // Same guard as the adult mapper: an unloadable skeleton keeps vanilla, and staying
    // un-applied means Active/Restore never act on a swap that did not happen.
    auto childSkel = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        (const char*)gLinkChildSkel + 7);
    if (childSkel == nullptr) {
        SPDLOG_WARN("[OoTMM] OoT child skeleton failed to load; keeping the MM human model");
        return;
    }
    sCustomModel = childSkel->GetInitData()->IsCustom;
    sApplied = true;

    static bool sBackedUp = false;
    if (!sBackedUp) {
        sBackedUp = true;
        sBackupSkel = gPlayerSkeletons[kHuman];
        sBackupCount = 0;
        for (const DlPatch& p : kDlPatches) {
            sBackupDLs[sBackupCount++] = *p.Slot;
        }
        std::memcpy(sBackupEyes, sPlayerEyesTextures[kHuman], sizeof(sBackupEyes));
        std::memcpy(sBackupMouth, sPlayerMouthTextures[kHuman], sizeof(sBackupMouth));
    }

    // Same 21-limb rig as MM human, so MM's animations drive it directly.
    gPlayerSkeletons[kHuman] = (FlexSkeletonHeader*)gLinkChildSkel;

    for (const DlPatch& p : kDlPatches) {
        *p.Slot = p.Value;
    }

    std::memcpy(sPlayerEyesTextures[kHuman], kChildEyes, sizeof(kChildEyes));
    std::memcpy(sPlayerMouthTextures[kHuman], kChildMouth, sizeof(kChildMouth));

    SPDLOG_INFO("[OoTMM] OoT child model enabled for MM's human form");
}

void Restore() {
    if (!sApplied) {
        return;
    }
    sApplied = false;
    sCustomModel = false;

    gPlayerSkeletons[kHuman] = sBackupSkel;
    for (int i = 0; i < sBackupCount; i++) {
        *kDlPatches[i].Slot = sBackupDLs[i];
    }
    std::memcpy(sPlayerEyesTextures[kHuman], sBackupEyes, sizeof(sBackupEyes));
    std::memcpy(sPlayerMouthTextures[kHuman], sBackupMouth, sizeof(sBackupMouth));
}

} // namespace OotmmChildLink

extern "C" int32_t OotmmChildLink_Active(void) {
    return sApplied ? 1 : 0;
}

extern "C" int32_t OotmmChildLink_CustomModelActive(void) {
    return sApplied && sCustomModel ? 1 : 0;
}
