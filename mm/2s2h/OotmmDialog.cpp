#include "OotmmDialog.h"

#include <libultraship/bridge/OotmmDialog.h>
#include <libultraship/libultraship.h>

#include <algorithm>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"

extern const char* fontTbl[156];
extern f32 sNESFontWidths[160];
extern Color_RGB16 sColorsNormalNES[];
void Message_DrawTextChar(PlayState* play, TexturePtr texture, Gfx** gfxP);
}

namespace {

constexpr s16 kBoxWidth = 256;
constexpr s16 kLineHeight = 12;
constexpr s16 kTextTop = 8;
constexpr s16 kTextRightInset = 16;
constexpr s16 kOptionInset = 10;
constexpr s16 kBarRightInset = 10;
constexpr s16 kBarWidth = 3;
constexpr s16 kSpaceAdvance = 6;
constexpr s16 kThreeChoiceShift = 22;
constexpr s16 kHiddenY = 300;

bool sActive = false;
bool sBuilt = false;
Ship::OotmmDialogSpec sSpec;
Ship::OotmmDialogLayout sLayout;
Ship::OotmmDialogController sController;
std::function<void(int)> sOnChoice;
u32 sHeldFrame = 0;
bool sHolding = false;
s16 sLeft = 0;
bool sHoldingPlayer = false;

void HoldPlayer(PlayState* play, bool hold) {
    if (play != nullptr && hold != sHoldingPlayer) {
        Player_SetCsActionWithHaltedActors(play, nullptr, hold ? PLAYER_CSACTION_WAIT : PLAYER_CSACTION_END);
    }
    sHoldingPlayer = hold;
}

std::string Normalize(std::string text) {
    static constexpr std::pair<char, const char*> kRaw[] = {
        { '\x01', "%r" }, { '\x02', "%g" }, { '\x03', "%b" }, { '\x04', "%y" },
        { '\x05', "%c" }, { '\x06', "%p" }, { '\x08', "%o" }, { '\x11', "\n" },
    };
    std::string result;
    result.reserve(text.size());
    for (const char c : text) {
        const auto raw = std::find_if(std::begin(kRaw), std::end(kRaw), [c](const auto& entry) { return entry.first == c; });
        if (raw != std::end(kRaw)) {
            result += raw->second;
        } else {
            result.push_back(c);
        }
    }
    return result;
}

void Assign(std::string body, std::vector<std::string> options) {
    options.resize(std::clamp<size_t>(options.size(), 2, 3));
    sSpec = {};
    sSpec.Body = Normalize(std::move(body));
    for (auto& option : options) {
        sSpec.Options.push_back(Normalize(std::move(option)));
    }
}

std::string EngineText() {
    return std::string(sSpec.Options.size() == 3 ? "\xC3" : "\xC2") + "\xBF";
}

void Activate() {
    sActive = true;
    sBuilt = false;
    sHolding = false;
}

bool IsChoosing(PlayState* play) {
    const MessageContext* msgCtx = &play->msgCtx;
    return sActive && msgCtx->msgLength != 0 && msgCtx->msgMode == MSGMODE_TEXT_DONE &&
           (msgCtx->textboxEndType == TEXTBOX_ENDTYPE_TWO_CHOICE || msgCtx->textboxEndType == TEXTBOX_ENDTYPE_THREE_CHOICE);
}

void Build(PlayState* play) {
    if (sBuilt) {
        return;
    }
    const MessageContext* msgCtx = &play->msgCtx;
    Ship::OotmmDialogFont font;
    for (size_t i = 0; i < font.Advance.size(); ++i) {
        font.Advance[i] = static_cast<int16_t>(static_cast<s32>(sNESFontWidths[i] * msgCtx->textCharScale));
    }
    font.Advance[0] = kSpaceAdvance;

    sLeft = msgCtx->unk11FF8 - (sSpec.Options.size() == 3 ? kThreeChoiceShift : 0);
    const s16 right = msgCtx->textboxXTarget + kBoxWidth - kTextRightInset;
    Ship::OotmmDialogBox box;
    box.BodyWidth = right - sLeft;
    box.OptionWidth = right - sLeft - kOptionInset;
    sLayout = Ship::OotmmDialogLayOut(sSpec, font, box);
    sController = Ship::OotmmDialogController(sLayout);
    sBuilt = true;
}

s16 RowY(PlayState* play, int row) {
    const s16 centering = static_cast<s16>((sLayout.VisibleLines - sController.View().ShownLines()) * kLineHeight / 2);
    return static_cast<s16>(play->msgCtx.textboxY + kTextTop + centering + row * kLineHeight);
}

void SetColor(MessageContext* msgCtx, Ship::OotmmDialogColor color) {
    static constexpr int kTable[] = { -1, 0, 1, 2, 4, 5, 3, 7 };
    const int index = kTable[static_cast<size_t>(color)];
    if (index < 0) {
        const bool dark = msgCtx->textBoxType == TEXTBOX_TYPE_5 || msgCtx->textBoxType == TEXTBOX_TYPE_D;
        msgCtx->textColorR = msgCtx->textColorG = msgCtx->textColorB = dark ? 0 : 255;
        return;
    }
    msgCtx->textColorR = sColorsNormalNES[index].r;
    msgCtx->textColorG = sColorsNormalNES[index].g;
    msgCtx->textColorB = sColorsNormalNES[index].b;
}

void DrawBar(PlayState* play, Gfx** gfxP) {
    if (!sController.View().Scrollable()) {
        return;
    }
    const MessageContext* msgCtx = &play->msgCtx;
    const s16 track = sLayout.VisibleLines * kLineHeight;
    const auto thumb = sController.View().Thumb(track);
    const s16 x = msgCtx->textboxX + kBoxWidth - kBarRightInset;
    const s16 y = msgCtx->textboxY + kTextTop;
    Gfx* gfx = *gfxP;
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, msgCtx->textColorAlpha / 4);
    gDPFillRectangle(gfx++, x, y, x + kBarWidth, y + track);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, msgCtx->textColorAlpha);
    gDPFillRectangle(gfx++, x, y + thumb.Offset, x + kBarWidth, y + thumb.Offset + thumb.Length);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
    *gfxP = gfx;
}

} // namespace

