#pragma once

struct PlayState;
struct Player;

void OotmmChecks_Init();
/// Arms a chest open whose vanilla give is suppressed; nothing is granted.
void OotmmChecks_OpenChestSilently(struct PlayState* play, struct Player* player);
