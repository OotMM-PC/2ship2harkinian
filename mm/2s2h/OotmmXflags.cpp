#include "OotmmXflags.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "2s2h/ObjectExtension/ObjectExtension.h"
#include "OotmmCheckLedger.h"
#include "OotmmDrops.h"
#include "OotmmIpc.h"
#include "OotmmItemModels.h"
#include "OotmmSession.h"

#include <array>
#include <initializer_list>
#include <optional>
#include <utility>

extern "C" {
#include "functions.h"
#include "global.h"
#include "overlays/actors/ovl_Obj_Grass/z_obj_grass.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "overlays/actors/ovl_Obj_Grass_Carry/z_obj_grass_carry.h"

extern PlayState* gPlayState;
}

namespace {

constexpr int kExtraScene = 0x71;

struct Xflag {
    int Scene = 0;
    int Setup = 0;
    int Room = 0;
    int Slice = 0;
    int Id = 0;
};

struct XflagTag {
    Xflag Value;
};
ObjectExtension::Register<XflagTag> sXflagTagRegister;

struct GrassGroups {
    std::array<std::optional<Xflag>, 40> Groups;
};
ObjectExtension::Register<GrassGroups> sGrassGroupsRegister;

struct FairyDraw {
    ActorFunc Original = nullptr;
};
ObjectExtension::Register<FairyDraw> sFairyDrawRegister;

std::optional<Xflag> sPendingSpawn;

int sLastScene = -1;

constexpr const char* kXflagTypes[] = { "pot",       "grass",  "crate", "barrel",     "hive",   "rupee",
                                        "heart",     "fairy",  "butterfly", "wonder", "rock",   "redboulder",
                                        "redice",    "icicle", "snowball",  "soil",   "tree",   "bush",
                                        "fairy_spot" };

int SceneSetup(PlayState* play) {
    static constexpr std::pair<int, int> kExtraSetups[] = {
        { SCENE_IKANA, 3 },           { SCENE_13HUBUKINOMITI, 1 }, { SCENE_16GORON_HOUSE, 1 },
        { SCENE_33ZORACITY, 1 },      { SCENE_30GYOSON, 1 },       { SCENE_31MISAKI, 1 },
        { SCENE_DEKU_KING, 1 },       { SCENE_21MITURINMAE, 2 },   { SCENE_14YUKIDAMANOMITI, 1 },
        { SCENE_12HAKUGINMAE, 1 },    { SCENE_GORONRACE, 1 },      { SCENE_BACKTOWN, 1 },
    };
    for (const auto& [scene, extra] : kExtraSetups) {
        if (play->sceneId == scene) {
            return gSaveContext.sceneLayer > extra ? 0 : gSaveContext.sceneLayer;
        }
    }
    return 0;
}

bool Remap(int& id, std::initializer_list<std::pair<int, int>> table) {
    for (const auto& [from, to] : table) {
        if (id == from) {
            id = to;
            return true;
        }
    }
    return false;
}

bool AliasPot(Xflag& xf) {
    switch (xf.Scene) {
        case SCENE_20SICHITAI2:
            xf.Scene = SCENE_20SICHITAI;
            xf.Id += 3;
            break;
        case SCENE_16GORON_HOUSE:
            if (xf.Setup == 1) {
                xf.Setup = 0;
                if (xf.Room == 0) {
                    xf.Id -= 2;
                } else if (xf.Room == 1) {
                    xf.Id += 3;
                }
            }
            break;
        case SCENE_30GYOSON:
            if (xf.Setup == 1) {
                xf.Setup = 0;
                return Remap(xf.Id, { { 13, 123 }, { 14, 124 }, { 15, 125 }, { 16, 133 }, { 17, 119 },
                                      { 18, 129 }, { 19, 118 }, { 20, 128 }, { 21, 130 }, { 22, 120 },
                                      { 23, 121 }, { 24, 62 },  { 25, 61 },  { 26, 63 },  { 27, 131 } });
            }
            break;
        case SCENE_31MISAKI:
            if (xf.Setup == 1) {
                xf.Setup = 0;
                xf.Id += 9;
            }
            break;
        case SCENE_21MITURINMAE:
            if (xf.Setup == 2) {
                xf.Setup = 0;
                xf.Id -= 36;
            }
            break;
        case SCENE_10YUKIYAMANOMURA2:
            xf.Scene = SCENE_10YUKIYAMANOMURA;
            xf.Id = 38;
            break;
        case SCENE_GORONRACE:
            xf.Setup = 0;
            break;
    }
    return true;
}

bool ZoraCapeExtraPot(const Actor* actor, Xflag& xf) {
    if (actor->id != ACTOR_OBJ_TSUBO || gPlayState->sceneId != SCENE_31MISAKI || actor->params != 0x13F) {
        return false;
    }
    int id = static_cast<s16>(actor->home.pos.x);
    const bool known = Remap(id, { { 0x04D0, 0 }, { 0x0560, 1 }, { 0x05BD, 2 }, { 0x056F, 3 }, { 0x0543, 4 } });
    xf = Xflag{ kExtraScene, 0, 0, 0, known ? id : -1 };
    return true;
}

void AliasGrass(Xflag& xf) {
    switch (xf.Scene) {
        case SCENE_20SICHITAI2:
            xf.Scene = SCENE_20SICHITAI;
            xf.Id += xf.Room == 0 ? 6 : 3;
            break;
        case SCENE_IKANA:
            if (xf.Setup == 3) {
                xf.Setup = 0;
                xf.Id += 30;
            }
            break;
        case SCENE_30GYOSON:
            if (xf.Setup == 1) {
                xf.Setup = 0;
                xf.Id -= 4;
            }
            break;
        case SCENE_21MITURINMAE:
            if (xf.Setup == 2) {
                xf.Setup = 0;
                xf.Id += 8;
            }
            break;
    }
}

void AliasGrassUnit(Xflag& xf) {
    if (xf.Scene == SCENE_20SICHITAI2) {
        xf.Scene = SCENE_20SICHITAI;
        if (xf.Room == 0) {
            xf.Id += 2;
        }
    }
}

void AliasRock(Xflag& xf) {
    switch (xf.Scene) {
        case SCENE_16GORON_HOUSE:
            if (xf.Setup == 1) {
                xf.Setup = 0;
                xf.Id -= 5;
            }
            break;
        case SCENE_31MISAKI:
            xf.Setup = 0;
            break;
        case SCENE_30GYOSON:
            if (xf.Setup == 1) {
                xf.Setup = 0;
                Remap(xf.Id, { { 73, 77 },   { 74, 70 },  { 75, 86 },  { 76, 78 },  { 77, 71 },   { 78, 74 },
                               { 79, 72 },   { 80, 94 },  { 81, 72 },  { 82, 76 },  { 83, 79 },   { 84, 75 },
                               { 85, 89 },   { 86, 69 },  { 87, 91 },  { 88, 90 },  { 89, 66 },   { 90, 80 },
                               { 91, 88 },   { 92, 92 },  { 93, 68 },  { 94, 93 },  { 95, 67 },   { 96, 81 },
                               { 97, 84 },   { 98, 85 },  { 99, 83 },  { 100, 87 }, { 101, 103 }, { 102, 82 },
                               { 103, 99 },  { 104, 97 }, { 105, 95 }, { 106, 98 }, { 107, 96 },  { 108, 102 },
                               { 109, 101 }, { 110, 100 } });
            }
            break;
    }
}

void AliasFreestandingRupee(Xflag& xf) {
    switch (xf.Scene) {
        case SCENE_21MITURINMAE:
            if (xf.Setup == 2) {
                xf.Setup = 0;
                xf.Id -= 37;
            }
            break;
        case SCENE_ALLEY:
            Remap(xf.Id, { { 0x14, 0x13 }, { 0x15, 0x13 }, { 0x17, 0x16 }, { 0x18, 0x16 }, { 0x1A, 0x19 }, { 0x1B, 0x19 } });
            break;
        case SCENE_17SETUGEN:
            xf.Scene = SCENE_17SETUGEN2;
            xf.Id -= 18;
            break;
        case SCENE_INISIE_R:
            if (xf.Room == 0x03 && Remap(xf.Id, { { 0, 0 }, { 3, 1 }, { 4, 2 } })) {
                xf.Room = 0x01;
            }
            break;
    }
}

void AliasFlowerpot(Xflag& xf) {
    if (xf.Scene == SCENE_10YUKIYAMANOMURA2) {
        xf.Scene = SCENE_10YUKIYAMANOMURA;
        xf.Id -= 6;
    }
}

void AliasLargeCrate(Xflag& xf) {
    if (xf.Scene == SCENE_11GORONNOSATO2) {
        xf.Scene = SCENE_11GORONNOSATO;
    }
}

void AliasSnowball(Xflag& xf) {
    switch (xf.Scene) {
        case SCENE_13HUBUKINOMITI:
            Remap(xf.Id, { { 28, 26 }, { 23, 26 }, { 24, 42 }, { 19, 42 }, { 27, 30 }, { 29, 30 }, { 46, 31 },
                           { 22, 31 }, { 48, 43 }, { 20, 43 }, { 45, 41 }, { 47, 41 }, { 25, 44 }, { 21, 44 } });
            break;
        case SCENE_10YUKIYAMANOMURA:
            Remap(xf.Id, { { 12, 17 }, { 13, 17 }, { 45, 14 }, { 16, 14 }, { 48, 46 }, { 15, 52 }, { 51, 52 },
                           { 47, 18 }, { 44, 18 } });
            break;
        case SCENE_17SETUGEN:
            Remap(xf.Id, { { 14, 59 }, { 16, 58 }, { 46, 58 }, { 20, 57 }, { 48, 57 }, { 13, 55 }, { 51, 55 },
                           { 17, 22 }, { 42, 22 }, { 41, 60 }, { 29, 60 }, { 53, 19 }, { 43, 19 }, { 50, 5 },
                           { 40, 56 }, { 47, 56 }, { 44, 30 }, { 52, 30 }, { 21, 54 }, { 28, 54 }, { 45, 18 },
                           { 49, 18 }, { 15, 61 } });
            break;
    }
}

bool AliasLargeSnowball(Xflag& xf) {
    if (xf.Scene == SCENE_10YUKIYAMANOMURA2) {
        xf.Scene = SCENE_10YUKIYAMANOMURA;
        if (xf.Id == 45) {
            xf.Id = 41;
        } else if (xf.Id >= 47 && xf.Id <= 50) {
            xf.Id += 6;
        } else {
            return false;
        }
    }
    if (xf.Scene == SCENE_14YUKIDAMANOMITI && xf.Setup == 1 && xf.Id >= 17 && xf.Id <= 19) {
        xf.Id -= 8;
        xf.Setup = 0;
    }
    if (xf.Scene == SCENE_12HAKUGINMAE && xf.Setup == 1 &&
        Remap(xf.Id, { { 11, 43 }, { 12, 44 }, { 13, 45 }, { 20, 42 } })) {
        xf.Setup = 0;
    }
    return true;
}

void AliasPalm(Xflag& xf) {
    if (xf.Scene == SCENE_31MISAKI && xf.Setup == 1) {
        xf.Setup = 0;
        xf.Id -= 1;
    } else if (xf.Scene == SCENE_30GYOSON && xf.Setup == 1) {
        xf.Setup = 0;
        Remap(xf.Id, { { 31, 10 }, { 32, 13 }, { 33, 11 }, { 34, 12 } });
    }
}

void AliasSnowTree(Xflag& xf) {
    if (xf.Scene == SCENE_13HUBUKINOMITI && xf.Setup == 1) {
        xf.Setup = 0;
        xf.Id -= 5;
    } else if (xf.Scene == SCENE_14YUKIDAMANOMITI && xf.Setup == 1) {
        xf.Setup = 0;
        Remap(xf.Id, { { 0, 24 }, { 1, 23 }, { 2, 22 }, { 3, 25 } });
    }
}

void AliasTree(Xflag& xf) {
    if (xf.Scene == SCENE_GORONRACE) {
        xf.Setup = 0;
    } else if (xf.Scene == SCENE_BACKTOWN && xf.Setup == 1) {
        xf.Setup = 0;
        Remap(xf.Id, { { 11, 7 }, { 12, 21 } });
    }
}

bool IsFreestandingRupee(const Actor* actor) {
    const int type = actor->params & 0xFF;
    return type == ITEM00_RUPEE_GREEN || type == ITEM00_RUPEE_BLUE || type == ITEM00_RUPEE_RED;
}

bool Alias(const Actor* actor, Xflag& xf) {
    switch (actor->id) {
        case ACTOR_OBJ_TSUBO:
            return AliasPot(xf);
        case ACTOR_EN_KUSA:
            AliasGrass(xf);
            return true;
        case ACTOR_OBJ_GRASS_UNIT:
            AliasGrassUnit(xf);
            return true;
        case ACTOR_EN_KUSA2:
            if (xf.Scene == SCENE_BACKTOWN) {
                xf.Id = 16;
            }
            return true;
        case ACTOR_EN_ISHI:
            AliasRock(xf);
            return true;
        case ACTOR_EN_ITEM00:
            if (IsFreestandingRupee(actor)) {
                AliasFreestandingRupee(xf);
            }
            return true;
        case ACTOR_OBJ_FLOWERPOT:
            AliasFlowerpot(xf);
            return true;
        case ACTOR_OBJ_KIBAKO2:
            AliasLargeCrate(xf);
            return true;
        case ACTOR_OBJ_SNOWBALL:
            AliasSnowball(xf);
            return true;
        case ACTOR_OBJ_SNOWBALL2:
            return AliasLargeSnowball(xf);
        case ACTOR_OBJ_YASI:
            AliasPalm(xf);
            return true;
        case ACTOR_EN_SNOWWD:
            AliasSnowTree(xf);
            return true;
        case ACTOR_OBJ_TREE:
            AliasTree(xf);
            return true;
        case ACTOR_EN_ELF:
            return FAIRY_GET_TYPE(actor) >= FAIRY_TYPE_2;
        case ACTOR_OBJ_SWPRIZE:
            if (xf.Scene == SCENE_30GYOSON) {
                xf.Setup = 0;
            }
            return true;
        default:
            return true;
    }
}

bool RemapsGrotto(const Actor* actor) {
    switch (actor->id) {
        case ACTOR_OBJ_KIBAKO:
        case ACTOR_OBJ_KIBAKO2:
        case ACTOR_OBJ_FLOWERPOT:
        case ACTOR_OBJ_TARU:
        case ACTOR_OBJ_SNOWBALL:
        case ACTOR_OBJ_SNOWBALL2:
        case ACTOR_OBJ_HAMISHI:
        case ACTOR_OBJ_COMB:
            return false;
        default:
            return true;
    }
}

std::optional<Xflag> Resolve(Actor* actor) {
    if (actor == nullptr || gPlayState == nullptr) {
        return std::nullopt;
    }
    if (const auto* tag = ObjectExtension::GetInstance().Get<XflagTag>(actor)) {
        return tag->Value;
    }
    Xflag xf;
    if (ZoraCapeExtraPot(actor, xf)) {
        return xf.Id < 0 ? std::nullopt : std::optional<Xflag>(xf);
    }
    const int index = GetActorListIndex(actor);
    if (index < 0) {
        return std::nullopt;
    }
    xf = Xflag{ gPlayState->sceneId, SceneSetup(gPlayState), actor->room, 0, index };
    if (RemapsGrotto(actor) && xf.Scene == SCENE_KAKUSIANA) {
        if (xf.Room == 0x04) {
            xf.Room = 0x20 | (gSaveContext.respawn[RESPAWN_MODE_UNK_3].data & 0x1F);
        } else if (xf.Room == 0x0A && sLastScene == SCENE_30GYOSON) {
            xf.Room = 0x0F;
        }
    }
    if (actor->id == ACTOR_OBJ_COMB && xf.Room == 0x0A && sLastScene == SCENE_30GYOSON) {
        xf.Room = 0x0F;
    }
    if (!Alias(actor, xf)) {
        return std::nullopt;
    }
    return xf;
}

uint32_t NativeId(const Xflag& xf) {
    return (static_cast<uint32_t>(xf.Slice) << 16) | (static_cast<uint32_t>(xf.Setup & 3) << 14) |
           (static_cast<uint32_t>(xf.Room) << 8) | static_cast<uint32_t>(xf.Id & 0xFF);
}

const Ship::OotmmPlacement* Lookup(const Xflag& xf) {
    if (xf.Scene < 0 || xf.Id < 0 || !OotmmSession_IsActive()) {
        return nullptr;
    }
    const auto& state = OotmmSession_GetState();
    const uint32_t nativeId = NativeId(xf);
    for (const char* type : kXflagTypes) {
        if (const auto* placement =
                state.FindCheck(Ship::OotmmGame::Mm, static_cast<uint32_t>(xf.Scene), type, nativeId)) {
            return placement;
        }
    }
    return nullptr;
}

bool Collected(const Ship::OotmmPlacement& placement) {
    return OotmmCheckLedger_Contains(placement.CheckId) || OotmmIpc_IsCheckCompleted(placement.CheckId);
}

const Ship::OotmmPlacement* Pending(const std::optional<Xflag>& xf) {
    if (!xf.has_value()) {
        return nullptr;
    }
    const auto* placement = Lookup(*xf);
    return placement != nullptr && !Collected(*placement) ? placement : nullptr;
}

std::optional<Xflag> Sliced(Actor* actor, int slice) {
    auto xf = Resolve(actor);
    if (xf.has_value()) {
        xf->Slice = slice;
    }
    return xf;
}

void Report(const Ship::OotmmPlacement& placement, bool present) {
    OotmmIpc_SendCheckCollected(placement.CheckId, present);
    OotmmCheckLedger_Record(placement.CheckId);
}

bool DropPlacement(const Ship::OotmmPlacement* placement, PlayState* play, float x, float y, float z,
                   Actor** spawned = nullptr) {
    if (placement == nullptr) {
        return false;
    }
    if (placement->ItemId == "NOTHING") {
        Report(*placement, false);
        return true;
    }
    if (!OotmmDrops_HasCheck(placement->CheckId)) {
        Actor* drop = OotmmDrops_SpawnPlacement(play, x, y, z, *placement);
        if (spawned != nullptr) {
            *spawned = drop;
        }
    }
    return true;
}

void AdoptFreestanding(Actor* actor) {
    if (actor == nullptr || actor->id != ACTOR_EN_ITEM00) {
        return;
    }
    if (!IsFreestandingRupee(actor) && (actor->params & 0xFF) != ITEM00_RECOVERY_HEART) {
        return;
    }
    const auto* placement = Pending(Resolve(actor));
    if (placement != nullptr) {
        OotmmDrops_Adopt(actor, *placement, reinterpret_cast<EnItem00*>(actor)->collectibleFlag);
    }
}

Actor* FindActor(PlayState* play, s16 id) {
    for (int category = 0; category < ACTORCAT_MAX; ++category) {
        for (Actor* actor = play->actorCtx.actorLists[category].first; actor != nullptr; actor = actor->next) {
            if (actor->id == id) {
                return actor;
            }
        }
    }
    return nullptr;
}

std::optional<Xflag> GrassElementXflag(ObjGrass* manager, ObjGrassElement* element) {
    if (manager == nullptr || element == nullptr) {
        return std::nullopt;
    }
    const auto* groups = ObjectExtension::GetInstance().Get<GrassGroups>(&manager->actor);
    if (groups == nullptr) {
        return std::nullopt;
    }
    const auto offset = reinterpret_cast<uintptr_t>(element) - reinterpret_cast<uintptr_t>(manager->grassGroups);
    const size_t group = offset / sizeof(ObjGrassGroup);
    if (group >= groups->Groups.size() || !groups->Groups[group].has_value()) {
        return std::nullopt;
    }
    const auto elementOffset =
        reinterpret_cast<uintptr_t>(element) - reinterpret_cast<uintptr_t>(manager->grassGroups[group].elements);
    Xflag xf = *groups->Groups[group];
    xf.Slice = static_cast<int>(elementOffset / sizeof(ObjGrassElement));
    return xf;
}

void DrawFairy(Actor* actor, PlayState* play) {
    const auto* placement = Pending(Resolve(actor));
    if (placement == nullptr) {
        const auto* original = ObjectExtension::GetInstance().Get<FairyDraw>(actor);
        if (original != nullptr && original->Original != nullptr) {
            original->Original(actor, play);
        }
        return;
    }
    if (actor->scale.x > 0.0001f) {
        const float scale = 0.25f / actor->scale.x;
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    }
    const bool cloaked = !placement->CloakItemId.empty();
    OotmmItemModel_DrawById(play, cloaked ? placement->CloakItemId : placement->ItemId,
                            cloaked ? placement->CloakItemName : placement->ItemName);
}

void AdoptFairy(Actor* actor) {
    if (actor == nullptr || actor->id != ACTOR_EN_ELF) {
        return;
    }
    const int type = FAIRY_GET_TYPE(actor);
    if (type < FAIRY_TYPE_2 || type == FAIRY_TYPE_4 || Pending(Resolve(actor)) == nullptr ||
        ObjectExtension::GetInstance().Has<FairyDraw>(actor)) {
        return;
    }
    ObjectExtension::GetInstance().Set<FairyDraw>(actor, FairyDraw{ actor->draw });
    actor->draw = DrawFairy;
}

bool DropActor(Actor* actor, PlayState* play) {
    return OotmmXflags_Drop(actor, play) != 0;
}

} // namespace

