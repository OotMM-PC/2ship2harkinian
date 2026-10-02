#pragma once

#include <libultraship/bridge/GameIpcSession.h>

void OotmmItemPresentation_Queue(Ship::GameIpcItemPresentation presentation);
bool OotmmItemPresentation_IsPending();
bool OotmmItemPresentation_IsDelivering(const std::string& checkId);

void OotmmItemPresentation_Update();
