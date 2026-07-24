#include "OotmmOcarinaButtons.h"

#include "OotmmIpc.h"
#include "OotmmSession.h"

#include <string>
#include <string_view>

namespace {

constexpr uint32_t kButtonA = 0x8000;
constexpr uint32_t kButtonCUp = 0x0008;
constexpr uint32_t kButtonCDown = 0x0004;
constexpr uint32_t kButtonCLeft = 0x0002;
constexpr uint32_t kButtonCRight = 0x0001;

bool ButtonShuffleActive() {
    return OotmmSession_IsActive() &&
           OotmmSession_GetState().GetBoolSetting("ocarinaButtonsShuffleMm", false);
}

bool OwnsButton(std::string_view suffix) {
    const auto& state = OotmmSession_GetState();
    const std::string prefix =
        state.GetBoolSetting("sharedOcarinaButtons", false) ? "SHARED_BUTTON_" : "MM_BUTTON_";
    return OotmmIpc_GetInventory().Has(prefix + std::string(suffix));
}

} // namespace

extern "C" int32_t Ootmm_IsOcarinaButtonAvailable(int32_t button) {
    if (!ButtonShuffleActive()) {
        return true;
    }

    switch (button) {
        case OOTMM_OCARINA_BUTTON_A:
            return OwnsButton("A");
        case OOTMM_OCARINA_BUTTON_C_DOWN:
            return OwnsButton("C_DOWN");
        case OOTMM_OCARINA_BUTTON_C_RIGHT:
            return OwnsButton("C_RIGHT");
        case OOTMM_OCARINA_BUTTON_C_LEFT:
            return OwnsButton("C_LEFT");
        case OOTMM_OCARINA_BUTTON_C_UP:
            return OwnsButton("C_UP");
        default:
            return true;
    }
}

extern "C" uint32_t Ootmm_FilterOcarinaButtons(uint32_t buttons) {
    if (!ButtonShuffleActive()) {
        return buttons;
    }
    if (!Ootmm_IsOcarinaButtonAvailable(OOTMM_OCARINA_BUTTON_A)) {
        buttons &= ~kButtonA;
    }
    if (!Ootmm_IsOcarinaButtonAvailable(OOTMM_OCARINA_BUTTON_C_UP)) {
        buttons &= ~kButtonCUp;
    }
    if (!Ootmm_IsOcarinaButtonAvailable(OOTMM_OCARINA_BUTTON_C_DOWN)) {
        buttons &= ~kButtonCDown;
    }
    if (!Ootmm_IsOcarinaButtonAvailable(OOTMM_OCARINA_BUTTON_C_LEFT)) {
        buttons &= ~kButtonCLeft;
    }
    if (!Ootmm_IsOcarinaButtonAvailable(OOTMM_OCARINA_BUTTON_C_RIGHT)) {
        buttons &= ~kButtonCRight;
    }
    return buttons;
}
