#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t OotmmItemApply_BombchuCapacity(void);
int16_t OotmmWallet_ScaleRupees(int16_t amount);
int32_t OotmmWallet_HasWallet(void);
int32_t OotmmWallet_Bottomless(void);
void OotmmItemApply_AddBombchuAmmo(int32_t amount);
int32_t OotmmItemApply_SelectBombDrop(int32_t bombDrop, int32_t bombchuDrop);
struct PlayState;
int32_t OotmmItemApply_TrapDamageTick(struct PlayState* play, int32_t kind);

int32_t OotmmItemApply_TradeSlotCandidates(uint8_t slot, uint8_t* outItems, int32_t capacity);

int32_t OotmmItemApply_OwnedTradeItems(uint8_t slot, uint8_t* outItems, int32_t capacity);

void OotmmItemApply_SetTradeItemOwned(uint8_t slot, uint8_t item, int32_t owned);
void OotmmItemApply_RecordTradeGrant(uint8_t item);
int32_t OotmmItemApply_ConsumeTradeItem(uint8_t item, uint8_t slot);
int32_t OotmmItemApply_ToggleTradeItem(uint8_t slot);
void OotmmItemApply_RememberTradeItems(void);
void OotmmItemApply_ResetTradeCycle(void);

#ifdef __cplusplus
}

#include <nlohmann/json.hpp>
bool OotmmItemApply_Embed(nlohmann::json& save);
void OotmmItemApply_NoteSaveRead(const std::string& fileName, bool owlSave);
void OotmmItemApply_RefreshSaveSnapshot();
nlohmann::json OotmmItemApply_LegacySnapshot(int fileNum);

uint32_t OotmmItemApply_LedgerGet(const std::string& key);
void OotmmItemApply_LedgerSet(const std::string& key, uint32_t value);
void OotmmItemApply_Init();
void OotmmItemApply_RecordBossClear(int dungeonId);
void OotmmItemApply_ResetLedgerForNewSave();
void OotmmItemApply_Reconcile();
nlohmann::json OotmmItemApply_TrapsApplied();
#endif
