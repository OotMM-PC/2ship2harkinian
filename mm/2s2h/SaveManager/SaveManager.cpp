#include <libultraship/bridge/OotmmAppliedLedger.h>
#include <libultraship/bridge/OotmmSaveRecovery.h>
#include "SaveManager.h"

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <optional>
#include <map>
#include <set>
#include <string_view>
#include <atomic>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "BenJsonConversions.hpp"
#include "BenPort.h"
#include "2s2h/OotmmCheckLedger.h"
#include "2s2h/OotmmItemApply.h"
#include "2s2h/OotmmSession.h"
#include <ship/window/Window.h>
#include <ship/utils/AtomicFile.h>

extern "C" {
#include "z64save.h"
#include "macros.h"
#include "src/overlays/gamestates/ovl_file_choose/z_file_select.h"
extern FileSelectState* gFileSelectState;
}

// This entire thing is temporary until we have a more robust save system that
// supports backwards compatibility, migrations, threaded saving, save sections, etc.

#define FLASH_SAVE_UNAVAILABLE ((FlashSave)-1)

#undef GET_NEWF

#define GET_NEWF(save, index) (save.saveInfo.playerData.newf[index])

#define IS_VALID_FILE(save)                                                                    \
    ((GET_NEWF(save, 0) == 'Z') && (GET_NEWF(save, 1) == 'E') && (GET_NEWF(save, 2) == 'L') && \
     (GET_NEWF(save, 3) == 'D') && (GET_NEWF(save, 4) == 'A') && (GET_NEWF(save, 5) == '3'))

namespace {

std::atomic<bool> sCopyActive{false};
std::atomic<bool> sCopyFailed{false};
bool sCopyRecoveryNotified = false;

struct CopyOriginal {
    std::optional<std::string> bytes;
    bool written = false;
};
std::unordered_map<std::string, CopyOriginal> sCopyOriginals;

bool CaptureCopyOriginal(const std::filesystem::path& path) {
    if (sCopyOriginals.contains(path.string())) {
        return true;
    }
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return false;
    }
    CopyOriginal original;
    if (exists) {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            return false;
        }
        original.bytes = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (input.bad()) {
            return false;
        }
    }
    sCopyOriginals.emplace(path.string(), std::move(original));
    return true;
}

bool RestoreFailedCopy() {
    bool restored = true;
    for (auto& [path, original] : sCopyOriginals) {
        if (!original.written) {
            continue;
        }
        std::error_code error;
        bool success;
        if (original.bytes) {
            success = Ship::WriteFileAtomically(path, *original.bytes, error);
        } else {
            std::filesystem::remove(path, error);
            success = !error;
        }
        if (success) {
            original.written = false;
        } else {
            restored = false;
            SPDLOG_ERROR("[OoTMM] Failed to restore cancelled copy destination {}: {}", path, error.message());
        }
    }
    if (restored) {
        sCopyOriginals.clear();
        sCopyRecoveryNotified = false;
    } else if (!sCopyRecoveryNotified) {
        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        gui->GetGameOverlay()->TextDrawNotification(30.0f, true,
            "Copy recovery is blocked. Close programs using the save files, then retry or cancel the copy.");
        sCopyRecoveryNotified = true;
    }
    return restored;
}


struct ReadSnapshot {
    std::vector<uint8_t> native;
    std::string section;
    nlohmann::json applied;
    nlohmann::json checks;
    nlohmann::json shared;
};
std::unordered_map<const void*, ReadSnapshot> sReadSnapshots;

void RememberReadSnapshot(void* buffer, size_t size, const char* section,
                          const nlohmann::json& save, const std::string& fileName) {
    if (!OotmmSession_IsActive() || gFileSelectState == nullptr) {
        return;
    }
    if (sReadSnapshots.size() >= 64) {
        sReadSnapshots.clear();
    }
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    sReadSnapshots[buffer] = {
        std::vector<uint8_t>(bytes, bytes + size), section,
        save[section].contains("ootmmApplied") ? save[section]["ootmmApplied"]
            : OotmmItemApply_LegacySnapshot(fileName.at(4) - '1'),
        save.value("ootmmChecks", nlohmann::json::array()),
        save[section].value("ootmmSharedResources", nlohmann::json(nullptr))
    };
}

std::filesystem::path SavesFolderPath() {
    std::filesystem::path result(Ship::Context::GetPathRelativeToAppDirectory("saves", appShortName));
    const auto& state = OotmmSession_GetState();
    if (!state.IsActive() || !state.HasSeed()) {
        return result;
    }
    if (!state.GetBootConfig().NativeSaveSubdir.empty()) {
        result /= state.GetBootConfig().NativeSaveSubdir;
    }
    return result / ("ootmm-" + state.GetNativeSaveTag());
}

} // namespace

