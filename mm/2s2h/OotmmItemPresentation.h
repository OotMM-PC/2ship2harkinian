#pragma once

#ifdef __cplusplus
#include <libultraship/bridge/GameIpcSession.h>

struct Player;

void OotmmItemPresentation_Queue(Ship::GameIpcItemPresentation presentation);
bool OotmmItemPresentation_IsPending();
bool OotmmItemPresentation_IsDelivering(const std::string& checkId);
bool OotmmItemPresentation_PlayerReceiving(const Player* player);

void OotmmItemPresentation_Update();
#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
int OotmmItemPresentation_DebugLastFanfare();
#endif

extern "C" {
#endif

void OotmmItemPresentation_PlayGetItemFanfare(void);

#ifdef __cplusplus
}
#endif
