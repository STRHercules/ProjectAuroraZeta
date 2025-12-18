// Lightweight background music player.
#pragma once

#include <array>
#include <string>

namespace Engine::Audio {

enum class MusicTrack {
    MainMenu = 0,
    Gameplay = 1,
    Boss = 2,
    Death = 3,
    Count
};

class MusicPlayer {
public:
    MusicPlayer() = default;
    ~MusicPlayer();

    MusicPlayer(const MusicPlayer&) = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;

    bool initialize();
    void shutdown();

    bool load(MusicTrack track, const std::string& path);
    bool play(MusicTrack track, bool loop);
    void stop();

    void setVolume(float volume01);
    float volume() const { return volume01_; }

    bool isInitialized() const { return initialized_; }

private:
    struct TrackSlot {
        std::string path;
        void* handle{nullptr};
        bool loaded{false};
    };

    static constexpr std::size_t kTrackCount = static_cast<std::size_t>(MusicTrack::Count);

    bool initialized_{false};
    bool warnedNoBackend_{false};
    float volume01_{0.65f};
    MusicTrack current_{MusicTrack::Count};
    std::array<TrackSlot, kTrackCount> tracks_{};
};

}  // namespace Engine::Audio

