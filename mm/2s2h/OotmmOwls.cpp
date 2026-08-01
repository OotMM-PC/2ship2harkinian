#include "OotmmOwls.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <array>
#include <cstdarg>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

struct OwlDef {
    const char* ItemId;
    const char* PreActivatedFlag;
};

// Indexed by OwlWarpId (OWL_WARP_GREAT_BAY_COAST .. OWL_WARP_STONE_TOWER).
constexpr std::array<OwlDef, 10> kOwls = { {
    { "MM_OWL_GREAT_BAY", "greatbay" },
    { "MM_OWL_ZORA_CAPE", "zoracape" },
    { "MM_OWL_SNOWHEAD", "snowhead" },
    { "MM_OWL_MOUNTAIN_VILLAGE", "mountain" },
    { "MM_OWL_CLOCK_TOWN", "clocktown" },
    { "MM_OWL_MILK_ROAD", "milkroad" },
    { "MM_OWL_WOODFALL", "woodfall" },
    { "MM_OWL_SOUTHERN_SWAMP", "swamp" },
    { "MM_OWL_IKANA_CANYON", "canyon" },
    { "MM_OWL_STONE_TOWER", "tower" },
} };

bool ShuffleActive() {
    return OotmmSession_IsActive() &&
           OotmmSession_GetState().GetStringSetting("owlShuffle", "none") == "anywhere";
}

RegisterShipInitFunc sInit([]() {
    if (!ShuffleActive()) {
        return;
    }
    // Striking a statue no longer grants its warp; the owl item does.
    // TODO: striking a statue should complete its check; the check system does not exist yet,
    // so an unowned statue can only be lit by receiving its item.
    REGISTER_VB_SHOULD(VB_OWL_STATUE_ACTIVATE, { *should = false; });
    REGISTER_VB_SHOULD(VB_OWL_STATUE_BE_ACTIVE, {
        const int owlId = va_arg(args, int);
        *should = *should || ((OotmmOwls_ActivatedMask() >> owlId) & 1);
    });
});

} // namespace

extern "C" uint32_t OotmmOwls_ActivatedMask(void) {
    if (!ShuffleActive()) {
        return 0;
    }
    const auto& state = OotmmSession_GetState();
    const auto& inventory = OotmmIpc_GetInventory();
    uint32_t mask = 0;
    for (size_t i = 0; i < kOwls.size(); i++) {
        if (inventory.Has(kOwls[i].ItemId) ||
            state.WorldFlagContains("mmPreActivatedOwls", kOwls[i].PreActivatedFlag)) {
            mask |= 1u << i;
        }
    }
    return mask;
}
