#include "OotmmClocks.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmProgress.h>

#include <cstdint>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

constexpr uint32_t kAllHalfDays = 0x3F;
constexpr uint32_t kGraceLinear = 0x1E0;

uint32_t Linear(uint32_t day, uint16_t time) {
    return day * 0x10000 + static_cast<uint16_t>(time - 0x4000);
}

uint32_t HalfDayLinear(uint32_t halfDay) {
    return 0x10000 + halfDay * 0x8000;
}

uint32_t MoonCrashLinear(const uint32_t mask) {
    uint32_t last = 0;
    for (uint32_t i = 0; i < 6; i++) {
        if (mask & (1u << i)) {
            last = i + 1;
        }
    }
    return HalfDayLinear(last);
}

void ApplyGracePeriod() {
    if (!OotmmSession_IsActive()) {
        return;
    }
    const uint32_t moonCrash = MoonCrashLinear(OotmmClocks_HalfDayMask());
    const uint32_t linear =
        Linear(static_cast<uint32_t>(gSaveContext.save.day), gSaveContext.save.time);
    const bool crashed = linear >= moonCrash && linear < 0x40000;
    if (crashed || (linear < moonCrash && linear + kGraceLinear >= moonCrash)) {
        const uint32_t grace = moonCrash - kGraceLinear;
        gSaveContext.save.day = static_cast<s32>(grace >> 16);
        gSaveContext.save.time = static_cast<u16>((grace & 0xFFFF) + 0x4000);
    }
}

RegisterShipInitFunc sInit([]() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>(
        [](s16) { ApplyGracePeriod(); });
});

} // namespace

extern "C" void OotmmClocks_FixupSpawnTime(void) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    if (!(gSaveContext.save.day == 0 ||
          (gSaveContext.save.day == 1 && gSaveContext.save.time == CLOCK_TIME(6, 0)))) {
        return;
    }

    Sram_ClearFlagsAtDawnOfTheFirstDay();

    const uint32_t mask = OotmmClocks_HalfDayMask();
    int firstHalfDay = 6;
    for (int i = 0; i < 6; i++) {
        if (mask & (1u << i)) {
            firstHalfDay = i;
            break;
        }
    }

    if (firstHalfDay == 0) {
        if (static_cast<uint16_t>(gSaveContext.save.entrance) != ENTRANCE(SOUTH_CLOCK_TOWN, 0)) {
            gSaveContext.save.day = 1;
            gSaveContext.save.time = CLOCK_TIME(6, 1);
        }
        return;
    }

    if (firstHalfDay & 1) {
        gSaveContext.save.day = firstHalfDay / 2 + 1;
        gSaveContext.save.time = CLOCK_TIME(18, 0);
    } else {
        gSaveContext.save.day = firstHalfDay / 2;
        gSaveContext.save.time = CLOCK_TIME(6, 0) - 1;
    }
}

extern "C" int OotmmClocks_Enabled(void) {
    return OotmmSession_IsActive() && OotmmSession_GetState().GetBoolSetting("clocks", false) &&
           OotmmIpc_GetInventory().GetRevision() != 0;
}

extern "C" uint32_t OotmmClocks_HalfDayMask(void) {
    if (!OotmmClocks_Enabled()) {
        return kAllHalfDays;
    }
    return Ship::OotmmClockMask(OotmmSession_GetState(), OotmmIpc_GetInventory());
}
