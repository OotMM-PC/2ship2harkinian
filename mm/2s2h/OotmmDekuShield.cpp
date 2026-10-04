#include "OotmmDekuShield.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "CustomMessage/CustomMessage.h"
#include "OotmmCustomItems.h"
#include "OotmmSession.h"

#include <string>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "z64.h"
}

namespace {

constexpr u16 kShieldGoneText = 0xF6;
bool sShieldGonePending = false;

bool HitByFire(const ColliderElement& element, u8 acFlags) {
    return (acFlags & AC_HIT) && element.acHitElem != nullptr && element.acHitElem->atDmgInfo.effect == 1;
}

void BurnDekuShield(Actor* actor, bool*) {
    Player* player = reinterpret_cast<Player*>(actor);
    if (!OotmmSession_IsActive() || gPlayState == nullptr || player != GET_PLAYER(gPlayState) ||
        player->transformation != PLAYER_FORM_HUMAN || player->currentShield != PLAYER_SHIELD_HEROS_SHIELD ||
        !OotmmCustomItems_WearingDekuShield()) {
        return;
    }
    if (!player->bodyIsBurning && !HitByFire(player->shieldCylinder.elem, player->shieldCylinder.base.acFlags) &&
        !HitByFire(player->shieldQuad.elem, player->shieldQuad.base.acFlags)) {
        return;
    }
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, PLAYER_SHIELD_NONE);
    Player_SetEquipmentData(gPlayState, player);
    sShieldGonePending = true;
    Message_StartTextbox(gPlayState, kShieldGoneText, nullptr);
}

void LoadShieldGoneText(u16*, bool* loadFromMessageTable) {
    if (!sShieldGonePending || gPlayState == nullptr) {
        return;
    }
    sShieldGonePending = false;
    CustomMessage::Entry entry;
    entry.textboxType = 0x00;
    entry.textboxYPos = 0x30;
    entry.autoFormat = false;
    entry.msg = "\x03Your ";
    entry.msg.push_back('\x00');
    entry.msg += "shield\x03 is gone!\x1C";
    entry.msg.push_back('\x00');
    entry.msg += "\x20\xBF";
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

} // namespace

extern "C" void OotmmDekuShield_Init(void) {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::ShouldActorUpdate>(ACTOR_PLAYER, BurnDekuShield);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(kShieldGoneText, LoadShieldGoneText);
}
