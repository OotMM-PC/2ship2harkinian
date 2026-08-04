#pragma once

#include <libultraship/bridge/OotmmGameState.h>
#include <libultraship/bridge/GameIpcSession.h>
#include <libultraship/bridge/OotmmInventory.h>

#include <optional>
#include <string>
#include <vector>

void OotmmIpc_Init();
void OotmmIpc_Pump();
void OotmmIpc_Shutdown();
bool OotmmIpc_SendCrossGameTransition(const Ship::OotmmEntranceMapping& mapping,
                                      std::optional<uint32_t> ootAge);
bool OotmmIpc_ConsumeTransitionAccepted();
const Ship::OotmmInventory& OotmmIpc_GetInventory();
bool OotmmIpc_SetDebugItemValue(const std::string& itemId, uint32_t value);
bool OotmmIpc_RequestDebugItemPresentation(const std::string& itemId, uint32_t recipientPlayer);
bool OotmmIpc_TryPopItemPresentation(Ship::GameIpcItemPresentation& presentation);
bool OotmmIpc_IsConnected();

bool OotmmIpc_PresenceActive();
bool OotmmIpc_SendPlayerPose(const Ship::OotmmPlayerPose& pose);
bool OotmmIpc_SendPvpHit(const Ship::OotmmPvpHit& hit);
bool OotmmIpc_TakeRemotePresence(std::vector<Ship::OotmmPlayerPose>& poses,
                                 std::vector<Ship::OotmmPvpHit>& hits);

extern "C" int gOotmmPvpEnabled;
extern "C" int gOotmmShowNames;
extern "C" int gOotmmGameSpeedPercent;
extern "C" int gOotmmGameSpeedSmooth;
