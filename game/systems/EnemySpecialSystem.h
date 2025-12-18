// Handles enemy-specific mechanics that aren't covered by generic chase AI.
#pragma once

#include <random>
#include <vector>

#include "../../engine/ecs/Registry.h"
#include "../../engine/core/Time.h"
#include "../../engine/assets/Texture.h"
#include "../../engine/gameplay/RPGCombat.h"
#include "../EnemyDefinition.h"

namespace Game {

struct SpriteSheetVisual {
    Engine::TexturePtr texture{};
    int frameWidth{0};
    int frameHeight{0};
    int frameCount{1};
    float frameDuration{0.08f};
};

class EnemySpecialSystem {
public:
    void setEnemyDefinitions(const std::vector<EnemyDefinition>* defs) { enemyDefs_ = defs; }
    void setFireballVisual(const SpriteSheetVisual& vis) { fireballVis_ = vis; }

    void update(Engine::ECS::Registry& registry,
                const Engine::TimeStep& step,
                float contactDamageBase,
                bool useRpgCombat,
                const Engine::Gameplay::RPG::ResolverConfig& rpgCfg);

private:
    const std::vector<EnemyDefinition>* enemyDefs_{nullptr};
    SpriteSheetVisual fireballVis_{};
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace Game
