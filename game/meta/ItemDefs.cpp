#include "ItemDefs.h"

namespace Game {

std::vector<ItemDefinition> defaultItemCatalog() {
    std::vector<ItemDefinition> items;
    int id = 0;
    // Combat
    items.push_back({id++, "Power Coil", "+10 Attack damage.", ItemKind::Combat, ItemRarity::Common, 40,
                     ItemEffect::Damage, 10.0f});
    items.push_back({id++, "Reinforced Plating", "+25 Max HP.", ItemKind::Combat, ItemRarity::Common, 45,
                     ItemEffect::Health, 25.0f});
    items.push_back({id++, "Vector Thrusters", "+8% Move Speed.", ItemKind::Combat, ItemRarity::Common, 35,
                     ItemEffect::Speed, 0.08f});
    // Support
    items.push_back({id++, "Field Medkit", "Use to heal 35% HP.", ItemKind::Support, ItemRarity::Common, 30,
                     ItemEffect::Heal, 0.35f});
    items.push_back({id++, "Cryo Capsule", "Freeze time for 2.5s.", ItemKind::Support, ItemRarity::Rare, 70,
                     ItemEffect::FreezeTime, 2.5f});
    items.push_back({id++, "Deployable Turret", "Temporary turret that fires for 12s.", ItemKind::Support, ItemRarity::Rare, 90,
                     ItemEffect::Turret, 12.0f});
    // Unique (Legendary)
    items.push_back({id++, "Chrono Prism", "Slows nearby enemies and grants +15% attack speed.", ItemKind::Unique,
                     ItemRarity::Legendary, 140, ItemEffect::AttackSpeed, 0.15f});
    items.push_back({id++, "Phase Leech", "Hits restore 5% damage as HP.", ItemKind::Unique, ItemRarity::Legendary,
                     150, ItemEffect::Lifesteal, 0.05f});
    items.push_back({id++, "Storm Core", "Projectiles chain to 2 extra targets.", ItemKind::Unique, ItemRarity::Legendary,
                     160, ItemEffect::Chain, 2.0f});
    return items;
}

std::vector<ItemDefinition> goldShopCatalog() {
    std::vector<ItemDefinition> items;
    int id = 1000;
    items.push_back({id++, "Cataclysm Rounds", "Increase all damage by 50%.", ItemKind::Unique,
                     ItemRarity::Legendary, 4, ItemEffect::DamagePercent, 0.5f});
    items.push_back({id++, "Survey Optics", "Increase weapon and vision range by 3.", ItemKind::Unique,
                     ItemRarity::Legendary, 4, ItemEffect::RangeVision, 3.0f});
    items.push_back({id++, "Temporal Accelerator", "Increase attack speed by 50%.", ItemKind::Unique,
                     ItemRarity::Legendary, 4, ItemEffect::AttackSpeedPercent, 0.5f});
    items.push_back({id++, "Speed Boots", "Increase movement speed by 1.", ItemKind::Unique,
                     ItemRarity::Legendary, 3, ItemEffect::MoveSpeedFlat, 1.0f});
    items.push_back({id++, "Nanite Surge", "Increase HP, Shields, Energy by 25%.", ItemKind::Unique,
                     ItemRarity::Legendary, 5, ItemEffect::BonusVitalsPercent, 0.25f});
    items.push_back({id++, "Cooldown Nexus", "Abilities cooldown 25% faster.", ItemKind::Unique,
                     ItemRarity::Legendary, 5, ItemEffect::CooldownFaster, 0.25f});
    items.push_back({id++, "Charge Matrix", "Abilities that use charges get +1 max.", ItemKind::Unique,
                     ItemRarity::Legendary, 4, ItemEffect::AbilityCharges, 1.0f});
    items.push_back({id++, "Vital Austerity", "Abilities that cost vitals cost 25% less.", ItemKind::Unique,
                     ItemRarity::Legendary, 4, ItemEffect::VitalCostReduction, 0.25f});
    return items;
}

}  // namespace Game
