#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Every item the trade slot can hold, in cycle order. Returns the count written.
int32_t OotmmItemApply_TradeSlotCandidates(uint8_t slot, uint8_t* outItems, int32_t capacity);

/// The slot's merged ownership (native save plus launcher grants). Returns the count written.
int32_t OotmmItemApply_OwnedTradeItems(uint8_t slot, uint8_t* outItems, int32_t capacity);

/// Keeps the native slot occupant, the ownership ledger and launcher debug grants consistent.
void OotmmItemApply_SetTradeItemOwned(uint8_t slot, uint8_t item, int32_t owned);

#ifdef __cplusplus
}

void OotmmItemApply_Init();
/// Clears the applied ledger so a fresh file re-receives every item the seed already gave.
void OotmmItemApply_ResetLedgerForNewSave();
void OotmmItemApply_Reconcile();
#endif