// Migrations
// The idea here is that we can read in any version of the save as generic JSON, then apply migrations
// to the JSON to ensure it's in the correct shape for the current to_json/from_json helpers to convert
// it to the current struct that the game uses.
//
// To add a new migration:
// - Increment CURRENT_SAVE_VERSION
// - Create the migration file in the Migrations folder with the name `{CURRENT_SAVE_VERSION}.cpp`
// - Add the migration function definition below and add it to the `migrations` map with the key being the previous
// version
const uint32_t CURRENT_SAVE_VERSION = 7;

void SaveManager_Migration_1(nlohmann::json& j);
void SaveManager_Migration_2(nlohmann::json& j);
void SaveManager_Migration_3(nlohmann::json& j);
void SaveManager_Migration_4(nlohmann::json& j);
void SaveManager_Migration_5(nlohmann::json& j);
void SaveManager_Migration_6(nlohmann::json& j);
void SaveManager_Migration_7(nlohmann::json& j);

const std::unordered_map<uint32_t, std::function<void(nlohmann::json&)>> migrations = {
    // Pre-1.0.0 Migrations, deprecated
    { 0, SaveManager_Migration_1 },
    { 1, SaveManager_Migration_2 },
    { 2, SaveManager_Migration_3 },
    { 3, SaveManager_Migration_4 },
    // Base Migration
    { 4, SaveManager_Migration_5 },
    { 5, SaveManager_Migration_6 },
    { 6, SaveManager_Migration_7 },
};

int SaveManager_MigrateSave(nlohmann::json& j) {
    try {
        int version = j.value("version", 0);

        if (version > (int)CURRENT_SAVE_VERSION) {
            SPDLOG_ERROR("Save version is greater than current version");
            return -1;
        }

        if (version >= 4 && !j.contains("newCycleSave") && !j.contains("owlSave")) {
            SPDLOG_ERROR("Save file is missing newCycleSave and owlSave");
            return -1;
        }

        while (version < (int)CURRENT_SAVE_VERSION) {
            if (migrations.contains(version)) {
                auto migration = migrations.at(version);
                if (version < 4) {
                    migration(j); // Pre-1.0.0 Migrations, deprecated
                } else {
                    // In the case of copying files, the owl save is copied first, so the new cycle may not exist yet.
                    if (j.contains("newCycleSave")) {
                        migration(j["newCycleSave"]);
                    }
                    // Only migrate the owl save if it exists
                    if (j.contains("owlSave")) {
                        migration(j["owlSave"]);
                    }
                }
            }
            version = j["version"] = version + 1;
        }
        return 0;
    } catch (std::exception& e) {
        SPDLOG_ERROR("Failed to migrate save file: {}", e.what());
        return -1;
    } catch (...) {
        SPDLOG_ERROR("Failed to migrate save file");
        return -1;
    }
}

bool SaveManager_ValidateOotmmRecovery(const std::string& contents, const std::string& saveId, uint32_t slot,
                                      nlohmann::json& validated, std::string& error) {
    validated = nullptr;
    error = "Invalid MM recovery payload or identity.";
    if (contents.size() > 8 * 1024 * 1024 || saveId.empty() || slot > 2) {
        return false;
    }
    try {
        bool unique = true;
        std::vector<std::set<std::string>> keys;
        auto candidate = nlohmann::json::parse(contents, [&](int depth, nlohmann::json::parse_event_t event,
                                                            nlohmann::json& value) {
            if (depth > 64) {
                unique = false;
            }
            if (event == nlohmann::json::parse_event_t::object_start) {
                keys.emplace_back();
            } else if (event == nlohmann::json::parse_event_t::key) {
                if (keys.empty() || !keys.back().insert(value.get<std::string>()).second) {
                    unique = false;
                }
            } else if (event == nlohmann::json::parse_event_t::object_end && !keys.empty()) {
                keys.pop_back();
            }
            return true;
        });
        if (!unique || !candidate.is_object() || SaveManager_MigrateSave(candidate) != 0) {
            return false;
        }
        size_t envelopes = 0;
        std::map<uint64_t, nlohmann::json> revisions;
        for (const auto* name : {"newCycleSave", "owlSave"}) {
            if (!candidate.contains(name)) {
                continue;
            }
            ++envelopes;
            const auto& section = candidate.at(name);
            Save native{};
            if (std::string_view(name) == "owlSave") {
                SaveContext context = section.get<SaveContext>();
                native = context.save;
            } else {
                native = section.at("save").get<Save>();
            }
            if (!IS_VALID_FILE(native)) {
                error = "MM recovery save has invalid file magic.";
                return false;
            }
            if (section.contains("ootmmApplied")) {
                Ship::OotmmAppliedLedger applied;
                if (!applied.RestoreSnapshot(section.at("ootmmApplied"))) {
                    error = "MM recovery save has invalid applied-item counters.";
                    return false;
                }
            }
            if (section.contains("ootmmSharedResources")) {
                const auto snapshot = Ship::OotmmSharedResources::Parse(
                    section.at("ootmmSharedResources").dump(), saveId, slot);
                if (!snapshot) {
                    error = "MM recovery resources do not match this save and slot.";
                    return false;
                }
                const auto body = snapshot->Json();
                const auto [found, inserted] = revisions.emplace(snapshot->Revision, body);
                if (!inserted && found->second != body) {
                    error = "MM recovery resources contain conflicting revisions.";
                    return false;
                }
            }
        }
        if (envelopes == 0) {
            return false;
        }
        validated = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string("MM recovery save cannot be loaded: ") + exception.what();
        return false;
    }
}


