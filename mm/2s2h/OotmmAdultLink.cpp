#include "OotmmAdultLink.h"
#include "OotmmChildLink.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmCustomEquipment.h"
#include "OotmmSession.h"

#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <cstring>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"

extern Gfx* gPlayerWaistDLs[2 * PLAYER_FORM_MAX];
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
extern PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX];
extern PlayerAnimationHeader* D_8085BE84[PLAYER_ANIMGROUP_MAX][PLAYER_ANIMTYPE_MAX];
}

namespace {

// The interpreter, skeleton loader and segment resolver all accept path pointers as resources.
#define ADULT_SYM(name) \
    alignas(4) constexpr char name[] = "__OTR__objects/ot_obj_link_boy/" #name
ADULT_SYM(gLinkAdultSkel);
ADULT_SYM(gLinkAdultWaistNearDL);
ADULT_SYM(gLinkAdultRightHandHoldingHylianShieldNearDL);
ADULT_SYM(gLinkAdultRightHandHoldingMirrorShieldNearDL);
ADULT_SYM(gLinkAdultHylianShieldAndSheathNearDL);
ADULT_SYM(gLinkAdultMirrorShieldAndSheathNearDL);
ADULT_SYM(gLinkAdultMasterSwordAndSheathNearDL);
ADULT_SYM(gLinkAdultSheathNearDL);
ADULT_SYM(gLinkAdultLeftHandHoldingBgsNearDL);
ADULT_SYM(gLinkAdultLeftHandNearDL);
ADULT_SYM(gLinkAdultLeftHandClosedNearDL);
ADULT_SYM(gLinkAdultLeftHandHoldingMasterSwordNearDL);
ADULT_SYM(gLinkAdultRightHandNearDL);
ADULT_SYM(gLinkAdultRightHandClosedNearDL);
ADULT_SYM(gLinkAdultRightHandHoldingBowNearDL);
ADULT_SYM(gLinkAdultRightHandHoldingBowFirstPersonDL);
ADULT_SYM(gLinkAdultRightHandHoldingOotNearDL);
ADULT_SYM(gLinkAdultRightHandHoldingHookshotNearDL);
ADULT_SYM(gLinkAdultRightHandHoldingHookshotFarDL);
ADULT_SYM(gLinkAdultHandHoldingBottleDL);
ADULT_SYM(gLinkAdultRightShoulderNearDL);
ADULT_SYM(gLinkAdultBowStringDL);
ADULT_SYM(gLinkAdultEyesOpenTex);
ADULT_SYM(gLinkAdultEyesHalfTex);
ADULT_SYM(gLinkAdultEyesClosedfTex);
ADULT_SYM(gLinkAdultEyesRollRightTex);
ADULT_SYM(gLinkAdultEyesRollLeftTex);
ADULT_SYM(gLinkAdultEyesUnk1Tex);
ADULT_SYM(gLinkAdultEyesUnk2Tex);
ADULT_SYM(gLinkAdultEyesShockTex);
ADULT_SYM(gLinkAdultMouth1Tex);
ADULT_SYM(gLinkAdultMouth2Tex);
ADULT_SYM(gLinkAdultMouth3Tex);
ADULT_SYM(gLinkAdultMouth4Tex);
#undef ADULT_SYM

#define ADULT_DL(name) ((Gfx*)(name))

// OoT's player draw binds segment 0x0C to this; vanilla OoT limb DLs jump through it
// for their cull mode, so it must be bound whenever ported OoT geometry draws in MM.
alignas(8) Gfx sCullBackDL[] = {
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};

// Slots with no adult counterpart show nothing rather than child-scaled geometry.
alignas(8) Gfx sAdultEmptyDL[] = {
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
PlayerAgeProperties sBackupAgeProperties;
PlayerAnimationHeader* sBackupDoorA[PLAYER_ANIMTYPE_MAX];
PlayerAnimationHeader* sBackupDoorB[PLAYER_ANIMTYPE_MAX];

struct DlPatch {
    Gfx** Slot;
    Gfx* Value;
};

// OoTMM's MM-equipment-on-adult combo models are not extractable, so same-role OoT DLs stand
// in: Hero's Shield as the Hylian Shield, every sword as the Master Sword.
const DlPatch kDlPatches[] = {
    { &gPlayerWaistDLs[kHumanNear], ADULT_DL(gLinkAdultWaistNearDL) },
    { &gPlayerWaistDLs[kHumanFar], ADULT_DL(gLinkAdultWaistNearDL) },
    { &gPlayerHandHoldingShields[0], ADULT_DL(gLinkAdultRightHandHoldingHylianShieldNearDL) },
    { &gPlayerHandHoldingShields[1], ADULT_DL(gLinkAdultRightHandHoldingHylianShieldNearDL) },
    { &gPlayerHandHoldingShields[2], ADULT_DL(gLinkAdultRightHandHoldingMirrorShieldNearDL) },
    { &gPlayerHandHoldingShields[3], ADULT_DL(gLinkAdultRightHandHoldingMirrorShieldNearDL) },
    { &gPlayerSheath12DLs[kHumanNear], sAdultEmptyDL },
    { &gPlayerSheath12DLs[kHumanFar], sAdultEmptyDL },
    { &gPlayerSheath13DLs[kHumanNear], sAdultEmptyDL },
    { &gPlayerSheath13DLs[kHumanFar], sAdultEmptyDL },
    { &gPlayerSheath14DLs[kHumanNear], sAdultEmptyDL },
    { &gPlayerSheath14DLs[kHumanFar], sAdultEmptyDL },
    // OoT has no standalone back-shield DL; the shield-and-sheath combos stand in, and their
    // sheath mesh coincides with gLinkAdultSheathNearDL below.
    { &gPlayerShields[0], ADULT_DL(gLinkAdultHylianShieldAndSheathNearDL) },
    { &gPlayerShields[1], ADULT_DL(gLinkAdultHylianShieldAndSheathNearDL) },
    { &gPlayerShields[2], ADULT_DL(gLinkAdultMirrorShieldAndSheathNearDL) },
    { &gPlayerShields[3], ADULT_DL(gLinkAdultMirrorShieldAndSheathNearDL) },
    { &gPlayerSheathedSwords[0], ADULT_DL(gLinkAdultMasterSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[1], ADULT_DL(gLinkAdultMasterSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[2], ADULT_DL(gLinkAdultMasterSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[3], ADULT_DL(gLinkAdultMasterSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[4], ADULT_DL(gLinkAdultMasterSwordAndSheathNearDL) },
    { &gPlayerSheathedSwords[5], ADULT_DL(gLinkAdultMasterSwordAndSheathNearDL) },
    { &gPlayerSwordSheaths[0], ADULT_DL(gLinkAdultSheathNearDL) },
    { &gPlayerSwordSheaths[1], ADULT_DL(gLinkAdultSheathNearDL) },
    { &gPlayerSwordSheaths[2], ADULT_DL(gLinkAdultSheathNearDL) },
    { &gPlayerSwordSheaths[3], ADULT_DL(gLinkAdultSheathNearDL) },
    { &gPlayerSwordSheaths[4], ADULT_DL(gLinkAdultSheathNearDL) },
    { &gPlayerSwordSheaths[5], ADULT_DL(gLinkAdultSheathNearDL) },
    { &gPlayerLeftHandTwoHandSwordDLs[kHumanNear], ADULT_DL(gLinkAdultLeftHandHoldingBgsNearDL) },
    { &gPlayerLeftHandTwoHandSwordDLs[kHumanFar], ADULT_DL(gLinkAdultLeftHandHoldingBgsNearDL) },
    { &gPlayerLeftHandOpenDLs[kHumanNear], ADULT_DL(gLinkAdultLeftHandNearDL) },
    { &gPlayerLeftHandOpenDLs[kHumanFar], ADULT_DL(gLinkAdultLeftHandNearDL) },
    { &gPlayerLeftHandClosedDLs[kHumanNear], ADULT_DL(gLinkAdultLeftHandClosedNearDL) },
    { &gPlayerLeftHandClosedDLs[kHumanFar], ADULT_DL(gLinkAdultLeftHandClosedNearDL) },
    { &gPlayerLeftHandOneHandSwordDLs[kHumanNear], ADULT_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL) },
    { &gPlayerLeftHandOneHandSwordDLs[kHumanFar], ADULT_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL) },
    { &D_801C018C[0], ADULT_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL) },
    { &D_801C018C[1], ADULT_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL) },
    { &D_801C018C[2], ADULT_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL) },
    { &D_801C018C[3], ADULT_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL) },
    { &D_801C018C[4], ADULT_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL) },
    { &D_801C018C[5], ADULT_DL(gLinkAdultLeftHandHoldingMasterSwordNearDL) },
    { &gPlayerRightHandOpenDLs[kHumanNear], ADULT_DL(gLinkAdultRightHandNearDL) },
    { &gPlayerRightHandOpenDLs[kHumanFar], ADULT_DL(gLinkAdultRightHandNearDL) },
    { &gPlayerRightHandClosedDLs[kHumanNear], ADULT_DL(gLinkAdultRightHandClosedNearDL) },
    { &gPlayerRightHandClosedDLs[kHumanFar], ADULT_DL(gLinkAdultRightHandClosedNearDL) },
    { &gPlayerRightHandBowDLs[kHumanNear], ADULT_DL(gLinkAdultRightHandHoldingBowNearDL) },
    { &gPlayerRightHandBowDLs[kHumanFar], ADULT_DL(gLinkAdultRightHandHoldingBowNearDL) },
    { &gPlayerRightHandInstrumentDLs[kHumanNear], ADULT_DL(gLinkAdultRightHandHoldingOotNearDL) },
    { &gPlayerRightHandInstrumentDLs[kHumanFar], ADULT_DL(gLinkAdultRightHandHoldingOotNearDL) },
    { &gPlayerRightHandHookshotDLs[kHumanNear], ADULT_DL(gLinkAdultRightHandHoldingHookshotNearDL) },
    { &gPlayerRightHandHookshotDLs[kHumanFar], ADULT_DL(gLinkAdultRightHandHoldingHookshotNearDL) },
    { &gPlayerLeftHandBottleDLs[kHumanNear], ADULT_DL(gLinkAdultHandHoldingBottleDL) },
    { &gPlayerLeftHandBottleDLs[kHumanFar], ADULT_DL(gLinkAdultHandHoldingBottleDL) },
    { &sPlayerFirstPersonLeftForearmDLs[kHuman], sAdultEmptyDL },
    { &sPlayerFirstPersonLeftHandDLs[kHuman], ADULT_DL(gLinkAdultLeftHandClosedNearDL) },
    { &sPlayerFirstPersonRightShoulderDLs[kHuman], ADULT_DL(gLinkAdultRightShoulderNearDL) },
    // Must be OoT's dedicated first-person weapon meshes: the third-person ones sit at the wrong
    // depth and angle under MM's first-person limb override.
    { &sPlayerFirstPersonRightHandDLs[kHuman], ADULT_DL(gLinkAdultRightHandHoldingBowFirstPersonDL) },
    { &sPlayerFirstPersonRightHandHookshotDLs[kHuman], ADULT_DL(gLinkAdultRightHandHoldingHookshotFarDL) },
};

