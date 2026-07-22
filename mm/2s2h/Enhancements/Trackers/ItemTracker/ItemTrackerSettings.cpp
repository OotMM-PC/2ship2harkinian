#include "ItemTrackerSettings.h"
#include "ItemTracker.h"
#include "2s2h/BenGui/UIWidgets.hpp"
#include "ShipUtils.h"
#include "ship/config/Config.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/BenPort.h"

namespace BenGui {
extern std::shared_ptr<ItemTrackerWindow> mItemTrackerWindow;
}

void ItemTrackerSettingsWindow::UpdateElement() {
}

#define WIDGET_COLOR UIWidgets::Colors(CVarGetInteger("gSettings.Menu.Theme", 5))
#define CVAR_NAME_VISIBILITY_MODE "gSettings.ItemTracker.VisibilityMode"
#define CVAR_NAME_VISIBILITY_BTN "gSettings.ItemTracker.VisibilityBtn"
#define CVAR_VISIBILITY_MODE CVarGetInteger(CVAR_NAME_VISIBILITY_MODE, ITEM_TRACKER_VISIBILITY_MODE_ALWAYS)
#define CVAR_VISIBILITY_BTN CVarGetInteger(CVAR_NAME_VISIBILITY_BTN, BTN_CUSTOM_MODIFIER1)

static const char* windowTypes[2] = { "Floating", "Window" };

static std::unordered_map<int32_t, const char*> sItemTrackerVisibilityModes = {
    { ITEM_TRACKER_VISIBILITY_MODE_ALWAYS, "Always" },
    { ITEM_TRACKER_VISIBILITY_MODE_ONLY_ON_PAUSE_MENU, "Only on Pause Menu" },
    { ITEM_TRACKER_VISIBILITY_MODE_BUTTON_TOGGLE, "Button Toggle" },
    { ITEM_TRACKER_VISIBILITY_MODE_BUTTON_HOLD, "Button Hold" },
};

std::vector<TrackerGroup> itemTrackerGroupsAvailable;
void SaveItemTrackerLayout();

std::vector<std::pair<TrackerItemType, u32>> GetItemsFromRange(TrackerItemType itemType, u32 start, u32 end) {
    std::vector<std::pair<TrackerItemType, u32>> items;
    for (u32 itemId = start; itemId <= end; itemId++) {
        items.push_back({ itemType, itemId });
    }
    return items;
}

std::string GetItemTrackerItemName(TrackerItemType itemType, u32 itemId) {
    switch (itemType) {
        case TRACKER_ITEM_SWORD: {
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_KOKIRI) {
                return "Kokiri Sword";
            }
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_RAZOR) {
                return "Razor Sword";
            }
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_GILDED) {
                return "Gilded Sword";
            }
            return "Sword (None)";
        } break;
        case TRACKER_ITEM_SHIELD: {
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_HERO) {
                return "Hero's Shield";
            }
            if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_MIRROR) {
                return "Mirror Shield";
            }
            return "Shield (None)";
        } break;
        case TRACKER_ITEM_WALLET: {
            if (CUR_UPG_VALUE(UPG_WALLET) >= 2) {
                return "Giant's Wallet";
            }
            if (CUR_UPG_VALUE(UPG_WALLET) >= 1) {
                return "Adult's Wallet";
            }
            return "Wallet (None)";
        } break;
        case TRACKER_ITEM_MAGIC: {
            if (gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired) {
                return "Magic Upgrade";
            }
            if (gSaveContext.save.saveInfo.playerData.isMagicAcquired) {
                return "Power of Magic";
            }
            return "Magic (None)";
        } break;
        default:
            break;
    }

    return "";
}

struct DragDropPayload {
    TrackerItemType itemType;
    u32 itemId;
    int sourceGroupIndex;
    int sourceItemIndex;
    bool fromAddMode;
};

