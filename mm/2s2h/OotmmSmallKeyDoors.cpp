#include "OotmmSmallKeyDoors.h"

#include "OotmmDungeons.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

bool SkeletonKeyCovers() {
    const auto& state = OotmmSession_GetState();
    if (!state.GetBoolSetting("skeletonKeyMm", false)) {
        return false;
    }
    return OotmmIpc_GetInventory().Has(
        state.GetBoolSetting("sharedSkeletonKey", false) ? "SHARED_SKELETON_KEY" : "MM_SKELETON_KEY");
}

} // namespace

extern "C" int OotmmSmallKeyDoorIsOpen(PlayState* play, Actor* actor) {
    if (!OotmmSession_IsActive() || play == nullptr || actor == nullptr) {
        return 0;
    }
    if (OotmmDungeons_SmallKeysRemoved()) {
        return 1;
    }
    return SkeletonKeyCovers() ? 1 : 0;
}
