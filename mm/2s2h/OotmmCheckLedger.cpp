#include "OotmmCheckLedger.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

extern "C" {
#include "z64save.h"

extern PlayState* gPlayState;
}

namespace {

std::unordered_set<std::string> sReported;

void LoadFromFile(s16 fileNum) {
    sReported.clear();
    nlohmann::json j;
    if (SaveManager_ReadSaveFile(SaveManager_GetFileName(fileNum + 1), j) != 0) {
        return;
    }
    const auto it = j.find("ootmmChecks");
    if (it == j.end() || !it->is_array()) {
        return;
    }
    for (const auto& entry : *it) {
        if (entry.is_string()) {
            sReported.emplace(entry.get<std::string>());
        }
    }
    for (const auto& checkId : sReported) {
        if (!OotmmIpc_IsCheckCompleted(checkId)) {
            OotmmIpc_SendCheckCollected(checkId);
        }
    }
}

}

void OotmmCheckLedger_Record(const std::string& checkId) {
    if (!checkId.empty()) {
        sReported.emplace(checkId);
    }
}

void OotmmCheckLedger_Forget(const std::string& checkId) {
    sReported.erase(checkId);
}

bool OotmmCheckLedger_Contains(const std::string& checkId) {
    return sReported.count(checkId) != 0;
}

void OotmmCheckLedger_Embed(nlohmann::json& save) {
    if (gPlayState == nullptr) {
        return;
    }
    std::unordered_set<std::string> reported = sReported;
    const auto& state = OotmmSession_GetState();
    for (const auto& placement : state.GetPlacements()) {
        if (placement.CheckGame == Ship::OotmmGame::Mm &&
            (placement.SourcePlayer == 0 || placement.SourcePlayer == state.GetPlayerId()) &&
            OotmmIpc_IsCheckCompleted(placement.CheckId)) {
            reported.emplace(placement.CheckId);
        }
    }
    std::vector<std::string> checks(reported.begin(), reported.end());
    std::sort(checks.begin(), checks.end());
    save["ootmmChecks"] = checks;
}

void OotmmCheckLedger_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>([](s16) { sReported.clear(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>(LoadFromFile);
}
