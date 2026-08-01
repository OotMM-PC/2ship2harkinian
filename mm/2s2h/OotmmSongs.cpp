#include "OotmmSongs.h"

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Enhancements/Audio/AudioCollection.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <array>
#include <string>

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
#include "z64audio.h"
#include "audio/soundfont.h"
#include "sequence.h"

#include "2s2h/OotmmSongsPlayer.h"

SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path);
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path);

extern PlayState* gPlayState;
}

namespace {

constexpr char kColorDefault = '\x00';
constexpr char kColorRed = '\x01';
constexpr char kColorGreen = '\x02';
constexpr char kColorBlue = '\x03';
constexpr char kColorYellow = '\x04';
constexpr char kColorLightBlue = '\x05';
constexpr char kColorPink = '\x06';
constexpr char kNewline = '\x11';
constexpr char kTwoChoice = '\xC2';
constexpr char kEnd = '\xBF';

struct SongDef {
    const char* Name;
    bool TheInPlayedText;
    char Color;
    const char* ItemId;
    const char* SharedItemId;
    const char* NoteItemId;
    const char* SharedNoteItemId;
    uint32_t NotesNeeded;
    const char* FanfareLabel;
    bool NeedsWarpFont;
    int32_t EffectActor;
    uint16_t EffectParams;
    const char* WarpEntrance;
    uint32_t WarpNativeId;
    const char* WarpQuestion; // may embed a newline control code
};

// Upstream order (packages/generator/src/mm/ocarina.c): warp songs, then Zelda's, then Saria's.
constexpr std::array<SongDef, OOTMM_SONG_COUNT> kSongs = { {
    { "Minuet of Forest", true, kColorGreen, "MM_SONG_TP_FOREST", "SHARED_SONG_TP_FOREST",
      "MM_SONG_NOTE_TP_FOREST", "SHARED_SONG_NOTE_TP_FOREST", 6, "MinuetOfForest", true,
      ACTOR_OCEFF_WIPE5, 0, "OOT_WARP_SONG_MEADOW", 0x600, "Warp to the Lost Woods?" },
    { "Bolero of Fire", true, kColorRed, "MM_SONG_TP_FIRE", "SHARED_SONG_TP_FIRE",
      "MM_SONG_NOTE_TP_FIRE", "SHARED_SONG_NOTE_TP_FIRE", 8, "BoleroOfFire", true,
      ACTOR_OCEFF_WIPE5, 1, "OOT_WARP_SONG_CRATER", 0x4F6, "Warp to the Death Mountain\x11" "Crater?" },
    { "Serenade of Water", true, kColorBlue, "MM_SONG_TP_WATER", "SHARED_SONG_TP_WATER",
      "MM_SONG_NOTE_TP_WATER", "SHARED_SONG_NOTE_TP_WATER", 5, "SerenadeOfWater", true,
      ACTOR_OCEFF_WIPE5, 2, "OOT_WARP_SONG_LAKE", 0x604, "Warp to Lake Hylia?" },
    { "Requiem of Spirit", true, kColorYellow, "MM_SONG_TP_SPIRIT", "SHARED_SONG_TP_SPIRIT",
      "MM_SONG_NOTE_TP_SPIRIT", "SHARED_SONG_NOTE_TP_SPIRIT", 6, "RequiemOfSpirit", true,
      ACTOR_OCEFF_WIPE5, 3, "OOT_WARP_SONG_DESERT", 0x1F1, "Warp to the Desert Colossus?" },
    { "Nocturne of Shadow", true, kColorPink, "MM_SONG_TP_SHADOW", "SHARED_SONG_TP_SHADOW",
      "MM_SONG_NOTE_TP_SHADOW", "SHARED_SONG_NOTE_TP_SHADOW", 7, "NocturneOfShadow", true,
      ACTOR_OCEFF_WIPE5, 4, "OOT_WARP_SONG_GRAVE", 0x568, "Warp to the graveyard?" },
    { "Prelude of Light", true, kColorLightBlue, "MM_SONG_TP_LIGHT", "SHARED_SONG_TP_LIGHT",
      "MM_SONG_NOTE_TP_LIGHT", "SHARED_SONG_NOTE_TP_LIGHT", 6, "PreludeOfLight", true,
      ACTOR_OCEFF_WIPE, 1, "OOT_WARP_SONG_TEMPLE", 0x5F4, "Warp to the Temple of Time?" },
    { "Zelda's Lullaby", false, kColorBlue, "MM_SONG_ZELDA", "SHARED_SONG_ZELDA",
      "MM_SONG_NOTE_ZELDA", "SHARED_SONG_NOTE_ZELDA", 6, "ZeldasLullaby", false,
      ACTOR_OCEFF_WIPE, 0, nullptr, 0, nullptr },
    { "Saria's Song", false, kColorGreen, "MM_SONG_SARIA", "SHARED_SONG_SARIA",
      "MM_SONG_NOTE_SARIA", "SHARED_SONG_NOTE_SARIA", 6, "SariasSong", false,
      ACTOR_OCEFF_WIPE3, 0, nullptr, 0, nullptr },
} };

int32_t sPlayedSong = OOTMM_SONG_NONE;
int32_t sActiveSong = OOTMM_SONG_NONE;

struct WarpRequest {
    bool Pending = false;
    bool CrossGame = false;
    bool Sent = false;
    uint32_t MmEntrance = 0;
    Ship::OotmmEntranceMapping Mapping;
};
WarpRequest sWarp;

const SongDef& Def(int32_t song) {
    return kSongs[song - 1];
}

bool IsWarpSong(int32_t song) {
    return song >= OOTMM_SONG_MINUET && song <= OOTMM_SONG_PRELUDE;
}

void ResetWarp() {
    sWarp = WarpRequest{};
}

bool OwnsSong(int32_t song) {
    const SongDef& def = Def(song);
    const auto& inventory = OotmmIpc_GetInventory();
    if (inventory.Has(def.ItemId) || inventory.Has(def.SharedItemId)) {
        return true;
    }
    return inventory.Count(def.NoteItemId) + inventory.Count(def.SharedNoteItemId) >= def.NotesNeeded;
}

uint16_t OwnedMask() {
    uint16_t mask = 0;
    for (int32_t song = OOTMM_SONG_NONE + 1; song < OOTMM_SONG_MAX; song++) {
        if (OwnsSong(song)) {
            mask |= static_cast<uint16_t>(1 << song);
        }
    }
    return mask;
}

std::string PlayedBody(int32_t song) {
    std::string body = "You played ";
    char color;
    std::string name;
    if (song == OOTMM_SONG_NONE) {
        // MM's native Sun's Song entry (0x1B7D) is empty, so it takes the custom text path too.
        body += "the ";
        color = kColorYellow;
        name = "Sun's Song";
    } else {
        const SongDef& def = Def(song);
        if (def.TheInPlayedText) {
            body += "the ";
        }
        color = def.Color;
        name = def.Name;
    }
    body += color;
    body += name;
    body += kColorDefault;
    body += '!';
    body += kEnd;
    return body;
}

std::string WarpConfirmBody(int32_t song) {
    const SongDef& def = Def(song);
    std::string body;
    body += def.Color;
    body += def.WarpQuestion;
    body += kColorDefault;
    // The two-line Bolero question drops the spacer line, matching upstream.
    if (std::string(def.WarpQuestion).find(kNewline) == std::string::npos) {
        body += kNewline;
        body += ' ';
    }
    body += kNewline;
    body += kColorGreen;
    body += kTwoChoice;
    body += "OK";
    body += kNewline;
    body += "No";
    body += kEnd;
    return body;
}

void ShowRestrictedText(PlayState* play) {
    Message_StartTextbox(play, 0x1B95, NULL); // "Your notes echoed far... but nothing happened."
    play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
}

void HandleSongAct(PlayState* play) {
    const int32_t song = sPlayedSong;
    sPlayedSong = OOTMM_SONG_NONE;
    sActiveSong = OOTMM_SONG_NONE;

    if (IsWarpSong(song)) {
        if (play->interfaceCtx.restrictions.songOfSoaring != 0) {
            ShowRestrictedText(play);
            return;
        }
        CustomMessage::Entry entry;
        entry.textboxType = 6;
        entry.textboxYPos = 1;
        entry.autoFormat = false;
        CustomMessage::StartTextbox(WarpConfirmBody(song), entry);
        play->msgCtx.ocarinaMode = OCARINA_MODE_ACTIVE;
        sActiveSong = song;
        return;
    }

    if (song == OOTMM_SONG_SARIA) {
        Audio_PlaySubBgm(NA_BGM_SARIAS_SONG);
    }
    play->msgCtx.ocarinaMode = OCARINA_MODE_END;
}

void ResolveWarp(PlayState* play) {
    const SongDef& def = Def(sActiveSong);
    sActiveSong = OOTMM_SONG_NONE;

    const Ship::OotmmEntranceMapping* mapping =
        OotmmSession_GetState().FindEntrance(Ship::OotmmGame::Oot, def.WarpNativeId);

    ResetWarp();
    sWarp.CrossGame = mapping == nullptr || mapping->ToGame == Ship::OotmmGame::Oot;
    if (sWarp.CrossGame) {
        if (!OotmmIpc_IsConnected()) {
            CustomMessage::StartTextbox("The launcher is not listening, so the song cannot carry you there.");
            play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
            return;
        }
        sWarp.Mapping.FromGame = Ship::OotmmGame::Mm;
        sWarp.Mapping.From = def.WarpEntrance;
        sWarp.Mapping.ToGame = Ship::OotmmGame::Oot;
        sWarp.Mapping.To = mapping != nullptr ? mapping->To : def.WarpEntrance;
        sWarp.Mapping.ToNativeId = mapping != nullptr && mapping->ToNativeId.has_value() ? *mapping->ToNativeId
                                                                                         : def.WarpNativeId;
    } else {
        if (!mapping->ToNativeId.has_value()) {
            ShowRestrictedText(play);
            return;
        }
        sWarp.MmEntrance = *mapping->ToNativeId;
    }

    play->msgCtx.ocarinaMode = OCARINA_MODE_WARP_TO_SOUTH_CLOCK_TOWN;
    sWarp.Pending = true;
}

bool EnsureWarpFont(const SongDef& def) {
    static int32_t sWarpFontIndex = -2;
    if (sWarpFontIndex == -2) {
        SoundFont* font = ResourceMgr_LoadAudioSoundFontByName("custom/fonts/OotWarpSongs");
        sWarpFontIndex = font != nullptr ? font->fntIndex : -1;
    }
    if (sWarpFontIndex < 0) {
        return false;
    }
    const std::string path = std::string("custom/music/") + def.FanfareLabel + "_fanfare";
    SequenceData* sequence = ResourceMgr_LoadSeqPtrByName(path.c_str());
    if (sequence == nullptr) {
        return false;
    }
    // The shipped jingles still name OoT's warp-song font; point them at the shipped copy.
    sequence->fonts[0] = static_cast<uint8_t>(sWarpFontIndex);
    sequence->numFonts = 1;
    return true;
}

RegisterShipInitFunc sInit([]() {
    REGISTER_VB_SHOULD(VB_MSG_CAPTURE_MSGMODE_TEXT_CLOSING_OCARINA_ACTION, {
        if (OotmmSession_IsActive() && sPlayedSong != OOTMM_SONG_NONE && gPlayState != nullptr) {
            *should = true;
            HandleSongAct(gPlayState);
        }
    });
});

} // namespace

