#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "functions.h"
}

#define CVAR_NAME "gEnhancements.Cycle.StopOceansideSpiderHouseSquatter"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

void RegisterOceansideSpiderHouseSquatter() {
    /*
     * Normally, the Oceanside Spider House squatter appears at the entrance once the player has acquired the 30
     * Skulltula tokens. Mikau's grave on Great Bay Coast, for some reason, sets the flag for the squatter to move in
     * and sit in the inner area. If Link has not spoken to him before this point, his reward will be unavailable for
     * the remainder of the cycle. This fix changes it so that this will not happen until the player speaks to the
     * squatter.
     */
    COND_HOOK(OnFlagSet, CVAR, [](FlagType flagType, u32 flag) {
        if (flagType == FLAG_WEEK_EVENT_REG) {
            if (flag == WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN) {
                // Quietly unset it, unless we received the reward
                if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_COLLECTED_REWARD)) {
                    CLEAR_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);
                }
            } else if (flag == WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_COLLECTED_REWARD) {
                // Collected the reward, so go ahead and set the other flag
                SET_WEEKEVENTREG(WEEKEVENTREG_OCEANSIDE_SPIDER_HOUSE_BUYER_MOVED_IN);
            }
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterOceansideSpiderHouseSquatter, { CVAR_NAME });
