#include "OotmmIpc.h"
#include "OotmmDeathLink.h"
#include "OotmmSession.h"
#include <nlohmann/json.hpp>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmItemPresentation.h"
#include "build.h"
#include "global.h"

#include <libultraship/bridge/GameIpcSession.h>
#include <libultraship/bridge/OotmmItemGrant.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <utility>

extern "C" void OotmmEquipment_ResetPuppetTunicTint(int32_t playerId);
extern "C" void OotmmPresence_RefreshPuppetModel(uint16_t playerId);
extern "C" void gfx_texture_cache_clear();

namespace {

Ship::GameIpcSession sSession;
Ship::GameIpcSettings sSettings;
Ship::OotmmInventory sInventory;
uint64_t sSaveRequestId = 0;
std::optional<uint64_t> sPendingSaveRequest;
std::vector<std::pair<std::string, int>> sPendingModelArchives;

void SaveSelectionNotice(const std::string& message) {
    Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGameOverlay()->TextDrawNotification(
        8.0f, true, "%s", message.c_str());
}

void MountNextModelArchive() {
    using Clock = std::chrono::steady_clock;
    static Clock::time_point sMountEnd{};
    static long long sMountMs = -1;
    if (sMountMs >= 0) {
        const auto frameMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - sMountEnd).count();
        SPDLOG_INFO("[OoTMM] Model mount took {} ms, next frame {} ms", sMountMs, frameMs);
        sMountMs = -1;
    }
    if (sPendingModelArchives.empty()) {
        return;
    }
    const auto start = Clock::now();
    const auto [path, player] = sPendingModelArchives.front();
    sPendingModelArchives.erase(sPendingModelArchives.begin());
    const auto resources = Ship::Context::GetInstance()->GetResourceManager();
    if (resources->GetArchiveManager()->AddArchive(path) == nullptr) {
        SPDLOG_ERROR("[OoTMM] Could not mount model archive {}", path);
        return;
    }
    char mask[16];
    std::snprintf(mask, sizeof(mask), "p%02xobjs/*", player & 0xFF);
    resources->UnloadResources(std::string(mask));
    resources->UnloadResources("alt/" + std::string(mask));
    gfx_texture_cache_clear();
    OotmmEquipment_ResetPuppetTunicTint(player);
    OotmmPresence_RefreshPuppetModel(static_cast<uint16_t>(player));
    sMountEnd = Clock::now();
    sMountMs = std::chrono::duration_cast<std::chrono::milliseconds>(sMountEnd - start).count();
}

} // namespace

void OotmmIpc_InitHooks() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
        []() { MountNextModelArchive(); });
}

extern "C" int gOotmmPvpEnabled = 0;
extern "C" int gOotmmShowNames = 1;
extern "C" int gOotmmGameSpeedPercent = 100;
extern "C" int gOotmmGameSpeedSmooth = 0;
extern "C" int gOotmmSteadyDeityBeams = 1;
extern "C" int gOotmmStopTransformSlide = 1;

void OotmmIpc_Init() {
    sInventory.Reset();
    sPendingSaveRequest.reset();
    sSession.Start("mm", gBuildVersion);
}

