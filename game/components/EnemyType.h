// Identifies which EnemyDefinition an enemy instance was spawned from.
#pragma once

namespace Game {

struct EnemyType {
    int defIndex{-1};  // index into GameRoot::enemyDefs_
};

}  // namespace Game

