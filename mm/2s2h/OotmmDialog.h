#pragma once

#ifdef __cplusplus

#include "CustomMessage/CustomMessage.h"

#include <functional>
#include <string>
#include <vector>

namespace OotmmDialog {

/// Replaces the message being opened with a scrolling option box. Call from an OnOpenText hook.
void Load(CustomMessage::Entry entry, std::string body, std::vector<std::string> options);

/// Opens a scrolling option box of its own and reports the option the player confirms.
void Start(std::string body, std::vector<std::string> options, std::function<void(int)> onChoice);

struct DebugState {
    bool Active = false;
    int Top = 0;
    int Total = 0;
    int Choice = 0;
    int CursorRow = -1;
    bool Scrollable = false;
    std::vector<std::string> Shown;
    std::vector<int> OptionRows;
    std::vector<int> OptionLines;
};

DebugState Debug();

} // namespace OotmmDialog

extern "C" {
#endif

struct PlayState;

void OotmmDialog_Reset(void);
void OotmmDialog_Draw(struct PlayState* play, Gfx** gfxP);
s16 OotmmDialog_CursorY(struct PlayState* play, s16 vanillaY);
void OotmmDialog_HandleInput(struct PlayState* play);
s32 OotmmDialog_HoldAdvance(struct PlayState* play);

#ifdef __cplusplus
}
#endif
