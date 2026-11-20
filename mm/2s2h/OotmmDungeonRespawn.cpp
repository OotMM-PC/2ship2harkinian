#include "OotmmDungeonRespawn.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmSession.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"

extern PlayState* gPlayState;
}

namespace {

constexpr uint16_t kTerminaFieldMoonCrash = 0x54C0;

struct Record {
    std::string Game;
    uint16_t Entrance = 0;
    uint8_t Room = 0;
    float Pos[3] = {};
    int16_t Yaw = 0;
    int16_t Params = 0;
    int8_t Data = 0;
    uint32_t TempSwitch = 0;
    uint32_t Unk18 = 0;
    uint32_t TempCollect = 0;
};

struct Store {
    std::optional<Record> Current;
    bool Pending = false;
    std::optional<Record> FwChild;
    std::optional<Record> FwAdult;
};

Store sStore;
bool sLoaded = false;
bool sFirstLoad = true;
bool sArrivalPending = false;

std::filesystem::path StorePath() {
    return std::filesystem::u8path(OotmmSession_GetState().GetBootConfig().StatePath + ".dungeon-respawn.json");
}

bool AdultAge() {
    const auto age = OotmmSession_GetState().GetBootConfig().OotAge;
    return age.has_value() && *age == 0;
}

nlohmann::json ToJson(const Record& record) {
    return { { "game", record.Game },
             { "entrance", record.Entrance },
             { "room", record.Room },
             { "pos", { record.Pos[0], record.Pos[1], record.Pos[2] } },
             { "yaw", record.Yaw },
             { "params", record.Params },
             { "data", record.Data },
             { "tempSwch", record.TempSwitch },
             { "unk18", record.Unk18 },
             { "tempCollect", record.TempCollect } };
}

std::optional<Record> FromJson(const nlohmann::json& value) {
    if (!value.is_object() || !value.contains("game") || !value["game"].is_string() || !value.contains("pos") ||
        !value["pos"].is_array() || value["pos"].size() != 3) {
        return std::nullopt;
    }
    Record record;
    record.Game = value["game"].get<std::string>();
    record.Entrance = value.value("entrance", uint16_t{ 0 });
    record.Room = value.value("room", uint8_t{ 0 });
    for (size_t i = 0; i < 3; i++) {
        record.Pos[i] = value["pos"][i].is_number() ? value["pos"][i].get<float>() : 0.0f;
    }
    record.Yaw = value.value("yaw", int16_t{ 0 });
    record.Params = value.value("params", int16_t{ 0 });
    record.Data = value.value("data", int8_t{ 0 });
    record.TempSwitch = value.value("tempSwch", uint32_t{ 0 });
    record.Unk18 = value.value("unk18", uint32_t{ 0 });
    record.TempCollect = value.value("tempCollect", uint32_t{ 0 });
    return record;
}

void Load() {
    sStore = {};
    sLoaded = true;
    std::ifstream in(StorePath());
    if (!in) {
        return;
    }
    const auto root = nlohmann::json::parse(in, nullptr, false);
    if (!root.is_object()) {
        return;
    }
    if (root.contains("current")) {
        sStore.Current = FromJson(root["current"]);
    }
    sStore.Pending = sStore.Current.has_value() && root.value("pending", false);
    if (root.contains("fwMmChild")) {
        sStore.FwChild = FromJson(root["fwMmChild"]);
    }
    if (root.contains("fwMmAdult")) {
        sStore.FwAdult = FromJson(root["fwMmAdult"]);
    }
}

void Persist() {
    nlohmann::json root = nlohmann::json::object();
    std::ifstream in(StorePath());
    if (in) {
        auto existing = nlohmann::json::parse(in, nullptr, false);
        if (existing.is_object()) {
            root = std::move(existing);
        }
    }
    in.close();
    root["current"] = sStore.Current ? ToJson(*sStore.Current) : nlohmann::json(nullptr);
    root["pending"] = sStore.Pending;
    root["fwMmChild"] = sStore.FwChild ? ToJson(*sStore.FwChild) : nlohmann::json(nullptr);
    root["fwMmAdult"] = sStore.FwAdult ? ToJson(*sStore.FwAdult) : nlohmann::json(nullptr);
    const auto path = StorePath();
    const auto temporary = std::filesystem::path(path).concat(".tmp");
    {
        std::ofstream out(temporary, std::ios::trunc);
        out << root.dump();
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
}

Store& Ensure() {
    if (!sLoaded) {
        Load();
    }
    return sStore;
}

bool IsDungeonScene(int16_t scene) {
    switch (scene) {
        case SCENE_MITURIN:
        case SCENE_HAKUGIN:
        case SCENE_SEA:
        case SCENE_INISIE_N:
        case SCENE_INISIE_R:
        case SCENE_KINSTA1:
        case SCENE_KINDAN2:
        case SCENE_REDEAD:
        case SCENE_TORIDE:
        case SCENE_KAIZOKU:
        case SCENE_PIRATE:
        case SCENE_CASTLE:
        case SCENE_RANDOM:
        case SCENE_SOUGEN:
        case SCENE_LAST_DEKU:
        case SCENE_LAST_GORON:
        case SCENE_LAST_ZORA:
        case SCENE_LAST_LINK:
        case SCENE_MITURIN_BS:
        case SCENE_HAKUGIN_BS:
        case SCENE_SEA_BS:
        case SCENE_INISIE_BS:
        case SCENE_IKNINSIDE:
        case SCENE_LAST_BS:
            return true;
        default:
            return false;
    }
}

bool IsDungeonEntrance(uint16_t entrance) {
    switch (entrance & ~0xF) {
        case ENTRANCE(WOODFALL_TEMPLE, 0):
        case ENTRANCE(SNOWHEAD_TEMPLE, 0):
        case ENTRANCE(GREAT_BAY_TEMPLE, 0):
        case ENTRANCE(STONE_TOWER_TEMPLE_INVERTED, 0):
        case ENTRANCE(STONE_TOWER_TEMPLE, 0):
        case ENTRANCE(SWAMP_SPIDER_HOUSE, 0):
        case ENTRANCE(OCEANSIDE_SPIDER_HOUSE, 0):
        case ENTRANCE(BENEATH_THE_WELL, 0):
        case ENTRANCE(BENEATH_THE_WELL, 1):
        case ENTRANCE(IKANA_CASTLE, 3):
        case ENTRANCE(SECRET_SHRINE, 0):
        case ENTRANCE(PIRATES_FORTRESS_EXTERIOR, 0):
            return true;
        default:
            return false;
    }
}

bool KeepsRecord(int16_t scene) {
    return scene == SCENE_SPOT00 ||
           (scene == SCENE_00KEIKOKU && static_cast<uint16_t>(gSaveContext.save.entrance) == kTerminaFieldMoonCrash);
}

Record FromRespawn(const RespawnData& respawn) {
    Record record;
    record.Game = "mm";
    record.Entrance = respawn.entrance;
    record.Room = respawn.roomIndex;
    record.Pos[0] = respawn.pos.x;
    record.Pos[1] = respawn.pos.y;
    record.Pos[2] = respawn.pos.z;
    record.Yaw = respawn.yaw;
    record.Params = respawn.playerParams;
    record.Data = respawn.data;
    record.TempSwitch = respawn.tempSwitchFlags;
    record.Unk18 = respawn.unk_18;
    record.TempCollect = respawn.tempCollectFlags;
    return record;
}

RespawnData ToRespawn(const Record& record) {
    RespawnData respawn{};
    respawn.pos.x = record.Pos[0];
    respawn.pos.y = record.Pos[1];
    respawn.pos.z = record.Pos[2];
    respawn.yaw = record.Yaw;
    respawn.playerParams = record.Params;
    respawn.entrance = record.Entrance;
    respawn.roomIndex = record.Room;
    respawn.data = record.Data;
    respawn.tempSwitchFlags = record.TempSwitch;
    respawn.unk_18 = record.Unk18;
    respawn.tempCollectFlags = record.TempCollect;
    return respawn;
}

void ApplyLocal(PlayState* play, const Record& record) {
    gSaveContext.respawn[RESPAWN_MODE_TOP] = ToRespawn(record);
    gSaveContext.respawnFlag = -6;
    if (play != nullptr) {
        play->nextEntrance = record.Entrance;
    }
}

bool BeginForeign() {
    auto& store = Ensure();
    store.Pending = true;
    Persist();
    Ship::OotmmEntranceMapping mapping;
    mapping.FromGame = Ship::OotmmGame::Mm;
    mapping.From = "MM dungeon entrance return";
    mapping.ToGame = Ship::OotmmGame::Oot;
    mapping.To = "Dungeon Entrance";
    mapping.ToNativeId = kOotmmDungeonRespawnEntrance;
    return OotmmSession_BeginCrossGameTransition(mapping);
}

void OnSceneReady(int16_t scene) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    auto& store = Ensure();
    if (IsDungeonScene(scene)) {
        const Record arrival = FromRespawn(gSaveContext.respawn[RESPAWN_MODE_DOWN]);
        if (!store.Current.has_value() ||
            (IsDungeonEntrance(static_cast<uint16_t>(gSaveContext.save.entrance)) &&
             (store.Current->Game != arrival.Game || (store.Current->Entrance & ~0xF) != (arrival.Entrance & ~0xF)))) {
            store.Current = arrival;
            store.Pending = false;
            Persist();
        }
        return;
    }
    if (KeepsRecord(scene)) {
        return;
    }
    if (store.Current.has_value() || store.Pending) {
        store.Current.reset();
        store.Pending = false;
        Persist();
    }
}

void OnSaveLoad() {
    if (!OotmmSession_IsActive()) {
        return;
    }
    const bool transition = sFirstLoad && OotmmSession_GetState().GetBootConfig().BootEntrance.has_value();
    sFirstLoad = false;
    Load();
    if (sArrivalPending) {
        sArrivalPending = false;
        if (sStore.Current.has_value() && sStore.Current->Game == "mm") {
            sStore.Pending = false;
            Persist();
            ApplyLocal(nullptr, *sStore.Current);
        }
        return;
    }
    if (!transition && (sStore.Current.has_value() || sStore.Pending)) {
        sStore.Current.reset();
        sStore.Pending = false;
        Persist();
    }
}

} // namespace