// OoT has no up/down eye textures; Unk1/Unk2 take those slots and shock stands in for wincing.
const TexturePtr kAdultEyes[PLAYER_EYES_MAX] = {
    (TexturePtr)gLinkAdultEyesOpenTex,      (TexturePtr)gLinkAdultEyesHalfTex,
    (TexturePtr)gLinkAdultEyesClosedfTex,   (TexturePtr)gLinkAdultEyesRollRightTex,
    (TexturePtr)gLinkAdultEyesRollLeftTex,  (TexturePtr)gLinkAdultEyesUnk1Tex,
    (TexturePtr)gLinkAdultEyesUnk2Tex,      (TexturePtr)gLinkAdultEyesShockTex,
};

const TexturePtr kAdultMouth[PLAYER_MOUTH_MAX] = {
    (TexturePtr)gLinkAdultMouth1Tex,
    (TexturePtr)gLinkAdultMouth2Tex,
    (TexturePtr)gLinkAdultMouth3Tex,
    (TexturePtr)gLinkAdultMouth4Tex,
};

// Zora metrics with Fierce Deity's ledge offsets, composed from the live entries so animation
// pointers stay identical to what z_player.c compares against.
void BuildAdultAgeProperties(PlayerAgeProperties* out) {
    const PlayerAgeProperties* zora = &sPlayerAgeProperties[PLAYER_FORM_ZORA];
    const PlayerAgeProperties* fd = &sPlayerAgeProperties[PLAYER_FORM_FIERCE_DEITY];

    *out = *zora;
    out->unk_44 = fd->unk_44;
    for (int i = 0; i < 4; i++) {
        out->unk_4A[i] = fd->unk_4A[i];
        out->unk_62[i] = fd->unk_62[i];
    }
    out->voiceSfxIdOffset = 0;
    out->surfaceSfxIdOffset = 0x80;
}