extern "C" int OotmmXflags_IsShuffled(Actor* actor) {
    return Pending(Resolve(actor)) != nullptr;
}

int OotmmXflags_LastScene() {
    return sLastScene;
}

int64_t OotmmXflags_NativeId(Actor* actor) {
    const auto xf = Resolve(actor);
    return xf.has_value() ? (static_cast<int64_t>(xf->Scene) << 32) | NativeId(*xf) : -1;
}

extern "C" int OotmmXflags_DropAt(Actor* actor, PlayState* play, float x, float y, float z) {
    return DropPlacement(Pending(Resolve(actor)), play, x, y, z);
}

extern "C" int OotmmXflags_Drop(Actor* actor, PlayState* play) {
    return actor != nullptr && OotmmXflags_DropAt(actor, play, actor->world.pos.x, actor->world.pos.y, actor->world.pos.z);
}

extern "C" int OotmmXflags_DropSlice(Actor* actor, PlayState* play, int slice, float x, float y, float z) {
    return DropPlacement(Pending(Sliced(actor, slice)), play, x, y, z);
}

extern "C" int OotmmXflags_DropStill(Actor* actor, PlayState* play) {
    if (actor == nullptr) {
        return 0;
    }
    Actor* drop = nullptr;
    if (!DropPlacement(Pending(Resolve(actor)), play, actor->world.pos.x, actor->world.pos.y, actor->world.pos.z,
                       &drop)) {
        return 0;
    }
    if (drop != nullptr) {
        drop->speed = 0.0f;
        drop->velocity = Vec3f{ 0.0f, 0.0f, 0.0f };
    }
    return 1;
}

