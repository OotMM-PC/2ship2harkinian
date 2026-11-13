#include "OotmmDebugChecks.h"

#include "2s2h/BenGui/UIWidgets.hpp"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmChecks.h"
#include "OotmmCsmc.h"
#include "OotmmXflags.h"
#include "OotmmPresence.h"
#include "OotmmDrops.h"
#include "OotmmCustomItems.h"

extern "C" {
struct OotmmDebugSfxRequest {
    u16 sfxId;
    u8 token;
    s8* reverbAdd;
    Vec3f* pos;
    f32* freqScale;
    f32* volume;
};
extern OotmmDebugSfxRequest sSfxRequests[0x100];
extern u8 sSfxRequestWriteIndex;
}
#include "OotmmFaroresWind.h"
#include "OotmmItemApply.h"
#include "OotmmItemPresentation.h"
#include "OotmmNpcChecks.h"
#include "OotmmPauseInfo.h"
#include "OotmmBottles.h"
#include "OotmmIpc.h"
#include "OotmmOwls.h"
#include <libultraship/bridge/OotmmRuntimeTest.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "OotmmSession.h"
#include "OotmmTriforce.h"
#include "OotmmSaveMenu.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64snap.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
#include "src/overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "src/overlays/actors/ovl_En_Bigpamet/z_en_bigpamet.h"
#include "src/overlays/actors/ovl_En_Syateki_Man/z_en_syateki_man.h"
#include "src/overlays/actors/ovl_En_Fu/z_en_fu.h"
#include "src/overlays/actors/ovl_En_Lift_Nuts/z_en_lift_nuts.h"
#include "src/overlays/actors/ovl_Obj_Grass/z_obj_grass.h"
#include "src/overlays/actors/ovl_Mir_Ray3/z_mir_ray3.h"
#include "src/overlays/actors/ovl_Obj_Lightblock/z_obj_lightblock.h"
#include "src/overlays/actors/ovl_Boss_07/z_boss_07.h"
#include "src/overlays/gamestates/ovl_file_choose/z_file_select.h"

extern s16 sEnvHazard;
extern PlayState* gPlayState;
void EnKnight_DebugFinish(Actor* thisx, PlayState* play);
extern FileSelectState* gFileSelectState;
extern f32 sTakarayaWallHeights[11][8];
}

namespace {
#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
std::atomic<uint64_t> sRuntimePad{0};
#endif

constexpr int32_t kChestItem = 1;

struct AppearanceOption {
    const char* Label;
    bool HasClass;
    Ship::OotmmCsmcClass Class;
};

const AppearanceOption kAppearances[] = {
    { "Vanilla", false, Ship::OotmmCsmcClass::Normal },
    { "Normal (junk)", true, Ship::OotmmCsmcClass::Normal },
    { "Major", true, Ship::OotmmCsmcClass::Major },
    { "Boss Key", true, Ship::OotmmCsmcClass::BossKey },
    { "Small Key", true, Ship::OotmmCsmcClass::Key },
    { "Skulltula Token", true, Ship::OotmmCsmcClass::Spider },
    { "Stray Fairy", true, Ship::OotmmCsmcClass::Fairy },
    { "Heart", true, Ship::OotmmCsmcClass::Heart },
    { "Soul", true, Ship::OotmmCsmcClass::Soul },
    { "Map/Compass", true, Ship::OotmmCsmcClass::MapCompass },
};

int sAppearance = 2;

std::unordered_map<Actor*, int32_t> sDebugChests;

bool IsDebugChest(Actor* actor) {
    return sDebugChests.count(actor) != 0;
}

int32_t FreeChestFlag() {
    for (int32_t flag = 31; flag >= 0; flag--) {
        if (Flags_GetTreasure(gPlayState, flag) != 0) {
            continue;
        }
        if (OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm,
                                              OotmmChecks_CheckScene(gPlayState->sceneId), "chest",
                                              static_cast<uint32_t>(flag)) != nullptr) {
            continue;
        }
        bool used = false;
        for (const auto& [chest, usedFlag] : sDebugChests) {
            if (usedFlag == flag) {
                used = true;
                break;
            }
        }
        if (!used) {
            return flag;
        }
    }
    return -1;
}

bool PlaceChest() {
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return false;
    }
    const int32_t flag = FreeChestFlag();
    if (flag < 0) {
        return false;
    }

    Vec3f pos = player->actor.world.pos;
    pos.x += Math_SinS(player->actor.shape.rot.y) * 80.0f;
    pos.z += Math_CosS(player->actor.shape.rot.y) * 80.0f;
    const s32 params = (kChestItem << 5) | flag;
    Actor* chest = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_BOX, pos.x, pos.y, pos.z,
                               0, player->actor.shape.rot.y, 0, params);
    if (chest == nullptr) {
        return false;
    }

    sDebugChests.emplace(chest, flag);
    const auto& appearance = kAppearances[sAppearance];
    if (appearance.HasClass) {
        OotmmCsmc_SetChestOverride(chest, appearance.Class);
    }
    return true;
}

struct DropOption {
    std::string ItemId;
    std::string ItemName;
    std::string ItemGame;
};

std::vector<DropOption> sDropOptions;
int sDropSelection = 0;
char sDropFilter[64] = {};

void BuildDropOptions() {
    if (!sDropOptions.empty()) {
        return;
    }
    std::unordered_set<std::string> seen;
    for (const auto& placement : OotmmSession_GetState().GetPlacements()) {
        if (placement.ItemId.empty() || !seen.insert(placement.ItemId).second) {
            continue;
        }
        sDropOptions.push_back({ placement.ItemId, placement.ItemName, placement.ItemGame });
    }
    std::sort(sDropOptions.begin(), sDropOptions.end(),
              [](const DropOption& a, const DropOption& b) { return a.ItemName < b.ItemName; });
}

bool MatchesFilter(const std::string& name, const char* filter) {
    if (filter[0] == '\0') {
        return true;
    }
    std::string lowerName = name;
    std::string lowerFilter = filter;
    const auto lower = [](unsigned char character) { return static_cast<char>(std::tolower(character)); };
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), lower);
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), lower);
    return lowerName.find(lowerFilter) != std::string::npos;
}

bool DropSelectedItem() {
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || sDropSelection >= static_cast<int>(sDropOptions.size())) {
        return false;
    }
    const DropOption& option = sDropOptions[sDropSelection];
    const float x = player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * 60.0f;
    const float z = player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * 60.0f;
    return OotmmDrops_Spawn(gPlayState, x, player->actor.world.pos.y + 20.0f, z, option.ItemId,
                            option.ItemName, option.ItemGame, "");
}