void OotmmIpc_Pump() {
    OotmmItemPresentation_Update();
    if (sSession.Pump(sSettings, &sInventory)) {
        gOotmmGameSpeedPercent = sSettings.SpeedPercent;
        gOotmmGameSpeedSmooth = sSettings.SpeedSmooth ? 1 : 0;
        gOotmmPvpEnabled = sSettings.Pvp ? 1 : 0;
        gOotmmShowNames = sSettings.ShowNames ? 1 : 0;
        gOotmmSteadyDeityBeams = sSettings.SteadyDeityBeams ? 1 : 0;
        gOotmmStopTransformSlide = sSettings.StopTransformSlide ? 1 : 0;
        sSession.AcknowledgeSettings(sSettings.Revision);
    }
    if (const auto saveRequest = sSession.ConsumeSaveRequest()) {
        sSession.SendSaveCompleted(*saveRequest, OotmmSession_SaveForLauncher());
    }
    if (sSettings.ZTargetHold >= 0 && gSaveContext.options.zTargetSetting != sSettings.ZTargetHold) {
        gSaveContext.options.zTargetSetting = static_cast<u8>(sSettings.ZTargetHold);
    }
    std::string response;
    while (sSession.TryReceive(response)) {
        auto parsed = nlohmann::json::parse(response, nullptr, false);
        if (parsed.is_object() && parsed.contains("type") && parsed["type"] == "deathLink") {
            OotmmDeathLink_Receive();
            continue;
        }
        if (parsed.is_object() && parsed.contains("type") && parsed["type"] == "mountModelArchive") {
            sPendingModelArchives.emplace_back(parsed.value("path", ""), parsed.value("player", 0));
            continue;
        }
        if (!parsed.is_object() || !parsed.contains("type") || parsed["type"] != "nativeSaveResult" ||
            !parsed.contains("version") || parsed["version"] != 1 || !sPendingSaveRequest ||
            !parsed.contains("requestId") || !parsed["requestId"].is_number_unsigned() ||
            parsed["requestId"].get<uint64_t>() != *sPendingSaveRequest ||
            !parsed.contains("accepted") || !parsed["accepted"].is_boolean()) {
            continue;
        }
        if (!parsed["accepted"].get<bool>()) {
            sPendingSaveRequest.reset();
            SaveSelectionNotice(parsed.contains("reason") && parsed["reason"].is_string()
                ? parsed["reason"].get<std::string>() : "Save selection was rejected. Please retry.");
        }
    }
    if (sPendingSaveRequest && !sSession.IsConnected()) {
        sPendingSaveRequest.reset();
        SaveSelectionNotice("Launcher disconnected. Reconnect before selecting another save.");
    }
    Ship::GameIpcItemPresentation presentation;
    while (sSession.TryPopItemPresentation(presentation)) {
        OotmmItemPresentation_Queue(std::move(presentation));
    }
}

void OotmmIpc_Shutdown() {
    sSession.Stop();
    sInventory.Reset();
}

bool OotmmIpc_SendCrossGameTransition(const Ship::OotmmEntranceMapping& mapping,
                                      std::optional<uint32_t> ootAge) {
    return sSession.RequestCrossGameTransition(
        "mm", mapping.ToGame == Ship::OotmmGame::Oot ? "oot" : "mm", mapping.To, mapping.ToNativeId, ootAge);
}

bool OotmmIpc_ConsumeTransitionAccepted() {
    return sSession.ConsumeTransitionAccepted();
}

const Ship::GameIpcSettings& OotmmIpc_GetSettings() {
    return sSettings;
}

const Ship::OotmmInventory& OotmmIpc_GetInventory() {
    return sInventory;
}

bool OotmmIpc_SetDebugItemValue(const std::string& itemId, uint32_t value) {
    return sSession.SetDebugInventoryValue(itemId, value);
}

bool OotmmIpc_RequestDebugItemPresentation(const std::string& itemId, uint32_t recipientPlayer) {
    return sSession.RequestDebugItemPresentation(itemId, recipientPlayer);
}

bool OotmmIpc_TryPopItemPresentation(Ship::GameIpcItemPresentation& presentation) {
    return sSession.TryPopItemPresentation(presentation);
}

bool OotmmIpc_IsConnected() {
    return sSession.IsConnected();
}

bool OotmmIpc_PresenceActive() {
    return sSession.PresenceActive();
}

bool OotmmIpc_SendPlayerPose(const Ship::OotmmPlayerPose& pose) {
    return sSession.SendPlayerPose(pose);
}

bool OotmmIpc_SendPvpHit(const Ship::OotmmPvpHit& hit) {
    return sSession.SendPvpHit(hit);
}

bool OotmmIpc_SendEndgameReached(const char* boss) {
    return sSession.Send(nlohmann::json{ { "type", "endgameReached" }, { "boss", boss } }.dump());
}

bool OotmmIpc_SendDeathLink() {
    return sSession.Send(nlohmann::json{ { "type", "deathLink" }, { "death", nlohmann::json::object() } }.dump());
}

bool OotmmIpc_TakeRemotePresence(std::vector<Ship::OotmmPlayerPose>& poses,
                                 std::vector<Ship::OotmmPvpHit>& hits) {
    return sSession.TakeRemotePresence(poses, hits);
}