void DrawItemTrackerGroupPreview(TrackerGroup& group, bool addMode, int groupIndex) {
    static std::pair<TrackerItemType, u32> selectedItem = { TRACKER_ITEM_SLOT, 0 };

    if (ImGui::BeginChild(group.name.c_str(), ImVec2(0, 0),
                          ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX |
                              ImGuiChildFlags_AutoResizeY)) {
        if (ImGui::BeginTable(group.name.c_str(), group.columns)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

            // Use index-based loop to avoid iterator invalidation
            for (size_t itemIndex = 0; itemIndex < group.items.size(); itemIndex++) {
                auto& [itemType, itemId] = group.items[itemIndex];
                ImGui::TableNextColumn();
                ImGui::PushID((int)itemIndex);
                if (DrawItemTrackerSlot(itemType, itemId, 1.0f, true)) {
                    if (addMode) {
                        selectedItem = { itemType, itemId };
                        ImGui::OpenPopup("AddToGroup");
                    } else {
                        group.items.erase(
                            std::remove(group.items.begin(), group.items.end(), std::make_pair(itemType, itemId)),
                            group.items.end());
                        SaveItemTrackerLayout();
                    }
                }

                // Drag and drop functionality
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    DragDropPayload payload;
                    payload.itemType = itemType;
                    payload.itemId = itemId;
                    payload.sourceGroupIndex = groupIndex;
                    payload.sourceItemIndex = (int)itemIndex;
                    payload.fromAddMode = addMode;
                    ImGui::SetDragDropPayload("TRACKER_ITEM", &payload, sizeof(DragDropPayload));

                    // Display preview while dragging
                    DrawItemTrackerSlot(itemType, itemId, 0.5f, true);
                    ImGui::EndDragDropSource();
                }

                // Only allow dropping on removeMode slots
                if (!addMode && ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("TRACKER_ITEM")) {
                        DragDropPayload* payload = (DragDropPayload*)imguiPayload->Data;

                        if (payload->fromAddMode) {
                            // Dragging from addMode to removeMode: insert at this position
                            group.items.insert(group.items.begin() + itemIndex,
                                               std::make_pair(payload->itemType, payload->itemId));
                            SaveItemTrackerLayout();
                        } else {
                            // Dragging from removeMode to removeMode: move item
                            if (payload->sourceGroupIndex == groupIndex) {
                                // Moving within same group
                                if (payload->sourceItemIndex != (int)itemIndex) {
                                    auto item = group.items[payload->sourceItemIndex];
                                    group.items.erase(group.items.begin() + payload->sourceItemIndex);

                                    // Adjust target index if we removed an item before it
                                    int targetIndex = (int)itemIndex;
                                    if (payload->sourceItemIndex < (int)itemIndex) {
                                        targetIndex--;
                                    }
                                    group.items.insert(group.items.begin() + targetIndex, item);
                                    SaveItemTrackerLayout();
                                }
                            } else {
                                // Moving between different groups
                                auto item = std::make_pair(payload->itemType, payload->itemId);
                                itemTrackerGroups[payload->sourceGroupIndex].items.erase(
                                    itemTrackerGroups[payload->sourceGroupIndex].items.begin() +
                                    payload->sourceItemIndex);
                                group.items.insert(group.items.begin() + itemIndex, item);
                                SaveItemTrackerLayout();
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                // Check for popup in the same ID scope
                if (addMode && ImGui::BeginPopup("AddToGroup")) {
                    UIWidgets::PushStyleMenuItem(WIDGET_COLOR);
                    ImGui::SeparatorText("Select group to add item to");
                    for (size_t i = 0; i < itemTrackerGroups.size(); i++) {
                        if (ImGui::Selectable(itemTrackerGroups[i].name.c_str(), false)) {
                            itemTrackerGroups[i].items.push_back(selectedItem);
                            SaveItemTrackerLayout();
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    UIWidgets::PopStyleMenuItem();
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
            ImGui::PopStyleVar(2);
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

bool DrawTrackerWindowOptions(int32_t windowIndex, TrackerGroup& group) {
    ImGui::PushID(windowIndex);
    int32_t columns = group.columns;
    float scale = group.scale;
    std::string trackerInputRename;
    if (windowIndex >= 0) {
        if (UIWidgets::InputString("##name", &trackerInputRename,
                                   UIWidgets::InputOptions()
                                       .LabelPosition(UIWidgets::LabelPosition::None)
                                       .Color(WIDGET_COLOR)
                                       .PlaceholderText(group.name)
                                       .DefaultValue(group.name))) {
            if (!trackerInputRename.empty() && trackerInputRename != group.name) {
                group.name = trackerInputRename;
                SaveItemTrackerLayout();
            }
        }
    }
    if (UIWidgets::SliderInt("Columns", &columns,
                             UIWidgets::IntSliderOptions()
                                 .Min(1)
                                 .Max(12)
                                 .DefaultValue(6)
                                 .LabelPosition(UIWidgets::LabelPosition::None)
                                 .Format("Columns: %i")
                                 .Color(WIDGET_COLOR))) {
        group.columns = columns;
        SaveItemTrackerLayout();
    }
    if (CVarGetInteger("gSettings.ItemTracker.WindowGroup", 0)) {
        if (UIWidgets::SliderFloat("Scale", &scale,
                                   UIWidgets::FloatSliderOptions()
                                       .Min(0.2f)
                                       .Max(3.0f)
                                       .DefaultValue(1.0f)
                                       .LabelPosition(UIWidgets::LabelPosition::None)
                                       .Format("Scale: %.1f")
                                       .Step(0.5f)
                                       .Color(WIDGET_COLOR))) {
            group.scale = scale;
            SaveItemTrackerLayout();
        }
    }
    if (UIWidgets::Button("Remove Group", { .color = UIWidgets::Colors::Red })) {
        itemTrackerGroups.erase(itemTrackerGroups.begin() + windowIndex);
        SaveItemTrackerLayout();
        ImGui::PopID();
        return true;
    }
    ImGui::PopID();
    return false;
}

void LoadAvailableWindows() {
    itemTrackerGroupsAvailable.clear();

    itemTrackerGroupsAvailable.push_back(TrackerGroup{
        .name = "Inventory",
        .columns = 6,
        .scale = 1.0f,
        .items = GetItemsFromRange(TRACKER_ITEM_SLOT, SLOT_OCARINA, SLOT_BOTTLE_6),
    });

    itemTrackerGroupsAvailable.push_back(TrackerGroup{
        .name = "Masks",
        .columns = 6,
        .scale = 1.0f,
        .items = GetItemsFromRange(TRACKER_ITEM_SLOT, SLOT_MASK_POSTMAN, SLOT_MASK_FIERCE_DEITY),
    });

    itemTrackerGroupsAvailable.push_back(TrackerGroup{
        .name = "Quest",
        .columns = 4,
        .scale = 1.0f,
        .items = {
            { TRACKER_ITEM_SWORD, 0 },
            { TRACKER_ITEM_SHIELD, 0 },
            { TRACKER_ITEM_MAGIC, 0 },
            { TRACKER_ITEM_WALLET, 0 },
        },
    });
}

void ApplyDefaultItemPreset() {
    itemTrackerGroups.clear();

    std::set<std::string> defaultGroups = {
        "Inventory",
        "Masks",
        "Quest",
    };

    for (auto& group : itemTrackerGroupsAvailable) {
        if (defaultGroups.count(group.name)) {
            itemTrackerGroups.push_back(group);
        }
    }
}

void SaveItemTrackerLayout() {
    auto itemTrackerLayout = nlohmann::json::array();
    for (auto& group : itemTrackerGroups) {
        auto groupJson = nlohmann::json::object({
            { "name", group.name },
            { "columns", group.columns },
            { "scale", group.scale },
            { "items", group.items },
        });
        itemTrackerLayout.push_back(groupJson);
    }
    Ship::Context::GetInstance()->GetConfig()->SetBlock("CVars.ItemTrackerLayout", itemTrackerLayout);
    Ship::Context::GetInstance()->GetConfig()->Save();
}

void LoadItemTrackerConfig() {
    auto allConfig = Ship::Context::GetInstance()->GetConfig()->GetNestedJson();

    // Verify that the config has CVars.ItemTrackerLayout and its an array
    if (allConfig.find("CVars") != allConfig.end() && allConfig["CVars"].is_object() &&
        allConfig["CVars"].find("ItemTrackerLayout") != allConfig["CVars"].end() &&
        allConfig["CVars"]["ItemTrackerLayout"].is_array()) {

        itemTrackerGroups.clear();
        auto& itemTrackerLayout = allConfig["CVars"]["ItemTrackerLayout"];
        for (auto& group : itemTrackerLayout) {
            std::vector<std::pair<TrackerItemType, u32>> items;

            // Manually parse items array
            if (group.contains("items") && group["items"].is_array()) {
                for (auto& item : group["items"]) {
                    if (item.is_array() && item.size() >= 2) {
                        items.push_back({ static_cast<TrackerItemType>(item[0].get<int>()), item[1].get<u32>() });
                    }
                }
            }

            itemTrackerGroups.push_back(TrackerGroup{
                .name = group.value("name", std::string("")),
                .columns = group.value("columns", (u8)6),
                .scale = group.value("scale", 1.0f),
                .items = items,
            });
        }
    }
}

void DrawTrackerOptions() {
    if (ImGui::BeginTable("OptionsTable", 3)) {
        ImGui::TableNextColumn();
        if (CVarGetInteger("gWindows.ItemTracker", 0)) {
            UIWidgets::WindowButton("Disable Item Tracker", "gWindows.ItemTracker", BenGui::mItemTrackerWindow,
                                    { .size = UIWidgets::Sizes::Inline, .color = UIWidgets::Colors::Red });
        } else {
            UIWidgets::WindowButton("Enable Item Tracker", "gWindows.ItemTracker", BenGui::mItemTrackerWindow,
                                    { .size = UIWidgets::Sizes::Inline, .color = UIWidgets::Colors::Green });
        }

        UIWidgets::CVarCheckbox("Split Window Groups", "gSettings.ItemTracker.WindowGroup");
        UIWidgets::CVarCheckbox("Show Item Counts", "gSettings.ItemTracker.ItemCounts",
                                UIWidgets::CheckboxOptions().DefaultValue(true));
        ImGui::TextWrapped(
            "Click or drag & drop individual items, or use the corner buttons to add/remove entire groups.");
        ImGui::TableNextColumn();

        UIWidgets::CVarCombobox("Window Type", "gSettings.ItemTracker.WindowType", windowTypes,
                                UIWidgets::ComboboxOptions()
                                    .ComponentAlignment(UIWidgets::ComponentAlignment::Right)
                                    .LabelPosition(UIWidgets::LabelPosition::Far));
        UIWidgets::CVarCombobox("Visibility", CVAR_NAME_VISIBILITY_MODE, &sItemTrackerVisibilityModes,
                                UIWidgets::ComboboxOptions()
                                    .DefaultIndex(ITEM_TRACKER_VISIBILITY_MODE_ALWAYS)
                                    .ComponentAlignment(UIWidgets::ComponentAlignment::Right)
                                    .LabelPosition(UIWidgets::LabelPosition::Far));
        if (CVAR_VISIBILITY_MODE == ITEM_TRACKER_VISIBILITY_MODE_BUTTON_TOGGLE ||
            CVAR_VISIBILITY_MODE == ITEM_TRACKER_VISIBILITY_MODE_BUTTON_HOLD) {
            UIWidgets::CVarBtnSelector("Button Combination:", CVAR_NAME_VISIBILITY_BTN,
                                       UIWidgets::BtnSelectorOptions().DefaultValue(BTN_CUSTOM_MODIFIER1));
        }

        ImGui::TableNextColumn();

        if (UIWidgets::Button("Restore Default Groups", { .color = UIWidgets::Colors::Gray })) {
            ApplyDefaultItemPreset();
            SaveItemTrackerLayout();
        }

        if (!CVarGetInteger("gSettings.ItemTracker.WindowGroup", 0)) {
            UIWidgets::CVarSliderFloat("Scale", "gSettings.ItemTracker.Scale",
                                       UIWidgets::FloatSliderOptions()
                                           .Min(0.2f)
                                           .Max(3.0f)
                                           .DefaultValue(1.0f)
                                           .LabelPosition(UIWidgets::LabelPosition::None)
                                           .Format("Scale: %.1f")
                                           .Step(0.5f)
                                           .Color(WIDGET_COLOR));
        }

        UIWidgets::CVarSliderFloat("Opacity", "gSettings.ItemTracker.Opacity",
                                   UIWidgets::FloatSliderOptions()
                                       .Min(0.0f)
                                       .Max(1.0f)
                                       .DefaultValue(0.5f)
                                       .Format("Opacity: %.1f")
                                       .LabelPosition(UIWidgets::LabelPosition::None)
                                       .Step(0.1f)
                                       .Color(WIDGET_COLOR));
        static std::string trackerInputName;
        UIWidgets::InputString("##Group Name", &trackerInputName,
                               UIWidgets::InputOptions()
                                   .LabelPosition(UIWidgets::LabelPosition::None)
                                   .Color(WIDGET_COLOR)
                                   .PlaceholderText("Enter new group name")
                                   .Size({ ImGui::GetContentRegionAvail().x - 45.0f, 0 }));
        ImGui::SameLine();
        if (UIWidgets::Button(ICON_FA_PLUS, { .color = UIWidgets::Colors::Green })) {
            itemTrackerGroups.push_back(TrackerGroup{
                .name = trackerInputName.c_str(),
                .columns = 6,
                .scale = 1.0f,
            });
            SaveItemTrackerLayout();
            trackerInputName.clear();
        }

        ImGui::EndTable();
    }
}

void DrawTrackerAvailableGroups() {
    if (ImGui::BeginChild("Available Groups")) {
        int groupIndex = 0;
        for (auto& group : itemTrackerGroupsAvailable) {
            ImGui::PushID(groupIndex);
            ImGui::SeparatorText(group.name.c_str());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - (ImGui::CalcTextSize("aaa").x * 1.5f));
            if (UIWidgets::Button(ICON_FA_PLUS, { .size = ImVec2(0, 0), .color = UIWidgets::Colors::Green })) {
                itemTrackerGroups.push_back(group);
                SaveItemTrackerLayout();
            }
            DrawItemTrackerGroupPreview(group, true, groupIndex);
            ImGui::PopID();
            groupIndex++;
        }
    }
    ImGui::EndChild();
}

void DrawTrackerActiveGroups() {
    if (ImGui::BeginChild("Active Groups")) {
        int groupIndex = 0;
        for (auto& group : itemTrackerGroups) {
            ImGui::PushID(groupIndex);
            ImGui::SeparatorText(group.name.c_str());

            ImGui::SameLine(ImGui::GetContentRegionMax().x - (ImGui::CalcTextSize("aaaaaaaaa").x * 1.5f));
            if (UIWidgets::Button(ICON_FA_CHEVRON_UP, { .size = ImVec2(0, 0), .color = WIDGET_COLOR })) {
                if (groupIndex > 0) {
                    std::swap(itemTrackerGroups[groupIndex], itemTrackerGroups[groupIndex - 1]);
                    SaveItemTrackerLayout();
                }
            }
            ImGui::SameLine();
            if (UIWidgets::Button(ICON_FA_CHEVRON_DOWN, { .size = ImVec2(0, 0), .color = WIDGET_COLOR })) {
                if (groupIndex < static_cast<int>(itemTrackerGroups.size()) - 1) {
                    std::swap(itemTrackerGroups[groupIndex], itemTrackerGroups[groupIndex + 1]);
                    SaveItemTrackerLayout();
                }
            }
            ImGui::SameLine();
            if (UIWidgets::Button(ICON_FA_ELLIPSIS_H, { .size = ImVec2(0, 0), .color = UIWidgets::Colors::Gray })) {
                ImGui::OpenPopup("GroupOptions");
            }
            ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x - 400, ImGui::GetItemRectMin().y),
                                    ImGuiCond_Always);
            if (ImGui::BeginPopup("GroupOptions")) {
                if (DrawTrackerWindowOptions(groupIndex, group)) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            DrawItemTrackerGroupPreview(group, false, groupIndex);
            ImGui::PopID();
            groupIndex++;
        }
    }
    ImGui::EndChild();
}

void ItemTrackerSettingsWindow::DrawElement() {
    ImGui::SetNextWindowSize(ImVec2(733, 472), ImGuiCond_FirstUseEver);
    if (ImGui::BeginChild("Item Tracker Settings")) {
        DrawTrackerOptions();

        UIWidgets::PushStyleTabs(WIDGET_COLOR);
        if (ImGui::BeginTable("TrackerTabs", 2)) {
            ImGui::TableNextColumn();
            if (ImGui::BeginChild("TrackerChild")) {
                if (ImGui::BeginTabBar("TrackerTabs")) {
                    if (ImGui::BeginTabItem("Available Groups")) {
                        DrawTrackerAvailableGroups();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();

            ImGui::TableNextColumn();
            if (ImGui::BeginChild("WindowChild")) {
                if (ImGui::BeginTabBar("WindowTab")) {
                    if (ImGui::BeginTabItem("Active Groups")) {
                        DrawTrackerActiveGroups();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();
            ImGui::EndTable();
        }
        UIWidgets::PopStyleTabs();
    }
    ImGui::EndChild();
}

void ItemTrackerSettingsWindow::InitElement() {
    LoadAvailableWindows();
    ApplyDefaultItemPreset();
    LoadItemTrackerConfig();
}
