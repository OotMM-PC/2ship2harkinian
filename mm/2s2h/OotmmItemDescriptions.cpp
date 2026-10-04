#include "OotmmItemDescriptions.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "CustomMessage/CustomMessage.h"
#include "OotmmCustomItems.h"
#include "OotmmSession.h"

#include <cstring>
#include <string>

extern "C" {
#include "variables.h"
#include "z64.h"
#include "message_data_static.h"
}

namespace {

constexpr u16 kDescriptionBase = 0x1700;
constexpr u16 kHookshotDescription = kDescriptionBase + ITEM_HOOKSHOT;
constexpr u16 kQuestDescriptionBase = 0x1737;
constexpr char kRed = '\x01';
constexpr char kYellow = '\x04';
constexpr char kClear = '\x00';
constexpr char kNewline = '\x11';
constexpr char kEnd = '\xBF';
constexpr u8 kNoIcon = 0xFE;
constexpr u8 kOcarinaIcon = 0x4C;

struct Description {
    u8 Item;
    u8 Icon;
    const char* Title;
    const char* Body;
};

const Description kDescriptions[] = {
    { ITEM_OCARINA_FAIRY, kOcarinaIcon, "Fairy Ocarina",
      "This is a memento from\nSaria.\n\x12Play it with \xB0 and the four \xB2\nButtons. Press \xB1 to stop." },
    { ITEM_OOTMM_SPELL_WIND, kNoIcon, "Farore's Wind",
      "This is warp magic you can use\nwith \xB2. Warp when you are in\ndanger!" },
    { ITEM_OOTMM_SPELL_LOVE, kNoIcon, "Nayru's Love",
      "Cast this to create a powerful\nprotective barrier. It's defensive\nmagic you can use with \xB2." },
    { ITEM_OOTMM_SPELL_FIRE, kNoIcon, "Din's Fire",
      "Its fireball engulfs everything!\nIt's attack magic you can use\nwith \xB2." },
    { ITEM_OOTMM_BOOTS_IRON, kNoIcon, "Iron Boots", "So heavy, you can't run.\nSo heavy, you can't float." },
    { ITEM_OOTMM_BOOTS_HOVER, kNoIcon, "Hover Boots",
      "With these mysterious boots\nyou can hover above the ground.\nThe downside? No traction!" },
    { ITEM_OOTMM_TUNIC_GORON, kNoIcon, "Goron Tunic",
      "Wearing this heat-resistant tunic\nallows you to go to hot places." },
    { ITEM_OOTMM_TUNIC_ZORA, kNoIcon, "Zora Tunic", "Wear this diving suit and you\nwon't drown underwater." },
    { ITEM_OOTMM_HAMMER, kNoIcon, "Megaton Hammer",
      "Press \x04\xB2| to smash and break \njunk! It's so heavy, you need to \nuse two hands to swing it!" },
    { ITEM_OOTMM_BOOMERANG, kNoIcon, "Boomerang",
      "Press \x04\xB2| to throw and watch it come\nback! The boomerang can stun or\ndefeat enemies!" },
    { ITEM_OOTMM_SLINGSHOT, kNoIcon, "Fairy Slingshot", "Press \x04\xB2| to unleash a deku seed\nat your target!" },
    { ITEM_OOTMM_RUTO_LETTER, kNoIcon, "Letter", "It looks like there is something\nalready inside this bottle." },
    { ITEM_OOTMM_MASK_GERUDO, kNoIcon, "Gerudo Mask", "With its charming eyes, it makes\na great lady's disguise." },
    { ITEM_OOTMM_MASK_SKULL, kNoIcon, "Skull Mask", "A mysterious aura emanates from\nthis mask." },
    { ITEM_OOTMM_MASK_SPOOKY, kNoIcon, "Spooky Mask", "This mask was manufactured from\nthe plank of a coffin." },
};

const Description kDekuShield = { ITEM_SHIELD_HERO, kNoIcon, "Deku Shield", "A basic shield.\nVulnerable to fire." };

std::string Encode(const char* text) {
    std::string out;
    for (const char* c = text; *c != '\0'; ++c) {
        out.push_back(*c == '\n' ? kNewline : *c == '|' ? kClear : *c);
    }
    return out;
}

void Load(const Description& description, u16 templateId, char titleColor, bool* loadFromMessageTable) {
    MessageContext* msgCtx = &gPlayState->msgCtx;
    Font* font = &msgCtx->font;
    Message_FindMessageNES(gPlayState, templateId);
    const auto* vanilla = reinterpret_cast<const MessageTableEntry*>(font->messageStart);
    std::string message(vanilla->segment, MESSAGE_HEADER_SIZE);
    message[2] = static_cast<char>(description.Icon);
    message.push_back(titleColor);
    message += description.Title;
    message.push_back(kNewline);
    message.push_back(kClear);
    message += Encode(description.Body);
    message.push_back(kEnd);
    msgCtx->msgLength = static_cast<s32>(message.size());
    std::memcpy(&font->msgBuf, message.data(), message.size());
    *loadFromMessageTable = false;
}

} // namespace

extern "C" void OotmmItemDescriptions_Init(void) {
    for (const Description& description : kDescriptions) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
            kDescriptionBase + description.Item, [&description](u16*, bool* loadFromMessageTable) {
                if (OotmmSession_IsActive() && gPlayState != nullptr) {
                    Load(description, kHookshotDescription, kRed, loadFromMessageTable);
                }
            });
    }
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        kQuestDescriptionBase + ITEM_SHIELD_HERO, [](u16* textId, bool* loadFromMessageTable) {
            if (OotmmSession_IsActive() && gPlayState != nullptr && OotmmCustomItems_WearingDekuShield()) {
                Load(kDekuShield, *textId, kYellow, loadFromMessageTable);
            }
        });
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        kDescriptionBase + ITEM_LONGSHOT, [](u16* textId, bool*) {
            if (OotmmSession_IsActive()) {
                *textId = kHookshotDescription;
            }
        });
}
