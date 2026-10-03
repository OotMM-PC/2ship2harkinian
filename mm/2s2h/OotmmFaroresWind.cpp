#include "OotmmFaroresWind.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "OotmmSession.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstring>

extern "C" {
#include "z64.h"

#include "OotmmCustomItemsPlayer.h"

extern PlayState* gPlayState;
}

namespace {

constexpr int kAgeAdult = 0;
constexpr int kAgeChild = 1;
constexpr uint32_t kOwnerNone = 0;
constexpr uint32_t kOwnerOot = 1;
constexpr uint32_t kOwnerMm = 2;

std::array<RespawnData, 2> sPoints{};
std::array<RespawnData, 2> sRespawnTops{};
bool sArrivalPending = false;

int CurrentAge() {
    const auto age = OotmmSession_GetState().GetBootConfig().OotAge;
    return age.has_value() && *age == kAgeAdult ? kAgeAdult : kAgeChild;
}

void Clear() {
    sPoints = {};
    sRespawnTops = {};
}

nlohmann::json ToJson(const RespawnData& data) {
    return { { "pos", { data.pos.x, data.pos.y, data.pos.z } },
             { "yaw", data.yaw },
             { "playerParams", data.playerParams },
             { "entrance", data.entrance },
             { "roomIndex", data.roomIndex },
             { "data", data.data },
             { "tempSwitchFlags", data.tempSwitchFlags },
             { "unk_18", data.unk_18 },
             { "tempCollectFlags", data.tempCollectFlags } };
}

RespawnData FromJson(const nlohmann::json& json) {
    RespawnData data{};
    const auto& pos = json.at("pos");
    data.pos.x = pos.at(0).get<f32>();
    data.pos.y = pos.at(1).get<f32>();
    data.pos.z = pos.at(2).get<f32>();
    data.yaw = json.at("yaw").get<s16>();
    data.playerParams = json.at("playerParams").get<s16>();
    data.entrance = json.at("entrance").get<u16>();
    data.roomIndex = json.at("roomIndex").get<u8>();
    data.data = json.at("data").get<s8>();
    data.tempSwitchFlags = json.at("tempSwitchFlags").get<u32>();
    data.unk_18 = json.at("unk_18").get<u32>();
    data.tempCollectFlags = json.at("tempCollectFlags").get<u32>();
    return data;
}

void Load(int32_t fileNum) {
    Clear();
    nlohmann::json save;
    if (SaveManager_ReadSaveFile(SaveManager_GetFileName(fileNum + 1), save) != 0) {
        return;
    }
    const auto it = save.find("ootmmFw");
    if (it == save.end() || !it->is_object()) {
        return;
    }
    try {
        const auto& points = it->at("points");
        const auto& tops = it->at("respawnTop");
        for (size_t age = 0; age < sPoints.size(); ++age) {
            sPoints[age] = FromJson(points.at(age));
            sRespawnTops[age] = FromJson(tops.at(age));
        }
    } catch (const nlohmann::json::exception& e) {
        SPDLOG_ERROR("[OoTMM] Invalid Farore's Wind save data: {}", e.what());
        Clear();
    }
}

bool IsSet(const RespawnData& point) {
    return point.data > 0;
}

int AgeForKey(std::string_view key) {
    return key == "fwAdult" ? kAgeAdult : kAgeChild;
}

void ApplyArrival() {
    const auto& point = sPoints[CurrentAge()];
    gSaveContext.respawnFlag = 8;
    gSaveContext.respawn[RESPAWN_MODE_HUMAN] = point;
    gSaveContext.respawn[RESPAWN_MODE_TOP] = sRespawnTops[CurrentAge()];
    gSaveContext.save.entrance = point.entrance;
}

} // namespace

RespawnData& OotmmFaroresWind_Point() {
    return sPoints[CurrentAge()];
}

RespawnData& OotmmFaroresWind_RespawnTop() {
    return sRespawnTops[CurrentAge()];
}

bool OotmmFaroresWind_IsCross(const RespawnData& point) {
    return IsSet(point) && point.entrance == kOotmmFwCrossEntrance;
}

bool OotmmFaroresWind_BeginCrossWarp() {
    Ship::OotmmEntranceMapping mapping;
    mapping.FromGame = Ship::OotmmGame::Mm;
    mapping.From = "MM Farore's Wind";
    mapping.ToGame = Ship::OotmmGame::Oot;
    mapping.To = "Farore's Wind";
    mapping.ToNativeId = kOotmmFwCrossEntrance;
    return OotmmSession_BeginCrossGameTransition(mapping);
}

bool OotmmFaroresWind_IsSharedKey(std::string_view key) {
    return key == "fwChild" || key == "fwAdult";
}

uint32_t OotmmFaroresWind_ReadShared(std::string_view key) {
    const auto& point = sPoints[AgeForKey(key)];
    if (!IsSet(point)) {
        return kOwnerNone;
    }
    return OotmmFaroresWind_IsCross(point) ? kOwnerOot : kOwnerMm;
}

void OotmmFaroresWind_WriteShared(std::string_view key, uint32_t owner) {
    const int age = AgeForKey(key);
    auto& point = sPoints[age];
    const RespawnData before = point;
    if (owner == kOwnerOot) {
        point = {};
        point.data = 1;
        point.entrance = kOotmmFwCrossEntrance;
    } else if (owner == kOwnerNone || OotmmFaroresWind_IsCross(point)) {
        point.data = 0;
    }
    if (age == CurrentAge() && gPlayState != nullptr && std::memcmp(&before, &point, sizeof(point)) != 0) {
        OotmmCustomItems_SyncFaroresWind(gPlayState);
    }
}

std::optional<uint16_t> OotmmFaroresWind_PrepareArrival(int32_t fileNum) {
    sArrivalPending = false;
    Load(fileNum);
    const auto& point = sPoints[CurrentAge()];
    if (!IsSet(point) || OotmmFaroresWind_IsCross(point)) {
        SPDLOG_ERROR("[OoTMM] Farore's Wind arrival without an MM warp point");
        return std::nullopt;
    }
    sArrivalPending = true;
    return point.entrance;
}

void OotmmFaroresWind_Embed(nlohmann::json& save) {
    if (gPlayState == nullptr) {
        return;
    }
    nlohmann::json points = nlohmann::json::array();
    nlohmann::json tops = nlohmann::json::array();
    for (size_t age = 0; age < sPoints.size(); ++age) {
        points.push_back(ToJson(sPoints[age]));
        tops.push_back(ToJson(sRespawnTops[age]));
    }
    save["ootmmFw"] = { { "points", std::move(points) }, { "respawnTop", std::move(tops) } };
}

void OotmmFaroresWind_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>([](s16) {
        sArrivalPending = false;
        Clear();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16 fileNum) {
        const bool arrival = sArrivalPending;
        sArrivalPending = false;
        Load(fileNum);
        if (arrival && IsSet(sPoints[CurrentAge()]) && !OotmmFaroresWind_IsCross(sPoints[CurrentAge()])) {
            ApplyArrival();
        }
    });
}