extern "C" void SaveManager_RunValidationCommand(int argc, char** argv) {
    if (argc < 2 || std::string_view(argv[1]) != "--ootmm-validate-save") {
        return;
    }
    spdlog::set_level(spdlog::level::off);
    bool valid = false;
    std::string error = "Invalid MM validation request.";
    try {
        if (argc != 2) {
            throw std::runtime_error(error);
        }
        std::string input;
        char buffer[16384];
        while (std::cin.read(buffer, sizeof(buffer)) || std::cin.gcount() > 0) {
            if (input.size() + static_cast<size_t>(std::cin.gcount()) > 50 * 1024 * 1024) {
                throw std::runtime_error("MM validation request is too large.");
            }
            input.append(buffer, static_cast<size_t>(std::cin.gcount()));
        }
        if (!std::cin.eof()) {
            throw std::runtime_error("MM validation request could not be read.");
        }
        bool unique = true;
        std::set<std::string> keys;
        const auto request = nlohmann::json::parse(input, [&](int depth, nlohmann::json::parse_event_t event,
                                                            nlohmann::json& value) {
            if (depth > 2 || (event == nlohmann::json::parse_event_t::key &&
                              !keys.insert(value.get<std::string>()).second)) {
                unique = false;
            }
            return true;
        });
        if (!unique || !request.is_object() || request.size() != 4 || !request.contains("version") ||
            !request.at("version").is_number_unsigned() || request.at("version") != 1 ||
            !request.contains("saveId") || !request.at("saveId").is_string() ||
            !request.contains("slot") || !request.at("slot").is_number_unsigned() || request.at("slot") > 2 ||
            !request.contains("contents") || !request.at("contents").is_string()) {
            throw std::runtime_error(error);
        }
        const auto saveId = request.at("saveId").get<std::string>();
        if (saveId.size() != 32 || saveId.find_first_not_of("0123456789abcdef") != std::string::npos) {
            throw std::runtime_error(error);
        }
        nlohmann::json validated;
        valid = SaveManager_ValidateOotmmRecovery(request.at("contents").get<std::string>(), saveId,
                                                request.at("slot").get<uint32_t>(), validated, error);
    } catch (const std::exception& exception) {
        error = exception.what();
    }
    if (error.size() > 1024) {
        error.resize(1024);
    }
    const auto response = nlohmann::json{{"version", 1}, {"valid", valid}, {"reason", error}}.dump();
    std::cout << response << std::endl;
    std::exit(valid ? EXIT_SUCCESS : EXIT_FAILURE);
}

bool SaveManager_ValidateCurrentOotmmSave(std::string& error) {
    const auto& boot = OotmmSession_GetState().GetBootConfig();
    if (!boot.LogicalSlot.has_value()) {
        return true;
    }
    std::optional<std::string> contents;
    if (!Ship::ReadRecoveryBytes(SavesFolderPath() / SaveManager_GetFileName(*boot.LogicalSlot + 1), contents, error)) {
        return false;
    }
    if (!contents) {
        return true;
    }
    nlohmann::json validated;
    return SaveManager_ValidateOotmmRecovery(*contents, boot.SaveId, *boot.LogicalSlot, validated, error);
}

bool SaveManager_RecoverOotmmBackup(const std::optional<std::string>& expectedPrimary,
                                   const std::string& expectedBackup, std::string& error) {
    const auto& state = OotmmSession_GetState();
    const auto& boot = state.GetBootConfig();
    if (!state.IsActive() || !state.HasSeed() || !boot.LogicalSlot.has_value()) {
        error = "MM backup recovery requires an active bound save.";
        return false;
    }
    nlohmann::json validated;
    if (!SaveManager_ValidateOotmmRecovery(expectedBackup, boot.SaveId, *boot.LogicalSlot, validated, error)) {
        return false;
    }
    const auto primary = SavesFolderPath() / SaveManager_GetFileName(*boot.LogicalSlot + 1);
    const auto backup = SavesFolderPath() / SaveManager_GetFileName(*boot.LogicalSlot + 1, true);
    return Ship::RestoreNativeBackup(primary, backup, expectedPrimary, expectedBackup, error);
}

