
#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#ifdef __cplusplus
#include <string>
#include <optional>
#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
std::string SaveManager_GetFileName(int fileNum, bool isBackup = false);
bool SaveManager_HandleFileDropped(char* filePath);
bool BinarySaveConverter_HandleFileDropped(char* filePath);
int SaveManager_GetOpenFileSlot();
bool SaveManager_WriteSaveFile(const std::filesystem::path& fileName, nlohmann::json j);
bool SaveManager_ValidateOotmmRecovery(const std::string& contents, const std::string& saveId, uint32_t slot,
                                      nlohmann::json& validated, std::string& error);
bool SaveManager_RecoverOotmmBackup(const std::optional<std::string>& expectedPrimary,
                                   const std::string& expectedBackup, std::string& error);
bool SaveManager_ValidateCurrentOotmmSave(std::string& error);
int SaveManager_ReadSaveFile(const std::filesystem::path& fileName, nlohmann::json& j);
int32_t SaveManager_WriteInitialOotmmSave(uint8_t* saveBuffer, uint32_t pageNum, uint32_t pageCount);
extern "C" int32_t SaveManager_SysFlashrom_WriteData(uint8_t* addr, uint32_t pageNum, uint32_t pageCount);
extern "C" void SaveManager_BeginCopy(void);
extern "C" void SaveManager_ResetReadSnapshots(void);
extern "C" int32_t SaveManager_FinishCopy(void);
extern "C" int32_t SaveManager_CancelCopy(void);
#else
void SaveManager_BeginCopy(void);
void SaveManager_ResetReadSnapshots(void);
s32 SaveManager_FinishCopy(void);
s32 SaveManager_CancelCopy(void);
s32 SaveManager_SysFlashrom_WriteData(u8* addr, u32 pageNum, u32 pageCount);
s32 SaveManager_SysFlashrom_ReadData(void* addr, u32 pageNum, u32 pageCount);
#endif

#endif // SAVE_MANAGER_H