extern "C" Actor* OotmmXflags_SpawnSlice(Actor* actor, PlayState* play, int slice, float x, float y, float z,
                                         int* handled) {
    Actor* drop = nullptr;
    *handled = DropPlacement(Pending(Sliced(actor, slice)), play, x, y, z, &drop);
    return drop;
}

extern "C" int OotmmXflags_IsAnySliceShuffled(Actor* actor, int count) {
    for (int slice = 0; slice < count; ++slice) {
        if (Pending(Sliced(actor, slice)) != nullptr) {
            return 1;
        }
    }
    return 0;
}

extern "C" void OotmmXflags_TagChild(Actor* child, Actor* parent, int slice) {
    if (child == nullptr) {
        return;
    }
    const auto xf = Sliced(parent, slice);
    if (!xf.has_value()) {
        return;
    }
    ObjectExtension::GetInstance().Set<XflagTag>(child, XflagTag{ *xf });
    AdoptFreestanding(child);
}

extern "C" int OotmmXflags_Collect(Actor* actor) {
    const auto* placement = Pending(Resolve(actor));
    if (placement == nullptr) {
        return 0;
    }
    OotmmDrops_Report(placement->CheckId, placement->ItemId, placement->ItemId != "NOTHING");
    return 1;
}

extern "C" void OotmmXflags_PrepareSpawn(Actor* parent, int slice) {
    sPendingSpawn = Sliced(parent, slice);
}

