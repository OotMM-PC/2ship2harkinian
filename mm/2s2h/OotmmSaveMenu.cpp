#include "OotmmSaveMenu.h"

#include "CustomMessage/CustomMessage.h"
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

std::string MenuText() {
    std::string msg;
    msg += "\x17";
    msg += sSaveSucceeded ? "Game saved." : "Save failed. Previous save kept.";
    msg += "\x11";
    msg += "\x02";
    msg += "\xC3";
    msg += sSaveSucceeded ? "     Resume" : "     Retry";
    msg += "\x11";
    msg += sSaveSucceeded ? "     Return to Spawn" : "     Resume";
    msg += "\x11";
    msg += sSaveSucceeded ? "     Quit Game" : "     Quit without saving";
    msg += "\xBF";
    return msg;
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
