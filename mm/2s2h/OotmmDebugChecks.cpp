#include "OotmmDebugChecks.h"

#include "2s2h/BenGui/UIWidgets.hpp"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmChecks.h"
#include "OotmmCsmc.h"
#include "OotmmDrops.h"
#include "OotmmNpcChecks.h"
#include "OotmmSession.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
#include "src/overlays/gamestates/ovl_file_choose/z_file_select.h"

extern PlayState* gPlayState;
}

namespace {

// Never granted: the give is suppressed before the item is offered.
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

// Spawned chest -> the borrowed treasure flag, so the flag can be scrubbed on open.
std::unordered_map<Actor*, int32_t> sDebugChests;

bool IsDebugChest(Actor* actor) {
    return sDebugChests.count(actor) != 0;
}

// A treasure flag no real check owns and nothing in the scene has set.
int32_t FreeChestFlag() {
    for (int32_t flag = 31; flag >= 0; flag--) {
        if (Flags_GetTreasure(gPlayState, flag) != 0) {
            continue;
        }
        if (OotmmSession_GetState().FindCheck(Ship::OotmmGame::Mm,
                                              Play_GetOriginalSceneId(gPlayState->sceneId), "chest",
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

// The seed's distinct items, so every model a drop can show is testable.
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

void OotmmDebugChecks_Init() {
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

    // The borrowed flag only sequences the open; the scene keeps no record of it.
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
