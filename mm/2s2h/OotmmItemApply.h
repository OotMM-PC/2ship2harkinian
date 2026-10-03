#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t OotmmItemApply_BombchuCapacity(void);
void OotmmItemApply_AddBombchuAmmo(int32_t amount);
int32_t OotmmItemApply_SelectBombDrop(int32_t bombDrop, int32_t bombchuDrop);

/// Every item the trade slot can hold, in cycle order. Returns the count written.
int32_t OotmmItemApply_TradeSlotCandidates(uint8_t slot, uint8_t* outItems, int32_t capacity);

/// The slot's merged ownership (native save plus launcher grants). Returns the count written.
int32_t OotmmItemApply_OwnedTradeItems(uint8_t slot, uint8_t* outItems, int32_t capacity);

/// Keeps the native slot occupant, the ownership ledger and launcher debug grants consistent.
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

void OotmmItemApply_Init();
void OotmmItemApply_RecordBossClear(int dungeonId);
/// Clears the applied ledger so a fresh file re-receives every item the seed already gave.
void OotmmItemApply_ResetLedgerForNewSave();
void OotmmItemApply_Reconcile();
#endif
