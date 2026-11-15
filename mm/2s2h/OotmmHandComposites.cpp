#include <ship/Context.h>
#include <ship/utils/StrHash64.h>

extern "C" {
#include "z64.h"
}

#include "OotmmHandComposites.h"

namespace {

constexpr int kMaxCommands = 16;

struct Buffer {
    const char* NativePath = nullptr;
    Gfx Commands[kMaxCommands];
};

Buffer sBuffers[16];
int sBufferCount = 0;

Gfx* BufferFor(const char* nativePath) {
    for (int i = 0; i < sBufferCount; i++) {
        if (sBuffers[i].NativePath == nativePath) {
            return sBuffers[i].Commands;
        }
    }
    if (sBufferCount >= (int)(sizeof(sBuffers) / sizeof(sBuffers[0]))) {
        return nullptr;
    }
    sBuffers[sBufferCount].NativePath = nativePath;
    return sBuffers[sBufferCount++].Commands;
}

bool IsExpanded(uint8_t op) {
    return op == G_SETTIMG_OTR_HASH || op == G_DL_OTR_HASH || op == G_VTX_OTR_HASH || op == G_BRANCH_Z_OTR ||
           op == G_MARKER || op == G_MTX_OTR || op == G_MOVEMEM_OTR;
}

} // namespace

extern "C" const char* OotmmHandComposites_NativePath(const void* commands) {
    for (int i = 0; i < sBufferCount; i++) {
        if (sBuffers[i].Commands == commands) {
            return sBuffers[i].NativePath;
        }
    }
    return nullptr;
}

extern "C" Gfx* OotmmHandComposites_Build(const char* nativePath, const char* portedFistPath) {
    const Gfx* src =
        (const Gfx*)Ship::Context::GetInstance()->GetResourceManager()->GetResourceRawPointer(nativePath);
    if (src == nullptr) {
        return nullptr;
    }

    static const uint64_t kFistHashes[] = {
        CRC64("objects/object_link_child/gLinkHumanLeftHandClosedDL"),
        CRC64("objects/object_link_child/gLinkHumanRightHandClosedDL"),
    };

    Gfx* dst = BufferFor(nativePath);
    if (dst == nullptr) {
        return nullptr;
    }

    bool replaced = false;
    int si = 0;
    for (int di = 0; di < kMaxCommands - 1;) {
        const uint8_t op = (uint8_t)(src[si].words.w0 >> 24);
        if (op == G_DL_OTR_HASH) {
            const uint64_t hash = ((uint64_t)src[si + 1].words.w0 << 32) | (uint32_t)src[si + 1].words.w1;
            if (hash == kFistHashes[0] || hash == kFistHashes[1]) {
                dst[di].words.w0 = (uintptr_t)G_DL_OTR_FILEPATH << 24;
                dst[di].words.w1 = (uintptr_t)portedFistPath;
                di++;
                si += 2;
                replaced = true;
                continue;
            }
            dst[di++] = src[si++];
            dst[di++] = src[si++];
            continue;
        }

        dst[di++] = src[si++];
        if (IsExpanded(op) && di < kMaxCommands) {
            dst[di++] = src[si++];
        }
        if (op == G_ENDDL) {
            return replaced ? dst : nullptr;
        }
    }
    return nullptr;
}
