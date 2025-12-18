// Lightweight sound effects (SFX) player with cached audio chunks.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

namespace Engine::Audio {

class SfxPlayer {
public:
    SfxPlayer() = default;
    ~SfxPlayer();

    SfxPlayer(const SfxPlayer&) = delete;
    SfxPlayer& operator=(const SfxPlayer&) = delete;

    bool initialize();
    void shutdown();

    bool preload(const std::string& path);
    bool play(const std::string& path, float volumeMul01 = 1.0f);

    bool playRandom(const std::vector<std::string>& paths, std::mt19937& rng, float volumeMul01 = 1.0f);

    void setVolume(float volume01);
    float volume() const { return volume01_; }

    bool isInitialized() const { return initialized_; }

private:
    struct ChunkSlot {
        std::string path;
        void* handle{nullptr};
    };

    ChunkSlot* getOrLoad(const std::string& path);
    void freeAll();

    bool initialized_{false};
    float volume01_{0.75f};
    std::unordered_map<std::string, ChunkSlot> chunks_{};
};

}  // namespace Engine::Audio
