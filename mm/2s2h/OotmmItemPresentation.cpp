#include "OotmmItemPresentation.h"

#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmItemModels.h"
#include "OotmmIpc.h"
#include "OotmmSession.h"
#include "OotmmSongs.h"
#include "2s2h/Enhancements/Audio/AudioCollection.h"
#include <libultraship/bridge/OotmmItemFanfare.h>

#include <spdlog/spdlog.h>

#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

extern "C" {
#include "functions.h"
#include "z64.h"
#include "sequence.h"
extern PlayState* gPlayState;
SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path);
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path);
}

namespace {

std::unordered_map<int16_t, Ship::GameIpcItemPresentation> sPresentations;
std::deque<int16_t> sPresentationOrder;
std::unordered_set<int16_t> sPendingPresentations;
int32_t sNextPresentation = 1;
std::unordered_map<std::string, uint32_t> sDeliveredChecks;
std::optional<Ship::GameIpcItemPresentation> sCutscenePresentation;

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
    message += OotmmSession_GetState().DungeonItemText(presentation.ItemId);
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

int32_t SongFanfare(int32_t song) {
    const int32_t seq = OotmmSongs_FanfareSeqId(song);
    return seq > 0 ? seq : NA_BGM_GET_ITEM;
}

int32_t ForeignFanfare(const char* name, const char* fontPath) {
    const int32_t seq = AudioCollection::Instance->GetSequenceNumByName(name);
    SoundFont* font = ResourceMgr_LoadAudioSoundFontByName(fontPath);
    SequenceData* data = ResourceMgr_LoadSeqPtrByName((std::string("custom/music/") + name + "_fanfare").c_str());
    if (seq <= 0 || font == nullptr || data == nullptr) {
        return NA_BGM_GET_ITEM;
    }
    data->fonts[0] = static_cast<uint8_t>(font->fntIndex);
    data->numFonts = 1;
    return seq;
}

int32_t FanfareSeq(Ship::OotmmFanfare fanfare) {
    using F = Ship::OotmmFanfare;
    switch (fanfare) {
        case F::HeartContainer:
            return NA_BGM_GET_HEART;
        case F::HeartPiece:
            return NA_BGM_GET_SMALL_ITEM;
        case F::Song:
            return ForeignFanfare("FairyOcarina", "custom/fonts/OotFairyFountain");
        case F::SongTime:
            return NA_BGM_OCARINA_TIME;
        case F::SongEpona:
            return NA_BGM_OCARINA_EPONA;
        case F::SongSun:
            return NA_BGM_OCARINA_SUNS;
        case F::SongStorms:
            return NA_BGM_OCARINA_STORM;
        case F::Mask:
            return NA_BGM_GET_NEW_MASK;
        case F::Medallion:
            return ForeignFanfare("Medallion", "custom/fonts/OotOrchestra");
        case F::Stone:
            return ForeignFanfare("SpiritualStone", "custom/fonts/OotOrchestra");
        case F::SongZelda:
            return SongFanfare(OOTMM_SONG_ZELDA);
        case F::SongSaria:
            return SongFanfare(OOTMM_SONG_SARIA);
        case F::SongLight:
            return SongFanfare(OOTMM_SONG_PRELUDE);
        case F::SongForest:
            return SongFanfare(OOTMM_SONG_MINUET);
        case F::SongFire:
            return SongFanfare(OOTMM_SONG_BOLERO);
        case F::SongWater:
            return SongFanfare(OOTMM_SONG_SERENADE);
        case F::SongShadow:
            return SongFanfare(OOTMM_SONG_NOCTURNE);
        case F::SongSpirit:
            return SongFanfare(OOTMM_SONG_REQUIEM);
        case F::SongSoaring:
            return NA_BGM_OCARINA_SOARING;
        case F::SongHealing:
            return NA_BGM_OCARINA_HEALING;
        case F::SongAwakening:
            return NA_BGM_OCARINA_SONATA;
        case F::SongGoronHalf:
            return NA_BGM_OCARINA_LULLABY_INTRO;
        case F::SongGoron:
            return NA_BGM_OCARINA_LULLABY;
        case F::SongZora:
            return NA_BGM_OCARINA_NEW_WAVE;
        case F::SongEmptiness:
            return NA_BGM_OCARINA_ELEGY;
        case F::SongOrder:
            return NA_BGM_OCARINA_OATH;
        default:
            return NA_BGM_GET_ITEM;
    }
}

void PlayItemFanfare(const Ship::GameIpcItemPresentation& presentation, bool isShort) {
    const auto& inventory = OotmmIpc_GetInventory();
    const auto fanfare = Ship::OotmmItemFanfare_Resolve(
        presentation.ItemId, isShort,
        [&inventory](std::string_view id) { return inventory.Count(std::string(id)); },
        static_cast<uint32_t>(OotmmSession_GetState().GetIntSetting("strayFairyRewardCount", 15)));
    switch (fanfare) {
        case Ship::OotmmFanfare::None:
            break;
        case Ship::OotmmFanfare::SfxRupee:
            Audio_PlaySfx(NA_SE_SY_GET_RUPY);
            break;
        case Ship::OotmmFanfare::SfxMinor:
            Audio_PlaySfx(NA_SE_SY_GET_ITEM);
            break;
        case Ship::OotmmFanfare::SfxMinorQuick:
            Audio_PlaySfx(NA_SE_SY_GET_BOXITEM);
            break;
        default:
            Audio_PlayFanfare(static_cast<u16>(FanfareSeq(fanfare)));
            break;
    }
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

    if (presentation->Background && Ship::OotmmItemCatalog::IsFast(presentation->ItemId)) {
        CUSTOM_ITEM_FLAGS |= CustomItem::QUIET_PICKUP;
        PlayItemFanfare(*presentation, true);
        return;
    }

    const std::string message = BuildMessage(*presentation);
    CustomMessage::Entry entry = { .textboxType = 2, .msg = message };
    if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
        sCutscenePresentation = *presentation;
        CustomMessage::SetActiveCustomMessage(entry.msg, entry);
    } else {
        CUSTOM_ITEM_FLAGS |= CustomItem::QUIET_PICKUP;
        PlayItemFanfare(*presentation, true);
        CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10", entry);
    }
    if (!presentation->CheckKey.empty() && play != nullptr) {
        sDeliveredChecks[presentation->CheckKey] = play->gameplayFrames;
    }
}

} // namespace

extern "C" void OotmmItemPresentation_PlayGetItemFanfare(void) {
    if (!sCutscenePresentation.has_value()) {
        Audio_PlayFanfare(NA_BGM_GET_ITEM);
        return;
    }
    PlayItemFanfare(*sCutscenePresentation, false);
    sCutscenePresentation.reset();
}

void OotmmItemPresentation_Queue(Ship::GameIpcItemPresentation presentation) {
    const bool background = presentation.Background;
    if (background && (Ship::OotmmItemCatalog::IsNothing(presentation.ItemId) ||
                       (presentation.Quiet && !Ship::OotmmItemCatalog::IsFast(presentation.ItemId)))) {
        return;
    }
    const int16_t id = static_cast<int16_t>(sNextPresentation);
    sNextPresentation =
        sNextPresentation == std::numeric_limits<int16_t>::max() ? 1 : sNextPresentation + 1;
    sPresentations[id] = std::move(presentation);
    sPendingPresentations.emplace(id);
    GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
        .showGetItemCutscene = !background,
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
