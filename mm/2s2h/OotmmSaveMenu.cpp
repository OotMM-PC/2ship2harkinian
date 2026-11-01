#include "OotmmSaveMenu.h"

#include "CustomMessage/CustomMessage.h"
#include "OotmmDungeonRespawn.h"
#include "OotmmSession.h"

#include <libultraship/libultraship.h>

#include <string>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

enum SaveMenuChoice {
    kResume,
    kReturnToSpawn,
    kQuitGame,
};

bool sActive = false;
bool sSaveSucceeded = false;
bool sWarpPage = false;

std::string ChoiceText(const char* title, const char* first, const char* second, const char* third) {
    std::string msg;
    msg += "\x17";
    msg += title;
    msg += "\x11";
    msg += "\x02";
    msg += "\xC3";
    msg += "     ";
    msg += first;
    msg += "\x11";
    msg += "     ";
    msg += second;
    msg += "\x11";
    msg += "     ";
    msg += third;
    msg += "\xBF";
    return msg;
}

std::string MenuText() {
    if (sWarpPage) {
        return ChoiceText("Where to?", "Return to Spawn", "Return to Dungeon Entrance", "Back");
    }
    if (!sSaveSucceeded) {
        return ChoiceText("Save failed. Previous save kept.", "Retry", "Resume", "Quit without saving");
    }
    return ChoiceText("Game saved.", "Resume", OotmmDungeonRespawn_Available() ? "Warp..." : "Return to Spawn",
                      "Quit Game");
}

void ShowMenu() {
    CustomMessage::Entry options;
    options.autoFormat = false;
    CustomMessage::StartTextbox(MenuText(), options);
}

} // namespace

extern "C" void OotmmSaveMenu_Open(struct PlayState* play) {
    if (!OotmmSession_IsActive() || play == nullptr) {
        return;
    }
    sSaveSucceeded = gSaveContext.flashSaveAvailable && gSaveContext.fileNum >= 0 &&
                     gSaveContext.fileNum < 3 && SysFlashrom_AwaitResult() == 0;
    sWarpPage = false;
    ShowMenu();
    sActive = true;
}

extern "C" int OotmmSaveMenu_Active(void) {
    return sActive ? 1 : 0;
}

extern "C" int OotmmSaveMenu_Update(struct PlayState* play) {
    if (!sActive) {
        return 1;
    }
    if (play->msgCtx.msgMode != MSGMODE_TEXT_DONE || !Message_ShouldAdvance(play)) {
        return 0;
    }

    const uint8_t choice = play->msgCtx.choiceIndex;
    if (!sSaveSucceeded && choice == 0) {
        Audio_PlaySfx(NA_SE_SY_DECIDE);
        sSaveSucceeded = OotmmSession_SaveProgress() != 0;
        ShowMenu();
        return 0;
    }
    if (sWarpPage) {
        Audio_PlaySfx(NA_SE_SY_DECIDE);
        if (choice == 2) {
            sWarpPage = false;
            ShowMenu();
            return 0;
        }
        sActive = false;
        sWarpPage = false;
        Message_CloseTextbox(play);
        if (choice == 0) {
            OotmmSession_ReturnToSpawn();
        } else {
            OotmmDungeonRespawn_Warp(play);
        }
        return 1;
    }
    if (sSaveSucceeded && choice == kReturnToSpawn && OotmmDungeonRespawn_Available()) {
        Audio_PlaySfx(NA_SE_SY_DECIDE);
        sWarpPage = true;
        ShowMenu();
        return 0;
    }
    sActive = false;
    Audio_PlaySfx(NA_SE_SY_DECIDE);
    Message_CloseTextbox(play);

    switch (choice) {
        case kReturnToSpawn:
            if (sSaveSucceeded) {
                OotmmSession_ReturnToSpawn();
            }
            break;
        case kQuitGame:
            Ship::Context::GetInstance()->GetWindow()->Close();
            break;
        default:
            break;
    }
    return 1;
}
