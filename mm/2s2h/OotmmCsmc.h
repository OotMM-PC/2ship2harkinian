#pragma once

#include <libultraship/bridge/OotmmCsmc.h>

struct Actor;

void OotmmCsmc_Init();
/// Renders a chest as the given class regardless of placements (test chests).
void OotmmCsmc_SetChestOverride(struct Actor* chest, Ship::OotmmCsmcClass itemClass);
void OotmmCsmc_ClearChestOverrides();
