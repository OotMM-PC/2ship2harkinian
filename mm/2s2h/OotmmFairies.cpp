#include "OotmmFairies.h"

#include "OotmmSession.h"

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

extern "C" int OotmmFairies_HeldCount(int type) {
    if (!OotmmSession_IsActive()) {
        return -1;
    }
    if (type <= 0 || type > 4) {
        return 0;
    }
    const int required = OotmmSession_GetState().GetIntSetting("strayFairyRewardCount", 15);
    return gSaveContext.save.saveInfo.inventory.strayFairies[type - 1] >= required ? STRAY_FAIRY_TOTAL
                                                                                   : 0;
}
