#include "OotmmHints.h"

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmNpcChecks.h"
#include "OotmmSession.h"
#include "OotmmSpecialConds.h"
#include "OotmmXflags.h"

#include <cctype>
#include <string>
#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_Gs/z_en_gs.h"

extern PlayState* gPlayState;
}

namespace {

constexpr u16 kGossipText = 0x20D0;
constexpr u16 kRemainsText = 0x1C18;
constexpr u16 kOathText = 0x2013;
constexpr s16 kLocationSign = 0x1000;
constexpr u16 kLocationSignText = kLocationSign | 0x300;
constexpr s16 kMoonCondSign = 0x1101;
constexpr u16 kMoonSignText = kMoonCondSign | 0x300;
constexpr s16 kRemainsTalkParams = 0x18;
constexpr char kFast = '\x17';
constexpr char kNoSkip = '\x19';
constexpr char kNewline = '\x11';
constexpr char kBoxBreak = '\x10';
constexpr char kEnd = '\xBF';
constexpr char kEvent = '\xE0';
constexpr u8 kRemainsIcon = 0x55;
constexpr size_t kRemainsHintBase = 9;
constexpr int kRemainsHintCount = 4;

int sRemainsHint = 0;
bool sRemainsAdvancing = false;
bool sOathPending = false;
bool sOathShowing = false;

char ColorCode(char token) {
    switch (token) {
        case 'r': return '\x01';
        case 'g': return '\x02';
        case 'b': return '\x03';
        case 'y': return '\x04';
        case 'c': return '\x05';
        case 'p': return '\x06';
        case 'o': return '\x08';
        default: return '\x00';
    }
}

std::string Native(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '%' && i + 1 < text.size()) {
            out += ColorCode(text[++i]);
        } else if (c == '\n') {
            out += kNewline;
        } else if (c == '\f') {
            out += kBoxBreak;
            out += kFast;
        } else {
            out += c;
        }
    }
    return out;
}

std::string Capitalized(std::string text) {
    if (!text.empty() && std::islower(static_cast<unsigned char>(text[0]))) {
        text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    }
    return text;
}