bool SaveManager_WriteSaveFile(const std::filesystem::path& fileName, nlohmann::json j) {
    static bool failureNotified = false;
    const std::filesystem::path folder = SavesFolderPath();
    const std::filesystem::path filePath = folder / fileName;

    try {
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        const bool copyDestination = sCopyActive.load() && gFileSelectState != nullptr &&
            (fileName == SaveManager_GetFileName(gFileSelectState->copyDestFileIndex + 1) ||
             fileName == SaveManager_GetFileName(gFileSelectState->copyDestFileIndex + 1, true));
        if (!error && copyDestination && !CaptureCopyOriginal(filePath)) {
            return false;
        }
        if (error || !Ship::WriteFileAtomically(filePath, j.dump(4) + "\n", error)) {
            SPDLOG_ERROR("Failed to write save file {}: {}. Previous save preserved.",
                         filePath.string(), error.message());
            if (!failureNotified) {
                auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
                gui->GetGameOverlay()->TextDrawNotification(30.0f, true,
                    "Save failed. The previous save has been preserved. See the log for details.");
                failureNotified = true;
            }
            return false;
        }
        if (copyDestination) {
            sCopyOriginals.at(filePath.string()).written = true;
        }
        failureNotified = false;
        return true;
    } catch (const std::exception& error) {
        SPDLOG_ERROR("Failed to write save file {}: {}", filePath.string(), error.what());
        return false;
    }
}

void SaveManager_DeleteSaveFile(const std::filesystem::path& fileName) {
    const std::filesystem::path filePath = SavesFolderPath() / fileName;

    const bool copying = sCopyActive.load();
    if (copying && sCopyFailed.load()) {
        return;
    }
    try {
        if (copying && !CaptureCopyOriginal(filePath)) {
            sCopyFailed.store(true);
            return;
        }
        if (std::filesystem::exists(filePath) && std::filesystem::remove(filePath) && copying) {
            sCopyOriginals.at(filePath.string()).written = true;
        }
    } catch (...) {
        if (copying) {
            sCopyFailed.store(true);
        }
        SPDLOG_ERROR("Failed to delete save file");
    }
}

int SaveManager_ReadSaveFile(const std::filesystem::path& fileName, nlohmann::json& j) {
    const std::filesystem::path filePath = SavesFolderPath() / fileName;

    if (!std::filesystem::exists(filePath)) {
        return -1;
    }

    try {
        std::ifstream i(filePath);
        i >> j;
        i.close();
        return 0;
    } catch (...) {
        SPDLOG_ERROR("Failed to read save file");
        return -2;
    }
}

void SaveManager_MoveInvalidSaveFile(const std::filesystem::path& fileName, const std::string& message) {
    const std::filesystem::path folder = SavesFolderPath();
    const std::filesystem::path filePath = folder / fileName;
    const std::filesystem::path backupFilePath =
        folder / (fileName.stem().string() + "_invalid_" + std::to_string(std::time(nullptr)) + ".json");

    try {
        if (std::filesystem::exists(filePath)) {
            std::filesystem::rename(filePath, backupFilePath);
        }

        SPDLOG_INFO("{}", message.c_str());
        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        gui->GetGameOverlay()->TextDrawNotification(30.0f, true, message.c_str());
    } catch (...) { SPDLOG_ERROR("Failed to move invalid save file"); }
}

int SaveManager_GetOpenFileSlot() {
    std::string fileName = "file1.json";
    const std::filesystem::path folder = SavesFolderPath();
    if (!std::filesystem::exists(folder / fileName)) {
        return 1;
    }

    fileName = "file2.json";
    if (!std::filesystem::exists(folder / fileName)) {
        return 2;
    }

    fileName = "file3.json";
    if (!std::filesystem::exists(folder / fileName)) {
        return 3;
    }

    return -1;
}

FlashSave SaveManager_GetFlashSaveFromPages(u32 pageNum, u32 pageCount) {
    FlashSave flashSave = FLASH_SAVE_UNAVAILABLE;

    for (u32 i = 0; i < FLASH_SAVE_MAX; i++) {
        // Verify that the requested pages align with expected values
        if (pageNum == (u32)gFlashSaveStartPages[i] &&
            (pageCount == (u32)gFlashSaveNumPages[i] || pageCount == (u32)gFlashSpecialSaveNumPages[i])) {
            flashSave = static_cast<FlashSave>(i);
            break;
        }
    }

    return flashSave;
}