bool ComputeIsAdult() {
    if (!OotmmSession_IsActive()) {
        return false;
    }
    const auto& state = OotmmSession_GetState();
    if (!state.GetBoolSetting("crossAge", false)) {
        return false;
    }
    const auto age = state.GetBootConfig().OotAge;
    return age.has_value() && *age == 0;
}

void ApplyAdultLink() {
    if (sApplied) {
        return;
    }

    // A model archive can shadow the skeleton with a resource this build cannot parse;
    // SkelAnime dereferences whatever this resolves to. Failing stays un-applied, so
    // Active/Restore never act on a swap that did not happen.
    auto adultSkel = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        (const char*)gLinkAdultSkel + 7);
    if (adultSkel == nullptr) {
        SPDLOG_WARN("[OoTMM] adult skeleton failed to load; keeping the MM human model");
        return;
    }
    // A model pack's skeleton keeps its own waist limb, mirroring SoH's custom-model rule.
    sCustomModel = adultSkel->GetInitData()->IsCustom;
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
        sBackupAgeProperties = sPlayerAgeProperties[kHuman];
        std::memcpy(sBackupDoorA, D_8085BE84[PLAYER_ANIMGROUP_doorA], sizeof(sBackupDoorA));
        std::memcpy(sBackupDoorB, D_8085BE84[PLAYER_ANIMGROUP_doorB], sizeof(sBackupDoorB));
    }

    // Same 21-limb layout as the child skeleton, so MM's human animations drive it directly.
    gPlayerSkeletons[kHuman] = (FlexSkeletonHeader*)gLinkAdultSkel;

    for (const DlPatch& p : kDlPatches) {
        *p.Slot = p.Value;
    }

    std::memcpy(sPlayerEyesTextures[kHuman], kAdultEyes, sizeof(kAdultEyes));
    std::memcpy(sPlayerMouthTextures[kHuman], kAdultMouth, sizeof(kAdultMouth));

    BuildAdultAgeProperties(&sPlayerAgeProperties[kHuman]);

    // OoTMM's adult-height door animation is not extractable; the tall-form rows stand in.
    for (int t = 0; t < PLAYER_ANIMTYPE_MAX; t++) {
        D_8085BE84[PLAYER_ANIMGROUP_doorA][t] = D_8085BE84[PLAYER_ANIMGROUP_doorA_free][t];
        D_8085BE84[PLAYER_ANIMGROUP_doorB][t] = D_8085BE84[PLAYER_ANIMGROUP_doorB_free][t];
    }

    SPDLOG_INFO("[OoTMM] Adult Link visuals enabled for MM (crossAge)");
}