void DrawDropSection() {
    ImGui::SeparatorText("Dropped items");
    ImGui::TextWrapped("Drops a collectible that draws as the item and presents it on pickup. "
                       "Grants nothing.");
    BuildDropOptions();
    if (sDropOptions.empty()) {
        ImGui::TextDisabled("No seed items available.");
        return;
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    ImGui::InputTextWithHint("##DropFilter", "Filter", sDropFilter, sizeof(sDropFilter));
    if (sDropSelection >= static_cast<int>(sDropOptions.size())) {
        sDropSelection = 0;
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    if (ImGui::BeginCombo("Item", sDropOptions[sDropSelection].ItemName.c_str())) {
        for (int i = 0; i < static_cast<int>(sDropOptions.size()); i++) {
            if (!MatchesFilter(sDropOptions[i].ItemName, sDropFilter)) {
                continue;
            }
            if (ImGui::Selectable(sDropOptions[i].ItemName.c_str(), i == sDropSelection)) {
                sDropSelection = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(gPlayState == nullptr);
    if (ImGui::Button("Drop item")) {
        DropSelectedItem();
    }
    ImGui::EndDisabled();
}

struct NpcOption {
    std::string NativeId;
    std::string Label;
    std::string ItemLabel;
    int32_t Entrance = -1;
};

std::vector<NpcOption> sNpcOptions;
int sNpcSelection = 0;
char sNpcFilter[64] = {};

constexpr int32_t kGossipGrottoEntrance = 0x1400;
constexpr int32_t kScrubGrottoEntrance = 0x1490;

int32_t EntranceForNpc(const std::string& nativeId, int32_t scene) {
    if (nativeId == "MM_SCRUB_TELESCOPE") {
        return kScrubGrottoEntrance;
    }
    if (nativeId == "MM_GOSSIP_HEART_PIECE") {
        return kGossipGrottoEntrance;
    }
    return scene >= 0 ? Entrance_Create(scene, 0, 0) : -1;
}

void BuildNpcOptions() {
    if (!sNpcOptions.empty()) {
        return;
    }
    for (const auto& placement : OotmmSession_GetState().GetPlacements()) {
        if (placement.CheckGame != Ship::OotmmGame::Mm || placement.CheckType != "npc" ||
            placement.CheckNativeId.empty()) {
            continue;
        }
        const int32_t scene = placement.CheckScene.has_value() ? static_cast<int32_t>(*placement.CheckScene) : -1;
        sNpcOptions.push_back({ placement.CheckNativeId,
                                placement.CheckName.empty() ? placement.CheckNativeId : placement.CheckName,
                                placement.ItemName.empty() ? placement.ItemId : placement.ItemName,
                                EntranceForNpc(placement.CheckNativeId, scene) });
    }
    std::sort(sNpcOptions.begin(), sNpcOptions.end(),
              [](const NpcOption& a, const NpcOption& b) { return a.Label < b.Label; });
}

void DrawNpcSection() {
    ImGui::SeparatorText("NPC interactions");
    ImGui::TextWrapped("Go loads the NPC's scene. Complete the interaction in-game so the actor owns "
                       "the grant, dialogue, and state transition.");
    BuildNpcOptions();
    if (sNpcOptions.empty()) {
        ImGui::TextDisabled("No npc checks in the seed.");
        return;
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    ImGui::InputTextWithHint("##NpcFilter", "Filter", sNpcFilter, sizeof(sNpcFilter));
    if (sNpcSelection >= static_cast<int>(sNpcOptions.size())) {
        sNpcSelection = 0;
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    if (ImGui::BeginCombo("Check", sNpcOptions[sNpcSelection].Label.c_str())) {
        for (int i = 0; i < static_cast<int>(sNpcOptions.size()); i++) {
            if (!MatchesFilter(sNpcOptions[i].Label, sNpcFilter) &&
                !MatchesFilter(sNpcOptions[i].ItemLabel, sNpcFilter) &&
                !MatchesFilter(sNpcOptions[i].NativeId, sNpcFilter)) {
                continue;
            }
            const bool collected = OotmmNpcChecks_IsCollected(sNpcOptions[i].NativeId.c_str()) != 0;
            const std::string label = collected ? sNpcOptions[i].Label + " (collected)" : sNpcOptions[i].Label;
            if (ImGui::Selectable(label.c_str(), i == sNpcSelection)) {
                sNpcSelection = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const NpcOption& selected = sNpcOptions[sNpcSelection];
    ImGui::BeginDisabled(gPlayState == nullptr || selected.Entrance < 0);
    if (ImGui::Button("Go")) {
        const int32_t entrance = OotmmSession_ApplyResolvedMmEntrance(selected.Entrance);
        if (entrance >= 0) {
            gPlayState->nextEntrance = entrance;
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gPlayState->transitionType = TRANS_TYPE_INSTANT;
            gSaveContext.nextTransitionType = TRANS_TYPE_INSTANT;
        }
    }
    ImGui::EndDisabled();
    const bool collected = OotmmNpcChecks_IsCollected(selected.NativeId.c_str()) != 0;
    if (collected) {
        ImGui::SameLine();
        ImGui::TextDisabled("Collected");
    }
    ImGui::TextDisabled("Contains: %s", selected.ItemLabel.c_str());
    if (selected.Entrance < 0) {
        ImGui::TextDisabled("This check has no routable scene in the seed.");
    }
}

} // namespace

void OotmmDebugChecks_DrawTab() {
    ImGui::TextWrapped(
        "Places test chests for previewing check assets. They open like real ones and grant nothing.");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    if (ImGui::BeginCombo("Appearance", kAppearances[sAppearance].Label)) {
        for (int i = 0; i < static_cast<int>(std::size(kAppearances)); i++) {
            if (ImGui::Selectable(kAppearances[i].Label, i == sAppearance)) {
                sAppearance = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(gPlayState == nullptr);
    if (ImGui::Button("Place chest")) {
        PlaceChest();
    }
    ImGui::EndDisabled();
    if (!sDebugChests.empty()) {
        ImGui::TextDisabled("%d debug chest(s) in this scene.", static_cast<int>(sDebugChests.size()));
    }
    DrawDropSection();
    DrawNpcSection();
}

#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
extern "C" int OotmmDebugChecks_ReadRuntimePad(uint16_t* buttons, int8_t* x, int8_t* y) {
    const uint64_t value = sRuntimePad.load(std::memory_order_relaxed);
    if ((value & (1ull << 32)) == 0) {
        return 0;
    }
    *buttons = static_cast<uint16_t>(value);
    *x = static_cast<int8_t>(value >> 16);
    *y = static_cast<int8_t>(value >> 24);
    return 1;
}
#endif

#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
static nlohmann::json TakarayaMaze() {
    if (gPlayState == nullptr || gPlayState->sceneId != SCENE_TAKARAYA) {
        return nullptr;
    }
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& row : sTakarayaWallHeights) {
        std::string cells;
        for (const f32 height : row) {
            cells.push_back(height == 0.0f ? '#' : '.');
        }
        rows.push_back(cells);
    }
    return rows;
}

static void ApplySetupString(const char* variable, const std::string& setup);

static void ApplyRuntimeSetup(const char* variable) {
    const char* probe = std::getenv(variable);
    const char* directory = std::getenv("OOTMM_RUNTIME_TEST_DIR");
    if (probe == nullptr || directory == nullptr) {
        return;
    }
    const auto marker = std::filesystem::u8path(directory) / (std::string(variable) + ".applied");
    if (std::filesystem::exists(marker)) {
        return;
    }
    std::ofstream(marker) << probe;
    ApplySetupString(variable, probe);
}

static void ApplySetupString(const char* variable, const std::string& setup) {
    nlohmann::json applied = nlohmann::json::array();
    size_t start = 0;
    while (start < setup.size()) {
        const size_t end = std::min(setup.find(',', start), setup.size());
        const std::string token = setup.substr(start, end - start);
        start = end + 1;
        const size_t equals = token.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = token.substr(0, equals);
        const std::string value = token.substr(equals + 1);
        const size_t colon = value.find(':');
        const long first = std::strtol(value.c_str(), nullptr, 0);
        const long second = colon == std::string::npos ? 0 : std::strtol(value.c_str() + colon + 1, nullptr, 0);
        auto& info = gSaveContext.save.saveInfo;
        if (key == "remote") {
            Ship::GameIpcItemPresentation presentation;
            presentation.ItemId = value;
            presentation.ItemName = value;
            presentation.ItemGame = value.starts_with("MM_") ? "mm" : "oot";
            for (const auto& placement : OotmmSession_GetState().GetPlacements()) {
                if (placement.ItemId == value) {
                    presentation.ItemName = placement.ItemName;
                    presentation.ItemGame = placement.ItemGame;
                    break;
                }
            }
            presentation.DestinationGame = presentation.ItemGame == "mm" || presentation.ItemGame == "oot"
                                               ? presentation.ItemGame
                                               : "mm";
            presentation.SourcePlayer = 2;
            presentation.RecipientPlayer = 1;
            presentation.ViewerPlayer = 1;
            presentation.Background = true;
            OotmmItemPresentation_Queue(presentation);
        } else if (key == "drop") {
            const Ship::OotmmPlacement* match = nullptr;
            for (const auto& placement : OotmmSession_GetState().GetPlacements()) {
                if (placement.ItemId == value) {
                    match = &placement;
                    break;
                }
            }
            Player* player = gPlayState != nullptr ? GET_PLAYER(gPlayState) : nullptr;
            if (match == nullptr || player == nullptr) {
                continue;
            }
            const float x = player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * 60.0f;
            const float z = player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * 60.0f;
            if (!OotmmDrops_Spawn(gPlayState, x, player->actor.world.pos.y + 20.0f, z, match->ItemId, match->ItemName,
                                  match->ItemGame, "")) {
                continue;
            }
        } else if (key == "collect") {
            OotmmNpcChecks_Collect(value.c_str());
        } else if (key == "goto" && gPlayState != nullptr) {
            gPlayState->nextEntrance = static_cast<u16>(first);
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
        } else if (key == "bombnear" && gPlayState != nullptr) {
            Player* holder = GET_PLAYER(gPlayState);
            Actor* best = nullptr;
            f32 bestDist = 0.0f;
            for (auto& list : gPlayState->actorCtx.actorLists) {
                for (Actor* actor = list.first; actor != nullptr; actor = actor->next) {
                    if (actor->id == first) {
                        f32 dist = Actor_WorldDistXZToActor(&holder->actor, actor);
                        if (best == nullptr || dist < bestDist) {
                            best = actor;
                            bestDist = dist;
                        }
                    }
                }
            }
            if (best != nullptr) {
                auto* bomb = reinterpret_cast<EnBom*>(Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_BOM, best->world.pos.x,
                                                                  best->world.pos.y + 10.0f, best->world.pos.z, 0, 0, 0, 0));
                if (bomb != nullptr) {
                    bomb->timer = 1;
                    bomb->collider2.elements[0].dim.worldSphere.center.x = static_cast<s16>(best->world.pos.x);
                    bomb->collider2.elements[0].dim.worldSphere.center.y = static_cast<s16>(best->world.pos.y + 10.0f);
                    bomb->collider2.elements[0].dim.worldSphere.center.z = static_cast<s16>(best->world.pos.z);
                }
            }
        } else if (key == "bombat" && gPlayState != nullptr) {
            for (auto& list : gPlayState->actorCtx.actorLists) {
                for (Actor* actor = list.first; actor != nullptr; actor = actor->next) {
                    if (actor->id == first && (second == 0 || (actor->params & 0xFFFF) == second)) {
                        auto* bomb = reinterpret_cast<EnBom*>(Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_BOM, actor->world.pos.x, actor->world.pos.y, actor->world.pos.z, 0, 0, 0, 0));
                        if (bomb != nullptr) {
                            bomb->timer = 1;
                            bomb->collider2.elements[0].dim.worldSphere.center.x = static_cast<s16>(actor->world.pos.x);
                            bomb->collider2.elements[0].dim.worldSphere.center.y = static_cast<s16>(actor->world.pos.y);
                            bomb->collider2.elements[0].dim.worldSphere.center.z = static_cast<s16>(actor->world.pos.z);
                        }
                        break;
                    }
                }
            }
        } else if (key == "knightfinish" && gPlayState != nullptr) {
            for (auto& list : gPlayState->actorCtx.actorLists) {
                for (Actor* actor = list.first; actor != nullptr; actor = actor->next) {
                    if (actor->id == ACTOR_EN_KNIGHT) {
                        EnKnight_DebugFinish(actor, gPlayState);
                    }
                }
            }
        } else if (key == "invuln" && gPlayState != nullptr) {
            GET_PLAYER(gPlayState)->invincibilityTimer = static_cast<s8>(first);
        } else if (key == "aim" && gPlayState != nullptr) {
            Player* player = GET_PLAYER(gPlayState);
            player->actor.focus.rot.x = static_cast<s16>(first);
            player->actor.focus.rot.y = static_cast<s16>(second);
        } else if (key == "acthp" && gPlayState != nullptr) {
            for (auto& list : gPlayState->actorCtx.actorLists) {
                for (Actor* actor = list.first; actor != nullptr; actor = actor->next) {
                    if (actor->id == first) {
                        actor->colChkInfo.health = static_cast<u8>(second);
                    }
                }
            }
        } else if (key == "minigame") {
            gSaveContext.minigameScore = static_cast<s32>(first);
            if (gPlayState != nullptr) {
                for (auto& list : gPlayState->actorCtx.actorLists) {
                    for (Actor* actor = list.first; actor != nullptr; actor = actor->next) {
                        if (actor->id == ACTOR_EN_SYATEKI_MAN) {
                            reinterpret_cast<EnSyatekiMan*>(actor)->score = static_cast<s32>(first);
                        } else if (actor->id == ACTOR_EN_FU) {
                            auto* fu = reinterpret_cast<EnFu*>(actor);
                            fu->unk_548 = fu->unk_54C;
                        } else if (actor->id == ACTOR_EN_LIFT_NUTS) {
                            auto* nuts = reinterpret_cast<EnLiftNuts*>(actor);
                            if (nuts->minigameScore != nullptr) {
                                *nuts->minigameScore = static_cast<s16>(first);
                            }
                        }
                    }
                }
            }
        } else if (key == "tspeed") {
            gSaveContext.save.timeSpeedOffset = static_cast<s32>(first);
        } else if (key == "dograce") {
            gSaveContext.eventInf[0] = static_cast<u8>(EVENTINF_DOG_RACE_STATE_ENDED | (first << 3));
            gSaveContext.unk_3F5C = static_cast<s32>(second);
        } else if (key == "moonmask" && first >= 0 && first < static_cast<long>(ARRAY_COUNT(gSaveContext.masksGivenOnMoon))) {
            gSaveContext.masksGivenOnMoon[first] = static_cast<u8>(second);
        } else if (key == "breg") {
            BREG(static_cast<s32>(first)) = static_cast<s16>(second);
        } else if (key == "postmanstop") {
            gSaveContext.postmanTimerStopOsTime =
                gSaveContext.timerStartOsTimes[TIMER_ID_POSTMAN] + gSaveContext.timerPausedOsTimes[TIMER_ID_POSTMAN] +
                (static_cast<OSTime>(SECONDS_TO_TIMER(static_cast<s32>(first))) * 10000 * 3000) / 64;
            gSaveContext.timerStates[TIMER_ID_POSTMAN] = TIMER_STATE_POSTMAN_STOP;
        } else if (key == "einf") {
            SET_EVENTINF(static_cast<s32>(first));
        } else if (key == "spawn" && gPlayState != nullptr) {
            const Player* player = GET_PLAYER(gPlayState);
            Actor_Spawn(&gPlayState->actorCtx, gPlayState, static_cast<s16>(first), player->actor.world.pos.x,
                        player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, static_cast<s32>(second));
        } else if (key == "picto") {
            Snap_SetFlag(static_cast<s32>(first));
        } else if (key == "lottery") {
            for (auto& day : gSaveContext.save.saveInfo.lotteryCodes) {
                for (auto& digit : day) {
                    digit = static_cast<s8>(first);
                }
            }
        } else if (key == "day") {
            gSaveContext.save.day = static_cast<s32>(first);
        } else if (key == "grotto") {
            gSaveContext.respawn[RESPAWN_MODE_UNK_3].data = static_cast<s8>(first);
        } else if (key == "time") {
            gSaveContext.save.time = static_cast<u16>(first);
            gSaveContext.save.isNight = (first >= 0xC000 || first < 0x4000) ? 1 : 0;
        } else if (key == "rupees") {
            info.playerData.rupees = static_cast<s16>(first);
        } else if (key == "health") {
            info.playerData.health = static_cast<s16>(first);
        } else if (key == "bank") {
            HS_SET_BANK_RUPEES(static_cast<u32>(first));
        } else if (key == "week" && first >= 0 && first < static_cast<long>(ARRAY_COUNT(info.weekEventReg))) {
            info.weekEventReg[first] |= static_cast<u8>(second);
        } else if (key == "item" && first >= 0 && first < static_cast<long>(ARRAY_COUNT(info.inventory.items))) {
            info.inventory.items[first] = static_cast<u8>(second);
        } else if (key == "invitem") {
            const auto colon = value.find(':');
            if (colon != std::string::npos) {
                OotmmIpc_SetDebugItemValue(value.substr(0, colon),
                                           static_cast<uint32_t>(std::strtoul(value.c_str() + colon + 1, nullptr, 0)));
            }
        } else if (key == "seeds") {
            OotmmCustomItems_SetSlingshotAmmo(static_cast<int32_t>(first));
        } else if (key == "bosscycle") {
            gSaveContext.save.shipSaveInfo.ootmmBossCycle = static_cast<u8>(first);
        } else if (key == "quest") {
            SET_QUEST_ITEM(first);
        } else if (key == "ammo" && first >= 0 && first < static_cast<long>(ARRAY_COUNT(info.inventory.ammo))) {
            info.inventory.ammo[first] = static_cast<s8>(second);
        } else if (key == "upgrade") {
            Inventory_ChangeUpgrade(static_cast<s16>(first), static_cast<u32>(second));
        } else if (key == "magic") {
            info.playerData.isMagicAcquired = true;
            info.playerData.magicLevel = 1;
            info.playerData.magic = static_cast<s8>(first);
            gSaveContext.magicCapacity = MAGIC_NORMAL_METER;
        } else if (key == "form") {
            gSaveContext.save.playerForm = static_cast<u8>(first);
        } else if (key == "tunic") {
            OotmmCustomItems_SetEquippedTunic(static_cast<int32_t>(first));
        } else if (key == "keys" && first >= 0 && first < 4) {
            info.inventory.dungeonKeys[first] = static_cast<s8>(second);
        } else if (key == "fairies" && first >= 0 && first < 5) {
            info.inventory.strayFairies[first] = static_cast<s8>(second);
        } else if (key == "bosskey" && first >= 0 && first < 4) {
            info.inventory.dungeonItems[first] |= 1;
        } else if (key == "tokens") {
            info.skullTokenCount = static_cast<u32>(first);
        } else if (key == "ruto") {
            OotmmItemApply_LedgerSet("native:rutoLetterUsed", static_cast<uint32_t>(first));
        } else if (key == "chest" && first >= 0 && first < static_cast<long>(ARRAY_COUNT(gSaveContext.cycleSceneFlags)) &&
                   second >= 0 && second < 32) {
            gSaveContext.cycleSceneFlags[first].chest |= 1u << second;
            if (gPlayState != nullptr && Play_GetOriginalSceneId(gPlayState->sceneId) == first) {
                gPlayState->actorCtx.sceneFlags.chest |= 1u << second;
            }
        } else if (key == "switch" && gPlayState != nullptr && gPlayState->sceneId == first && second >= 0 && second < 128) {
            Flags_SetSwitch(gPlayState, static_cast<s32>(second));
        } else if (key == "unchest" && first >= 0 &&
                   first < static_cast<long>(ARRAY_COUNT(gSaveContext.cycleSceneFlags)) && second >= 0 && second < 32) {
            gSaveContext.cycleSceneFlags[first].chest &= ~(1u << second);
            gSaveContext.save.saveInfo.permanentSceneFlags[first].chest &= ~(1u << second);
            if (gPlayState != nullptr && Play_GetOriginalSceneId(gPlayState->sceneId) == first) {
                gPlayState->actorCtx.sceneFlags.chest &= ~(1u << second);
            }
        } else if (key == "clear" && gPlayState != nullptr && gPlayState->sceneId == first && second >= 0 && second < 32) {
            Flags_SetClear(gPlayState, static_cast<s32>(second));
            Flags_SetClearTemp(gPlayState, static_cast<s32>(second));
        } else if (key == "mswitch" && first >= 0 &&
                   first < static_cast<long>(ARRAY_COUNT(gSaveContext.cycleSceneFlags)) && second >= 0 && second < 64) {
            if (second < 32) {
                gSaveContext.cycleSceneFlags[first].switch0 |= 1u << second;
            } else {
                gSaveContext.cycleSceneFlags[first].switch1 |= 1u << (second - 32);
            }
            if (gPlayState != nullptr && Play_GetOriginalSceneId(gPlayState->sceneId) == first) {
                gPlayState->actorCtx.sceneFlags.switches[second / 32] |= 1u << (second % 32);
            }
        } else if (key == "dpad" && first >= EQUIP_SLOT_D_RIGHT && first <= EQUIP_SLOT_D_UP && second >= 0 &&
                   second < static_cast<long>(ARRAY_COUNT(info.inventory.items))) {
            DPAD_BUTTON_ITEM_EQUIP(0, first) = info.inventory.items[second];
            DPAD_SLOT_EQUIP(0, first) = static_cast<u8>(second);
            if (gPlayState != nullptr) {
                Interface_Dpad_LoadItemIcon(gPlayState, static_cast<u8>(first));
            }
        } else if (key == "c" && first >= EQUIP_SLOT_C_LEFT && first <= EQUIP_SLOT_C_RIGHT && second >= 0 &&
                   (second < static_cast<long>(ARRAY_COUNT(info.inventory.items)) ||
                    OotmmCustomItems_IsCustomSlot(static_cast<uint8_t>(second)))) {
            BUTTON_ITEM_EQUIP(0, first) = OotmmCustomItems_IsCustomSlot(static_cast<uint8_t>(second))
                                              ? OotmmCustomItems_ItemInSlot(static_cast<uint8_t>(second))
                                              : info.inventory.items[second];
            C_SLOT_EQUIP(0, first) = static_cast<u8>(second);
        } else if (key == "shield") {
            SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, static_cast<u32>(first));
            if (gPlayState != nullptr && GET_PLAYER(gPlayState) != nullptr) {
                Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
            }
        } else if (key == "sword") {
            SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, static_cast<u32>(first));
            BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = static_cast<u8>(ITEM_SWORD_KOKIRI + first - EQUIP_VALUE_SWORD_KOKIRI);
        } else if (key == "bsword") {
            BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = static_cast<u8>(first);
            SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, static_cast<u32>(first - ITEM_SWORD_KOKIRI + EQUIP_VALUE_SWORD_KOKIRI));
        } else {
            continue;
        }
        applied.push_back(token);
    }
    Ship::OotmmRuntimeTest::Get().Observe(
        { { "event", "runtimeSetup" }, { "variable", variable }, { "applied", applied } });
}
#endif

void OotmmDebugChecks_Init() {
#if !defined(NDEBUG) || defined(OOTMM_RUNTIME_TEST)
    if (Ship::OotmmRuntimeTest::Get().Enabled()) {
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>(
            [](s16) { ApplyRuntimeSetup("OOTMM_RUNTIME_MM_SETUP"); });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
            static bool lateApplied = false;
            static int settledFrames = 0;
            if (!lateApplied && gPlayState != nullptr && OotmmSession_IsActive() &&
                !OotmmSession_HasPendingInitialSave() && ++settledFrames >= 30) {
                lateApplied = true;
                ApplyRuntimeSetup("OOTMM_RUNTIME_MM_SETUP_LATE");
            }
            const char* directory = std::getenv("OOTMM_RUNTIME_TEST_DIR");
            std::error_code ec;
            if (directory != nullptr && gPlayState != nullptr) {
                const auto livePath = std::filesystem::u8path(directory) / "live-setup.txt";
                if (std::filesystem::exists(livePath, ec)) {
                    std::string setup;
                    std::getline(std::ifstream(livePath), setup);
                    std::filesystem::remove(livePath, ec);
                    ApplySetupString("live-setup", setup);
                    bool reload = setup.empty();
                    for (const char* key : { "form=", "time=", "day=", "week=", "reload=" }) {
                        reload = reload || setup.find(key) != std::string::npos;
                    }
                    if (reload) {
                        gPlayState->nextEntrance = gSaveContext.save.entrance;
                        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
                        gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
                    }
                }
            }
        });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
            if (gGameState == nullptr || !OotmmSession_IsActive()) {
                sRuntimePad.store(0, std::memory_order_relaxed);
                return;
            }
            static OSContPad previous{};
            const auto pad = Ship::OotmmRuntimeTest::Get().Advance();
            static int pendingRoom = -1;
            if (const auto room = Ship::OotmmRuntimeTest::Get().TakeRoom();
                room && gPlayState != nullptr && *room < gPlayState->roomList.count &&
                *room != gPlayState->roomCtx.curRoom.num &&
                Room_RequestNewRoom(gPlayState, &gPlayState->roomCtx, *room)) {
                pendingRoom = *room;
            }
            if (pendingRoom >= 0 && gPlayState != nullptr && gPlayState->roomCtx.status == 0 &&
                gPlayState->roomCtx.curRoom.num == pendingRoom) {
                Room_FinishRoomChange(gPlayState, &gPlayState->roomCtx);
                pendingRoom = -1;
            }
            if (const auto warp = Ship::OotmmRuntimeTest::Get().TakeWarp(); warp && gPlayState != nullptr) {
                Player* player = GET_PLAYER(gPlayState);
                player->actor.world.pos = { warp->X, warp->Y, warp->Z };
                player->actor.prevPos = player->actor.home.pos = player->actor.world.pos;
                player->actor.world.rot.y = player->actor.shape.rot.y = player->yaw = warp->Yaw;
                player->actor.velocity = { 0.0f, 0.0f, 0.0f };
                player->actor.speed = player->speedXZ = 0.0f;
            }
            sRuntimePad.store((1ull << 32) | pad.Buttons |
                (static_cast<uint64_t>(static_cast<uint8_t>(pad.X)) << 16) |
                (static_cast<uint64_t>(static_cast<uint8_t>(pad.Y)) << 24), std::memory_order_relaxed);
            auto& input = gGameState->input[0];
            input = {};
            input.prev = previous;
            input.cur.button = pad.Buttons;
            input.cur.stick_x = pad.X;
            input.cur.stick_y = pad.Y;
            input.press.button = input.cur.button & ~previous.button;
            input.rel.button = previous.button & ~input.cur.button;
            input.press.stick_x = input.cur.stick_x - previous.stick_x;
            input.press.stick_y = input.cur.stick_y - previous.stick_y;
            PadUtils_UpdateRelXY(&input);
            previous = input.cur;
        });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
            if (gPlayState == nullptr && OotmmSession_IsActive()) {
                nlohmann::json state = { { "game", "mm" }, { "mode", "menu" } };
                if (gFileSelectState != nullptr) {
                    state["mode"] = "file-select";
                    state["menuMode"] = gFileSelectState->menuMode;
                    state["configMode"] = gFileSelectState->configMode;
                    state["selectMode"] = gFileSelectState->selectMode;
                    state["buttonIndex"] = gFileSelectState->buttonIndex;
                    state["copySource"] = gFileSelectState->selectedFileIndex;
                    state["copyDestination"] = gFileSelectState->copyDestFileIndex;
                }
                Ship::OotmmRuntimeTest::Get().Observe(std::move(state));
                return;
            }
            if (gPlayState == nullptr || !OotmmSession_IsActive()) {
                return;
            }
            const Player* player = GET_PLAYER(gPlayState);
            if (player == nullptr) {
                return;
            }
            static bool magicSaveAttempted = false;
            const char* magicSaveProbe = std::getenv("OOTMM_RUNTIME_SAVE_DURING_MAGIC");
            const std::string magicSavePhase = magicSaveProbe != nullptr ? magicSaveProbe : "";
            const bool magicSaveReady =
                (magicSavePhase == "1" && gSaveContext.isMagicRequested && gSaveContext.magicToAdd > 0) ||
                (magicSavePhase == "capacity" && gSaveContext.magicState == MAGIC_STATE_STEP_CAPACITY) ||
                (magicSavePhase == "fill" && gSaveContext.magicState == MAGIC_STATE_FILL);
            if (!magicSaveAttempted && magicSaveReady) {
                magicSaveAttempted = true;
                const auto beforeMagic = gSaveContext.save.saveInfo.playerData.magic;
                const auto pendingMagic = gSaveContext.magicToAdd;
                const auto magicState = gSaveContext.magicState;
                const auto fillTarget = gSaveContext.magicFillTarget;
                const auto capacity = gSaveContext.magicCapacity;
                const bool saved = OotmmSession_SaveProgress();
                Ship::OotmmRuntimeTest::Get().Observe({ { "event", "pendingMagicSave" },
                    { "magicBefore", beforeMagic }, { "pendingMagic", pendingMagic },
                    { "phase", magicSavePhase }, { "magicState", magicState },
                    { "fillTarget", fillTarget }, { "capacity", capacity },
                    { "saved", saved } });
            }
            if (const char* floor = std::getenv("OOTMM_RUNTIME_MIN_HEALTH"); floor != nullptr) {
                auto& health = gSaveContext.save.saveInfo.playerData.health;
                health = std::max<s16>(health, static_cast<s16>(std::atoi(floor)));
            }
            int clampId = 0;
            int clampHp = 0;
            if (const char* clamp = std::getenv("OOTMM_RUNTIME_ACTOR_HP");
                clamp != nullptr && std::sscanf(clamp, "%i,%d", &clampId, &clampHp) == 2) {
                for (auto& list : gPlayState->actorCtx.actorLists) {
                    for (Actor* actor = list.first; actor != nullptr; actor = actor->next) {
                        if (actor->id == clampId && static_cast<s8>(actor->colChkInfo.health) > clampHp) {
                            actor->colChkInfo.health = static_cast<u8>(clampHp);
                        }
                    }
                }
            }
            if (!Ship::OotmmRuntimeTest::Get().ShouldObserve()) {
                return;
            }
            nlohmann::json checks = nlohmann::json::array();
            for (const auto& placement : OotmmSession_GetState().GetPlacements()) {
                if (placement.CheckGame == Ship::OotmmGame::Mm &&
                    OotmmIpc_IsCheckCompleted(placement.CheckId)) {
                    checks.push_back(placement.CheckId);
                }
            }
            nlohmann::json actors = nlohmann::json::array();
            nlohmann::json majora = nlohmann::json::array();
            nlohmann::json grassElements = nlohmann::json::array();
            for (const auto& list : gPlayState->actorCtx.actorLists) {
                for (const Actor* actor = list.first; actor != nullptr; actor = actor->next) {
                    actors.push_back({ { "id", actor->id }, { "params", actor->params },
                                       { "x", actor->world.pos.x }, { "y", actor->world.pos.y },
                                       { "z", actor->world.pos.z }, { "ry", actor->shape.rot.y }, { "flags", actor->flags },
                                       { "home", { actor->home.rot.x, actor->home.rot.y, actor->home.rot.z } },
                                       { "category", actor->category }, { "parent", actor->parent != nullptr },
                                       { "update", actor->update != nullptr }, { "draw", actor->draw != nullptr },
                                       { "hp", actor->colChkInfo.health },
                                       { "xflag", OotmmXflags_NativeId(const_cast<Actor*>(actor)) } });
                    if (actor->id == ACTOR_BOSS_07 && actor->init == nullptr) {
                        const auto* boss = reinterpret_cast<const Boss07*>(actor);
                        majora.push_back({ { "params", actor->params }, { "damagedTimer", boss->damagedTimer },
                                           { "shake", boss->maskShakeTimer }, { "subAction", boss->subAction } });
                    }
                    if (actor->id == ACTOR_MIR_RAY3) {
                        const auto* ray = reinterpret_cast<const MirRay3*>(actor);
                        const auto& quad = ray->colliderQuad.dim.quad;
                        actors.back()["beam"] = { { "strength", ray->unk_214 },
                                                  { "lit", (ray->unk_210 & 2) != 0 },
                                                  { "from", { quad[1].x, quad[1].y, quad[1].z } },
                                                  { "to", { quad[3].x, quad[3].y, quad[3].z } } };
                    } else if (actor->id == ACTOR_OBJ_LIGHTBLOCK) {
                        actors.back()["lightHits"] = reinterpret_cast<const ObjLightblock*>(actor)->collisionCounter;
                    } else if (actor->id == ACTOR_EN_BIGPAMET) {
                        const auto* snapper = reinterpret_cast<const EnBigpamet*>(actor);
                        actors.back()["acFlags"] = snapper->collider.base.acFlags;
                        actors.back()["acDmg"] = snapper->collider.elem.acHitElem != nullptr
                                                     ? snapper->collider.elem.acHitElem->atDmgInfo.dmgFlags
                                                     : 0;
                        actors.back()["vy"] = actor->velocity.y;
                    }
                    if (actor->id != ACTOR_OBJ_GRASS) {
                        continue;
                    }
                    const auto* grass = reinterpret_cast<const ObjGrass*>(actor);
                    for (int group = 0; group < grass->activeGrassGroups; group++) {
                        for (int element = 0; element < grass->grassGroups[group].count; element++) {
                            const auto& entry = grass->grassGroups[group].elements[element];
                            if (entry.flags & OBJ_GRASS_ELEM_REMOVED) {
                                continue;
                            }
                            grassElements.push_back(
                                { { "x", entry.pos.x }, { "y", entry.pos.y }, { "z", entry.pos.z },
                                  { "xflag", OotmmXflags_GrassElementNativeId(const_cast<Actor*>(actor), group,
                                                                               element) } });
                        }
                    }
                }
            }
            nlohmann::json setupActors = nlohmann::json::array();
            const int setupCount = std::abs(gPlayState->numSetupActors);
            for (int i = 0; gPlayState->setupActorList != nullptr && i < setupCount; i++) {
                const ActorEntry& entry = gPlayState->setupActorList[i];
                setupActors.push_back({ { "id", entry.id & 0xFFF }, { "params", entry.params },
                                        { "x", entry.pos.x }, { "y", entry.pos.y }, { "z", entry.pos.z },
                                        { "halfDays", ((entry.rot.x & 7) << 7) | (entry.rot.z & 0x7F) } });
            }
            nlohmann::json cycleFlags = nlohmann::json::array();
            for (int scene = 0; scene < static_cast<int>(ARRAY_COUNT(gSaveContext.cycleSceneFlags)); scene++) {
                const auto& flags = gSaveContext.cycleSceneFlags[scene];
                if (flags.chest | flags.switch0 | flags.switch1 | flags.collectible) {
                    cycleFlags.push_back({ scene, flags.chest, flags.switch0, flags.switch1, flags.collectible });
                }
            }
            static u8 sfxReadIndex = sSfxRequestWriteIndex;
            static u32 sfxSerial = 0;
            static std::vector<std::pair<u32, u16>> recentSfx;
            while (sfxReadIndex != sSfxRequestWriteIndex) {
                recentSfx.emplace_back(++sfxSerial, sSfxRequests[sfxReadIndex].sfxId);
                if (recentSfx.size() > 64) {
                    recentSfx.erase(recentSfx.begin());
                }
                sfxReadIndex++;
            }
            Ship::OotmmRuntimeTest::Get().Observe({
                { "weather", gWeatherMode },
                { "bgm", AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN) },
                { "bottleTimers", gSaveContext.bottleTimerStates },
                { "game", "mm" }, { "scene", gPlayState->sceneId }, { "entrance", gSaveContext.save.entrance }, { "cycleFlags", cycleFlags }, { "csId", CutsceneManager_GetCurrentCsId() }, { "room", gPlayState->roomCtx.curRoom.num }, { "rooms", gPlayState->roomList.count },
                { "health", gSaveContext.save.saveInfo.playerData.health },
                { "healthCapacity", gSaveContext.save.saveInfo.playerData.healthCapacity },
                { "ootmmEquip", { { "boots", OotmmCustomItems_EquippedBoots() }, { "tunic", OotmmCustomItems_EquippedTunic() },
                                  { "mask", OotmmCustomItems_EquippedMask() } } },
                { "envHazard", { { "hazard", sEnvHazard }, { "timer", gSaveContext.timerStates[TIMER_ID_ENV_HAZARD] } } },
                { "zTarget", gSaveContext.options.zTargetSetting },
                { "cvars", [] {
                     nlohmann::json values = nlohmann::json::object();
                     const char* names = std::getenv("OOTMM_RUNTIME_CVARS");
                     std::string list = names != nullptr ? names : "";
                     size_t start = 0;
                     while (start < list.size()) {
                         size_t end = list.find(',', start);
                         end = end == std::string::npos ? list.size() : end;
                         const std::string name = list.substr(start, end - start);
                         if (!name.empty()) {
                             values[name] = CVarGetInteger(name.c_str(), -1);
                         }
                         start = end + 1;
                     }
                     return values;
                 }() },
                { "fw", { { "data", OotmmFaroresWind_Point().data }, { "entrance", OotmmFaroresWind_Point().entrance },
                          { "x", OotmmFaroresWind_Point().pos.x }, { "y", OotmmFaroresWind_Point().pos.y },
                          { "z", OotmmFaroresWind_Point().pos.z } } },
                { "ammo", gSaveContext.save.saveInfo.inventory.ammo },
                { "heartPieces", (gSaveContext.save.saveInfo.inventory.questItems >> 28) & 0xF },
                { "dungeonKeys", std::vector<int>(std::begin(gSaveContext.save.saveInfo.inventory.dungeonKeys), std::begin(gSaveContext.save.saveInfo.inventory.dungeonKeys) + 4) },
                { "dungeonItems", std::vector<int>(std::begin(gSaveContext.save.saveInfo.inventory.dungeonItems), std::begin(gSaveContext.save.saveInfo.inventory.dungeonItems) + 4) },
                { "waterBoxes", [] {
                     nlohmann::json boxes = nlohmann::json::array();
                     const CollisionHeader* header = gPlayState->colCtx.colHeader;
                     for (u16 i = 0; header != nullptr && i < header->numWaterBoxes && i < 12; i++) {
                         const WaterBox& box = header->waterBoxes[i];
                         boxes.push_back({ box.minPos.x, box.minPos.y, box.minPos.z, box.xLength, box.zLength });
                     }
                     return boxes;
                 }() },
                { "strayFairies", std::vector<int>(std::begin(gSaveContext.save.saveInfo.inventory.strayFairies), std::begin(gSaveContext.save.saveInfo.inventory.strayFairies) + 5) },
                { "skullTokenCount", gSaveContext.save.saveInfo.skullTokenCount },
                { "swordEquip", GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) },
                { "fierceDeity", INV_CONTENT(ITEM_MASK_FIERCE_DEITY) },
                { "sceneFlags", { gPlayState->actorCtx.sceneFlags.chest, gPlayState->actorCtx.sceneFlags.switches[0], gPlayState->actorCtx.sceneFlags.switches[1], gPlayState->actorCtx.sceneFlags.collectible[0] } },
                { "rupees", gSaveContext.save.saveInfo.playerData.rupees },
                { "trapState", {
                    { "burning", GET_PLAYER(gPlayState)->bodyIsBurning },
                    { "shock", GET_PLAYER(gPlayState)->bodyShockTimer },
                    { "invincibility", GET_PLAYER(gPlayState)->invincibilityTimer },
                    { "applied", OotmmItemApply_TrapsApplied() }
                } },
                { "nativeMagic", {
                    { "value", gSaveContext.save.saveInfo.playerData.magic },
                    { "level", gSaveContext.save.saveInfo.playerData.magicLevel },
                    { "acquired", gSaveContext.save.saveInfo.playerData.isMagicAcquired },
                    { "double", gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired },
                    { "capacity", gSaveContext.magicCapacity },
                    { "fillTarget", gSaveContext.magicFillTarget },
                    { "state", gSaveContext.magicState },
                    { "toAdd", gSaveContext.magicToAdd },
                    { "requested", gSaveContext.isMagicRequested }
                } },
                { "x", player->actor.world.pos.x }, { "y", player->actor.world.pos.y },
                { "z", player->actor.world.pos.z }, { "yaw", player->actor.shape.rot.y },
                { "stateFlags2", player->stateFlags2 }, { "stateFlags3", player->stateFlags3 }, { "transition", gPlayState->transitionTrigger }, { "transitionMode", gPlayState->transitionMode }, { "magic", gSaveContext.save.saveInfo.playerData.magic }, { "stateFlags1", player->stateFlags1 }, { "message", gPlayState->msgCtx.currentTextId },
                { "bItem", BUTTON_ITEM_EQUIP(CUR_FORM, EQUIP_SLOT_B) },
                { "cItems", nlohmann::json::array({ BUTTON_ITEM_EQUIP(CUR_FORM, EQUIP_SLOT_C_LEFT), BUTTON_ITEM_EQUIP(CUR_FORM, EQUIP_SLOT_C_DOWN), BUTTON_ITEM_EQUIP(CUR_FORM, EQUIP_SLOT_C_RIGHT) }) },
                { "dpadItems", nlohmann::json::array({ DPAD_GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_D_UP), DPAD_GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_D_DOWN), DPAD_GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_D_LEFT), DPAD_GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_D_RIGHT) }) },
                { "form", gSaveContext.save.playerForm },
                { "aDoAction", gPlayState->interfaceCtx.aButtonDoAction }, { "currentMask", player->currentMask }, { "playerScale", player->actor.scale.y }, { "sfx", recentSfx }, { "camMode", gPlayState->cameraPtrs[gPlayState->activeCamId] != nullptr ? gPlayState->cameraPtrs[gPlayState->activeCamId]->mode : -1 }, { "heldItemAction", player->heldItemAction }, { "seedAmmo", OotmmCustomItems_SlingshotAmmo() }, { "currentBoots", player->currentBoots },
                { "csState", gPlayState->csCtx.state },
                { "bgmIO0", gAudioCtx.seqPlayers[SEQ_PLAYER_BGM_MAIN].seqScriptIO[0] },
                { "bgmIO4", gAudioCtx.seqPlayers[SEQ_PLAYER_BGM_MAIN].seqScriptIO[4] },
                { "sceneSeqState", gSceneSeqState },
                { "zTarget", gSaveContext.options.zTargetSetting },
                { "ambience", gPlayState->sceneSequences.ambienceId },
                { "itemAction", GET_PLAYER(gPlayState)->itemAction },
                { "heldItemAction", GET_PLAYER(gPlayState)->heldItemAction },
                { "firstPerson", GET_PLAYER(gPlayState)->unk_AA5 },
                { "actorCtxFlags", gPlayState->actorCtx.flags },
                { "csFrame", gPlayState->csCtx.curFrame },
                { "csIndex", gPlayState->csCtx.scriptIndex },
                { "sceneTimeSpeed", gPlayState->envCtx.sceneTimeSpeed },
                { "buttons", gPlayState->state.input[0].cur.button }, { "checks", checks },
                { "cButtons", { BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT), BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN),
                                BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_RIGHT) } },
                { "buttonStatus", { gSaveContext.buttonStatus[EQUIP_SLOT_C_LEFT], gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN],
                                    gSaveContext.buttonStatus[EQUIP_SLOT_C_RIGHT] } },
                { "shield", player->currentShield }, { "shieldEquip", GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) },
                { "pvp", { { "enabled", gOotmmPvpEnabled }, { "sent", gOotmmPvpHitsSent },
                           { "received", gOotmmPvpHitsReceived }, { "applied", gOotmmPvpHitsApplied } } },
                { "messageState", Message_GetState(&gPlayState->msgCtx) }, { "presentationPending", OotmmItemPresentation_IsPending() ? 1 : 0 }, { "backgroundShown", OotmmItemPresentation_DebugBackgroundShown() }, { "fanfare", { { "resolved", OotmmItemPresentation_DebugLastFanfare() }, { "seq", AudioSeq_GetActiveSeqId(SEQ_PLAYER_FANFARE) }, { "font", gAudioCtx.seqPlayers[SEQ_PLAYER_FANFARE].defaultFont } } },
                { "messageMode", gPlayState->msgCtx.msgMode },
                { "ocarinaMode", gPlayState->msgCtx.ocarinaMode },
                { "ocarinaStaff", gPlayState->msgCtx.msgMode != MSGMODE_OCARINA_PLAYING
                                      ? nlohmann::json(nullptr)
                                      : nlohmann::json({ gPlayState->msgCtx.ocarinaStaff->buttonIndex,
                                                         gPlayState->msgCtx.ocarinaStaff->state,
                                                         gPlayState->msgCtx.ocarinaStaff->pos }) },
                { "pauseState", gPlayState->pauseCtx.state },
                { "saveSucceeded", OotmmSaveMenu_SaveSucceeded() != 0 },
                { "sramStatus", gPlayState->sramCtx.status },
                { "cycleCount", gSaveContext.save.saveInfo.playerData.threeDayResetCount },
                { "playerForm", gSaveContext.save.playerForm },
                { "melee", { { "state", player->meleeWeaponState }, { "anim", player->meleeWeaponAnimation },
                             { "at0", player->meleeWeaponQuads[0].base.atFlags }, { "at1", player->meleeWeaponQuads[1].base.atFlags },
                             { "dmg", player->meleeWeaponQuads[0].elem.atDmgInfo.dmgFlags },
                             { "tip", { player->meleeWeaponInfo[0].tip.x, player->meleeWeaponInfo[0].tip.y, player->meleeWeaponInfo[0].tip.z } },
                             { "base", { player->meleeWeaponInfo[0].base.x, player->meleeWeaponInfo[0].base.y, player->meleeWeaponInfo[0].base.z } } } },
                { "day", gSaveContext.save.day },
                { "time", gSaveContext.save.time },
                { "weekEventReg", std::vector<u8>(std::begin(gSaveContext.save.saveInfo.weekEventReg),
                                                  std::end(gSaveContext.save.saveInfo.weekEventReg)) },
                { "eventInf", std::vector<u8>(std::begin(gSaveContext.eventInf), std::end(gSaveContext.eventInf)) },
                { "minigameScore", gSaveContext.minigameScore },
                { "ootmmWisps", gSaveContext.save.shipSaveInfo.ootmmWisps },
                { "choiceAnswer", gPlayState->msgCtx.unk1206C },
                { "pausePage", gPlayState->pauseCtx.pageIndex },
                { "infoMenu", nlohmann::json({ OotmmPauseInfo_Screen(), OotmmPauseInfo_Cursor() }) },
                { "bottles", [] {
                     nlohmann::json bottles = nlohmann::json::array();
                     for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
                         bottles.push_back(OotmmBottles_ContentOf(gSaveContext.save.saveInfo.inventory.items[slot]));
                     }
                     return bottles;
                 }() },
                { "chateau", CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI) ? 1 : 0 },
                { "rutoLetter", OotmmItemApply_LedgerGet("native:rutoLetterUsed") },
                { "pauseMainState", gPlayState->pauseCtx.mainState },
                { "itemCursor", gPlayState->pauseCtx.cursorPoint[PAUSE_ITEM] },
                { "itemCursorSlot", gPlayState->pauseCtx.cursorSlot[PAUSE_ITEM] },
                { "itemCursorItem", gPlayState->pauseCtx.cursorItem[PAUSE_ITEM] },
                { "itemCursorSpecial", gPlayState->pauseCtx.cursorSpecialPos },
                { "owned", OotmmIpc_GetInventory().GetValues() },
                { "items", gSaveContext.save.saveInfo.inventory.items }, { "questItems", gSaveContext.save.saveInfo.inventory.questItems },
                { "tradeItems", { gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_DEED],
                                  gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_KEY_MAMA],
                                  gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_COUPLE] } },
                { "mapCursor", gPlayState->pauseCtx.cursorPoint[PAUSE_WORLD_MAP] },
                { "questCursor", gPlayState->pauseCtx.cursorPoint[PAUSE_QUEST] },
                { "questCursorItem", gPlayState->pauseCtx.cursorItem[PAUSE_QUEST] },
                { "actors", actors }, { "majora", majora }, { "scenesVisible", gSaveContext.save.saveInfo.scenesVisible }, { "triforce", { { "count", OotmmTriforce_Count() }, { "goal", OotmmTriforce_Goal() }, { "displayMax", OotmmTriforce_DisplayMax() }, { "won", OotmmTriforce_HasWon() } } }, { "grassElements", grassElements }, { "setupActors", setupActors }, { "owlWarps", OotmmOwls_ActivatedMask() }, { "owlActivationFlags", gSaveContext.save.saveInfo.playerData.owlActivationFlags },
                { "cameraYaw", Camera_GetInputDirYaw(GET_ACTIVE_CAM(gPlayState)) },
                { "maze", TakarayaMaze() }
            });
        });
    }
#endif
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveLoad>([](s16) {
        sDropOptions.clear();
        sDropSelection = 0;
        sNpcOptions.clear();
        sNpcSelection = 0;
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s8, s8) {
        sDebugChests.clear();
        OotmmCsmc_ClearChestOverrides();
    });

    REGISTER_VB_SHOULD(VB_GIVE_ITEM_FROM_CHEST, {
        EnBox* chest = va_arg(args, EnBox*);
        Player* player = GET_PLAYER(gPlayState);
        if (player != nullptr && IsDebugChest(&chest->dyna.actor)) {
            OotmmChecks_OpenChestSilently(gPlayState, player);
            *should = false;
        }
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneFlagSet>(
        [](s16 sceneId, FlagType flagType, u32 flag) {
            if (flagType != FLAG_CYCL_SCENE_CHEST || gPlayState == nullptr) {
                return;
            }
            for (const auto& [chest, usedFlag] : sDebugChests) {
                if (usedFlag == static_cast<int32_t>(flag)) {
                    gPlayState->actorCtx.sceneFlags.chest &= ~(1 << flag);
                    return;
                }
            }
        });
}
