#include "OotmmItemPresentation.h"

#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmItemModels.h"
#include "OotmmIpc.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

extern "C" {
#include "functions.h"
#include "z64.h"
extern PlayState* gPlayState;
}

namespace {

std::unordered_map<int16_t, Ship::GameIpcItemPresentation> sPresentations;
std::deque<int16_t> sPresentationOrder;
std::unordered_set<int16_t> sPendingPresentations;
int32_t sNextPresentation = 1;
std::unordered_map<std::string, uint32_t> sDeliveredChecks;

const Ship::GameIpcItemPresentation* FindPresentation(int16_t id) {
    const auto presentation = sPresentations.find(id);
    return presentation == sPresentations.end() ? nullptr : &presentation->second;
}

std::string GameName(const std::string& game) {
    return game == "oot" ? "Ocarina of Time" : "Majora's Mask";
}

std::string BuildMessage(const Ship::GameIpcItemPresentation& presentation) {
    const bool isTrap = Ship::OotmmItemCatalog::IsTrap(presentation.ItemId);
    const bool isShared = presentation.ItemId.rfind("SHARED_", 0) == 0;
    const bool crossGame = !isTrap && !isShared && presentation.DestinationGame != "mm";
    const std::string itemName =
        Ship::OotmmItemCatalog::TrimGameSuffix(presentation.ItemName);
    std::string message = "You got %g" + itemName;
    if (presentation.SourcePlayer != 0 && presentation.SourcePlayer != presentation.RecipientPlayer) {
        if (presentation.ViewerPlayer == presentation.SourcePlayer) {
            message += "%w! It was sent to %bPlayer " + std::to_string(presentation.RecipientPlayer);
            message += isTrap || isShared ? std::string("%w.")
                              : "'s %r" + GameName(presentation.DestinationGame) + "%w.";
            return message;
        } else {
            message += "%w from %bPlayer " + std::to_string(presentation.SourcePlayer);
        }
    }
    message += "%w!";
    if (crossGame) {
        message += " It was sent to %r" + GameName(presentation.DestinationGame) + "%w.";
    }
    return message;
}

void DrawPresentation(Actor* actor, PlayState* play) {
    const auto* presentation = FindPresentation(static_cast<int16_t>(CUSTOM_ITEM_PARAM));
    if (presentation == nullptr) {
        return;
    }
    if (actor->scale.x > 0.0001f) {
        const float scale = 0.21f / actor->scale.x;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }
    if (OotmmItemModel_DrawById(play, presentation->ItemId, presentation->ItemName)) {
        return;
    }
    static std::string sReported;
    if (sReported != presentation->ItemId) {
        sReported = presentation->ItemId;
        SPDLOG_WARN("OoTMM: no model for {} \"{}\"", presentation->ItemId, presentation->ItemName);
    }
    GetItem_Draw(play, GID_RUPEE_GREEN);
}

void GivePresentation(Actor* actor, PlayState* play) {
    sPendingPresentations.erase(static_cast<int16_t>(CUSTOM_ITEM_PARAM));
    const auto* presentation = FindPresentation(static_cast<int16_t>(CUSTOM_ITEM_PARAM));
    if (presentation == nullptr) {
        return;
    }

    sPresentationOrder.push_back(static_cast<int16_t>(CUSTOM_ITEM_PARAM));
    if (sPresentationOrder.size() > 32) {
        sPresentations.erase(sPresentationOrder.front());
        sPresentationOrder.pop_front();
    }

    const std::string message = BuildMessage(*presentation);
    CustomMessage::Entry entry = { .textboxType = 2, .msg = message };
    if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
        Audio_PlayFanfare(NA_BGM_GET_ITEM);
        CustomMessage::SetActiveCustomMessage(entry.msg, entry);
    } else {
        Audio_PlaySfx(NA_SE_SY_GET_ITEM);
        CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10", entry);
    }
    if (!presentation->CheckKey.empty() && play != nullptr) {
        sDeliveredChecks[presentation->CheckKey] = play->gameplayFrames;
    }
}

} // namespace

void OotmmItemPresentation_Queue(Ship::GameIpcItemPresentation presentation) {
    const int16_t id = static_cast<int16_t>(sNextPresentation);
    sNextPresentation =
        sNextPresentation == std::numeric_limits<int16_t>::max() ? 1 : sNextPresentation + 1;
    sPresentations[id] = std::move(presentation);
    sPendingPresentations.emplace(id);
    GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
        .showGetItemCutscene = true,
        .param = id,
        .giveItem = GivePresentation,
        .drawItem = DrawPresentation,
        .waitForSafePlayerState = false,
    });
}

bool OotmmItemPresentation_PlayerReceiving(const Player* player) {
    return player->getItemId > GI_NONE && player->getItemId < GI_MAX;
}

bool OotmmItemPresentation_IsDelivering(const std::string& checkId) {
    return sDeliveredChecks.contains("mm:" + checkId);
}

bool OotmmItemPresentation_IsPending() {
    return !sPendingPresentations.empty();
}

void OotmmItemPresentation_Update() {
    if (gPlayState == nullptr || sDeliveredChecks.empty()) {
        return;
    }
    const Player* player = GET_PLAYER(gPlayState);
    if (Message_GetState(&gPlayState->msgCtx) != TEXT_STATE_NONE ||
        player->csAction != PLAYER_CSACTION_NONE || (player->stateFlags1 & PLAYER_STATE1_400)) {
        return;
    }
    for (auto it = sDeliveredChecks.begin(); it != sDeliveredChecks.end();) {
        if (it->second == gPlayState->gameplayFrames) {
            ++it;
        } else {
            OotmmIpc_CompleteCheckPresentation(it->first);
            it = sDeliveredChecks.erase(it);
        }
    }
}
