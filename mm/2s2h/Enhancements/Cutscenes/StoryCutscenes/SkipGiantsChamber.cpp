#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
}

#define CVAR_NAME "gEnhancements.Cutscenes.SkipStoryCutscenes"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

/*
 * Mimics the flags and transitions set by func_808BA10C in z_door_warp1.c.
 */
void HandleGiantsCutsceneSkip() {
    GIEventTransition transition;
    switch (gPlayState->sceneId) {
        case SCENE_MITURIN_BS: // Odolwa's Lair
            SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE);
            SET_WEEKEVENTREG(WEEKEVENTREG_ENTERED_WOODFALL_TEMPLE_PRISON);
            transition.entrance = ENTRANCE(WOODFALL_TEMPLE, 1);
            transition.cutsceneIndex = 0;
            break;
        case SCENE_HAKUGIN_BS: // Goht's Lair
            SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_SNOWHEAD_TEMPLE);
            transition.entrance = ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 7);
            transition.cutsceneIndex = 0;
            break;
        case SCENE_SEA_BS: // Gyorg's Lair
            SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_GREAT_BAY_TEMPLE);
            transition.entrance = ENTRANCE(ZORA_CAPE, 9);
            transition.cutsceneIndex = 0xFFF0;
            break;
        case SCENE_INISIE_BS: // Twinmold's Lair
            SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_STONE_TOWER_TEMPLE);
            transition.entrance = ENTRANCE(IKANA_CANYON, 15);
            transition.cutsceneIndex = 0xFFF2;
            break;
    }

    /*
     * At the point of transition, the previous scene's information is lost, but we need it to set the proper Giants
     * flags. Here, we store the target transition information so that we can circumvent the Giants' Chamber cutscene.
     * We're not actually using the queued transition normally, but using its values to alter a Giants' Chamber
     * transition.
     */
    transition.transitionType = gPlayState->sceneId;
    GameInteractor::Instance->events.emplace_back(transition);
}

// Only reached if the cutscene is skipped
void handleGiantsCheck(SceneId sceneId) {
    // The Oath to Order check only occurs when freeing a Giant for the first time.
    if (gSaveContext.save.saveInfo.unk_EA8[1] == 1) {
        GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
            .showGetItemCutscene = !CVarGetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", 0),
            .giveItem =
                [](Actor* actor, PlayState* play) {
                    if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                        CustomMessage::SetActiveCustomMessage("You learned the Oath to Order!", { .textboxType = 2 });
                    } else {
                        CustomMessage::StartTextbox("You learned the Oath to Order!\x1C\x02\x10", { .textboxType = 2 });
                    }
                    Item_Give(gPlayState, ITEM_SONG_OATH);
                } });
    }
}

void RegisterSkipGiantsChamber() {
    /*
     * Skip Giants' Chamber cutscenes.
     */
    COND_VB_SHOULD(VB_PLAY_TRANSITION_CS, CVAR, {
        if (gSaveContext.save.entrance == ENTRANCE(GIANTS_CHAMBER, 0)) {
            /*
             * The warp gate processing silently queues up an event transition with information for the particular
             * area the player is in (Woodfall, Great Bay, etc.). This is necessary because the previous scene's
             * information is lost during a transition, and we want to skip the Giants' Chamber that we are otherwise
             * about to go to. We quietly use the queued transition to modify the transition in progress. The queued
             * transition must then be erased from the event queue, otherwise it will repeat once.
             */
            auto it = std::find_if(GameInteractor::Instance->events.begin(), GameInteractor::Instance->events.end(),
                                   [](const GIEvent& v) { return std::holds_alternative<GIEventTransition>(v); });
            if (it != GameInteractor::Instance->events.end()) {
                GIEventTransition transition = std::get<GIEventTransition>(*it);
                gSaveContext.save.entrance = transition.entrance;
                gSaveContext.save.cutsceneIndex = transition.cutsceneIndex;
                GameInteractor::Instance->events.erase(it);
                handleGiantsCheck((SceneId)transition.transitionType);
            }
        } else if (gSaveContext.save.entrance == ENTRANCE(WOODFALL, 0) && gSaveContext.save.cutsceneIndex == 0xFFF0) {
            // Odolwa's Lair repeat warps go straight to the Woodfall clear cutscene. Skip that too.
            SET_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE);
            SET_WEEKEVENTREG(WEEKEVENTREG_ENTERED_WOODFALL_TEMPLE_PRISON);
            gSaveContext.save.entrance = ENTRANCE(WOODFALL_TEMPLE, 1);
            gSaveContext.save.cutsceneIndex = 0;
        }
    });

    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, CVAR, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* actor = va_arg(args, Actor*);
        if (actor->id == ACTOR_DOOR_WARP1) {
            HandleGiantsCutsceneSkip();
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterSkipGiantsChamber, { CVAR_NAME });
