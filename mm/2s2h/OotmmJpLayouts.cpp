#include "OotmmJpLayouts.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "OotmmSession.h"

#include <iterator>
#include <libultraship/bridge/resourcebridge.h>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace {

struct ActorMove {
    s16 Index;
    Vec3s Pos;
};

struct VertexHeight {
    s16 Index;
    s16 Y;
};

struct PolyDistance {
    s16 Index;
    s16 Dist;
};

struct PathPatch {
    s16 Index;
    u8 Count;
    Vec3s Points[6];
};

constexpr s16 kStoneTowerTempleSunSwitch = 17;
constexpr s16 kStoneTowerSwitches[] = { 86, 91, 92 };
constexpr ActorMove kStoneTowerBlocks[] = {
    { 85, { 164, -600, 1510 } },
    { 90, { -156, -600, 1512 } },
};
constexpr PathPatch kStoneTowerPaths[] = {
    { 14, 5, { { 164, -506, 1511 }, { 164, -506, 1711 }, { 4, -506, 1711 }, { 4, -680, 1711 }, { 4, -680, 1871 } } },
    { 16,
      6,
      { { -156, -506, 1511 },
        { -156, -506, 1871 },
        { 4, -506, 1871 },
        { 4, -506, 2151 },
        { 4, -680, 2151 },
        { 4, -680, 2431 } } },
};

constexpr VertexHeight kCoastCollisionVertices[] = {
    { 558, -20 }, { 559, -20 }, { 560, 40 }, { 561, 40 }, { 562, 40 }, { 563, -20 }, { 564, 40 }, { 565, -20 },
};
constexpr PolyDistance kCoastCollisionPolys[] = { { 783, -40 }, { 784, -40 }, { 789, -20 }, { 790, -20 } };
constexpr s16 kCoastPlatformHeights[] = { 40, 40, 40,  40, -20, -20, -8, 28, 40, 40, 28,  -8,  -8,  28, 40,
                                          40, 28, -8,  -20, -20, 40, 40, 28, 28, -8, -8, -20, -20, 28, 28,
                                          40, 40, -8,  -8,  -20, -20, -20, -20, 40, 40, -20, -20, 40, 40 };
constexpr s16 kCoastPlatformBaseHeight = -20;
constexpr s32 kCoastPlatformBaseVertices = 4;
constexpr s16 kCoastStatueY = 40;
constexpr s16 kCoastStatueX = -2880;
constexpr s16 kCoastStatueZ = 3921;
constexpr const char* kCoastPlatformVtx = "scenes/nonmq/Z2_30GYOSON/Z2_30GYOSON_room_00Vtx_012038";
constexpr const char* kCoastPlatformBaseVtx = "scenes/nonmq/Z2_30GYOSON/Z2_30GYOSON_room_00Vtx_012BC8";

bool HasLayout(const char* member) {
    return OotmmSession_IsActive() && OotmmSession_GetState().WorldFlagContains("jpLayouts", member);
}

bool StoneTowerLayout() {
    return HasLayout("StoneTower") && !OotmmSession_GetState().WorldFlagContains("openDungeonsMm", "ST");
}

void PatchStoneTowerPaths() {
    if (gPlayState->setupPathList == nullptr) {
        return;
    }
    for (const auto& patch : kStoneTowerPaths) {
        Path* path = &gPlayState->setupPathList[patch.Index];
        Vec3s* points = static_cast<Vec3s*>(Lib_SegmentedToVirtual(path->points));
        path->count = patch.Count;
        for (u8 i = 0; i < patch.Count; i++) {
            points[i] = patch.Points[i];
        }
    }
}

void PatchCoastMesh() {
    auto* platform = static_cast<Vtx*>(ResourceGetDataByName(kCoastPlatformVtx));
    auto* base = static_cast<Vtx*>(ResourceGetDataByName(kCoastPlatformBaseVtx));
    if (platform == nullptr || base == nullptr) {
        return;
    }
    for (size_t i = 0; i < std::size(kCoastPlatformHeights); i++) {
        platform[i].v.ob[1] = kCoastPlatformHeights[i];
    }
    for (s32 i = 0; i < kCoastPlatformBaseVertices; i++) {
        base[i].v.ob[1] = kCoastPlatformBaseHeight;
    }
}

void ApplySceneLayouts(s8 sceneId, s8) {
    if (sceneId == SCENE_F40 && StoneTowerLayout()) {
        PatchStoneTowerPaths();
    }
    if (sceneId == SCENE_30GYOSON && HasLayout("GreatBayCoast")) {
        PatchCoastMesh();
    }
}

} // namespace

extern "C" void OotmmJpLayouts_PatchCollision(PlayState* play, CollisionHeader* colHeader) {
    if (play->sceneId != SCENE_30GYOSON || !HasLayout("GreatBayCoast") || colHeader == nullptr) {
        return;
    }
    for (const auto& vertex : kCoastCollisionVertices) {
        if (vertex.Index < colHeader->numVertices) {
            colHeader->vtxList[vertex.Index].y = vertex.Y;
        }
    }
    for (const auto& poly : kCoastCollisionPolys) {
        if (poly.Index < colHeader->numPolygons) {
            colHeader->polyList[poly.Index].dist = poly.Dist;
        }
    }
}

void OotmmJpLayouts_Init() {
    auto* interactor = GameInteractor::Instance;
    interactor->RegisterGameHook<GameInteractor::OnSceneInit>(ApplySceneLayouts);
    interactor->RegisterGameHookForID<GameInteractor::ShouldActorInit>(
        ACTOR_OBJ_LIGHTSWITCH, [](Actor* actor, bool* should) {
            if (gPlayState->sceneId == SCENE_INISIE_N && actor->room == 9 &&
                GetActorListIndex(actor) == kStoneTowerTempleSunSwitch && HasLayout("ST")) {
                *should = false;
            }
        });
    interactor->RegisterGameHookForID<GameInteractor::ShouldActorInit>(
        ACTOR_BG_F40_SWITCH, [](Actor* actor, bool* should) {
            if (gPlayState->sceneId != SCENE_F40 || actor->room != 0 || !StoneTowerLayout()) {
                return;
            }
            const s16 index = GetActorListIndex(actor);
            for (s16 disabled : kStoneTowerSwitches) {
                if (index == disabled) {
                    *should = false;
                }
            }
        });
    interactor->RegisterGameHookForID<GameInteractor::ShouldActorInit>(
        ACTOR_BG_F40_BLOCK, [](Actor* actor, bool*) {
            if (gPlayState->sceneId != SCENE_F40 || actor->room != 0 || !StoneTowerLayout()) {
                return;
            }
            const s16 index = GetActorListIndex(actor);
            for (const auto& block : kStoneTowerBlocks) {
                if (index == block.Index) {
                    actor->world.pos = actor->home.pos = { static_cast<f32>(block.Pos.x),
                                                           static_cast<f32>(block.Pos.y),
                                                           static_cast<f32>(block.Pos.z) };
                }
            }
        });
    interactor->RegisterGameHookForID<GameInteractor::ShouldActorInit>(
        ACTOR_OBJ_WARPSTONE, [](Actor* actor, bool*) {
            if (gPlayState->sceneId == SCENE_30GYOSON && HasLayout("GreatBayCoast") &&
                static_cast<s16>(actor->home.pos.x) == kCoastStatueX &&
                static_cast<s16>(actor->home.pos.z) == kCoastStatueZ) {
                actor->world.pos.y = actor->home.pos.y = kCoastStatueY;
            }
        });
}