std::string SaveManager_GetFileName(int fileNum, bool isBackup) {
    return "file" + std::to_string(fileNum) + (isBackup ? "backup" : "") + ".json";
}

std::string SaveManager_GetFileNameFromFlashSave(FlashSave flashSave) {
    if (flashSave == FLASH_SAVE_UNAVAILABLE)
        return "invalid";

    if (flashSave == FLASH_SAVE_SRAM_HEADER || flashSave == FLASH_SAVE_SRAM_HEADER_BACKUP) {
        return "global.json";
    }

    bool isBackup =
        flashSave == FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE_BACKUP || flashSave == FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP ||
        flashSave == FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE_BACKUP || flashSave == FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP ||
        flashSave == FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE_BACKUP || flashSave == FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP;

    int fileNum = -1;
    switch (flashSave) {
        case FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_1_OWL_SAVE:
        case FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP:
            fileNum = 1;
            break;
        case FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_2_OWL_SAVE:
        case FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP:
            fileNum = 2;
            break;
        case FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_3_OWL_SAVE:
        case FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP:
            fileNum = 3;
            break;
        default:
            break;
    }

    bool isOwlSave = flashSave == FLASH_SAVE_FILE_1_OWL_SAVE || flashSave == FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP ||
                     flashSave == FLASH_SAVE_FILE_2_OWL_SAVE || flashSave == FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP ||
                     flashSave == FLASH_SAVE_FILE_3_OWL_SAVE || flashSave == FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP;

    return "file" + std::to_string(fileNum) + (isBackup ? "backup" : "") + ".json";
}

bool SaveManager_HandleFileDropped(char* filePath) {
    try {
        std::ifstream fileStream(filePath);

        if (!fileStream.is_open()) {
            return false;
        }

        // Check if first byte is "{"
        if (fileStream.peek() != '{') {
            return false;
        }

        nlohmann::json j;
        try {
            fileStream >> j;
        } catch (nlohmann::json::exception& e) { return false; }

        if (!j.contains("type") || j["type"] != "2S2H_SAVE") {
            return false;
        }

        int saveSlot = SaveManager_GetOpenFileSlot();
        if (saveSlot == -1) {
            SPDLOG_ERROR("No save slot available");
            auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
            gui->GetGameOverlay()->TextDrawNotification(30.0f, true, "No save slot available");
            return true;
        }

        std::string fileName = SaveManager_GetFileName(saveSlot);

        SaveManager_WriteSaveFile(fileName, j);

        // Reset the file select state to reload the save metadata
        if (gFileSelectState != NULL) {
            STOP_GAMESTATE(&gFileSelectState->state);
            SET_NEXT_GAMESTATE(&gFileSelectState->state, FileSelect_Init, sizeof(FileSelectState));
        }

        SPDLOG_INFO("Successfully imported save into slot {}", saveSlot);
        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        gui->GetGameOverlay()->TextDrawNotification(30.0f, true, "Successfully imported save into slot %d", saveSlot);

        return true;
    } catch (std::exception& e) {
        SPDLOG_ERROR("Failed to load file: {}", e.what());
        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        gui->GetGameOverlay()->TextDrawNotification(30.0f, true, "Failed to load file");
        return false;
    } catch (...) {
        SPDLOG_ERROR("Failed to load file");
        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        gui->GetGameOverlay()->TextDrawNotification(30.0f, true, "Failed to load file");
        return false;
    }
}

