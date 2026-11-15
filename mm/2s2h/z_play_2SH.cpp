#include "BenPort.h"
#include "2s2h/resource/type/Scene.h"
#include <ship/utils/StringHelper.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include <ship/resource/ResourceManager.h>
#include <libultraship/bridge/crashhandlerbridge.h>

extern "C" {
#include "global.h"
extern uintptr_t gSegments[NUM_SEGMENTS];
}

Ship::IResource* OTRPlay_LoadFile(PlayState* play, const char* fileName) {
    auto res = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(fileName);
    return res.get();
}

s32 OTRScene_ExecuteCommands(PlayState* play, SOH::Scene* scene);

extern "C" void OTRPlay_InitScene(PlayState* play, s32 spawn) {
    play->curSpawn = spawn;
    play->linkActorEntry = nullptr;
    play->actorCsCamList = nullptr;
    play->setupEntranceList = nullptr;
    play->setupExitList = nullptr;
    play->naviQuestHints = nullptr;
    play->setupPathList = nullptr;
    play->sceneMaterialAnims = nullptr;
    play->roomCtx.unk74 = nullptr;
    play->numSetupActors = 0;
    Object_InitContext(&play->state, &play->objectCtx);
    LightContext_Init(play, &play->lightCtx);
    Scene_ResetTransitionActorList(&play->state, &play->transitionActors);
    Room_Init(play, &play->roomCtx);
    gSaveContext.worldMapArea = 0;
    OTRScene_ExecuteCommands(play, (SOH::Scene*)play->sceneSegment);
    Play_InitEnvironment(play, play->skyboxId);
}

static void SetSceneNote(const char* state, s32 sceneId, s32 spawn) {
    char note[240];
    snprintf(note, sizeof(note), "%s scene 0x%X, spawn %d, entrance 0x%X, layer %d, cutscene 0x%X, day %d time 0x%04X, form %d",
             state, sceneId, spawn, gSaveContext.save.entrance & 0xFFFF, gSaveContext.sceneLayer,
             gSaveContext.save.cutsceneIndex & 0xFFFF, gSaveContext.save.day, gSaveContext.save.time & 0xFFFF,
             gSaveContext.save.playerForm);
    CrashHandlerSetNote("scene", note);
}

extern "C" void OTRPlay_SpawnScene(PlayState* play, s32 sceneId, s32 spawn) {
    s32 pad;
    SceneTableEntry* scene = &gSceneTable[sceneId];
    SetSceneNote("loading", sceneId, spawn);

    scene->unk_D = 0;
    play->loadedScene = scene;
    play->sceneId = sceneId;
    play->sceneConfig = scene->drawConfig;
    std::string scenePath =
        StringHelper::Sprintf("scenes/nonmq/%s/%s", scene->segment.fileName, scene->segment.fileName);
    play->sceneSegment = OTRPlay_LoadFile(play, scenePath.c_str());
    scene->unk_D = 0;
    gSegments[2] = (uintptr_t)play->sceneSegment;
    OTRPlay_InitScene(play, spawn);
    Room_SetupFirstRoom(play, &play->roomCtx);
    SetSceneNote("in", sceneId, spawn);
}

extern "C" s32 OTRfunc_800973FC(PlayState* play, RoomContext* roomCtx) {
    if (roomCtx->status == 1) {
        // if (!osRecvMesg(&roomCtx->loadQueue, nullptr, OS_MESG_NOBLOCK)) {
        if (1) {
            roomCtx->status = 0;
            roomCtx->curRoom.segment = roomCtx->roomRequestAddr;
            gSegments[3] = (uintptr_t)roomCtx->roomRequestAddr;

            OTRScene_ExecuteCommands(play, (SOH::Scene*)roomCtx->curRoom.segment);
            func_80123140(play, GET_PLAYER(play));
            Actor_SpawnTransitionActors(play, &play->actorCtx);
            if (((play->sceneId != SCENE_IKANA) || (roomCtx->curRoom.num != 1)) && (play->sceneId != SCENE_IKNINSIDE)) {
                play->envCtx.lightSettingOverride = LIGHT_SETTING_OVERRIDE_NONE;
                play->envCtx.lightBlendOverride = LIGHT_BLEND_OVERRIDE_NONE;
            }
            func_800FEAB0();
            if (Environment_GetStormState(play) == STORM_STATE_OFF) {
                Environment_StopStormNatureAmbience(play);
            }
            // Insert hook
            GameInteractor_ExecuteAfterRoomSceneCommands(play->sceneId, roomCtx->curRoom.num);
            return 1;
        }

        return 0;
    }

    return 1;
}
