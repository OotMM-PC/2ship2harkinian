#include "OotmmSouls.h"

#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <array>
#include <string>
#include <string_view>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

struct SoulCategory {
    std::string_view Prefix;
    const char* EnableSetting;
    const char* SharedSetting;
};

constexpr std::array<SoulCategory, 5> kCategories = { {
    { "ENEMY_", "soulsEnemyMm", "sharedSoulsEnemy" },
    { "BOSS_", "soulsBossMm", nullptr },
    { "NPC_", "soulsNpcMm", "sharedSoulsNpc" },
    { "ANIMAL_", "soulsAnimalMm", "sharedSoulsAnimal" },
    { "MISC_", "soulsMiscMm", "sharedSoulsMisc" },
} };

bool sRoomHidesEnemy = false;

const SoulCategory* CategoryOf(std::string_view soul) {
    for (const SoulCategory& category : kCategories) {
        if (soul.starts_with(category.Prefix)) {
            return &category;
        }
    }
    return nullptr;
}

bool OwnsSoul(std::string_view soul) {
    const SoulCategory* category = CategoryOf(soul);
    const auto& state = OotmmSession_GetState();
    if (category == nullptr || !state.GetBoolSetting(category->EnableSetting, false)) {
        return true;
    }

    const auto& inventory = OotmmIpc_GetInventory();
    if (category->SharedSetting != nullptr && state.GetBoolSetting(category->SharedSetting, false) &&
        inventory.Has("SHARED_SOUL_" + std::string(soul))) {
        return true;
    }
    return inventory.Has("MM_SOUL_" + std::string(soul));
}

enum class SpawnVerdict {
    Allow,
    Block,
    BlockAndHoldRoom,
};

SpawnVerdict Gate(std::string_view soul) {
    return OwnsSoul(soul) ? SpawnVerdict::Allow : SpawnVerdict::Block;
}

SpawnVerdict GateEnemy(std::string_view soul) {
    return OwnsSoul(soul) ? SpawnVerdict::Allow : SpawnVerdict::BlockAndHoldRoom;
}

SpawnVerdict ResolveTownShop(uint16_t params) {
    switch (params & 0xF) {
        case 0:
            return Gate("NPC_ZORA_SHOPKEEPER");
        case 1:
            return Gate("NPC_GORON_SHOPKEEPER");
        case 2:
            return Gate("NPC_BOMBCHU_SHOPKEEPER");
        default:
            return SpawnVerdict::Allow;
    }
}

