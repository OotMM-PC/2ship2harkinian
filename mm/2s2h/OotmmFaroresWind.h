#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <optional>
#include <string_view>

extern "C" {
#include "z64save.h"
}

constexpr uint16_t kOotmmFwCrossEntrance = 0xFFFD;

RespawnData& OotmmFaroresWind_Point();
RespawnData& OotmmFaroresWind_RespawnTop();
bool OotmmFaroresWind_IsCross(const RespawnData& point);
bool OotmmFaroresWind_BeginCrossWarp();

bool OotmmFaroresWind_IsSharedKey(std::string_view key);
uint32_t OotmmFaroresWind_ReadShared(std::string_view key);
void OotmmFaroresWind_WriteShared(std::string_view key, uint32_t owner);

std::optional<uint16_t> OotmmFaroresWind_PrepareArrival(int32_t fileNum);
void OotmmFaroresWind_Embed(nlohmann::json& save);
void OotmmFaroresWind_Init();