static s32 WriteSaveData(u8* saveBuffer, u32 pageNum, u32 pageCount, bool initialOotmmSave) {
    FlashSave flashSave = SaveManager_GetFlashSaveFromPages(pageNum, pageCount);
    std::string fileName = SaveManager_GetFileNameFromFlashSave(flashSave);

    bool isBackup = false;
    const bool copying = OotmmSession_IsActive() && gFileSelectState != nullptr &&
        (gFileSelectState->configMode == CM_COPY_CONFIRM ||
         gFileSelectState->configMode == CM_COPY_WAIT_FOR_FLASH_SAVE) &&
        (fileName == SaveManager_GetFileName(gFileSelectState->copyDestFileIndex + 1) ||
         fileName == SaveManager_GetFileName(gFileSelectState->copyDestFileIndex + 1, true));
    nlohmann::json copySource;
    if (copying && SaveManager_ReadSaveFile(
            SaveManager_GetFileName(gFileSelectState->selectedFileIndex + 1), copySource) != 0) {
        SPDLOG_ERROR("[OoTMM] Cannot read source metadata for save copy");
        return -1;
    }
    if (copying) {
        for (const char* section : { "newCycleSave", "owlSave" }) {
            if (copySource.contains(section) && !copySource[section].contains("ootmmApplied")) {
                copySource[section]["ootmmApplied"] =
                    OotmmItemApply_LegacySnapshot(gFileSelectState->selectedFileIndex);
            }
            if (copySource.contains(section)) {
                copySource[section].erase("ootmmSharedResources");
                Ship::OotmmAppliedLedger validator;
                if (!validator.RestoreSnapshot(copySource[section]["ootmmApplied"])) {
                    SPDLOG_ERROR("[OoTMM] Invalid source applied snapshot during copy");
                    return -1;
                }
            }
        }
    }
    const auto embedMetadata = [&](nlohmann::json& save, const char* section) {
        if (!copying) {
            const auto read = sReadSnapshots.find(saveBuffer);
            if (OotmmSession_IsActive() && gFileSelectState != nullptr && read != sReadSnapshots.end() &&
                read->second.section == section &&
                std::memcmp(saveBuffer, read->second.native.data(), read->second.native.size()) == 0) {
                Ship::OotmmAppliedLedger validator;
                if (!validator.RestoreSnapshot(read->second.applied)) {
                    SPDLOG_ERROR("[OoTMM] Invalid source applied snapshot during repair");
                    return false;
                }
                save[section]["ootmmApplied"] = read->second.applied;
                save["ootmmChecks"] = read->second.checks;
                if (!read->second.shared.is_null()) {
                    save[section]["ootmmSharedResources"] = read->second.shared;
                } else {
                    save[section].erase("ootmmSharedResources");
                }
                return true;
            }
            if (initialOotmmSave) {
                save[section]["ootmmApplied"] = nlohmann::json::object();
                const auto& incoming = OotmmSession_GetState().GetBootConfig().SharedResources;
                if (incoming) {
                    save[section]["ootmmSharedResources"] = incoming->Json();
                }
            } else if (!OotmmItemApply_Embed(save[section])) {
                return false;
            }
            OotmmCheckLedger_Embed(save);
            return true;
        }
        if (!copySource.contains(section)) {
            SPDLOG_ERROR("[OoTMM] Source save is missing {} during copying", section);
            return false;
        }
        save[section]["ootmmApplied"] = copySource[section].contains("ootmmApplied")
            ? copySource[section]["ootmmApplied"]
            : OotmmItemApply_LegacySnapshot(gFileSelectState->selectedFileIndex);
        save[section].erase("ootmmSharedResources");
        save.erase("ootmmChecks");
        if (copySource.contains("ootmmChecks")) {
            save["ootmmChecks"] = copySource["ootmmChecks"];
        }
        return true;
    };

    if (flashSave == FLASH_SAVE_UNAVAILABLE) {
        return -1;
    }

    if (flashSave == FLASH_SAVE_SRAM_HEADER || flashSave == FLASH_SAVE_SRAM_HEADER_BACKUP) {
        SaveOptions saveOptions;
        memcpy(&saveOptions, saveBuffer, sizeof(SaveOptions));

        return SaveManager_WriteSaveFile(fileName, saveOptions) ? 0 : -1;
    }

    // A new cycle save with the "special" page count means that both the regular slot and the backup slot should be
    // saved together. We replicate that here by running the save again on the matching backup slot.
    // Note: This is not accounting for the sram header writing a disk backup. It does not feel important to do so.
    // If we ever feel like we want a global save backup, then we just need to add it to this condition.
    if ((flashSave == FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE || flashSave == FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE ||
         flashSave == FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE) &&
        pageCount == (u32)gFlashSpecialSaveNumPages[flashSave] && !copying) {
        SaveManager_SysFlashrom_WriteData(saveBuffer, gFlashSaveStartPages[flashSave + 1],
                                          gFlashSaveNumPages[flashSave + 1]);
    }

    switch (flashSave) {
        case FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE_BACKUP:
            isBackup = true;
            // fallthrough
        case FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE: {
            Save save;
            memcpy(&save, saveBuffer, sizeof(Save));

            if (IS_VALID_FILE(save)) {
                nlohmann::json j;

                // Read the existing save file to preserve the owl save
                int result = SaveManager_ReadSaveFile(fileName, j);
                if (result == -2) {
                    SaveManager_MoveInvalidSaveFile(
                        fileName,
                        "Something went wrong trying to preserve the owl save. Original save file has been backed up.");
                } else if (result == 0) {
                    result = SaveManager_MigrateSave(j);

                    if (result != 0) {
                        SaveManager_MoveInvalidSaveFile(
                            fileName, "Failed to migrate owl save. Original save file has been backed up.");
                    }
                }

                if (copying) {
                    j = copySource;
                }
                j["newCycleSave"]["save"] = save;
                if (!embedMetadata(j, "newCycleSave")) {
                    return -1;
                }
                j["version"] = CURRENT_SAVE_VERSION;
                j["type"] = "2S2H_SAVE";

                if (!SaveManager_WriteSaveFile(fileName, j)) {
                    return -1;
                }
                if (copying && !isBackup && pageCount == (u32)gFlashSpecialSaveNumPages[flashSave]) {
                    return SaveManager_SysFlashrom_WriteData(saveBuffer, gFlashSaveStartPages[flashSave + 1],
                                                            gFlashSaveNumPages[flashSave + 1]);
                }
                return 0;
            } else {
                // If IS_VALID_FILE fails, we should delete the save file, even if there is an owl save in it, because
                // they just deleted the new cycle save
                SaveManager_DeleteSaveFile(fileName);
            }
            break;
        }
        case FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP:
        case FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP:
        case FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP:
            isBackup = true;
            // fallthrough
        case FLASH_SAVE_FILE_1_OWL_SAVE:
        case FLASH_SAVE_FILE_2_OWL_SAVE:
        case FLASH_SAVE_FILE_3_OWL_SAVE: {
            SaveContext saveContext;
            memcpy(&saveContext, saveBuffer, offsetof(SaveContext, fileNum));

            nlohmann::json j;
            // Read the existing save file to preserve the new cycle save
            int result = SaveManager_ReadSaveFile(fileName, j);
            if (result == -2) {
                SaveManager_MoveInvalidSaveFile(fileName, "Something went wrong trying to preserve the new cycle save. "
                                                          "Original save file has been backed up.");
            } else if (result == 0) {
                result = SaveManager_MigrateSave(j);

                if (result != 0) {
                    SaveManager_MoveInvalidSaveFile(
                        fileName, "Failed to migrate new cycle save. Original save file has been backed up.");
                }
            }

            if (IS_VALID_FILE(saveContext.save)) {
                if (copying) {
                    j = copySource;
                }
                j["owlSave"] = saveContext;
                if (!embedMetadata(j, "owlSave")) {
                    return -1;
                }
                j["version"] = CURRENT_SAVE_VERSION;
                j["type"] = "2S2H_SAVE";

                return SaveManager_WriteSaveFile(fileName, j) ? 0 : -1;
            } else {
                // If IS_VALID_FILE fails, and there is still a new cycle save present, we just want to only remove the
                // owl save and write the new cycle save back
                if (j.contains("newCycleSave")) {
                    if (j.contains("owlSave")) {
                        j.erase("owlSave");
                    }
                    j["version"] = CURRENT_SAVE_VERSION;
                    j["type"] = "2S2H_SAVE";
                    return SaveManager_WriteSaveFile(fileName, j) ? 0 : -1;
                    // If there is no new cycle save, we should just delete the save file
                } else {
                    SaveManager_DeleteSaveFile(fileName);
                }
            }
            break;
        }
        default:
            break;
    }
    return -1;
}

