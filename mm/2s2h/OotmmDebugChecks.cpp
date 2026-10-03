#include "OotmmDebugChecks.h"

#include "2s2h/BenGui/UIWidgets.hpp"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmChecks.h"
#include "OotmmCsmc.h"
#include "OotmmDrops.h"
#include "OotmmSession.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"

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

bool MatchesFilter(const std::string& name) {
    if (sDropFilter[0] == '\0') {
        return true;
    }
    std::string lowerName = name;
    std::string lowerFilter = sDropFilter;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
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
            if (!MatchesFilter(sDropOptions[i].ItemName)) {
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
}

void OotmmDebugChecks_Init() {
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
