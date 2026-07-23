#pragma once

void OotmmIpc_Init();
void OotmmIpc_Pump();
void OotmmIpc_Shutdown();

extern "C" int gOotmmGameSpeedPercent;
extern "C" int gOotmmGameSpeedSmooth;
