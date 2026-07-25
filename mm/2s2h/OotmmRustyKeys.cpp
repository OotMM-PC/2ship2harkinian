#include "OotmmRustyKeys.h"

#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <array>
#include <cstdint>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "z64.h"
}

namespace {

enum RustyKeyId {
    KEY_TOURIST_INFORMATION,
    KEY_POTION_SHOP,
    KEY_POST_OFFICE,
    KEY_SWORDSMAN_SCHOOL,
    KEY_LOTTERY,
    KEY_BOMB_SHOP,
    KEY_TRADING_POST,
    KEY_CURIOSITY_SHOP,
    KEY_KAFEI_HIDEOUT,
    KEY_TOWN_ARCHERY,
    KEY_SWAMP_ARCHERY,
    KEY_OBSERVATORY,
    KEY_BLACKSMITH,
    KEY_MUSIC_HOUSE,
    KEY_LABORATORY,
    KEY_BENEATH_GRAVEYARD,
    KEY_DAMPE_HOUSE,
    KEY_MAYOR_RESIDENCE,
    KEY_MAYOR_RESIDENCE_OFFICE,
    KEY_MAYOR_RESIDENCE_SALON,
    KEY_MAYOR_RESIDENCE_KAFEI,
    KEY_TREASURE_CHEST_GAME,
    KEY_HONEY_DARLING,
    KEY_MILK_BAR,
    KEY_DOG_RACETRACK,
    KEY_CUCCO_SHACK,
    KEY_RANCH_HOUSE,
    KEY_RANCH_BARN,
    KEY_RANCH_HOUSE_ROOM,
    KEY_ZORA_SHOP,
    KEY_ZORA_JAPAS_ROOM,
    KEY_ZORA_TIJO_ROOM,
    KEY_ZORA_LULU_ROOM,
    KEY_ZORA_EVAN_ROOM,
    KEY_STOCK_POT_INN,
    KEY_STOCK_POT_INN_ROOF,
    KEY_GRANDMA_ROOM,
    KEY_STOCK_POT_INN_STAFF_ROOM,
    KEY_STOCK_POT_INN_DORMITORY,
    KEY_MAX,
};

constexpr std::array<const char*, KEY_MAX> kItemIds = {
    "MM_RUSTY_KEY_TOURIST_INFORMATION", "MM_RUSTY_KEY_POTION_SHOP", "MM_RUSTY_KEY_POST_OFFICE",
    "MM_RUSTY_KEY_SWORDSMAN_SCHOOL", "MM_RUSTY_KEY_LOTTERY", "MM_RUSTY_KEY_BOMB_SHOP",
    "MM_RUSTY_KEY_TRADING_POST", "MM_RUSTY_KEY_CURIOSITY_SHOP", "MM_RUSTY_KEY_KAFEI_HIDEOUT",
    "MM_RUSTY_KEY_TOWN_ARCHERY", "MM_RUSTY_KEY_SWAMP_ARCHERY", "MM_RUSTY_KEY_OBSERVATORY",
    "MM_RUSTY_KEY_BLACKSMITH", "MM_RUSTY_KEY_MUSIC_HOUSE", "MM_RUSTY_KEY_LABORATORY",
    "MM_RUSTY_KEY_BENEATH_GRAVEYARD", "MM_RUSTY_KEY_DAMPE_HOUSE", "MM_RUSTY_KEY_MAYOR_RESIDENCE",
    "MM_RUSTY_KEY_MAYOR_RESIDENCE_OFFICE", "MM_RUSTY_KEY_MAYOR_RESIDENCE_SALON",
    "MM_RUSTY_KEY_MAYOR_RESIDENCE_KAFEI", "MM_RUSTY_KEY_TREASURE_CHEST_GAME",
    "MM_RUSTY_KEY_HONEY_DARLING", "MM_RUSTY_KEY_MILK_BAR", "MM_RUSTY_KEY_DOG_RACETRACK",
    "MM_RUSTY_KEY_CUCCO_SHACK", "MM_RUSTY_KEY_RANCH_HOUSE", "MM_RUSTY_KEY_RANCH_BARN",
    "MM_RUSTY_KEY_RANCH_HOUSE_ROOM", "MM_RUSTY_KEY_ZORA_SHOP", "MM_RUSTY_KEY_ZORA_JAPAS_ROOM",
    "MM_RUSTY_KEY_ZORA_TIJO_ROOM", "MM_RUSTY_KEY_ZORA_LULU_ROOM", "MM_RUSTY_KEY_ZORA_EVAN_ROOM",
    "MM_RUSTY_KEY_STOCK_POT_INN", "MM_RUSTY_KEY_STOCK_POT_INN_ROOF", "MM_RUSTY_KEY_GRANDMA_ROOM",
    "MM_RUSTY_KEY_STOCK_POT_INN_STAFF_ROOM", "MM_RUSTY_KEY_STOCK_POT_INN_DORMITORY",
};

int ResolveDoor(int scene, int transitionId) {
    switch (scene) {
        case SCENE_20SICHITAI:
        case SCENE_20SICHITAI2:
            if (transitionId == 3) return KEY_TOURIST_INFORMATION;
            if (transitionId == 4) return KEY_POTION_SHOP;
            break;
        case SCENE_MAP_SHOP: return KEY_TOURIST_INFORMATION;
        case SCENE_WITCH_SHOP: return KEY_POTION_SHOP;
        case SCENE_ICHIBA:
            if (transitionId == 0) return KEY_POST_OFFICE;
            if (transitionId == 1) return KEY_SWORDSMAN_SCHOOL;
            if (transitionId == 2) return KEY_LOTTERY;
            if (transitionId == 3) return KEY_BOMB_SHOP;
            if (transitionId == 4) return KEY_TRADING_POST;
            if (transitionId == 5) return KEY_CURIOSITY_SHOP;
            break;
        case SCENE_BOMYA: return KEY_BOMB_SHOP;
        case SCENE_POSTHOUSE: return KEY_POST_OFFICE;
        case SCENE_TAKARAKUJI: return KEY_LOTTERY;
        case SCENE_DOUJOU: return KEY_SWORDSMAN_SCHOOL;
        case SCENE_8ITEMSHOP: return KEY_TRADING_POST;
        case SCENE_AYASHIISHOP:
            return transitionId == 1 ? KEY_KAFEI_HIDEOUT : KEY_CURIOSITY_SHOP;
        case SCENE_ALLEY: return KEY_KAFEI_HIDEOUT;
        case SCENE_CLOCKTOWER:
        case SCENE_SYATEKI_MIZU: return KEY_TOWN_ARCHERY;
        case SCENE_24KEMONOMITI:
        case SCENE_SYATEKI_MORI: return KEY_SWAMP_ARCHERY;
        case SCENE_TENMON_DAI:
        case SCENE_00KEIKOKU: return KEY_OBSERVATORY;
        case SCENE_10YUKIYAMANOMURA:
        case SCENE_10YUKIYAMANOMURA2:
        case SCENE_KAJIYA: return KEY_BLACKSMITH;
        case SCENE_IKANA:
        case SCENE_MUSICHOUSE: return KEY_MUSIC_HOUSE;
        case SCENE_30GYOSON:
        case SCENE_LABO: return KEY_LABORATORY;
        case SCENE_BOTI:
        case SCENE_DANPEI2TEST: return KEY_DAMPE_HOUSE;
        case SCENE_HAKASHITA: return KEY_BENEATH_GRAVEYARD;
        case SCENE_SONCHONOIE:
            if (transitionId == 0) return KEY_MAYOR_RESIDENCE_OFFICE;
            if (transitionId == 1) return KEY_MAYOR_RESIDENCE_SALON;
            if (transitionId == 2) return KEY_MAYOR_RESIDENCE_KAFEI;
            if (transitionId == 3) return KEY_MAYOR_RESIDENCE;
            break;
        case SCENE_MILK_BAR: return KEY_MILK_BAR;
        case SCENE_BOWLING: return KEY_HONEY_DARLING;
        case SCENE_TAKARAYA: return KEY_TREASURE_CHEST_GAME;
        case SCENE_F01_B: return KEY_DOG_RACETRACK;
        case SCENE_F01C: return KEY_CUCCO_SHACK;
        case SCENE_F01:
            if (transitionId == 0) return KEY_RANCH_HOUSE;
            if (transitionId == 1) return KEY_RANCH_BARN;
            if (transitionId == 2) return KEY_DOG_RACETRACK;
            if (transitionId == 3) return KEY_CUCCO_SHACK;
            break;
        case SCENE_OMOYA:
            if (transitionId == 0) return KEY_RANCH_BARN;
            if (transitionId == 1) return KEY_RANCH_HOUSE;
            if (transitionId == 2) return KEY_RANCH_HOUSE_ROOM;
            break;
        case SCENE_BANDROOM:
            if (transitionId == 0) return KEY_ZORA_SHOP;
            if (transitionId == 1) return KEY_ZORA_JAPAS_ROOM;
            if (transitionId == 2) return KEY_ZORA_TIJO_ROOM;
            if (transitionId == 3) return KEY_ZORA_LULU_ROOM;
            if (transitionId == 4) return KEY_ZORA_EVAN_ROOM;
            break;
        case SCENE_33ZORACITY:
            if (transitionId == 0) return KEY_ZORA_LULU_ROOM;
            if (transitionId == 1) return KEY_ZORA_EVAN_ROOM;
            if (transitionId == 2) return KEY_ZORA_JAPAS_ROOM;
            if (transitionId == 3) return KEY_ZORA_TIJO_ROOM;
            if (transitionId == 4) return KEY_ZORA_SHOP;
            break;
        case SCENE_YADOYA:
            if (transitionId == 0) return KEY_GRANDMA_ROOM;
            if (transitionId == 1) return KEY_STOCK_POT_INN_STAFF_ROOM;
            if (transitionId == 3) return KEY_STOCK_POT_INN_DORMITORY;
            if (transitionId == 4) return KEY_STOCK_POT_INN_ROOF;
            if (transitionId == 5) return KEY_STOCK_POT_INN;
            break;
        case SCENE_TOWN:
            if (transitionId == 0) return KEY_STOCK_POT_INN;
            if (transitionId == 1) return KEY_STOCK_POT_INN_ROOF;
            if (transitionId == 2) return KEY_TREASURE_CHEST_GAME;
            if (transitionId == 3) return KEY_TOWN_ARCHERY;
            if (transitionId == 4) return KEY_HONEY_DARLING;
            if (transitionId == 5) return KEY_MAYOR_RESIDENCE;
            if (transitionId == 6) return KEY_MILK_BAR;
            break;
    }
    return -1;
}

} // namespace

extern "C" int OotmmRustyDoorLocked(PlayState* play, Actor* actor) {
    if (!OotmmSession_IsActive() || !OotmmSession_GetState().GetBoolSetting("rustyKeysMm", false) ||
        play == nullptr || actor == nullptr) {
        return 0;
    }
    const int key = ResolveDoor(play->sceneId, static_cast<uint16_t>(actor->params) >> 10);
    return key >= 0 && !OotmmIpc_GetInventory().Has(kItemIds[key]);
}