extern "C" uint16_t OotmmSongs_AvailableMask(void) {
    if (!OotmmSession_IsActive() || gPlayState == nullptr) {
        return 0;
    }
    const u16 action = gPlayState->msgCtx.ocarinaAction;
    if (action != OCARINA_ACTION_FREE_PLAY && action != OCARINA_ACTION_CHECK_NOTIME) {
        return 0;
    }
    return OwnedMask();
}

extern "C" uint16_t OotmmSongs_ScarecrowBlockMask(void) {
    if (!OotmmSession_IsActive()) {
        return 0;
    }
    return OwnedMask();
}

extern "C" int32_t OotmmSongs_FanfareSeqId(int32_t song) {
    if (song <= OOTMM_SONG_NONE || song >= OOTMM_SONG_MAX) {
        return -1;
    }
    const SongDef& def = Def(song);
    const int32_t seqId = AudioCollection::Instance->GetSequenceNumByName(def.FanfareLabel);
    if (seqId < 0) {
        return -1;
    }
    if (def.NeedsWarpFont && !EnsureWarpFont(def)) {
        return -1;
    }
    return seqId;
}

extern "C" void OotmmSongs_NotePlayed(int32_t song) {
    sPlayedSong = song;
}

extern "C" int32_t OotmmSongs_Played(void) {
    return sPlayedSong;
}

