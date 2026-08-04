#include "OotmmIpc.h"

#include "OotmmItemPresentation.h"
#include "build.h"

#include <libultraship/bridge/GameIpcSession.h>

namespace {

Ship::GameIpcSession sSession;
Ship::GameIpcSettings sSettings;
Ship::OotmmInventory sInventory;

} // namespace

extern "C" int gOotmmPvpEnabled = 0;
extern "C" int gOotmmShowNames = 1;
extern "C" int gOotmmGameSpeedPercent = 100;
extern "C" int gOotmmGameSpeedSmooth = 0;

void OotmmIpc_Init() {
    sInventory.Reset();
    sSession.Start("mm", gBuildVersion);
}

void OotmmIpc_Pump() {
    if (sSession.Pump(sSettings, &sInventory)) {
        gOotmmGameSpeedPercent = sSettings.SpeedPercent;
        gOotmmGameSpeedSmooth = sSettings.SpeedSmooth ? 1 : 0;
        gOotmmPvpEnabled = sSettings.Pvp ? 1 : 0;
        gOotmmShowNames = sSettings.ShowNames ? 1 : 0;
        sSession.AcknowledgeSettings(sSettings.Revision);
    }
    Ship::GameIpcItemPresentation presentation;
    while (sSession.TryPopItemPresentation(presentation)) {
        OotmmItemPresentation_Queue(std::move(presentation));
    }
}

void OotmmIpc_Shutdown() {
    sSession.Stop();
    sInventory.Reset();
}

bool OotmmIpc_SendCrossGameTransition(const Ship::OotmmEntranceMapping& mapping,
                                      std::optional<uint32_t> ootAge) {
    return sSession.RequestCrossGameTransition(
        "mm", mapping.ToGame == Ship::OotmmGame::Oot ? "oot" : "mm", mapping.To, mapping.ToNativeId, ootAge);
}

bool OotmmIpc_ConsumeTransitionAccepted() {
    return sSession.ConsumeTransitionAccepted();
}

const Ship::OotmmInventory& OotmmIpc_GetInventory() {
    return sInventory;
}

bool OotmmIpc_SetDebugItemValue(const std::string& itemId, uint32_t value) {
    return sSession.SetDebugInventoryValue(itemId, value);
}

bool OotmmIpc_RequestDebugItemPresentation(const std::string& itemId, uint32_t recipientPlayer) {
    return sSession.RequestDebugItemPresentation(itemId, recipientPlayer);
}

bool OotmmIpc_TryPopItemPresentation(Ship::GameIpcItemPresentation& presentation) {
    return sSession.TryPopItemPresentation(presentation);
}

bool OotmmIpc_IsConnected() {
    return sSession.IsConnected();
}

bool OotmmIpc_PresenceActive() {
    return sSession.PresenceActive();
}

bool OotmmIpc_SendPlayerPose(const Ship::OotmmPlayerPose& pose) {
    return sSession.SendPlayerPose(pose);
}

bool OotmmIpc_SendPvpHit(const Ship::OotmmPvpHit& hit) {
    return sSession.SendPvpHit(hit);
}

bool OotmmIpc_TakeRemotePresence(std::vector<Ship::OotmmPlayerPose>& poses,
                                 std::vector<Ship::OotmmPvpHit>& hits) {
    return sSession.TakeRemotePresence(poses, hits);
}
