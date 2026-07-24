#pragma once

#include <libultraship/bridge/OotmmGameState.h>
#include <libultraship/bridge/OotmmInventory.h>

#include <optional>
#include <string>

void OotmmIpc_Init();
void OotmmIpc_Pump();
void OotmmIpc_Shutdown();
bool OotmmIpc_SendCrossGameTransition(const Ship::OotmmEntranceMapping& mapping,
                                      std::optional<uint32_t> ootAge);
bool OotmmIpc_ConsumeTransitionAccepted();
const Ship::OotmmInventory& OotmmIpc_GetInventory();
bool OotmmIpc_SetDebugItemValue(const std::string& itemId, uint32_t value);
bool OotmmIpc_IsConnected();

extern "C" int gOotmmGameSpeedPercent;
extern "C" int gOotmmGameSpeedSmooth;
