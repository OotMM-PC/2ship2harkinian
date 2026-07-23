#include "OotmmIpc.h"

#include "build.h"

#include <libultraship/bridge/GameIpcSession.h>

namespace {

Ship::GameIpcSession sSession;
Ship::GameIpcSettings sSettings;

} // namespace

extern "C" int gOotmmGameSpeedPercent = 100;
extern "C" int gOotmmGameSpeedSmooth = 0;

void OotmmIpc_Init() {
    sSession.Start("mm", gBuildVersion);
}

void OotmmIpc_Pump() {
    if (!sSession.Pump(sSettings)) {
        return;
    }

    gOotmmGameSpeedPercent = sSettings.SpeedPercent;
    gOotmmGameSpeedSmooth = sSettings.SpeedSmooth ? 1 : 0;
    sSession.AcknowledgeSettings(sSettings.Revision);
}

void OotmmIpc_Shutdown() {
    sSession.Stop();
}
