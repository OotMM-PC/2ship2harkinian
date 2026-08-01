#include "OotmmSpinUpgrade.h"

#include "OotmmIpc.h"
#include "OotmmSession.h"

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

constexpr float kFullCharge = 1.0f;
constexpr float kHalfCharge = 0.5f;

bool Owned() {
    const auto& state = OotmmSession_GetState();
    const auto& inventory = OotmmIpc_GetInventory();
    if (state.GetBoolSetting("sharedSpinUpgrade", false) && inventory.Has("SHARED_SPIN_UPGRADE")) {
        return true;
    }
    return inventory.Has("MM_SPIN_UPGRADE");
}

} // namespace

extern "C" int32_t OotmmSpinUpgrade_SpinLevel(void) {
    if (!OotmmSession_IsActive()) {
        return -1;
    }
    return Owned() ? 1 : 0;
}

extern "C" float OotmmSpinUpgrade_ChargeLimit(void) {
    const int32_t level = OotmmSpinUpgrade_SpinLevel();
    if (level < 0) {
        return CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK) ? kFullCharge
                                                                           : kHalfCharge;
    }
    return level > 0 ? kFullCharge : kHalfCharge;
}