extern "C" void OotmmXflags_ClearSpawn(void) {
    sPendingSpawn.reset();
}

extern "C" void OotmmXflags_ClaimSpawn(Actor* actor) {
    if (sPendingSpawn.has_value()) {
        ObjectExtension::GetInstance().Set<XflagTag>(actor, XflagTag{ *sPendingSpawn });
        sPendingSpawn.reset();
    }
}

extern "C" int OotmmXflags_PrepareFairy(Actor* butterfly) {
    const auto xf = Resolve(butterfly);
    sPendingSpawn = xf;
    const auto* placement = Pending(xf);
    if (placement != nullptr && placement->ItemId == "NOTHING") {
        Report(*placement, false);
        sPendingSpawn.reset();
        return 0;
    }
    return 1;
}

extern "C" void OotmmXflags_TagGrassGroup(Actor* manager, int group, Actor* unit) {
    if (manager == nullptr || group < 0 || group >= 40) {
        return;
    }
    GrassGroups groups;
    if (const auto* existing = ObjectExtension::GetInstance().Get<GrassGroups>(manager)) {
        groups = *existing;
    }
    groups.Groups[group] = Resolve(unit);
    ObjectExtension::GetInstance().Set<GrassGroups>(manager, std::move(groups));
}

void OotmmXflags_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s8, s8) {
        if (gPlayState != nullptr && gPlayState->sceneId != SCENE_KAKUSIANA) {
            sLastScene = gPlayState->sceneId;
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](Actor* actor) {
        AdoptFreestanding(actor);
        AdoptFairy(actor);
    });
    REGISTER_VB_SHOULD(VB_POT_DROP_COLLECTIBLE, {
        if (DropActor(va_arg(args, Actor*), gPlayState)) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_BARREL_OR_CRATE_DROP_COLLECTIBLE, {
        if (DropActor(va_arg(args, Actor*), gPlayState)) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_SNOWBALL_DROP_COLLECTIBLE, {
        if (DropActor(va_arg(args, Actor*), gPlayState)) {
            *should = false;
        }
    });
    REGISTER_VB_SHOULD(VB_GRASS_DROP_COLLECTIBLE, {
        const s16 source = static_cast<s16>(va_arg(args, int));
        if (gPlayState == nullptr) {
            return;
        }
        if (source == ACTOR_OBJ_GRASS) {
            auto* element = va_arg(args, ObjGrassElement*);
            auto* manager = reinterpret_cast<ObjGrass*>(FindActor(gPlayState, ACTOR_OBJ_GRASS));
            if (DropPlacement(Pending(GrassElementXflag(manager, element)), gPlayState, element->pos.x,
                              element->pos.y, element->pos.z)) {
                *should = false;
            }
        } else if (source == ACTOR_OBJ_GRASS_CARRY) {
            auto* carry = va_arg(args, ObjGrassCarry*);
            if (DropPlacement(Pending(GrassElementXflag(carry->grassManager, carry->grassElem)), gPlayState,
                              carry->actor.world.pos.x, carry->actor.world.pos.y, carry->actor.world.pos.z)) {
                *should = false;
            }
        } else if (DropActor(va_arg(args, Actor*), gPlayState)) {
            *should = false;
        }
    });
}
