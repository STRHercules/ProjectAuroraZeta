// Data definitions for shop/inventory items.
#pragma once

#include <string>
#include <vector>

namespace Game {

enum class ItemKind { Combat, Support, Unique };
enum class ItemRarity { Common, Rare, Legendary };
enum class ItemEffect {
    Damage,
    Health,
    Speed,
    Heal,
    FreezeTime,
    Turret,
    AttackSpeed,
    Lifesteal,
    Chain,
    DamagePercent,
    RangeVision,
    AttackSpeedPercent,
    MoveSpeedFlat,
    BonusVitalsPercent,
    CooldownFaster,
    AbilityCharges,
    VitalCostReduction
};

struct ItemDefinition {
    int id{-1};
    std::string name;
    std::string desc;
    ItemKind kind{ItemKind::Combat};
    ItemRarity rarity{ItemRarity::Common};
    int cost{0};
    ItemEffect effect{ItemEffect::Damage};
    float value{0.0f};
};

// Returns a static catalog of themed items used for shop/inventory.
std::vector<ItemDefinition> defaultItemCatalog();
// Gold shop catalog (traveling shop).
std::vector<ItemDefinition> goldShopCatalog();

}  // namespace Game
