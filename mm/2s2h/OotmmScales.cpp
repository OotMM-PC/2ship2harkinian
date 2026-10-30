#include "OotmmScales.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmCustomItems.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmScales.h>

#include <algorithm>
#include <cstdint>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"

extern PlayState* gPlayState;
}

namespace {

constexpr uint8_t kNoGroundVoidFrames = 20;
constexpr int32_t kVoidPlayerParams = 0xDFF;

constexpr float kDiveDepths[] = { 120.0f, 240.0f, 360.0f };
constexpr float kDiveStep = 40.0f;
constexpr uint8_t kDiveDoActions[] = { DO_ACTION_1, DO_ACTION_2, DO_ACTION_3, DO_ACTION_4,
                                       DO_ACTION_5, DO_ACTION_6, DO_ACTION_7, DO_ACTION_8 };

struct SafeGround {
    bool Valid = false;
    Vec3f Pos{};
    int16_t Yaw = 0;
    uint16_t Entrance = 0;
    int32_t Room = 0;
};

SafeGround sSafe;
uint8_t sNoGroundVoidTimer = 0;

bool SwimNeedsScale() {
    return OotmmSession_IsActive() &&
           Ship::OotmmSwimNeedsScale(OotmmSession_GetState(), Ship::OotmmGame::Mm);
}

bool OnStandableGround(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
           !(player->actor.bgCheckFlags & BGCHECKFLAG_WATER) &&
           !(player->stateFlags1 & PLAYER_STATE1_8000000) && player->actor.floorBgId == BGCHECK_SCENE;
}

bool EntranceLoadsCurrentScene(PlayState* play) {
    return Entrance_GetSceneIdAbsolute(static_cast<u16>(gSaveContext.save.entrance)) == play->sceneId;
}

void CaptureGround(PlayState* play, Player* player) {
    sSafe.Valid = true;
    sSafe.Pos = player->actor.world.pos;
    sSafe.Yaw = player->actor.world.rot.y;
    sSafe.Entrance = static_cast<uint16_t>(gSaveContext.save.entrance);
    sSafe.Room = play->roomCtx.curRoom.num;
    sNoGroundVoidTimer = 0;
}

void VoidToDryGround(PlayState* play) {
    Audio_PlaySfx(NA_SE_EV_WATER_CONVECTION);
    if (sSafe.Valid) {
        Play_SetRespawnData(play, RESPAWN_MODE_DOWN, sSafe.Entrance, sSafe.Room, kVoidPlayerParams, &sSafe.Pos,
                            sSafe.Yaw);
    }
    func_80169EFC(play);
}

void OnGameStateUpdate() {
    if (gPlayState == nullptr || !SwimNeedsScale()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }

    if (gPlayState->transitionTrigger == TRANS_TRIGGER_OFF && EntranceLoadsCurrentScene(gPlayState) &&
        OnStandableGround(player)) {
        CaptureGround(gPlayState, player);
        return;
    }

    if (player->transformation != PLAYER_FORM_HUMAN && player->transformation != PLAYER_FORM_FIERCE_DEITY) {
        return;
    }
    if (OotmmScales_Tier() != static_cast<int>(Ship::OotmmScaleTier::None)) {
        return;
    }
    if (player->currentBoots == PLAYER_BOOTS_GORON || player->currentBoots == PLAYER_BOOTS_DEKU ||
        OotmmCustomItems_EquippedBoots() == OOTMM_BOOTS_IRON) {
        return;
    }
    if (gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    if (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_200 | PLAYER_STATE1_400 |
                               PLAYER_STATE1_20000000)) {
        return;
    }
    if (gPlayState->pauseCtx.state != PAUSE_STATE_OFF ||
        Message_GetState(&gPlayState->msgCtx) != TEXT_STATE_NONE) {
        return;
    }
    if (!(player->stateFlags1 & PLAYER_STATE1_8000000)) {
        sNoGroundVoidTimer = 0;
        return;
    }

    if (!sSafe.Valid) {
        if (sNoGroundVoidTimer < kNoGroundVoidFrames) {
            sNoGroundVoidTimer++;
            return;
        }
        sNoGroundVoidTimer = 0;
    }
    VoidToDryGround(gPlayState);
}

} // namespace

extern "C" void OotmmScales_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(OnGameStateUpdate);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        sSafe = SafeGround{};
        sNoGroundVoidTimer = 0;
    });
}

extern "C" float OotmmScales_MaxDiveDepth(void) {
    const int32_t tier = OotmmScales_Tier();
    const int32_t rung = std::min<int32_t>(tier > 0 ? tier - 1 : 0, ARRAY_COUNT(kDiveDepths) - 1);
    return kDiveDepths[rung];
}

extern "C" int OotmmScales_DiveDoAction(float depthInWater) {
    if (!OotmmSession_IsActive()) {
        return -1;
    }
    int32_t step = static_cast<int32_t>((OotmmScales_MaxDiveDepth() - depthInWater) / kDiveStep);
    step = CLAMP(step, 0, static_cast<int32_t>(ARRAY_COUNT(kDiveDoActions)) - 1);
    return kDiveDoActions[step];
}

extern "C" int OotmmScales_ExtendsUnderwaterTime(void) {
    return OotmmSession_IsActive() && OotmmSession_GetState().GetBoolSetting("scalesMm", false) ? 1 : 0;
}

extern "C" int OotmmScales_Tier(void) {
    if (!OotmmSession_IsActive()) {
        return static_cast<int>(Ship::OotmmScaleTier::None);
    }
    const Ship::OotmmScaleTier tier =
        Ship::OotmmScaleTierOf(OotmmSession_GetState(), OotmmIpc_GetInventory(), Ship::OotmmGame::Mm,
                               CUR_UPG_VALUE(UPG_SCALE));
    return static_cast<int>(tier);
}