SpawnVerdict ResolveSpawn(PlayState* play, int16_t actorId, uint16_t params) {
    switch (actorId) {
        case ACTOR_EN_GINKO_MAN:
            return Gate("NPC_BANKER");
        case ACTOR_EN_AN:
            return Gate("NPC_ANJU");
        case ACTOR_EN_GURUGURU:
            return Gate("NPC_GURU_GURU");
        case ACTOR_EN_DAIKU:
        case ACTOR_EN_DAIKU2:
            if (play->sceneId == SCENE_SONCHONOIE && play->roomCtx.curRoom.num == 0x01) {
                return Gate("NPC_MAYOR_DOTOUR");
            }
            return Gate("NPC_CARPENTERS");
        case ACTOR_EN_MS:
            return Gate("NPC_BEAN_SALESMAN");
        case ACTOR_EN_MA_YTO:
        case ACTOR_EN_MA_YTS:
        case ACTOR_EN_MA4:
            return Gate("NPC_MALON");
        case ACTOR_EN_INVADEPOH:
            switch ((params >> 4) & 0xF) {
                case 0x4:
                case 0x5:
                case 0x7:
                case 0x8:
                case 0x9:
                case 0xB:
                case 0xC:
                    return Gate("NPC_MALON");
                default:
                    return SpawnVerdict::Allow;
            }
        case ACTOR_EN_ZOT:
        case ACTOR_EN_ZOW:
            return Gate("NPC_ZORA");
        case ACTOR_EN_BJT:
            return Gate("NPC_TOILET_HAND");
        case ACTOR_EN_BAL:
            return Gate("NPC_TINGLE");
        case ACTOR_EN_TAKARAYA:
            return Gate("NPC_BOMBCHU_BOWLING_LADY");
        case ACTOR_EN_KBT:
        case ACTOR_EN_KGY:
            return Gate("NPC_BLACKSMITHS");
        case ACTOR_EN_GB2:
            return Gate("NPC_POE_COLLECTOR");
        case ACTOR_EN_TAB:
            return Gate("NPC_TALON");
        case ACTOR_EN_BJI_01:
            return Gate("NPC_ASTRONOMER");
        case ACTOR_EN_MUTO:
        case ACTOR_EN_HEISHI:
        case ACTOR_EN_BAISEN:
            if (play->sceneId == SCENE_SONCHONOIE && play->roomCtx.curRoom.num != 0x01) {
                return SpawnVerdict::Allow;
            }
            [[fallthrough]];
        case ACTOR_EN_DT:
            return Gate("NPC_MAYOR_DOTOUR");
        case ACTOR_EN_TRT:
        case ACTOR_EN_TRT2:
        case ACTOR_EN_TRU:
        case ACTOR_EN_TRU_MT:
            return Gate("NPC_KOUME_KOTAKE");
        case ACTOR_EN_KITAN:
            return Gate("NPC_KEATON");
        case ACTOR_EN_TEST3:
            return Gate("NPC_KAFEI");
        case ACTOR_EN_TOTO:
            return Gate("NPC_TOTO");
        case ACTOR_EN_ZOV:
            return Gate("NPC_RUTO");
        case ACTOR_EN_ZOD:
        case ACTOR_EN_ZOS:
        case ACTOR_EN_ZOB:
            return Gate("NPC_ZORA_MUSICIANS");
        case ACTOR_EN_FU:
            return Gate("NPC_HONEY_DARLING");
        case ACTOR_EN_SOB1:
            return ResolveTownShop(params);
        case ACTOR_EN_JGAME_TSN:
        case ACTOR_EN_TSN:
            return Gate("NPC_CHEST_GAME_OWNER");
        case ACTOR_EN_LIFT_NUTS:
            return Gate("NPC_PLAYGROUND_SCRUBS");
        case ACTOR_EN_DNP:
            return Gate("NPC_DEKU_PRINCESS");
        case ACTOR_EN_DNQ:
            return Gate("NPC_DEKU_KING");
        case ACTOR_EN_TK:
            return Gate("NPC_DAMPE");
        case ACTOR_EN_PO_COMPOSER:
            return Gate("NPC_COMPOSER_BROS");
        case ACTOR_EN_STH:
        case ACTOR_EN_JA:
        case ACTOR_EN_YB:
        case ACTOR_EN_RZ:
        case ACTOR_EN_MM3:
        case ACTOR_EN_PM:
            return Gate("NPC_CITIZEN");
        case ACTOR_EN_DNO:
            return Gate("NPC_BUTLER_DEKU");
        case ACTOR_EN_BOM_BOWL_MAN:
        case ACTOR_EN_BOMJIMA:
        case ACTOR_EN_BOMJIMB:
        case ACTOR_EN_BOMBERS:
        case ACTOR_EN_BOMBERS2:
        case ACTOR_EN_BOMBAL:
            return Gate("NPC_BOMBERS");
        case ACTOR_EN_RSN:
            return Gate("NPC_BOMBCHU_SHOPKEEPER");
        case ACTOR_EN_OSSAN:
            switch (params & 0xF) {
                case 0x00:
                    return Gate("NPC_FISHING_POND_OWNER");
                case 0x01:
                    return Gate("NPC_ROOFTOP_MAN");
                default:
                    return SpawnVerdict::Allow;
            }
        case ACTOR_EN_ANI:
            return Gate("NPC_ROOFTOP_MAN");
        case ACTOR_EN_GO:
            if (params == 0x08) {
                return Gate("NPC_MEDIGORON");
            }
            return Gate("NPC_GORON");
        case ACTOR_EN_S_GORO:
            return Gate("NPC_GORON");
        case ACTOR_EN_GK:
            return Gate("NPC_GORON_CHILD");
        case ACTOR_EN_JG:
            return Gate("NPC_GORON_ELDER");
        case ACTOR_EN_DAI:
            return Gate("NPC_BIGGORON");
        case ACTOR_EN_SYATEKI_MAN:
            if (play->sceneId == SCENE_SYATEKI_MORI) {
                return Gate("NPC_BAZAAR_SHOPKEEPER");
            }
            return Gate("NPC_SHOOTING_GALLERY_OWNER");
        case ACTOR_EN_SYATEKI_OKUTA:
            // Out-of-cycle days would spawn the gallery octoroks into a broken minigame.
            if (play->sceneId == SCENE_SYATEKI_MIZU &&
                (gSaveContext.save.day > 3 || gSaveContext.save.day < 1)) {
                return SpawnVerdict::Block;
            }
            return Gate("NPC_SHOOTING_GALLERY_OWNER");
        case ACTOR_EN_MK:
            return Gate("NPC_SCIENTIST");
        case ACTOR_EN_IN:
        case ACTOR_EN_GM:
            return Gate("NPC_GORMAN");
        case ACTOR_EN_HS:
            return Gate("NPC_GROG");
        case ACTOR_EN_AOB_01:
            return Gate("NPC_DOG_LADY");
        case ACTOR_EN_AL:
            return Gate("NPC_AROMA");
        case ACTOR_EN_JS:
            return Gate("NPC_MOON_CHILDREN");
        case ACTOR_EN_SHN:
            return Gate("NPC_TOURIST_CENTER");
        case ACTOR_EN_NB:
            return Gate("NPC_OLD_HAG");
        case ACTOR_EN_KENDO_JS:
            return Gate("NPC_CARPET_MAN");
        case ACTOR_EN_OKUTA:
        case ACTOR_EN_BIGOKUTA:
            return GateEnemy("ENEMY_OCTOROK");
        case ACTOR_EN_WALLMAS:
            return GateEnemy("ENEMY_WALLMASTER");
        case ACTOR_EN_DODONGO:
            return GateEnemy("ENEMY_DODONGO");
        case ACTOR_EN_FIREFLY:
            return GateEnemy("ENEMY_KEESE");
        case ACTOR_EN_TITE:
            return GateEnemy("ENEMY_TEKTITE");
        case ACTOR_EN_PEEHAT:
            return GateEnemy("ENEMY_PEAHAT");
        case ACTOR_EN_DINOFOS:
            return GateEnemy("ENEMY_LIZALFOS_DINOLFOS");
        case ACTOR_EN_ST:
            return GateEnemy("ENEMY_SKULLTULA");
        case ACTOR_EN_AM:
        case ACTOR_EN_FAMOS:
            return GateEnemy("ENEMY_ARMOS");
        case ACTOR_EN_DEKUBABA:
        case ACTOR_EN_KAREBABA:
            return GateEnemy("ENEMY_DEKU_BABA");
        case ACTOR_EN_DEKUNUTS:
            return GateEnemy("ENEMY_DEKU_SCRUB");
        case ACTOR_EN_BBFALL:
        case ACTOR_EN_BB:
            return GateEnemy("ENEMY_BUBBLE");
        case ACTOR_EN_VM:
            return GateEnemy("ENEMY_BEAMOS");
        case ACTOR_EN_RD:
        case ACTOR_EN_TALK_GIBUD:
        case ACTOR_EN_RAILGIBUD:
            return GateEnemy("ENEMY_REDEAD_GIBDO");
        case ACTOR_EN_SW:
            if (params & 0x03) {
                return Gate("MISC_GS");
            }
            return GateEnemy("ENEMY_SKULLWALLTULA");
        case ACTOR_OBJ_MAKEKINSUTA:
            return Gate("MISC_GS");
        case ACTOR_EN_SB:
            return GateEnemy("ENEMY_SHELL_BLADE");
        case ACTOR_EN_RR:
            return GateEnemy("ENEMY_LIKE_LIKE");
        case ACTOR_EN_IK:
            return GateEnemy("ENEMY_IRON_KNUCKLE");
        case ACTOR_EN_FZ:
            return GateEnemy("ENEMY_FREEZARD");
        case ACTOR_EN_WF:
            return GateEnemy("ENEMY_WOLFOS");
        case ACTOR_EN_CROW:
            return GateEnemy("ENEMY_GUAY");
        case ACTOR_EN_TUBO_TRAP:
            return Gate("ENEMY_FLYING_POT");
        case ACTOR_EN_FLOORMAS:
            return GateEnemy("ENEMY_FLOORMASTER");
        case ACTOR_EN_SLIME:
            return GateEnemy("ENEMY_CHUCHU");
        case ACTOR_EN_DRAGON:
            return GateEnemy("ENEMY_DEEP_PYTHON");
        case ACTOR_EN_PR:
        case ACTOR_EN_PR2:
        case ACTOR_EN_PRZ:
            return GateEnemy("ENEMY_SKULLFISH");
        case ACTOR_EN_WDHAND:
            return GateEnemy("ENEMY_DEXIHAND");
        case ACTOR_EN_GRASSHOPPER:
            return GateEnemy("ENEMY_DRAGONFLY");
        case ACTOR_EN_SNOWMAN:
            return GateEnemy("ENEMY_EENO");
        case ACTOR_EN_EGOL:
            return GateEnemy("ENEMY_EYEGORE");
        case ACTOR_EN_PP:
            return GateEnemy("ENEMY_HIPLOOP");
        case ACTOR_EN_RAT:
            return GateEnemy("ENEMY_REAL_BOMBCHU");
        case ACTOR_EN_THIEFBIRD:
            return GateEnemy("ENEMY_TAKKURI");
        case ACTOR_EN_MKK:
            return GateEnemy("ENEMY_BOE");
        case ACTOR_EN_BAGUO:
            return GateEnemy("ENEMY_NEJIRON");
        case ACTOR_BOSS_05:
            return GateEnemy("ENEMY_BIO_BABA");
        case ACTOR_EN_JSO:
        case ACTOR_EN_JSO2:
            return GateEnemy("ENEMY_GARO");
        case ACTOR_EN_WIZ:
        case ACTOR_EN_WIZ_BROCK:
            return GateEnemy("ENEMY_WIZZROBE");
        case ACTOR_EN_DEATH:
            return GateEnemy("ENEMY_GOMESS");
        case ACTOR_EN_PAMETFROG:
        case ACTOR_EN_BIGSLIME:
        case ACTOR_EN_BIGPAMET:
            return GateEnemy("ENEMY_GEKKO");
        case ACTOR_EN_BAT:
            return GateEnemy("ENEMY_BAD_BAT");
        case ACTOR_EN_KAME:
            return GateEnemy("ENEMY_SNAPPER");
        case ACTOR_BOSS_04:
            return GateEnemy("ENEMY_WART");
        case ACTOR_EN_BSB:
            return GateEnemy("ENEMY_CAPTAIN_KEETA");
        case ACTOR_BOSS_01:
            return GateEnemy("BOSS_ODOLWA");
        case ACTOR_BOSS_HAKUGIN:
            return GateEnemy("BOSS_GOHT");
        case ACTOR_BOSS_03:
            return GateEnemy("BOSS_GYORG");
        case ACTOR_BOSS_02:
            return GateEnemy("BOSS_TWINMOLD");
        case ACTOR_EN_KNIGHT:
        case ACTOR_BOSS_06:
            return GateEnemy("BOSS_IGOS");
        case ACTOR_EN_NEO_REEBA:
            return GateEnemy("ENEMY_LEEVER");
        case ACTOR_EN_SKB:
            // Room 1 is the upper graveyard, where stalchildren run the grave-opening minigame.
            if (play->sceneId == SCENE_BOTI && play->roomCtx.curRoom.num == 0x01) {
                return SpawnVerdict::Allow;
            }
            [[fallthrough]];
        case ACTOR_EN_HINT_SKB:
        case ACTOR_EN_RAIL_SKB:
            return GateEnemy("ENEMY_STALCHILD");
        case ACTOR_EN_SELLNUTS:
        case ACTOR_EN_SCOPENUTS:
        case ACTOR_EN_AKINDONUTS:
            return Gate("MISC_BUSINESS_SCRUB");
        case ACTOR_EN_GE1:
        case ACTOR_EN_GE2:
        case ACTOR_EN_GE3:
            return Gate("NPC_THIEVES");
        case ACTOR_EN_KAIZOKU:
            return GateEnemy("ENEMY_THIEVES");
        case ACTOR_EN_NIW:
        case ACTOR_EN_NWC:
            return Gate("ANIMAL_CUCCO");
        case ACTOR_EN_COW:
            return Gate("ANIMAL_COW");
        case ACTOR_EN_DG:
        case ACTOR_EN_RACEDOG:
            return Gate("ANIMAL_DOG");
        case ACTOR_EN_BUTTE:
            return Gate("ANIMAL_BUTTERFLY");
        case ACTOR_EN_PO_SISTERS:
        case ACTOR_EN_POH:
        case ACTOR_EN_BIGPO:
            return GateEnemy("ENEMY_POE");
        case ACTOR_OBJ_SOUND:
            // The carpenters' hammering emitter in Clock Town, silenced with them.
            if ((play->sceneId == SCENE_CLOCKTOWER || play->sceneId == SCENE_TOWN) && (params & 0x7F) == 0x10) {
                return Gate("NPC_CARPENTERS");
            }
            return SpawnVerdict::Allow;
        default:
            return SpawnVerdict::Allow;
    }
}

