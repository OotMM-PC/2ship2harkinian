#pragma once

#include <libultraship/bridge/OotmmGameState.h>

#include <optional>

void OotmmIpc_Init();
void OotmmIpc_Pump();
void OotmmIpc_Shutdown();
bool OotmmIpc_SendCrossGameTransition(const Ship::OotmmEntranceMapping& mapping,
                                      std::optional<uint32_t> ootAge);
bool OotmmIpc_ConsumeTransitionAccepted();

extern "C" int gOotmmGameSpeedPercent;
extern "C" int gOotmmGameSpeedSmooth;