void OotmmDungeonRespawn_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) { OnSaveLoad(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::BeforeEndOfCycleSave>(
        []() { OotmmDungeonRespawn_Clear(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](s8 sceneId, s8) { OnSceneReady(static_cast<int16_t>(static_cast<uint8_t>(sceneId))); });
}

std::optional<uint16_t> OotmmDungeonRespawn_PrepareArrival() {
    Load();
    if (!sStore.Current.has_value() || sStore.Current->Game != "mm") {
        return std::nullopt;
    }
    sArrivalPending = true;
    return sStore.Current->Entrance;
}

extern "C" int32_t OotmmDungeonRespawn_Available(void) {
    return OotmmSession_IsActive() && Ensure().Current.has_value() ? 1 : 0;
}

extern "C" int32_t OotmmDungeonRespawn_Warp(PlayState* play) {
    if (!OotmmDungeonRespawn_Available() || play == nullptr) {
        return 0;
    }
    const Record record = *sStore.Current;
    if (record.Game != "mm") {
        return BeginForeign() ? 1 : 0;
    }
    ApplyLocal(play, record);
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    return 1;
}

extern "C" int32_t OotmmDungeonRespawn_RedirectRespawn(PlayState* play) {
    if (!OotmmDungeonRespawn_Available() || play == nullptr) {
        return 0;
    }
    const Record record = *sStore.Current;
    if (record.Game != "mm") {
        return BeginForeign() ? 1 : 0;
    }
    ApplyLocal(play, record);
    return 1;
}

extern "C" void OotmmDungeonRespawn_Clear(void) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    auto& store = Ensure();
    if (store.Current.has_value() || store.Pending) {
        store.Current.reset();
        store.Pending = false;
        Persist();
    }
}

extern "C" void OotmmDungeonRespawn_SaveForFaroresWind(void) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    auto& store = Ensure();
    (AdultAge() ? store.FwAdult : store.FwChild) = store.Current;
    Persist();
}

extern "C" void OotmmDungeonRespawn_RestoreForFaroresWind(void) {
    if (!OotmmSession_IsActive()) {
        return;
    }
    auto& store = Ensure();
    store.Current = AdultAge() ? store.FwAdult : store.FwChild;
    store.Pending = false;
    Persist();
}
