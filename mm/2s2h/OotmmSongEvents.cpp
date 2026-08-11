#include "OotmmSongEvents.h"

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmSession.h"

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "OotmmSongs.h"
#include "OotmmSongsPlayer.h"

extern PlayState* gPlayState;
void Message_OpenText(PlayState* play, u16 textId);
}

namespace {

// Song ids 0-19 mapped to MM's native ocarina songs; the OoT warp songs (6-11),
// the lullaby (0) and Saria's (2) only exist here as this port's cross-game songs.
constexpr int32_t kSongEventSongs[20] = {
    OCARINA_SONG_ZELDAS_LULLABY,
    OCARINA_SONG_EPONAS,
    OCARINA_SONG_SARIAS,
    OCARINA_SONG_STORMS,
    OCARINA_SONG_SUNS,
    OCARINA_SONG_TIME,
    -1, -1, -1, -1, -1, -1,
    OCARINA_SONG_HEALING,
    OCARINA_SONG_SOARING,
    OCARINA_SONG_SONATA,
    OCARINA_SONG_GORON_LULLABY,
    OCARINA_SONG_GORON_LULLABY_INTRO,
    OCARINA_SONG_NEW_WAVE,
    OCARINA_SONG_ELEGY,
    OCARINA_SONG_OATH,
};

constexpr int32_t kVanillaSongEvents[OOTMM_MM_SONG_EVENT_MAX] = {
    14, 15, 17, 12, 12, 12, 12, 12, 14, 14, 16, 3, 19,
};

bool ShuffleEnabled() {
    if (!OotmmSession_IsActive()) {
        return false;
    }
    const auto& state = OotmmSession_GetState();
    return state.GetBoolSetting("songEventsShuffleMm", false) &&
           state.GetSongEventsMm().size() >= OOTMM_MM_SONG_EVENT_MAX;
}

// The song id (0-19) equivalent of this port's cross-game song that just played.
int32_t PlayedCrossGameSong() {
    const int32_t custom = OotmmSongs_Played();
    if (custom >= OOTMM_SONG_MINUET && custom <= OOTMM_SONG_PRELUDE) {
        return 6 + (custom - OOTMM_SONG_MINUET);
    }
    if (custom == OOTMM_SONG_ZELDA) {
        return 0;
    }
    if (custom == OOTMM_SONG_SARIA) {
        return 2;
    }
    return -1;
}

void SpawnSongTag(PlayState* play, int32_t eventId, float x, float y, float z, s16 yaw = 0,
                  int32_t room = -1) {
    if (play == nullptr || eventId < 0 || eventId >= OOTMM_MM_SONG_EVENT_MAX) {
        return;
    }
    const s32 params = 0x4000 | eventId | ((room + 1) << 4);
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_OKARINA_TAG, x, y, z, 0, yaw, 0, params);
}

void SpawnSceneSongTags(s16 sceneId) {
    if (gPlayState == nullptr || !ShuffleEnabled()) {
        return;
    }
    switch (sceneId) {
        case SCENE_21MITURINMAE:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_TEMPLE_WOODFALL, 0.0f, 212.0f, 1040.0f);
            break;
        case SCENE_12HAKUGINMAE:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_TEMPLE_SNOWHEAD, -554.0f, 214.0f, 6303.0f);
            break;
        case SCENE_31MISAKI:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_TEMPLE_GREAT_BAY, -5582.0f, 14.0f, 1610.0f, 0x4000);
            break;
        case SCENE_GORON_HAKA:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_HEALING_DARMANI, 0.0f, 10.0f, 80.0f, 0x8000);
            break;
        case SCENE_MUSICHOUSE:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_HEALING_PAMELA_FATHER, 1.0f, -140.0f, 11.0f, 0x8000);
            break;
        case SCENE_00KEIKOKU:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_HEALING_KAMARO, -2533.0f, 47.0f, -2978.0f);
            break;
        case SCENE_30GYOSON:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_HEALING_MIKAU, -305.0f, 80.0f, 4793.0f);
            break;
        case SCENE_BOTI:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_AWAKENING_KEETA, -215.0f, 313.0f, -2140.0f, 0, 1);
            break;
        case SCENE_16GORON_HOUSE:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_LULLABY_KID, -1690.0f, -133.0f, 0.0f, 0x4000, 1);
            break;
        case SCENE_IKANA:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_STORMS_COMPOSER, -173.0f, 573.0f, -1793.0f, 0, 1);
            break;
        case SCENE_OKUJOU:
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_CLOCK_TOWER_ROOF, 0.0f, 20.0f, 222.0f);
            break;
        default:
            break;
    }
}

} // namespace

int32_t OotmmSongEvents_Assigned(int32_t eventId) {
    if (eventId < 0 || eventId >= OOTMM_MM_SONG_EVENT_MAX) {
        return -1;
    }
    if (ShuffleEnabled()) {
        const int32_t assigned = OotmmSession_GetState().GetSongEventsMm()[eventId];
        return assigned >= 0 && assigned < 20 ? assigned : -1;
    }
    return kVanillaSongEvents[eventId];
}

