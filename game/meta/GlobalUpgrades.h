#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Game::Meta {

struct UpgradeLevels {
    int attackPower{0};
    int attackSpeed{0};
    int health{0};
    int speed{0};
    int armor{0};
    int shields{0};
    int lifesteal{0};
    int regeneration{0};
    int lives{0};
    int difficulty{0};
    int mastery{0};
};

struct GlobalModifiers {
    float playerAttackPowerMult{1.0f};
    float playerAttackSpeedMult{1.0f};
    float playerHealthMult{1.0f};
    float playerSpeedMult{1.0f};
    float playerArmorMult{1.0f};
    float playerShieldsMult{1.0f};
    float playerLifestealAdd{0.0f};   // percent (e.g., 0.01 == +1%)
    float playerRegenAdd{0.0f};       // flat HP/sec
    int   playerLivesAdd{0};          // extra lives granted at run start
    float enemyPowerMult{1.0f};
    float enemyCountMult{1.0f};
    float masteryMult{1.0f};
};

struct UpgradeDef {
    std::string key;
    std::string displayName;
    std::string effectLabel;
    int maxLevel{-1};          // -1 = infinite
    int64_t baseCost{0};
    double growth{1.0};
};

// Returns a stable list of upgrade definitions in display order.
const std::vector<UpgradeDef>& upgradeDefinitions();

// Cost to purchase the next level given the current level.
int64_t costForNext(const UpgradeDef& def, int currentLevel);

// Clamp and sanitize level value for a definition.
int clampLevel(const UpgradeDef& def, int level);

// Compute final global modifiers from upgrade levels (includes mastery).
GlobalModifiers computeModifiers(const UpgradeLevels& levels);

// Human-readable current/next effect strings for UI.
std::string currentEffectString(const UpgradeDef& def, const UpgradeLevels& levels);
std::string nextEffectString(const UpgradeDef& def, const UpgradeLevels& levels);

// Utility to get a pointer to the level field by key; returns nullptr if key is unknown.
int* levelPtrByKey(UpgradeLevels& levels, std::string_view key);
const int* levelPtrByKey(const UpgradeLevels& levels, std::string_view key);

struct DepositResult {
    int64_t deposited{0};
    int64_t newVault{0};
    std::string newLastId;
    bool performed{false};
};

// Applies the anti-double-deposit guard; updates vault amount and last match id.
DepositResult applyDepositGuard(int64_t vaultGold,
                                int64_t runGold,
                                const std::string& matchId,
                                const std::string& lastDepositedId);

}  // namespace Game::Meta