extern "C" void OotmmSongs_ClearPlayed(void) {
    sPlayedSong = OOTMM_SONG_NONE;
}

extern "C" int32_t OotmmSongs_ShouldOverridePlayedText(PlayState* play) {
    if (sPlayedSong != OOTMM_SONG_NONE) {
        return true;
    }
    return OotmmSession_IsActive() && play->msgCtx.songPlayed == OCARINA_SONG_SUNS;
}

extern "C" void OotmmSongs_ShowPlayedText(PlayState* play) {
    CustomMessage::Entry entry;
    entry.textboxType = 3;
    entry.autoFormat = false;
    CustomMessage::SetActiveCustomMessage(PlayedBody(sPlayedSong), entry);
    Message_ContinueTextbox(play, CUSTOM_MESSAGE_ID);
}

extern "C" int32_t OotmmSongs_EffectActorId(void) {
    return sPlayedSong == OOTMM_SONG_NONE ? -1 : Def(sPlayedSong).EffectActor;
}

extern "C" uint16_t OotmmSongs_EffectActorParams(void) {
    return sPlayedSong == OOTMM_SONG_NONE ? 0 : Def(sPlayedSong).EffectParams;
}

extern "C" void OotmmSongs_Update(PlayState* play, Player* player) {
    if (play == NULL || player == NULL) {
        return;
    }
    if (!OotmmSession_IsActive()) {
        sPlayedSong = OOTMM_SONG_NONE;
        sActiveSong = OOTMM_SONG_NONE;
        ResetWarp();
        return;
    }

    const u16 mode = play->msgCtx.ocarinaMode;
    if (sWarp.Pending &&
        (mode < OCARINA_MODE_WARP_TO_GREAT_BAY_COAST || mode > OCARINA_MODE_WARP_TO_ENTRANCE)) {
        ResetWarp();
    }

    if (IsWarpSong(sActiveSong)) {
        if (mode == OCARINA_MODE_WARP) {
            ResolveWarp(play);
        } else if (mode == OCARINA_MODE_END || mode == OCARINA_MODE_NONE) {
            sActiveSong = OOTMM_SONG_NONE;
        }
    }
}

extern "C" int32_t OotmmSongs_OverrideSoaringDestination(PlayState* play) {
    if (!sWarp.Pending) {
        return false;
    }
    if (!sWarp.CrossGame) {
        const int32_t resolved = OotmmSession_ApplyResolvedMmEntrance(sWarp.MmEntrance);
        if (resolved < 0) {
            ResetWarp();
            return false;
        }
        play->nextEntrance = static_cast<u16>(resolved);
        return true;
    }
    if (!sWarp.Sent) {
        if (!OotmmSession_BeginCrossGameTransition(sWarp.Mapping)) {
            ResetWarp();
            return false;
        }
        sWarp.Sent = true;
    }
    return true;
}
