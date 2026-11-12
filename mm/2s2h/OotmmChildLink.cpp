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

#include "OotmmHandComposites.h"

namespace {

#define CHILD_SYM(name) \
    alignas(4) constexpr char name[] = "__OTR__objects/ot_obj_link_child/" #name
CHILD_SYM(gLinkChildSkel);
CHILD_SYM(gLinkChildLeftHandNearDL);
CHILD_SYM(gLinkChildLeftFistNearDL);
CHILD_SYM(gLinkChildLeftHandUpNearDL);
CHILD_SYM(gLinkChildRightHandNearDL);
CHILD_SYM(gLinkChildRightHandClosedNearDL);
CHILD_SYM(gLinkChildRightHandHoldingFairyOcarinaNearDL);
CHILD_SYM(gLinkChildRightShoulderNearDL);
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

const DlPatch kDlPatches[] = {
    { &gPlayerSheath12DLs[kHumanNear], sChildEmptyDL },
    { &gPlayerSheath12DLs[kHumanFar], sChildEmptyDL },
    { &gPlayerSheath13DLs[kHumanNear], sChildEmptyDL },
    { &gPlayerSheath13DLs[kHumanFar], sChildEmptyDL },
    { &gPlayerSheath14DLs[kHumanNear], sChildEmptyDL },
    { &gPlayerSheath14DLs[kHumanFar], sChildEmptyDL },
    { &gPlayerLeftHandOpenDLs[kHumanNear], CHILD_DL(gLinkChildLeftHandNearDL) },
    { &gPlayerLeftHandOpenDLs[kHumanFar], CHILD_DL(gLinkChildLeftHandNearDL) },
    { &gPlayerLeftHandClosedDLs[kHumanNear], CHILD_DL(gLinkChildLeftFistNearDL) },
    { &gPlayerLeftHandClosedDLs[kHumanFar], CHILD_DL(gLinkChildLeftFistNearDL) },
    { &gPlayerRightHandOpenDLs[kHumanNear], CHILD_DL(gLinkChildRightHandNearDL) },
    { &gPlayerRightHandOpenDLs[kHumanFar], CHILD_DL(gLinkChildRightHandNearDL) },
    { &gPlayerRightHandClosedDLs[kHumanNear], CHILD_DL(gLinkChildRightHandClosedNearDL) },
    { &gPlayerRightHandClosedDLs[kHumanFar], CHILD_DL(gLinkChildRightHandClosedNearDL) },
    { &gPlayerRightHandInstrumentDLs[kHumanNear], CHILD_DL(gLinkChildRightHandHoldingFairyOcarinaNearDL) },
    { &gPlayerRightHandInstrumentDLs[kHumanFar], CHILD_DL(gLinkChildRightHandHoldingFairyOcarinaNearDL) },
    { &gPlayerRightHandHookshotDLs[kHumanNear], CHILD_DL(gLinkChildRightHandClosedNearDL) },
    { &gPlayerRightHandHookshotDLs[kHumanFar], CHILD_DL(gLinkChildRightHandClosedNearDL) },
    { &gPlayerLeftHandBottleDLs[kHumanNear], CHILD_DL(gLinkChildLeftHandUpNearDL) },
    { &gPlayerLeftHandBottleDLs[kHumanFar], CHILD_DL(gLinkChildLeftHandUpNearDL) },
    { &sPlayerFirstPersonLeftForearmDLs[kHuman], sChildEmptyDL },
    { &sPlayerFirstPersonLeftHandDLs[kHuman], CHILD_DL(gLinkChildLeftFistNearDL) },
    { &sPlayerFirstPersonRightShoulderDLs[kHuman], CHILD_DL(gLinkChildRightShoulderNearDL) },
    { &sPlayerFirstPersonRightHandHookshotDLs[kHuman], CHILD_DL(gLinkChildRightHandClosedNearDL) },
};

struct HoldPatch {
    Gfx** Slot;
    const char* Native;
    const char* Fist;
};

const HoldPatch kHoldPatches[] = {
    { &gPlayerHandHoldingShields[0], OOTMM_HOLD_HEROS_SHIELD, gLinkChildRightHandClosedNearDL },
    { &gPlayerHandHoldingShields[1], OOTMM_HOLD_HEROS_SHIELD, gLinkChildRightHandClosedNearDL },
    { &gPlayerHandHoldingShields[2], OOTMM_HOLD_MIRROR_SHIELD, gLinkChildRightHandClosedNearDL },
    { &gPlayerHandHoldingShields[3], OOTMM_HOLD_MIRROR_SHIELD, gLinkChildRightHandClosedNearDL },
    { &gPlayerRightHandBowDLs[kHumanNear], OOTMM_HOLD_BOW, gLinkChildRightHandClosedNearDL },
    { &gPlayerRightHandBowDLs[kHumanFar], OOTMM_HOLD_BOW, gLinkChildRightHandClosedNearDL },
    { &gPlayerLeftHandTwoHandSwordDLs[kHumanNear], OOTMM_HOLD_GREAT_FAIRYS_SWORD, gLinkChildLeftFistNearDL },
    { &gPlayerLeftHandTwoHandSwordDLs[kHumanFar], OOTMM_HOLD_GREAT_FAIRYS_SWORD, gLinkChildLeftFistNearDL },
    { &gPlayerLeftHandOneHandSwordDLs[kHumanNear], OOTMM_HOLD_KOKIRI_SWORD, gLinkChildLeftFistNearDL },
    { &gPlayerLeftHandOneHandSwordDLs[kHumanFar], OOTMM_HOLD_KOKIRI_SWORD, gLinkChildLeftFistNearDL },
    { &D_801C018C[0], OOTMM_HOLD_KOKIRI_SWORD, gLinkChildLeftFistNearDL },
    { &D_801C018C[1], OOTMM_HOLD_KOKIRI_SWORD, gLinkChildLeftFistNearDL },
    { &D_801C018C[2], OOTMM_HOLD_RAZOR_SWORD, gLinkChildLeftFistNearDL },
    { &D_801C018C[3], OOTMM_HOLD_RAZOR_SWORD, gLinkChildLeftFistNearDL },
    { &D_801C018C[4], OOTMM_HOLD_GILDED_SWORD, gLinkChildLeftFistNearDL },
    { &D_801C018C[5], OOTMM_HOLD_GILDED_SWORD, gLinkChildLeftFistNearDL },
};

constexpr size_t kHoldCount = sizeof(kHoldPatches) / sizeof(kHoldPatches[0]);
Gfx* sHoldBackups[kHoldCount];

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

    auto childSkel = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        (const char*)gLinkChildSkel + 7);
    if (childSkel == nullptr) {
        SPDLOG_WARN("[OoTMM] OoT child skeleton failed to load; keeping the MM human model");
        return;
    }
    sCustomModel = childSkel->GetInitData()->IsCustom;
    sApplied = true;

    static bool sBackedUp = false;
    const bool firstApply = !sBackedUp;
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

    gPlayerSkeletons[kHuman] = (FlexSkeletonHeader*)gLinkChildSkel;

    for (const DlPatch& p : kDlPatches) {
        *p.Slot = p.Value;
    }

    for (size_t i = 0; i < kHoldCount; i++) {
        if (firstApply) {
            sHoldBackups[i] = *kHoldPatches[i].Slot;
        }
        Gfx* built = OotmmHandComposites_Build(kHoldPatches[i].Native, kHoldPatches[i].Fist);
        *kHoldPatches[i].Slot = built != nullptr ? built : sHoldBackups[i];
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
    for (size_t i = 0; i < kHoldCount; i++) {
        *kHoldPatches[i].Slot = sHoldBackups[i];
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