extern "C" s32 SaveManager_SysFlashrom_WriteData(u8* saveBuffer, u32 pageNum, u32 pageCount) {
    if (sCopyActive.load() && sCopyFailed.load()) {
        return -1;
    }
    const s32 result = WriteSaveData(saveBuffer, pageNum, pageCount, false);
    if (sCopyActive.load() && result != 0) {
        sCopyFailed.store(true);
    }
    return result;
}

extern "C" void SaveManager_BeginCopy(void) {
    const bool restored = RestoreFailedCopy();
    sCopyFailed.store(!restored);
    sCopyActive.store(OotmmSession_IsActive() != 0);
    if (restored && sCopyActive.load() && gFileSelectState != nullptr) {
        for (const bool backup : {false, true}) {
            if (!CaptureCopyOriginal(SavesFolderPath() /
                    SaveManager_GetFileName(gFileSelectState->copyDestFileIndex + 1, backup))) {
                sCopyFailed.store(true);
                break;
            }
        }
    }
}

extern "C" void SaveManager_ResetReadSnapshots(void) {
    sReadSnapshots.clear();
}

extern "C" int32_t SaveManager_FinishCopy(void) {
    if (sCopyFailed.load()) {
        RestoreFailedCopy();
    } else {
        sCopyOriginals.clear();
    }
    sCopyActive.store(false);
    return sCopyFailed.load() ? 0 : 1;
}

extern "C" int32_t SaveManager_CancelCopy(void) {
    if (!RestoreFailedCopy()) {
        return 0;
    }
    sCopyFailed.store(false);
    sCopyActive.store(false);
    return 1;
}