bool SoulsInactive(PlayState* play) {
    return play == nullptr || !OotmmSession_IsActive() || gSaveContext.gameMode != GAMEMODE_NORMAL;
}

} // namespace

extern "C" int32_t OotmmSouls_SuppressSpawn(PlayState* play, int16_t actorId, int32_t params) {
    if (SoulsInactive(play)) {
        return 0;
    }

    const SpawnVerdict verdict = ResolveSpawn(play, actorId, static_cast<uint16_t>(params));
    if (verdict == SpawnVerdict::Allow) {
        return 0;
    }
    if (verdict == SpawnVerdict::BlockAndHoldRoom) {
        sRoomHidesEnemy = true;
    }
    return 1;
}

extern "C" int32_t OotmmSouls_AdjustSpawnParams(PlayState* play, int16_t actorId, int32_t params) {
    if (SoulsInactive(play)) {
        return params;
    }
    if (actorId == ACTOR_EN_HORSE && ((params & 0x1FFF) == 0x13 || (params & 0x1FFF) == 0x14) &&
        !OwnsSoul("NPC_GORMAN")) {
        return params & 0xE000;
    }
    return params;
}

extern "C" void OotmmSouls_ResetRoomState(void) {
    sRoomHidesEnemy = false;
}

extern "C" int32_t OotmmSouls_RoomClearBlocked(void) {
    return OotmmSession_IsActive() && sRoomHidesEnemy;
}
