#include "OotmmTriforce.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmAppliedLedger.h>
#include <libultraship/bridge/OotmmItemGrant.h>

#include <string>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"

extern PlayState* gPlayState;
}

namespace {

const std::string kWinKey = "@triforce_win";

Ship::OotmmAppliedLedger sLedger;
bool sLedgerLoaded = false;
uint8_t sSafeFrames = 0;

// Keyed per-save-tag like the applied ledger: a new tag must not inherit an older save's win state.
std::string LedgerPathFor(int fileNum) {
    const auto& state = OotmmSession_GetState();
    return state.GetBootConfig().StatePath + ".triforce.mm-" + state.GetNativeSaveTag() + "." +
           std::to_string(fileNum);
}

std::string LedgerPath() {
    return LedgerPathFor(gSaveContext.fileNum);
}

Ship::OotmmAppliedLedger& Ledger() {
    if (!sLedgerLoaded) {
        sLedger.Reset();
        if (!sLedger.Load(LedgerPath())) {
            sLedger.Load(OotmmSession_GetState().GetBootConfig().StatePath + ".triforce." +
                         std::to_string(gSaveContext.fileNum));
        }
        sLedgerLoaded = true;
    }
    return sLedger;
}

std::string Goal() {
    return OotmmSession_GetState().GetStringSetting("goal", "both");
}

bool IsQuest() {
    return Goal() == "triforce3";
}

bool IsPiece(const std::string& itemId) {
    if (itemId.ends_with("_FULL")) {
        return false;
    }
    const auto ops = Ship::OotmmItemGrant::Resolve(itemId, 1);
    return !ops.empty() && ops.front().Kind == Ship::OotmmGrantKind::Triforce;
}

int PieceCount() {
    int total = 0;
    for (const auto& [itemId, count] : OotmmIpc_GetInventory().GetValues()) {
        if (count > 0 && IsPiece(itemId)) {
            total += static_cast<int>(count);
        }
    }
    return total;
}

bool OnSafeFrame(PlayState* play) {
    if (play == nullptr || gSaveContext.gameMode != GAMEMODE_NORMAL ||
        gSaveContext.minigameStatus == MINIGAME_STATUS_ACTIVE) {
        return false;
    }
    if (IS_PAUSED(&play->pauseCtx) || play->gameOverCtx.state != GAMEOVER_INACTIVE) {
        return false;
    }
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF) {
        return false;
    }
    if (play->msgCtx.msgMode != MSGMODE_NONE ||
        Message_GetState(&play->msgCtx) != TEXT_STATE_NONE) {
        return false;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr || Player_InBlockingCsMode(play, player)) {
        return false;
    }
    const u32 busy = PLAYER_STATE1_1 | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD |
                     PLAYER_STATE1_200 | PLAYER_STATE1_400 | PLAYER_STATE1_800000 |
                     PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000 | PLAYER_STATE1_80000000;
    return (player->stateFlags1 & busy) == 0;
}

bool SafeToWarp(PlayState* play) {
    if (!OnSafeFrame(play)) {
        sSafeFrames = 0;
        return false;
    }
    if (sSafeFrames < 4) {
        sSafeFrames++;
    }
    return sSafeFrames >= 4;
}

void CreditWarp(PlayState* play) {
    Player* player = GET_PLAYER(play);
    player->stateFlags1 |= PLAYER_STATE1_200;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    gSaveContext.save.day = 0;
    gSaveContext.save.time = CLOCK_TIME(6, 0) - 1;
    Sram_SaveSpecialNewDay(play);
    play->nextEntrance = ENTRANCE(TERMINA_FIELD, 0);
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextCutsceneIndex = 0xFFF7;
}

RegisterShipInitFunc sInit([]() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        if (gPlayState != nullptr) {
            OotmmTriforce_Update();
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        sLedgerLoaded = false;
        sSafeFrames = 0;
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>([](s16 fileNum) {
        if (!OotmmSession_IsActive()) {
            return;
        }
        sLedger.Reset();
        sLedger.Save(LedgerPathFor(fileNum));
        sLedgerLoaded = true;
        sSafeFrames = 0;
    });
});

} // namespace

extern "C" int OotmmTriforce_Active(void) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    const std::string goal = Goal();
    return goal == "triforce" || goal == "triforce3";
}

extern "C" int OotmmTriforce_Count(void) {
    return OotmmTriforce_Active() ? PieceCount() : 0;
}

extern "C" int OotmmTriforce_Goal(void) {
    if (!OotmmTriforce_Active()) {
        return 0;
    }
    if (IsQuest()) {
        return 3;
    }
    return OotmmSession_GetState().GetIntSetting("triforceGoal", 20);
}

extern "C" int OotmmTriforce_DisplayMax(void) {
    if (!OotmmTriforce_Active()) {
        return 0;
    }
    if (IsQuest() || !OotmmTriforce_HasWon()) {
        return OotmmTriforce_Goal();
    }
    return OotmmSession_GetState().GetIntSetting("triforcePieces", OotmmTriforce_Goal());
}

extern "C" int OotmmTriforce_HasWon(void) {
    return OotmmTriforce_Active() && Ledger().Applied(kWinKey) != 0;
}

extern "C" void OotmmTriforce_Update(void) {
    if (!OotmmTriforce_Active() || OotmmTriforce_HasWon()) {
        return;
    }
    const int goal = OotmmTriforce_Goal();
    if (goal <= 0 || PieceCount() < goal || !SafeToWarp(gPlayState)) {
        return;
    }
    Ledger().SetApplied(kWinKey, 1);
    sLedger.Save(LedgerPath());
    CreditWarp(gPlayState);
}
