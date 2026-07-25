#include "OotmmItemProbe.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmItemModels.h"
#include "OotmmSession.h"

#include <libultraship/bridge/OotmmItemRender.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "z64.h"

extern PlayState* gPlayState;
}

namespace {

constexpr size_t kItemsPerFrame = 16;

struct ProbeItem {
    std::string Id;
    std::string Name;
};

struct TierNames {
    const char* Suffix;
    std::vector<const char*> Names;
};

// ProgressiveRecipe picks models by display-name substring, so every tier name is
// its own resolution path and has to be probed separately.
const std::vector<TierNames> kTiers = {
    { "_OCARINA", { "Fairy Ocarina", "Ocarina of Time" } },
    { "_BOMB_BAG", { "Bomb Bag", "Big Bomb Bag", "Biggest Bomb Bag" } },
    { "_BOMBCHU_BAG", { "Bombchu Bag", "Big Bombchu Bag", "Biggest Bombchu Bag" } },
    { "_BOW", { "Fairy Bow", "Hero's Bow", "Big Quiver", "Biggest Quiver" } },
    { "_SLINGSHOT", { "Fairy Slingshot", "Large Bullet Bag", "Largest Bullet Bag" } },
    { "_HOOKSHOT", { "Hookshot", "Longshot", "Short Hookshot" } },
    { "_MAGIC_UPGRADE", { "Magic Upgrade", "Larger Magic Upgrade" } },
    { "_STRENGTH", { "Goron's Bracelet", "Silver Gauntlets", "Golden Gauntlets" } },
    { "_WALLET",
      { "Child's Wallet", "Child Wallet", "Adult's Wallet", "Adult Wallet", "Giant's Wallet",
        "Giant Wallet", "Colossal Wallet", "Bottomless Wallet" } },
    { "_SCALE", { "Bronze Scale", "Silver Scale", "Golden Scale" } },
    { "_SWORD",
      { "Kokiri Sword", "Master Sword", "Giant's Knife", "Biggoron's Sword", "Razor Sword",
        "Gilded Sword" } },
    { "_SWORD_GORON", { "Giant's Knife", "Biggoron's Sword" } },
    { "_SHIELD", { "Deku Shield", "Hylian Shield", "Hero's Shield", "Mirror Shield" } },
    { "_STICK_UPGRADE", { "Deku Stick Upgrade", "Second Deku Stick Upgrade" } },
    { "_NUT_UPGRADE", { "Deku Nut Upgrade", "Second Deku Nut Upgrade" } },
    { "_SONG_GORON_HALF", { "Goron Lullaby Intro", "Goron Lullaby" } },
};

std::vector<ProbeItem> sItems;
std::vector<std::string> sFailures;
size_t sCursor = 0;
bool sRunning = false;
bool sAutoDone = false;
int32_t sAutoDelay = 240;

bool EndsWith(const std::string& value, const char* suffix) {
    const size_t length = strlen(suffix);
    return value.size() >= length && value.compare(value.size() - length, length, suffix) == 0;
}

void AddTierNames() {
    const size_t base = sItems.size();
    for (size_t i = 0; i < base; i++) {
        const std::string id = sItems[i].Id;
        for (const auto& tier : kTiers) {
            if (!EndsWith(id, tier.Suffix)) {
                continue;
            }
            for (const char* name : tier.Names) {
                sItems.push_back({ id, name });
            }
        }
    }
}

void Collect() {
    sItems.clear();
    const auto& state = OotmmSession_GetState();
    for (const auto& item : state.GetSeedItems()) {
        sItems.push_back({ item.Id, item.Name });
    }
    for (const auto& item : state.GetCustomItemCatalog().GetItems()) {
        sItems.push_back({ item.Id, item.Name });
    }
    for (const auto& id : Ship::OotmmItemRenderCatalog::AllIds()) {
        sItems.push_back({ std::string(id), "" });
    }
    for (const auto& item : Ship::OotmmItemRenderCatalog::DebugOnlyItems()) {
        sItems.push_back({ std::string(item.Id), std::string(item.Name) });
    }

    AddTierNames();
    std::sort(sItems.begin(), sItems.end(), [](const ProbeItem& a, const ProbeItem& b) {
        return a.Id != b.Id ? a.Id < b.Id : a.Name < b.Name;
    });
    sItems.erase(std::unique(sItems.begin(), sItems.end(),
                             [](const ProbeItem& a, const ProbeItem& b) {
                                 return a.Id == b.Id && a.Name == b.Name;
                             }),
                 sItems.end());
}

void Report() {
    if (sFailures.empty()) {
        SPDLOG_INFO("[OoTMM probe] all {} items resolved a model", sItems.size());
        return;
    }
    SPDLOG_WARN("[OoTMM probe] {} of {} items fall back to the green rupee:", sFailures.size(),
                sItems.size());
    for (const auto& failure : sFailures) {
        SPDLOG_WARN("[OoTMM probe]   {}", failure);
    }
}

void Tick() {
    if (gPlayState == nullptr) {
        return;
    }
    if (!sAutoDone && !sRunning && OotmmSession_GetState().GetBootConfig().AllowDebugMenus &&
        --sAutoDelay <= 0) {
        sAutoDone = true;
        OotmmItemProbe_Start();
    }
    if (!sRunning) {
        return;
    }
    GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
    if (gfxCtx == nullptr) {
        return;
    }

    const size_t end = std::min(sCursor + kItemsPerFrame, sItems.size());
    for (; sCursor < end; sCursor++) {
        const ProbeItem& item = sItems[sCursor];
        // Run the real draw path, then rewind the display lists so nothing renders.
        Gfx* opa = gfxCtx->polyOpa.p;
        Gfx* xlu = gfxCtx->polyXlu.p;
        const bool drawn = OotmmItemModel_DrawById(gPlayState, item.Id, item.Name);
        gfxCtx->polyOpa.p = opa;
        gfxCtx->polyXlu.p = xlu;
        if (!drawn) {
            sFailures.push_back(item.Name.empty() ? item.Id
                                                  : item.Id + " \"" + item.Name + "\"");
        }
    }

    if (sCursor >= sItems.size()) {
        sRunning = false;
        Report();
    }
}

} // namespace

void OotmmItemProbe_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawWorldEnd>(Tick);
}

void OotmmItemProbe_Start() {
    if (sRunning) {
        return;
    }
    Collect();
    sFailures.clear();
    sCursor = 0;
    sRunning = true;
    SPDLOG_INFO("[OoTMM probe] probing {} items", sItems.size());
}

bool OotmmItemProbe_Running() {
    return sRunning;
}
