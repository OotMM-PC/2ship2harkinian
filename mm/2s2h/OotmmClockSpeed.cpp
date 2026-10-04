#include "OotmmClockSpeed.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "OotmmSession.h"

#include <string>

extern "C" {
#include "variables.h"
#include "z64.h"
}

namespace {

struct ClockSpeed {
    const char* Name;
    int Speed;
    int Inverted;
};

constexpr ClockSpeed kSpeeds[] = {
    { "veryslow", 1, 0 }, { "slow", 2, -1 }, { "fast", 6, -4 }, { "veryfast", 9, -6 }, { "superfast", 18, -12 },
};

constexpr int kDefaultInverted = -2;

const ClockSpeed* CurrentSpeed() {
    if (!OotmmSession_IsActive()) {
        return nullptr;
    }
    const std::string name = OotmmSession_GetState().GetStringSetting("clockSpeed", "default");
    for (const auto& speed : kSpeeds) {
        if (name == speed.Name) {
            return &speed;
        }
    }
    return nullptr;
}

void ApplyAutoInvert() {
    if (!OotmmSession_IsActive()) {
        return;
    }
    const std::string mode = OotmmSession_GetState().GetStringSetting("autoInvert", "never");
    if (mode == "always" ||
        (mode == "firstCycle" && gSaveContext.save.saveInfo.playerData.threeDayResetCount == 0)) {
        gSaveContext.save.timeSpeedOffset = OotmmClockSpeed_InvertedOffset();
    }
}

} // namespace

extern "C" int OotmmClockSpeed_SceneTimeSpeed(int timeSpeed) {
    const ClockSpeed* speed = CurrentSpeed();
    if (speed == nullptr || timeSpeed == 0) {
        return timeSpeed;
    }
    return speed->Speed;
}

extern "C" int OotmmClockSpeed_InvertedOffset(void) {
    const ClockSpeed* speed = CurrentSpeed();
    return speed == nullptr ? kDefaultInverted : speed->Inverted;
}

void OotmmClockSpeed_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::AfterEndOfCycleSave>([]() { ApplyAutoInvert(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>([](s16) { ApplyAutoInvert(); });
}