int32_t SaveManager_WriteInitialOotmmSave(uint8_t* saveBuffer, uint32_t pageNum, uint32_t pageCount) {
    return WriteSaveData(saveBuffer, pageNum, pageCount, true);
}

extern "C" s32 SaveManager_SysFlashrom_ReadData(void* saveBuffer, u32 pageNum, u32 pageCount) {
    FlashSave flashSave = SaveManager_GetFlashSaveFromPages(pageNum, pageCount);
    std::string fileName = SaveManager_GetFileNameFromFlashSave(flashSave);

    if (flashSave == FLASH_SAVE_UNAVAILABLE) {
        return -1;
    }

    if (flashSave == FLASH_SAVE_SRAM_HEADER || flashSave == FLASH_SAVE_SRAM_HEADER_BACKUP) {
        nlohmann::json j;
        int result = SaveManager_ReadSaveFile(fileName, j);
        if (result == -2) {
            SaveManager_MoveInvalidSaveFile(
                fileName,
                "Something went wrong trying to read global save file, the original file has been backed up.");
            return -1;
        } else if (result != 0) {
            return result;
        }

        try {
            SaveOptions saveOptions = j;

            memcpy(saveBuffer, &saveOptions, sizeof(SaveOptions));
            return 0;
        } catch (nlohmann::json::exception& je) {
            SPDLOG_ERROR("Failed to parse global settings json: {}", je.what());
            SaveManager_MoveInvalidSaveFile(
                fileName, "Failed to parse global settings json, the original file has been backed up.");
            return -1;
        }
    }

    bool isOwlSave = flashSave == FLASH_SAVE_FILE_1_OWL_SAVE || flashSave == FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP ||
                     flashSave == FLASH_SAVE_FILE_2_OWL_SAVE || flashSave == FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP ||
                     flashSave == FLASH_SAVE_FILE_3_OWL_SAVE || flashSave == FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP;

    nlohmann::json j;
    int result = SaveManager_ReadSaveFile(fileName, j);
    if (result == -2) {
        SaveManager_MoveInvalidSaveFile(
            fileName, "Something went wrong trying to read save file, the original file has been backed up.");
        return -1;
    } else if (result != 0) {
        return result;
    }

    result = SaveManager_MigrateSave(j);

    if (result != 0) {
        SaveManager_MoveInvalidSaveFile(fileName, "Failed to migrate save file, the original file has been backed up.");
        return -1;
    }

    if (isOwlSave) {
        if (!j.contains("owlSave")) {
            return -1;
        }

        try {
            SaveContext saveContext = j["owlSave"];

            // Recompute the checksum in case the save was edited externally or a migration changed it
            // By doing this we sacrifice "real" checksum verification of the save,
            saveContext.save.saveInfo.checksum = 0;
            saveContext.save.saveInfo.checksum = Sram_CalcChecksum(&saveContext, offsetof(SaveContext, fileNum));

            memcpy(saveBuffer, &saveContext, offsetof(SaveContext, fileNum));
            RememberReadSnapshot(saveBuffer, offsetof(SaveContext, fileNum), "owlSave", j, fileName);
            OotmmItemApply_NoteSaveRead(fileName, true);
            return 0;
        } catch (nlohmann::json::exception& je) {
            SPDLOG_ERROR("Failed to parse owl save json: {}", je.what());
            SaveManager_MoveInvalidSaveFile(fileName,
                                            "Failed to parse save json, the original file has been backed up.");
            return -1;
        } catch (...) {
            SPDLOG_ERROR("Failed to parse owl save json");
            SaveManager_MoveInvalidSaveFile(fileName,
                                            "Failed to parse save json, the original file has been backed up.");
            return -1;
        }
    } else {
        if (!j.contains("newCycleSave")) {
            return -1;
        }

        try {
            Save save = j["newCycleSave"]["save"];

            // Recompute the checksum, see message above
            save.saveInfo.checksum = 0;
            save.saveInfo.checksum = Sram_CalcChecksum(&save, sizeof(Save));

            memcpy(saveBuffer, &save, sizeof(Save));
            RememberReadSnapshot(saveBuffer, sizeof(Save), "newCycleSave", j, fileName);
            OotmmItemApply_NoteSaveRead(fileName, false);
            return 0;
        } catch (nlohmann::json::exception& je) {
            SPDLOG_ERROR("Failed to parse new cycle save json: {}", je.what());
            SaveManager_MoveInvalidSaveFile(fileName,
                                            "Failed to parse save json, the original file has been backed up.");
            return -1;
        } catch (...) {
            SPDLOG_ERROR("Failed to parse new cycle save json");
            SaveManager_MoveInvalidSaveFile(fileName,
                                            "Failed to parse save json, the original file has been backed up.");
            return -1;
        }
    }
}
