#include "MusicPlayer.h"

#include "AudioBackend.h"
#include "../core/Logger.h"

#include <algorithm>

#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
#    include <SDL_mixer.h>
#endif

namespace Engine::Audio {

static constexpr std::size_t toIndex(MusicTrack t) {
    return static_cast<std::size_t>(t);
}

MusicPlayer::~MusicPlayer() { shutdown(); }

bool MusicPlayer::initialize() {
    if (initialized_) return true;

#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    if (!acquireBackend()) return false;
    Mix_VolumeMusic(static_cast<int>(std::clamp(volume01_, 0.0f, 1.0f) * 128.0f));
    initialized_ = true;
    return true;
#else
    if (!warnedNoBackend_) {
        logWarn("Audio disabled: SDL2_mixer not found at build time.");
        warnedNoBackend_ = true;
    }
    initialized_ = false;
    return false;
#endif
}

void MusicPlayer::shutdown() {
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    if (initialized_) {
        stop();
        for (auto& slot : tracks_) {
            if (slot.handle) {
                Mix_FreeMusic(static_cast<Mix_Music*>(slot.handle));
            }
            slot = {};
        }
        releaseBackend();
    }
#endif
    initialized_ = false;
    current_ = MusicTrack::Count;
}

bool MusicPlayer::load(MusicTrack track, const std::string& path) {
    if (track == MusicTrack::Count) return false;
    auto& slot = tracks_[toIndex(track)];

    if (slot.loaded && slot.path == path) return true;

#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    if (!initialize()) return false;

    if (slot.handle) {
        Mix_FreeMusic(static_cast<Mix_Music*>(slot.handle));
        slot.handle = nullptr;
    }

    Mix_Music* mus = Mix_LoadMUS(path.c_str());
    if (!mus) {
        logWarn(std::string("Mix_LoadMUS failed for ") + path + ": " + Mix_GetError());
        slot = {};
        slot.path = path;
        return false;
    }

    slot.path = path;
    slot.handle = mus;
    slot.loaded = true;
    return true;
#else
    (void)path;
    if (!warnedNoBackend_) {
        logWarn("Audio disabled: cannot load music (SDL2_mixer missing).");
        warnedNoBackend_ = true;
    }
    slot.path = path;
    slot.loaded = false;
    slot.handle = nullptr;
    return false;
#endif
}

bool MusicPlayer::play(MusicTrack track, bool loop) {
    if (track == MusicTrack::Count) return false;
    auto& slot = tracks_[toIndex(track)];

#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    if (!initialize()) return false;
    if (!slot.loaded || !slot.handle) {
        logWarn("Music track not loaded; cannot play.");
        return false;
    }
    if (current_ == track && Mix_PlayingMusic()) {
        return true;
    }

    const int loops = loop ? -1 : 0;
    if (Mix_PlayMusic(static_cast<Mix_Music*>(slot.handle), loops) != 0) {
        logWarn(std::string("Mix_PlayMusic failed: ") + Mix_GetError());
        return false;
    }
    current_ = track;
    return true;
#else
    (void)loop;
    if (!warnedNoBackend_) {
        logWarn("Audio disabled: cannot play music (SDL2_mixer missing).");
        warnedNoBackend_ = true;
    }
    current_ = MusicTrack::Count;
    return false;
#endif
}

void MusicPlayer::stop() {
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    Mix_HaltMusic();
#endif
    current_ = MusicTrack::Count;
}

void MusicPlayer::setVolume(float volume01) {
    volume01_ = std::clamp(volume01, 0.0f, 1.0f);
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    if (initialized_) {
        Mix_VolumeMusic(static_cast<int>(volume01_ * 128.0f));
    }
#endif
}

}  // namespace Engine::Audio