namespace OotmmDialog {

void Load(CustomMessage::Entry entry, std::string body, std::vector<std::string> options) {
    Assign(std::move(body), std::move(options));
    entry.autoFormat = false;
    entry.msg = EngineText();
    CustomMessage::LoadCustomMessageIntoFont(entry);
    sOnChoice = nullptr;
    Activate();
}

void Start(std::string body, std::vector<std::string> options, std::function<void(int)> onChoice) {
    Assign(std::move(body), std::move(options));
    CustomMessage::Entry entry;
    entry.autoFormat = false;
    CustomMessage::StartTextbox(EngineText(), entry);
    sOnChoice = std::move(onChoice);
    Activate();
    HoldPlayer(gPlayState, true);
}

DebugState Debug() {
    DebugState state;
    if (gPlayState == nullptr || !sActive || !sBuilt || gPlayState->msgCtx.msgLength == 0) {
        return state;
    }
    state.Active = true;
    state.Top = sController.View().Top();
    state.Total = static_cast<int>(sLayout.Lines.size());
    state.Choice = gPlayState->msgCtx.choiceIndex;
    state.Scrollable = sController.View().Scrollable();
    for (int row = 0; row < sController.View().ShownLines(); ++row) {
        std::string text;
        for (const auto& c : sLayout.Lines[static_cast<size_t>(sController.View().Top() + row)].Chars) {
            text.push_back(c.Code);
        }
        state.Shown.push_back(text);
    }
    for (const auto first : sLayout.OptionFirstLine) {
        state.OptionRows.push_back(sController.View().Row(first));
        state.OptionLines.push_back(first);
    }
    if (state.Choice >= 0 && state.Choice < static_cast<int>(state.OptionRows.size())) {
        state.CursorRow = state.OptionRows[static_cast<size_t>(state.Choice)];
    }
    return state;
}

} // namespace OotmmDialog