void Load(u8 type, u8 position, u8 icon, const std::string& body, bool* loadFromMessageTable) {
    CustomMessage::Entry entry;
    entry.textboxType = type;
    entry.textboxYPos = position;
    entry.icon = icon;
    entry.autoFormat = false;
    entry.msg = std::string(1, kFast) + body + kEnd;
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

void LoadSentence(const std::string& text, bool* loadFromMessageTable, const std::string& suffix = {}) {
    Load(0x00, 0x30, 0xFE, Native(Ship::OotmmHintLayout(text)) + suffix, loadFromMessageTable);
}

std::string VanillaEnding(u16 textId) {
    const auto entry = CustomMessage::LoadVanillaMessageTableEntry(textId);
    const auto end = entry.msg.rfind(kEnd);
    const char last = end != std::string::npos && end > 0 ? entry.msg[end - 1] : '\0';
    return last == kNoSkip || last == kEvent ? std::string(1, last) : std::string();
}

const std::string& StaticReward(Ship::OotmmStaticReward reward) {
    return OotmmSession_GetState().GetHints().StaticReward(reward);
}

Ship::OotmmStaticReward NextReward(Ship::OotmmStaticReward reward, size_t offset) {
    return static_cast<Ship::OotmmStaticReward>(static_cast<size_t>(reward) + offset);
}

void LoadRewardHint(u16* textId, u16 shownId, const std::string& text, bool* loadFromMessageTable) {
    *textId = shownId;
    LoadSentence(text, loadFromMessageTable, VanillaEnding(shownId));
}

void BuildGreatFairyText(u16* textId, bool* loadFromMessageTable) {
    constexpr u16 kTownFirst = 0x578;
    constexpr u16 kTownRepeat = 0x580;
    constexpr u16 kRegionBase = 0x57F;
    const std::string intro = "Young one, please help us! If you bring the ";
    if (*textId == kTownFirst || *textId == kTownRepeat) {
        const auto& human = StaticReward(Ship::OotmmStaticReward::GreatFairyTown);
        const auto& alt = StaticReward(Ship::OotmmStaticReward::GreatFairyTownAlt);
        if (!human.empty() && !alt.empty()) {
            LoadRewardHint(textId, *textId,
                           intro + "missing %oStray Fairy%w here, we will give you " + human + " and " + alt + "!",
                           loadFromMessageTable);
        }
        return;
    }
    const size_t region = (*textId - kRegionBase) / 3;
    const auto& reward = StaticReward(NextReward(Ship::OotmmStaticReward::GreatFairyWoodfall, region - 1));
    if (reward.empty()) {
        return;
    }
    const int count = OotmmSession_GetState().GetIntSetting("strayFairyRewardCount", 15);
    const std::string fairies = count > 1 ? std::to_string(count) + " Stray Fairies" : std::string("Stray Fairy");
    LoadRewardHint(textId, *textId, intro + "%p" + fairies + "%w here, we will give you " + reward + "!",
                   loadFromMessageTable);
}

void BuildPowderKegText(u16* textId, bool* loadFromMessageTable) {
    const auto& reward = StaticReward(Ship::OotmmStaticReward::PowderKeg);
    if (!reward.empty() && !OotmmNpcChecks_IsCollected("MM_GORON_KEG")) {
        LoadRewardHint(textId, *textId, "If you pass my test, I will give you " + reward + "!", loadFromMessageTable);
    }
}

void BuildZoraEggsText(u16* textId, bool* loadFromMessageTable) {
    constexpr u16 kHintText = 0xFBF;
    const auto& reward = StaticReward(Ship::OotmmStaticReward::ZoraEggs);
    if (!reward.empty() && gPlayState != nullptr && gPlayState->sceneId == SCENE_LABO) {
        LoadRewardHint(textId, kHintText, "If you can bring all of the %bZora Eggs%w here, you will get " + reward + "!",
                       loadFromMessageTable);
    }
}

void BuildSpiderHouseText(u16* textId, bool* loadFromMessageTable) {
    constexpr u16 kHintText = 0x915;
    if (gPlayState == nullptr || (gPlayState->sceneId != SCENE_KINSTA1 && gPlayState->sceneId != SCENE_KINDAN2)) {
        return;
    }
    const auto& reward = StaticReward(gPlayState->sceneId == SCENE_KINDAN2 ? Ship::OotmmStaticReward::SpiderHouseOcean
                                                                           : Ship::OotmmStaticReward::SpiderHouseSwamp);
    if (!reward.empty()) {
        LoadRewardHint(textId, kHintText, "If you lift the curse... I'll give you... " + reward + "... Hurry... Please...",
                       loadFromMessageTable);
    }
}

void BuildBeaverText(u16* textId, bool* loadFromMessageTable) {
    constexpr u16 kHintText = 0x1260;
    const auto& first = StaticReward(Ship::OotmmStaticReward::Beaver1);
    const auto& second = StaticReward(Ship::OotmmStaticReward::Beaver2);
    if (!first.empty() && !second.empty()) {
        LoadRewardHint(textId, kHintText, "I've heard that the beavers are betting " + first + " and " + second + ".",
                       loadFromMessageTable);
    }
}

void BuildMoonChildText(u16*, bool* loadFromMessageTable) {
    const auto& reward = StaticReward(Ship::OotmmStaticReward::FierceDeity);
    if (reward.empty() || OotmmNpcChecks_IsCollected("MM_MASK_FIERCE_DEITY")) {
        return;
    }
    LoadSentence("You have only weak masks...\nHaving better masks would give you " + reward + "...",
                 loadFromMessageTable, "\x10So...you'll play?\x11\x11\x02\xC2Yes\x11No");
}

bool IsMoonTrial(s16 sceneId) {
    switch (sceneId) {
        case SCENE_LAST_DEKU:
        case SCENE_LAST_GORON:
        case SCENE_LAST_ZORA:
        case SCENE_LAST_LINK:
            return true;
        default:
            return false;
    }
}

uint32_t GossipKey(const EnGs* stone) {
    if (stone->actor.params == ENGS_1 || stone->actor.params == ENGS_2) {
        return (static_cast<u16>(stone->unk_198) & 0x1F) | 0x20;
    }
    const uint32_t key = stone->unk_195 & 0x1F;
    return IsMoonTrial(gPlayState->sceneId) ? key | 0x40 : key;
}

const EnGs* TalkingStone() {
    Player* player = GET_PLAYER(gPlayState);
    if (player != nullptr && player->talkActor != nullptr && player->talkActor->id == ACTOR_EN_GS) {
        return reinterpret_cast<const EnGs*>(player->talkActor);
    }
    return nullptr;
}

void BuildGossipText(u16*, bool* loadFromMessageTable) {
    const EnGs* stone = gPlayState != nullptr ? TalkingStone() : nullptr;
    if (stone == nullptr) {
        return;
    }
    const std::string hint = OotmmSession_GetState().GetGossipHint(Ship::OotmmGame::Mm, GossipKey(stone));
    if (!hint.empty()) {
        LoadSentence(hint, loadFromMessageTable);
    }
}

void BuildRemainsText(u16*, bool* loadFromMessageTable) {
    const auto& rewards = OotmmSession_GetState().GetHints().DungeonRewards;
    if (rewards.size() < kRemainsHintBase + kRemainsHintCount) {
        return;
    }
    if (!sRemainsAdvancing) {
        sRemainsHint = 0;
    }
    sRemainsAdvancing = false;
    std::string body = Native(Ship::OotmmHintLayout(Capitalized(rewards[kRemainsHintBase + sRemainsHint].Text) + "..."));
    if (sRemainsHint != kRemainsHintCount - 1) {
        body += kNoSkip;
    }
    Load(0x06, 0x00, static_cast<u8>(kRemainsIcon + sRemainsHint), body, loadFromMessageTable);
}

void AdvanceRemainsHints(Actor* actor) {
    if ((actor->params & 0x3F) != kRemainsTalkParams || gPlayState->msgCtx.currentTextId != kRemainsText ||
        sRemainsHint >= kRemainsHintCount - 1 || Message_GetState(&gPlayState->msgCtx) != TEXT_STATE_EVENT ||
        !Message_ShouldAdvance(gPlayState)) {
        return;
    }
    ++sRemainsHint;
    sRemainsAdvancing = true;
    Message_ContinueTextbox(gPlayState, kRemainsText);
}

std::string OathNotesText(const std::vector<Ship::OotmmHintRegion>& oath) {
    std::vector<std::pair<std::string, int>> places;
    for (const auto& region : oath) {
        if (region.RegionId == 0) {
            break;
        }
        bool found = false;
        for (auto& place : places) {
            if (place.first == region.Text) {
                ++place.second;
                found = true;
                break;
            }
        }
        if (!found) {
            places.emplace_back(region.Text, 1);
        }
    }
    if (places.empty()) {
        return {};
    }
    std::string text = "Have you found the %pOath to Order Notes%w?\n";
    for (size_t i = 0; i < places.size(); ++i) {
        text += std::to_string(places[i].second) + " " + places[i].first;
        if (i + 1 < places.size()) {
            text += '\n';
        }
    }
    return text;
}

void BuildOathText(u16*, bool* loadFromMessageTable) {
    if (!sOathShowing) {
        return;
    }
    sOathShowing = false;
    const auto& state = OotmmSession_GetState();
    const auto& oath = state.GetHints().OathToOrder;
    if (oath.empty()) {
        return;
    }
    std::string text = state.GetStringSetting("songs") == "notes" ? OathNotesText(oath) : std::string();
    if (text.empty()) {
        text = "Have you found the %pOath to Order %w" + oath[0].Text + "?";
    }
    LoadSentence(text, loadFromMessageTable);
}

void QueueOathHint(Actor*) {
    const auto& state = OotmmSession_GetState();
    sOathPending = gPlayState->sceneId == SCENE_OKUJOU && gSaveContext.sceneLayer == 0 &&
                   OotmmSpecialConds_MoonAccessible() && !state.GetBoolSetting("openMoon") &&
                   !state.GetBoolSetting("songEventsShuffleMm");
}

void ShowOathHint(Actor* actor) {
    if (!sOathPending || Message_GetState(&gPlayState->msgCtx) != TEXT_STATE_NONE) {
        return;
    }
    sOathPending = false;
    sOathShowing = true;
    Message_StartTextbox(gPlayState, kOathText, actor);
}

const Actor* TalkingSign() {
    Player* player = GET_PLAYER(gPlayState);
    if (player != nullptr && player->talkActor != nullptr && player->talkActor->id == ACTOR_EN_KANBAN) {
        return player->talkActor;
    }
    return nullptr;
}

const char* GenericGrottoName(int grotto) {
    switch (grotto) {
        case 0x13: return "Path to Snowhead Generic Grotto";
        case 0x14: return "Ikana Valley Generic Grotto";
        case 0x15: return "Zora Cape Generic Grotto";
        case 0x16: return "Road to Ikana Generic Grotto";
        case 0x17: return "Great Bay Coast Generic Grotto";
        case 0x18: return "Ikana Graveyard Generic Grotto";
        case 0x19: return "Twin Islands Generic Grotto";
        case 0x1A: return "Termina Field Pillar Generic Grotto";
        case 0x1B: return "Mountain Village Generic Grotto";
        case 0x1C: return "Woods of Mystery Generic Grotto";
        case 0x1D: return "Southern Swamp Generic Grotto";
        case 0x1E: return "Road to Southern Swamp Generic Grotto";
        case 0x1F: return "Termina Field Tall Grass Generic Grotto";
        default: return nullptr;
    }
}

std::string LocationName() {
    const char* name = nullptr;
    if (gPlayState->sceneId == SCENE_KAKUSIANA) {
        if (gPlayState->roomCtx.curRoom.num == 0x04) {
            name = GenericGrottoName(gSaveContext.respawn[RESPAWN_MODE_UNK_3].data & 0x1F);
        } else if (gPlayState->roomCtx.curRoom.num == 0x0A) {
            switch (OotmmXflags_LastScene()) {
                case SCENE_00KEIKOKU: name = "Termina Field Cow Grotto"; break;
                case SCENE_30GYOSON: name = "Great Bay Coast Cow Grotto"; break;
            }
        }
    }
    return name != nullptr ? name : "Unknown Location";
}

void BuildLocationSignText(u16*, bool* loadFromMessageTable) {
    const Actor* sign = gPlayState != nullptr ? TalkingSign() : nullptr;
    if (sign != nullptr && sign->params == kLocationSign) {
        Load(0x01, 0x30, 0xFE, Native(Ship::OotmmHintLayout(LocationName())), loadFromMessageTable);
    }
}

void BuildSignText(u16*, bool* loadFromMessageTable) {
    const Actor* sign = gPlayState != nullptr ? TalkingSign() : nullptr;
    if (sign == nullptr || sign->params != kMoonCondSign) {
        return;
    }
    const auto& conds = OotmmSession_GetState().GetHints().SpecialConds;
    if (const auto it = conds.find("MOON"); it != conds.end()) {
        LoadSentence(it->second, loadFromMessageTable);
    }
}

void SpawnSign(float x, float y, float z, s16 yaw, s16 params) {
    if (Object_GetSlot(&gPlayState->objectCtx, OBJECT_KANBAN) < 0) {
        Object_SpawnPersistent(&gPlayState->objectCtx, OBJECT_KANBAN);
    }
    Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_KANBAN, x, y, z, 0, yaw, 0, params);
}

