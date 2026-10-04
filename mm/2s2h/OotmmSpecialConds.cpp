#include "OotmmSpecialConds.h"

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"
#include "OotmmTriforce.h"

#include <string>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

bool sMajoraRefused = false;

bool SpecialCondMet(const char* name) {
    return OotmmSession_GetState().IsSpecialCondMet(name, OotmmIpc_GetInventory());
}

} // namespace

extern "C" int OotmmSpecialConds_MoonAccessible(void) {
    const std::string mode = OotmmSession_GetState().GetStringSetting("moon", "vanilla");
    if (mode == "open") {
        return 1;
    }
    if (mode == "custom") {
        return SpecialCondMet("MOON") ? 1 : 0;
    }
    return CHECK_QUEST_ITEM(QUEST_REMAINS_ODOLWA) && CHECK_QUEST_ITEM(QUEST_REMAINS_GOHT) &&
                   CHECK_QUEST_ITEM(QUEST_REMAINS_GYORG) && CHECK_QUEST_ITEM(QUEST_REMAINS_TWINMOLD)
               ? 1
               : 0;
}

extern "C" int OotmmSpecialConds_CanFightMajora(void) {
    bool allowed = true;
    if (OotmmTriforce_Active()) {
        allowed = OotmmTriforce_HasWon() != 0;
    } else if (OotmmSession_GetState().GetStringSetting("majoraChild", "none") == "custom") {
        allowed = SpecialCondMet("MAJORA");
    }
    sMajoraRefused = !allowed;
    return allowed ? 1 : 0;
}

void OotmmSpecialConds_Init() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0x21FF, [](uint16_t* textId, bool* loadFromMessageTable) {
            if (!sMajoraRefused || !OotmmSession_IsActive()) {
                return;
            }
            sMajoraRefused = false;
            auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
            entry.msg = "You're too weak...\xE0";
            CustomMessage::LoadCustomMessageIntoFont(entry);
            *loadFromMessageTable = false;
        });
}