extern "C" void OotmmDialog_Reset(void) {
    sActive = false;
    sOnChoice = nullptr;
    HoldPlayer(gPlayState, false);
}

extern "C" void OotmmDialog_Draw(PlayState* play, Gfx** gfxP) {
    if (!sActive) {
        return;
    }
    Build(play);
    MessageContext* msgCtx = &play->msgCtx;
    const s16 savedX = msgCtx->textPosX;
    const s16 savedY = msgCtx->textPosY;
    const s16 savedR = msgCtx->textColorR;
    const s16 savedG = msgCtx->textColorG;
    const s16 savedB = msgCtx->textColorB;

    for (int row = 0; row < sController.View().ShownLines(); ++row) {
        const auto& line = sLayout.Lines[static_cast<size_t>(sController.View().Top() + row)];
        const s16 x = sLeft + (line.Option >= 0 ? kOptionInset : 0);
        msgCtx->textPosY = RowY(play, row);
        for (const auto& c : line.Chars) {
            if (c.Code == ' ') {
                continue;
            }
            SetColor(msgCtx, c.Color);
            msgCtx->textPosX = x + c.X;
            Message_DrawTextChar(play, (TexturePtr)fontTbl[c.Code - ' '], gfxP);
        }
    }
    DrawBar(play, gfxP);

    msgCtx->textPosX = savedX;
    msgCtx->textPosY = savedY;
    msgCtx->textColorR = savedR;
    msgCtx->textColorG = savedG;
    msgCtx->textColorB = savedB;
}

extern "C" s16 OotmmDialog_CursorY(PlayState* play, s16 vanillaY) {
    if (!sActive) {
        return vanillaY;
    }
    Build(play);
    const size_t choice = play->msgCtx.choiceIndex;
    if (choice >= sLayout.OptionFirstLine.size()) {
        return kHiddenY;
    }
    const int row = sController.View().Row(sLayout.OptionFirstLine[choice]);
    return row < 0 ? kHiddenY : RowY(play, row);
}

extern "C" void OotmmDialog_HandleInput(PlayState* play) {
    if (!IsChoosing(play)) {
        return;
    }
    Build(play);
    MessageContext* msgCtx = &play->msgCtx;
    const Input* input = CONTROLLER1(&play->state);
    Ship::OotmmDialogInput navigation;
    navigation.Choice = msgCtx->choiceIndex;
    navigation.StickY = input->rel.stick_y;
    navigation.ScrollStickY = input->cur.right_stick_y;
    navigation.ScrollUp = CHECK_BTN_ALL(input->cur.button, BTN_DUP);
    navigation.ScrollDown = CHECK_BTN_ALL(input->cur.button, BTN_DDOWN);
    const auto outcome = sController.Navigate(navigation);
    msgCtx->choiceIndex = static_cast<u8>(outcome.Choice);
    if (outcome.Scrolled) {
        Audio_PlaySfx(NA_SE_SY_CURSOR);
    }
    if (sOnChoice && Message_ShouldAdvance(play)) {
        const auto onChoice = std::move(sOnChoice);
        const int choice = msgCtx->choiceIndex;
        Message_CloseTextbox(play);
        HoldPlayer(play, false);
        onChoice(choice);
    }
}

extern "C" s32 OotmmDialog_HoldAdvance(PlayState* play) {
    if (!IsChoosing(play)) {
        return false;
    }
    Build(play);
    if (sHolding && sHeldFrame == play->state.frames) {
        return true;
    }
    sHolding = false;
    const Input* input = CONTROLLER1(&play->state);
    if (CHECK_BTN_ALL(input->press.button, BTN_A) && sController.GuardConfirm()) {
        sHolding = true;
        sHeldFrame = play->state.frames;
        return true;
    }
    return false;
}