void RestoreChildLink() {
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
    sPlayerAgeProperties[kHuman] = sBackupAgeProperties;
    std::memcpy(D_8085BE84[PLAYER_ANIMGROUP_doorA], sBackupDoorA, sizeof(sBackupDoorA));
    std::memcpy(D_8085BE84[PLAYER_ANIMGROUP_doorB], sBackupDoorB, sizeof(sBackupDoorB));
}

} // namespace

extern "C" int32_t OotmmAdultLink_CustomModelActive(void) {
    return sApplied && sCustomModel ? 1 : 0;
}

extern "C" float OotmmAdultLink_VanillaHumanRootScale(void) {
    return (sApplied ? &sBackupAgeProperties : &sPlayerAgeProperties[kHuman])->unk_08;
}

extern "C" int32_t OotmmAdultLink_IsAdult(void) {
    return sApplied ? 1 : 0;
}

extern "C" Gfx* OotmmAdultLink_BowStringDL(Gfx* childDL) {
    return sApplied ? ADULT_DL(gLinkAdultBowStringDL) : childDL;
}

extern "C" float OotmmAdultLink_MeleeWeaponLength(int32_t meleeWeapon, float childLength) {
    if (!sApplied) {
        return childLength;
    }
    switch (meleeWeapon) {
        case PLAYER_MELEEWEAPON_SWORD_KOKIRI:
        case PLAYER_MELEEWEAPON_SWORD_RAZOR:
            return 4000.0f;
        case PLAYER_MELEEWEAPON_SWORD_GILDED:
        case PLAYER_MELEEWEAPON_SWORD_TWO_HANDED:
            return 5500.0f;
        default:
            return childLength;
    }
}

extern "C" void OotmmAdultLink_SetTunicColor(struct PlayState* play) {
    if (!sApplied && !OotmmChildLink_Active()) {
        return;
    }
    uint8_t r;
    uint8_t g;
    uint8_t b;
    OotmmEquipment_GetTunicColor(&r, &g, &b);

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sCullBackDL);
    gDPSetEnvColor(POLY_OPA_DISP++, r, g, b, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" Gfx* OotmmAdultLink_CullSegmentDL(void) {
    return sCullBackDL;
}

extern "C" void OotmmAdultLink_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        // Three states own the human form: adult, ported OoT child, vanilla. The current
        // mapper restores before the next applies, so a backup can never capture a mapped state.
        if (ComputeIsAdult()) {
            OotmmChildLink::Restore();
            ApplyAdultLink();
        } else {
            RestoreChildLink();
            if (OotmmChildLink::Wanted()) {
                OotmmChildLink::Apply();
            } else {
                OotmmChildLink::Restore();
            }
        }
    });
}
