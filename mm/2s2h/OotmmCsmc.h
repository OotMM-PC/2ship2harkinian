#pragma once

#include <libultraship/bridge/OotmmCsmc.h>

struct Actor;
struct PlayState;

void OotmmCsmc_Init();
/// Renders a chest as the given class regardless of placements (test chests).
void OotmmCsmc_SetChestOverride(struct Actor* chest, Ship::OotmmCsmcClass itemClass);
void OotmmCsmc_ClearChestOverrides();

/// OoTMM's glitter marker over a hidden check, tinted by the class of its item.
void OotmmCsmc_DrawGlitter(PlayState* play, Actor* actor, const Ship::OotmmPlacement& placement);

void* OotmmCsmc_ContainerDL(const Ship::OotmmPlacement& placement, const char* family);
uint32_t OotmmCsmc_TreeColor(const Ship::OotmmPlacement* placement);
void* OotmmCsmc_SkulltulaDL(const Ship::OotmmPlacement* placement, const char* part);
void* OotmmCsmc_CowRingDL(const Ship::OotmmPlacement* placement, bool collected, void* vanilla);
