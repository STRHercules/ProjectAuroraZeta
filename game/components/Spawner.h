// Marks an enemy spawner used by assassination mini-events.
#pragma once

namespace Game {

struct Spawner {
    float spawnTimer{1.5f};
    float interval{3.5f};
    int eventId{0};
};

}  // namespace Game