bool OotmmIpc_SendRenewalGrant(const std::string& itemId) {
    if (!sSession.Send(nlohmann::json{ { "type", "renewalGrant" }, { "itemId", itemId } }.dump())) {
        return false;
    }
    if (itemId.starts_with("OOT_")) {
        const auto& state = OotmmSession_GetState();
        Ship::OotmmItemGrant::NoteRemoteGrant(itemId, state.GetBootConfig().StatePath + ".vitals.oot",
                                              state.GetBoolSetting("sharedMagic", false));
    }
    return true;
}

bool OotmmIpc_SendCheckCollected(const std::string& checkId, bool present) {
    return sSession.SendCheckCollected("mm", checkId, present);
}

bool OotmmIpc_SendEntranceTaken(uint32_t source) {
    return sSession.SendEntranceTaken(source);
}

bool OotmmIpc_SendLocation(int scene, uint32_t entrance) {
    return sSession.SendLocation(scene, entrance);
}

bool OotmmIpc_IsCheckPresentationPending(const std::string& checkId) {
    return sSession.IsCheckPresentationPending("mm:" + checkId);
}

void OotmmIpc_CompleteCheckPresentation(const std::string& checkKey) {
    sSession.CompleteCheckPresentation(checkKey);
}

bool OotmmIpc_IsCheckCompleted(const std::string& checkId) {
    return sSession.IsCheckCompleted("mm", checkId);
}

uint64_t OotmmIpc_CompletedChecksRevision() {
    return sSession.CompletedChecksRevision();
}

extern "C" int32_t OotmmSession_FileSelectionPending(void) {
    return sPendingSaveRequest.has_value();
}

extern "C" int32_t OotmmSession_RequestFileSelection(int32_t slot) {
    if (!OotmmSession_IsActive()) {
        return 1;
    }
    const auto& boot = OotmmSession_GetState().GetBootConfig();
    if (slot >= 0 && slot < 3 && boot.LogicalSlot == static_cast<uint32_t>(slot)) {
        return 1;
    }
    if (slot < 0 || slot > 2 || !boot.LogicalSlot || *boot.LogicalSlot > 2 || boot.SaveId.empty()) {
        SaveSelectionNotice("Cannot select this save without a valid launcher slot binding.");
        return 0;
    }
    if (sPendingSaveRequest) {
        return 0;
    }
    if (!sSession.IsConnected() || sSaveRequestId >= 9007199254740991ULL) {
        SaveSelectionNotice("Connect to the launcher before selecting another save.");
        return 0;
    }
    const auto id = ++sSaveRequestId;
    const auto packet = nlohmann::json{{"type", "nativeSaveRequest"}, {"version", 1},
        {"requestId", id}, {"operation", "select"}, {"saveId", boot.SaveId},
        {"activeSlot", *boot.LogicalSlot}, {"sourceSlot", *boot.LogicalSlot}, {"targetSlot", slot}};
    if (sSession.Send(packet.dump())) {
        sPendingSaveRequest = id;
        SaveSelectionNotice("Opening selected save through the launcher...");
    } else {
        SaveSelectionNotice("Could not request the selected save. Please retry.");
    }
    return 0;
}

extern "C" int32_t OotmmSession_RequestFileCopy(int32_t source, int32_t target) {
    if (!OotmmSession_IsActive()) {
        return 1;
    }
    const auto& boot = OotmmSession_GetState().GetBootConfig();
    if (sPendingSaveRequest) {
        return 0;
    }
    if (source < 0 || source > 2 || target < 0 || target > 2 || source == target ||
        !boot.LogicalSlot || *boot.LogicalSlot > 2 || boot.SaveId.empty() ||
        !sSession.IsConnected() || sSaveRequestId >= 9007199254740991ULL) {
        SaveSelectionNotice("Connect to the launcher with a valid save binding before copying.");
        return 0;
    }
    const auto id = ++sSaveRequestId;
    const auto packet = nlohmann::json{{"type", "nativeSaveRequest"}, {"version", 1},
        {"requestId", id}, {"operation", "copy"}, {"saveId", boot.SaveId},
        {"activeSlot", *boot.LogicalSlot}, {"sourceSlot", source}, {"targetSlot", target}};
    if (sSession.Send(packet.dump())) {
        sPendingSaveRequest = id;
        SaveSelectionNotice("Copying both games' saves through the launcher...");
    } else {
        SaveSelectionNotice("Could not request the save copy. Please retry.");
    }
    return 0;
}