int32_t OotmmSongEvents_Matches(int32_t eventId, int32_t playedSong) {
    const int32_t assigned = OotmmSongEvents_Assigned(eventId);
    if (assigned < 0) {
        return 0;
    }
    if (PlayedCrossGameSong() == assigned) {
        return 1;
    }
    const int32_t expected = kSongEventSongs[assigned];
    if (expected < 0) {
        return 0;
    }
    // The full lullaby always satisfies a spot asking only for its intro.
    return playedSong == expected ||
           (expected == OCARINA_SONG_GORON_LULLABY_INTRO && playedSong == OCARINA_SONG_GORON_LULLABY);
}

static int32_t NearbyPadEvent(PlayState* play) {
    if (play == nullptr || !ShuffleEnabled()) {
        return -1;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr) {
        return -1;
    }

    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_SWITCH].first; actor != nullptr; actor = actor->next) {
        if (actor->id != ACTOR_EN_OKARINA_TAG || !(actor->params & 0x4000)) {
            continue;
        }
        const int32_t room = ((actor->params >> 4) & 0xFF) - 1;
        if (room >= 0 && room != play->roomCtx.curRoom.num) {
            continue;
        }
        const float dx = actor->world.pos.x - player->actor.world.pos.x;
        const float dy = actor->world.pos.y - player->actor.world.pos.y;
        const float dz = actor->world.pos.z - player->actor.world.pos.z;
        if (sqrtf(dx * dx + dz * dz) < 120.0f && fabsf(dy) < 80.0f) {
            return actor->params & 0xF;
        }
    }
    return -1;
}

int32_t OotmmSongEvents_PromptAction(PlayState* play, int32_t action) {
    if (action != OCARINA_ACTION_FREE_PLAY) {
        return action;
    }
    const int32_t eventId = NearbyPadEvent(play);
    if (eventId < 0) {
        return action;
    }

    const int32_t assigned = OotmmSongEvents_Assigned(eventId);
    if (assigned < 0) {
        return action;
    }

    int32_t crossGame = OOTMM_SONG_NONE;
    if (assigned >= 6 && assigned <= 11) {
        crossGame = OOTMM_SONG_MINUET + (assigned - 6);
    } else if (assigned == 0) {
        crossGame = OOTMM_SONG_ZELDA;
    } else if (assigned == 2) {
        crossGame = OOTMM_SONG_SARIA;
    }

    if (crossGame != OOTMM_SONG_NONE) {
        // The ownership mask must be action-independent: this also runs while the prompt is open.
        if (!(OotmmSongs_OwnedMask() & (1u << crossGame))) {
            return action;
        }
        return OCARINA_ACTION_PROMPT_OOTMM_MINUET + (crossGame - 1);
    }

    const int32_t song = kSongEventSongs[assigned];
    if (song < 0 || !CHECK_QUEST_ITEM(QUEST_SONG_SONATA + song)) {
        return action;
    }
    return OCARINA_ACTION_PROMPT_SONATA + song;
}

int32_t OotmmSongEvents_IsPadPrompt(PlayState* play) {
    if (play == nullptr) {
        return 0;
    }
    const int32_t action = play->msgCtx.ocarinaAction;
    return action != OCARINA_ACTION_FREE_PLAY &&
           action == OotmmSongEvents_PromptAction(play, OCARINA_ACTION_FREE_PLAY);
}

void OotmmSongEvents_OpenPadPromptText(PlayState* play) {
    // The vanilla prompt header (textbox type 3) is what routes the text's end into the staff.
    CustomMessage::Entry entry = CustomMessage::LoadVanillaMessageTableEntry(0x1B59);
    entry.autoFormat = false;
    CustomMessage::SetActiveCustomMessage("Play using \xB0 and \xB2; \xB1 to Stop.\xBF", entry);
    Message_OpenText(play, CUSTOM_MESSAGE_ID);
}

int32_t OotmmSongEvents_PadOwnsPlayedSong(PlayState* play) {
    if (play == nullptr) {
        return 0;
    }
    const int32_t eventId = NearbyPadEvent(play);
    return eventId >= 0 && OotmmSongEvents_Matches(eventId, play->msgCtx.lastPlayedSong);
}

void OotmmSongEvents_ClearPlayed(PlayState* play) {
    if (play == nullptr) {
        return;
    }
    play->msgCtx.lastPlayedSong = 0xFF;
    play->msgCtx.songPlayed = 0xFF;
    play->msgCtx.blockSunsSong = true;
    OotmmSongs_ClearPlayed();
}

void OotmmSongEvents_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](s8 sceneId, s8) { SpawnSceneSongTags(sceneId); });

    // These two spots have no fixed coordinates upstream; the tag inherits its host actor.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>([](Actor* actor) {
        if (actor == nullptr || gPlayState == nullptr || !ShuffleEnabled()) {
            return;
        }
        if (actor->id == ACTOR_EN_GB2) {
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_HEALING_POE_HUT, actor->world.pos.x,
                         actor->world.pos.y, actor->world.pos.z, actor->shape.rot.y, actor->room);
        } else if (actor->id == ACTOR_EN_HIDDEN_NUTS) {
            SpawnSongTag(gPlayState, OOTMM_MM_SONG_EVENT_AWAKENING_SCRUB, actor->world.pos.x,
                         actor->world.pos.y, actor->world.pos.z, actor->shape.rot.y, actor->room);
        }
    });
}
