#include "SfxPlayer.h"

#include "AudioBackend.h"
#include "../core/Logger.h"

#include <algorithm>
#include <random>

#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
#    include <SDL_mixer.h>
#endif

namespace Engine::Audio {

SfxPlayer::~SfxPlayer() { shutdown(); }

bool SfxPlayer::initialize() {
    if (initialized_) return true;
    if (!acquireBackend()) {
        initialized_ = false;
        return false;
    }
    initialized_ = true;
    return true;
}

void SfxPlayer::freeAll() {
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    for (auto& kv : chunks_) {
        if (kv.second.handle) {
            Mix_FreeChunk(static_cast<Mix_Chunk*>(kv.second.handle));
        }
    }
#endif
    chunks_.clear();
}

void SfxPlayer::shutdown() {
    if (!initialized_) {
        freeAll();
        return;
    }
    freeAll();
    releaseBackend();
    initialized_ = false;
}

void SfxPlayer::setVolume(float volume01) { volume01_ = std::clamp(volume01, 0.0f, 1.0f); }

Engine::Audio::SfxPlayer::ChunkSlot* SfxPlayer::getOrLoad(const std::string& path) {
    auto it = chunks_.find(path);
    if (it != chunks_.end()) return &it->second;

    ChunkSlot slot{};
    slot.path = path;
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    if (!initialize()) return nullptr;
    Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
    if (!chunk) {
        logWarn(std::string("Mix_LoadWAV failed for ") + path + ": " + Mix_GetError());
        chunks_.insert({path, slot});
        return nullptr;
    }
    slot.handle = chunk;
    auto [insIt, _] = chunks_.insert({path, slot});
    return &insIt->second;
#else
    chunks_.insert({path, slot});
    return nullptr;
#endif
}

bool SfxPlayer::preload(const std::string& path) { return getOrLoad(path) != nullptr; }

bool SfxPlayer::play(const std::string& path, float volumeMul01) {
#if defined(ZETA_HAS_SDL_MIXER) && (ZETA_HAS_SDL_MIXER == 1)
    auto* slot = getOrLoad(path);
    if (!slot || !slot->handle) return false;
    auto* chunk = static_cast<Mix_Chunk*>(slot->handle);
    const float v = std::clamp(volume01_ * std::clamp(volumeMul01, 0.0f, 1.0f), 0.0f, 1.0f);
    Mix_VolumeChunk(chunk, static_cast<int>(v * 128.0f));
    return Mix_PlayChannel(-1, chunk, 0) != -1;
#else
    (void)path;
    (void)volumeMul01;
    return false;
#endif
}

bool SfxPlayer::playRandom(const std::vector<std::string>& paths, std::mt19937& rng, float volumeMul01) {
    if (paths.empty()) return false;
    std::uniform_int_distribution<std::size_t> pick(0, paths.size() - 1);
    return play(paths[pick(rng)], volumeMul01);
}

}  // namespace Engine::Audio