void SpawnExtraSigns(s8, s8) {
    if (gPlayState == nullptr || !OotmmSession_IsActive()) {
        return;
    }
    const auto& state = OotmmSession_GetState();
    if (gPlayState->sceneId == SCENE_KAKUSIANA && state.GetStringSetting("erGrottos", "none") != "none") {
        if (gPlayState->roomCtx.curRoom.num == 0x04) {
            SpawnSign(2330.0f, -5.0f, -40.0f, 0x2000, kLocationSign);
        } else if (gPlayState->roomCtx.curRoom.num == 0x0A) {
            SpawnSign(2350.0f, 0.0f, 1200.0f, 0x2000, kLocationSign);
        }
    }
    if (gPlayState->sceneId != SCENE_OKUJOU || state.GetStringSetting("moon") != "custom" ||
        !state.GetHints().SpecialConds.contains("MOON")) {
        return;
    }
    SpawnSign(-120.0f, 20.0f, 150.0f, 0x2800, kMoonCondSign);
}

} // namespace

void OotmmHints_Init() {
    REGISTER_VB_SHOULD(VB_GS_CONSIDER_MASK_OF_TRUTH_EQUIPPED, { *should = false; });
    auto* interactor = GameInteractor::Instance;
    interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(kGossipText, BuildGossipText);
    interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(kRemainsText, BuildRemainsText);
    interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(kOathText, BuildOathText);
    interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(kMoonSignText, BuildSignText);
    interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(kLocationSignText, BuildLocationSignText);
    for (const u16 id : { 0x578, 0x580, 0x582, 0x583, 0x585, 0x586, 0x588, 0x589, 0x58B, 0x58C }) {
        interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(id, BuildGreatFairyText);
    }
    interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(0x0C81, BuildPowderKegText);
    for (const u16 id : { 0xFA0, 0xFA1, 0xFA6, 0xFA7, 0xFA8, 0xFAA, 0xFAC, 0xFAE, 0xFB0, 0xFB1, 0xFB2, 0xFBF }) {
        interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(id, BuildZoraEggsText);
    }
    for (const u16 id : { 0x910, 0x914 }) {
        interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(id, BuildSpiderHouseText);
    }
    for (const u16 id : { 0x125C, 0x125E, 0x125F, 0x1261 }) {
        interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(id, BuildBeaverText);
    }
    interactor->RegisterGameHookForID<GameInteractor::OnOpenText>(0x21FE, BuildMoonChildText);
    interactor->RegisterGameHook<GameInteractor::OnSceneInit>(SpawnExtraSigns);
    interactor->RegisterGameHookForID<GameInteractor::OnActorUpdate>(ACTOR_EN_TALK, AdvanceRemainsHints);
    interactor->RegisterGameHookForID<GameInteractor::OnActorInit>(ACTOR_DM_STK, QueueOathHint);
    interactor->RegisterGameHookForID<GameInteractor::OnActorUpdate>(ACTOR_DM_STK, ShowOathHint);
}
