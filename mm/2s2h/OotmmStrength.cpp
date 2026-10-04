#include "OotmmStrength.h"

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmSession.h"

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"

extern PlayState* gPlayState;
extern u8 gPlayerFormItemRestrictions[PLAYER_FORM_MAX][114];
}

namespace {

constexpr uint16_t kKegSellerFirst = 0x0C8E;
constexpr uint16_t kKegSellerRepeat = 0x0C8F;

bool KegStrength() {
    return OotmmSession_BoolSetting("kegStrength3") != 0;
}

bool StrongHuman() {
    return KegStrength() && CUR_UPG_VALUE(UPG_STRENGTH) >= 3;
}

void LoadKegText(uint16_t* textId, bool* loadFromMessageTable, const char* text) {
    if (!KegStrength()) {
        return;
    }
    auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
    entry.msg = text;
    entry.autoFormat = false;
    CustomMessage::ReplaceColorChars(&entry.msg);
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

void SyncKegRestriction() {
    gPlayerFormItemRestrictions[PLAYER_FORM_HUMAN][ITEM_POWDER_KEG] = StrongHuman() ? 1 : 0;
}

} // namespace

extern "C" int OotmmStrength_KegForStrong(void) {
    return KegStrength() && Player_GetStrength() >= PLAYER_STRENGTH_GORON;
}

extern "C" int OotmmStrength_StrongHuman(void) {
    return StrongHuman();
}

extern "C" void OotmmStrength_Init(void) {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>(SyncKegRestriction);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        kKegSellerFirst, [](uint16_t* textId, bool* loadFromMessageTable) {
            LoadKegText(textId, loadFromMessageTable,
                        "I'm the Goron who sells %rPowder\x11Kegs%w, the most famous product of\x11the Gorons.\x10"
                        "But the rules say I can't sell\x11%rPowder Kegs%w to anyone who isn't\x11%rstrong enough%w"
                        " to carry them. \x1E\x39\x1CSorry.\xBF");
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        kKegSellerRepeat, [](uint16_t* textId, bool* loadFromMessageTable) {
            LoadKegText(textId, loadFromMessageTable,
                        "The rules say I can't sell %rPowder\x11Kegs%w to anyone who isn't %rstrong\x11"
                        "enough%w to carry them. \x1E\x39\x1CSorry.\xBF");
        });
}
