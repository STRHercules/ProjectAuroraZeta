#include "AudioBackend.h"

#include "../core/Logger.h"

#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
#    include <SDL_mixer.h>
#endif

namespace Engine::Audio {

namespace {
int gRefCount = 0;
#if !defined(ZETA_HAS_SDL_MIXER) || (ZETA_HAS_SDL_MIXER != 1)
bool gWarnedNoBackend = false;
#endif
}  // namespace

bool backendAvailable() {
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    return true;
#else
    return false;
#endif
}

bool acquireBackend() {
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    if (gRefCount++ > 0) {
        return true;
    }

    const int flags = Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG);
    if ((flags & MIX_INIT_OGG) != MIX_INIT_OGG) {
        logWarn(std::string("Mix_Init(OGG) incomplete: ") + Mix_GetError());
    }
    if ((flags & MIX_INIT_MP3) != MIX_INIT_MP3) {
        logWarn(std::string("Mix_Init(MP3) incomplete: ") + Mix_GetError());
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        logError(std::string("Mix_OpenAudio failed: ") + Mix_GetError());
        Mix_Quit();
        gRefCount = 0;
        return false;
    }
    return true;
#else
    if (!gWarnedNoBackend) {
        logWarn("Audio disabled: SDL2_mixer not found at build time.");
        gWarnedNoBackend = true;
    }
    return false;
#endif
}

void releaseBackend() {
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    if (gRefCount <= 0) {
        gRefCount = 0;
        return;
    }
    gRefCount -= 1;
    if (gRefCount == 0) {
        Mix_HaltMusic();
        Mix_CloseAudio();
        Mix_Quit();
    }
#else
    // no-op
#endif
}

}  // namespace Engine::Audio
