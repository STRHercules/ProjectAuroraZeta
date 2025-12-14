#include "GlobalUpgrades.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Game::Meta {

namespace {
constexpr double kOnePercent = 0.01;
constexpr double kHalfPercent = 0.005;
constexpr double kTenPercent = 0.10;

UpgradeLevels sanitizeLevels(const UpgradeLevels& in) {
    UpgradeLevels out = in;
    for (const auto& def : upgradeDefinitions()) {
        if (int* ptr = levelPtrByKey(out, def.key)) {
            *ptr = clampLevel(def, *ptr);
        }
    }
    return out;
}
}  // namespace

const std::vector<UpgradeDef>& upgradeDefinitions() {
    static const std::vector<UpgradeDef> kDefs = {
        {"attack_power", "Attack Power", "+1% damage per level", -1, 250, 1.15},
        {"attack_speed", "Attack Speed", "+1% attack speed per level", -1, 250, 1.15},
        {"health", "Health", "+1% max HP per level", -1, 250, 1.15},
        {"speed", "Speed", "+1% move speed per level", -1, 250, 1.15},
        {"armor", "Armor", "+1% armor per level", -1, 250, 1.15},
        {"shields", "Shields", "+1% shields per level", -1, 250, 1.15},
        {"lifesteal", "Lifesteal", "+0.5% lifesteal per level", -1, 400, 1.18},
        {"regeneration", "Regeneration", "+0.5 HP/s per level", -1, 400, 1.18},
        {"lives", "Lives", "+1 life (max 3)", 3, 2000, 2.0},
        {"difficulty", "Difficulty", "+1% enemy power & count per level", -1, 800, 1.20},
        {"mastery", "Mastery", "+10% to all stats (incl. difficulty)", 1, 250000, 5.0},
    };
    return kDefs;
}

int64_t costForNext(const UpgradeDef& def, int currentLevel) {
    int clamped = clampLevel(def, currentLevel);
    if (def.maxLevel >= 0 && clamped >= def.maxLevel) return std::numeric_limits<int64_t>::max();
    double cost = static_cast<double>(def.baseCost) * std::pow(def.growth, static_cast<double>(clamped));
    cost = std::max(cost, 1.0);
    const double maxInt = static_cast<double>(std::numeric_limits<int64_t>::max());
    if (cost > maxInt) return std::numeric_limits<int64_t>::max();
    return static_cast<int64_t>(std::floor(cost));
}

int clampLevel(const UpgradeDef& def, int level) {
    int result = std::max(0, level);
    if (def.maxLevel >= 0) {
        result = std::min(result, def.maxLevel);
    }
    return result;
}

GlobalModifiers computeModifiers(const UpgradeLevels& rawLevels) {
    UpgradeLevels levels = sanitizeLevels(rawLevels);
    GlobalModifiers m{};
    m.masteryMult = 1.0f + static_cast<float>(kTenPercent * static_cast<double>(levels.mastery));

    auto percent = [&](int lvl) { return 1.0f + static_cast<float>(kOnePercent * static_cast<double>(lvl)) ; };
    m.playerAttackPowerMult = percent(levels.attackPower) * m.masteryMult;
    m.playerAttackSpeedMult = percent(levels.attackSpeed) * m.masteryMult;
    m.playerHealthMult = percent(levels.health) * m.masteryMult;
    m.playerSpeedMult = percent(levels.speed) * m.masteryMult;
    m.playerArmorMult = percent(levels.armor) * m.masteryMult;
    m.playerShieldsMult = percent(levels.shields) * m.masteryMult;
    m.playerLifestealAdd = static_cast<float>(kHalfPercent * static_cast<double>(levels.lifesteal)) * m.masteryMult;
    m.playerRegenAdd = 0.5f * static_cast<float>(levels.regeneration) * m.masteryMult;
    m.playerLivesAdd = levels.lives;
    float difficultyMul = percent(levels.difficulty);
    m.enemyPowerMult = difficultyMul * m.masteryMult;
    m.enemyCountMult = difficultyMul * m.masteryMult;
    return m;
}

static std::string percentString(double value, int precision = 1) {
    std::ostringstream ss;
    ss.setf(std::ios::fixed);
    ss.precision(precision);
    ss << value * 100.0 << "%";
    return ss.str();
}

std::string currentEffectString(const UpgradeDef& def, const UpgradeLevels& levels) {
    const int lvl = levelPtrByKey(levels, def.key) ? *levelPtrByKey(levels, def.key) : 0;
    auto mult1 = [&](double per) {
        double mult = 1.0 + per * static_cast<double>(lvl);
        std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(2);
        ss << "x" << mult;
        return ss.str();
    };

    if (def.key == "attack_power" || def.key == "attack_speed" || def.key == "health" || def.key == "speed" ||
        def.key == "armor" || def.key == "shields") {
        return mult1(kOnePercent);
    }
    if (def.key == "lifesteal") {
        return percentString(kHalfPercent * static_cast<double>(lvl), 2) + " lifesteal";
    }
    if (def.key == "regeneration") {
        std::ostringstream ss; ss << (0.5 * lvl) << " HP/s";
        return ss.str();
    }
    if (def.key == "lives") {
        return std::to_string(lvl) + " extra lives";
    }
    if (def.key == "difficulty") {
        return mult1(kOnePercent) + " enemies";
    }
    if (def.key == "mastery") {
        double mult = 1.0 + kTenPercent * static_cast<double>(lvl);
        std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(2);
        ss << "All stats x" << mult;
        return ss.str();
    }
    return "-";
}

std::string nextEffectString(const UpgradeDef& def, const UpgradeLevels& levels) {
    int lvl = levelPtrByKey(levels, def.key) ? *levelPtrByKey(levels, def.key) : 0;
    if (def.maxLevel >= 0 && lvl >= def.maxLevel) return "MAX";
    UpgradeLevels copy = levels;
    if (int* ptr = levelPtrByKey(copy, def.key)) *ptr += 1;
    return currentEffectString(def, copy);
}

int* levelPtrByKey(UpgradeLevels& levels, std::string_view key) {
    if (key == "attack_power") return &levels.attackPower;
    if (key == "attack_speed") return &levels.attackSpeed;
    if (key == "health") return &levels.health;
    if (key == "speed") return &levels.speed;
    if (key == "armor") return &levels.armor;
    if (key == "shields") return &levels.shields;
    if (key == "lifesteal") return &levels.lifesteal;
    if (key == "regeneration") return &levels.regeneration;
    if (key == "lives") return &levels.lives;
    if (key == "difficulty") return &levels.difficulty;
    if (key == "mastery") return &levels.mastery;
    return nullptr;
}

const int* levelPtrByKey(const UpgradeLevels& levels, std::string_view key) {
    return levelPtrByKey(const_cast<UpgradeLevels&>(levels), key);
}

DepositResult applyDepositGuard(int64_t vaultGold,
                                int64_t runGold,
                                const std::string& matchId,
                                const std::string& lastDepositedId) {
    DepositResult res{};
    if (matchId.empty() || matchId == lastDepositedId) {
        res.newVault = vaultGold;
        res.newLastId = lastDepositedId;
        res.performed = false;
        return res;
    }
    int64_t deposit = std::max<int64_t>(0, runGold);
    int64_t maxVal = std::numeric_limits<int64_t>::max();
    if (maxVal - vaultGold < deposit) {
        res.newVault = maxVal;
    } else {
        res.newVault = vaultGold + deposit;
    }
    res.deposited = deposit;
    res.newLastId = matchId;
    res.performed = true;
    return res;
}

}  // namespace Game::Meta
