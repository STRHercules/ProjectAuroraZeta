// Data structure describing an enemy archetype sourced from spritesheet assets.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "../engine/assets/Texture.h"

namespace Game {

enum class EnemyDeathAnimMode {
    DirectionalRows,  // use deathRows[dir]
    RandomRows,       // choose one row at death time from deathRandomRows
};

struct EnemyDefinition {
    std::string id;
    Engine::TexturePtr texture;
    // Optional pool of textures to pick from per-spawn (e.g., slime color variants).
    std::vector<Engine::TexturePtr> variantTextures{};

    // --- Core tuning (relative to WaveSettings) ---
    float sizeMultiplier{1.0f};
    float hpMultiplier{1.0f};
    float shieldMultiplier{1.0f};
    float speedMultiplier{1.0f};
    float damageMultiplier{1.0f};

    // Health regen expressed as fraction of maxHealth per second (0 disables).
    float healthRegenFracPerSecond{0.0f};
    float shieldRegenMultiplier{1.0f};
    float regenDelayMultiplier{1.0f};

    // Spawn behavior (e.g., goblin packs).
    int spawnGroupMin{1};
    int spawnGroupMax{1};

    // On-hit effects (applied on enemy contact hits).
    float onHitBleedChance{0.0f};
    float onHitBleedDuration{0.0f};
    float onHitBleedDpsMul{0.0f};   // DPS = contactDamage * mul
    float onHitPoisonChance{0.0f};
    float onHitPoisonDuration{0.0f};
    float onHitPoisonDpsMul{0.0f};  // DPS = contactDamage * mul
    float onHitFearChance{0.0f};
    float onHitFearDuration{0.0f};

    // Revive behavior (e.g., mummy/skelly).
    float reviveChance{0.0f};
    float reviveHealthFraction{0.35f};
    float reviveDelay{0.65f};  // seconds to "play dead" before resuming
    int reviveMaxCount{1};

    // Slime-only behavior.
    float slimeMultiplyChance{0.0f};      // chance per interval
    float slimeMultiplyInterval{0.0f};    // seconds between checks
    int slimeMultiplyMin{0};
    int slimeMultiplyMax{0};
    int slimeMultiplyMaxCount{0};         // per slime
    float slimeDeathExplodeChance{0.0f};  // explode vs melt
    float slimeDeathExplodeRadius{0.0f};  // world units
    float slimeDeathExplodeDamageMul{0.0f};
    int slimeDeathExplodeRow{-1};         // row index (0-based)
    int slimeDeathMeltRow{-1};            // row index (0-based)

    // Flame skull fireball.
    bool fireballEnabled{false};
    float fireballMinRange{0.0f};     // world units
    float fireballMaxRange{0.0f};     // world units
    float fireballCooldown{1.8f};     // seconds
    float fireballSpeed{260.0f};      // world units/sec
    float fireballDamageMul{1.0f};    // base = contactDamage * mul
    float fireballHitbox{10.0f};      // world units (square, used for AABB)
    float fireballLifetime{1.6f};     // seconds

    // --- Sprite-sheet animation metadata ---
    int frameWidth{16};
    int frameHeight{16};
    int frameCount{4};
    float frameDuration{0.16f};

    // Row mapping by direction: [Right, Left, Front, Back].
    std::array<int, 4> idleRows{0, 1, 2, 3};
    std::array<int, 4> moveRows{4, 5, 6, 7};
    std::array<int, 4> attackRows{8, 9, 10, 11};

    EnemyDeathAnimMode deathMode{EnemyDeathAnimMode::DirectionalRows};
    std::array<int, 4> deathRows{12, 13, 12, 12};
    std::vector<int> deathRandomRows{};
};

}  // namespace Game
