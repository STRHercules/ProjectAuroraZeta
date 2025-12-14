// Marks an enemy spawner used by assassination mini-events.
#pragma once

namespace Game {

struct Spawner {
    float spawnTimer{1.5f};
    float interval{3.5f};
    int eventId{0};
    float lifetime{0.0f};  // seconds until auto-despawn; 0 means infinite
};

}  // namespace Game
