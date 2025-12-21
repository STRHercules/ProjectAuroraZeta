// Data-first RPG content models (loot, talents, consumables, archetype bios).
#pragma once

#include <array>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../engine/gameplay/RPGStats.h"
#include "../meta/ItemDefs.h"

namespace Game::RPG {

using Engine::Gameplay::RPG::Attributes;
using Engine::Gameplay::RPG::StatContribution;

enum class EquipmentSlot {
    Head = 0,
    Chest,
    Legs,
    Boots,
    MainHand,
    OffHand,
    Ring1,
    Ring2,
    Amulet,
    Trinket,
    Cloak,
    Gloves,
    Belt,
    Ammo,
    Bag,
    Consumable,  // inventory-only RPG items (not equippable)
    Count
};

enum class Rarity { Common, Uncommon, Rare, Epic, Legendary, Unique };
enum class CombatType { Melee, Ranged, Magical, Ammo, Count };

enum class ImplicitStatId {
    AttackPower,
    SpellPower,
    Armor,
    ArmorPen,
    CritChance,
    AttackSpeed,
    MoveSpeed,
    LifeOnHit,
    Lifesteal,
    CleaveChance,
    CooldownReduction,
    ResourceRegen,
    StatusChance
};

enum class StatValueMode { Flat, Mult };

struct ImplicitStatPoolEntry {
    ImplicitStatId stat{ImplicitStatId::AttackPower};
    StatValueMode mode{StatValueMode::Flat};
    float min{0.0f};
    float max{0.0f};
    float weight{1.0f};
};

struct Affix {
    std::string id;
    std::string name;
    StatContribution stats;
};

struct ItemTemplate {
    std::string id;
    std::string name;
    EquipmentSlot slot{EquipmentSlot::Head};
    CombatType combatType{CombatType::Count};
    Rarity rarity{Rarity::Common};
    std::string iconSheet{};  // e.g. "Gear.png"
    int iconRow{0};           // 0-based
    int iconCol{0};           // 0-based
    // Optional inventory-only consumable hook (ties into data/rpg/consumables.json).
    std::string consumableId{};
    // Bag slot: expands inventory capacity when equipped.
    int bagSlots{0};
    // Socketing
    bool socketable{false};  // true for gems that can be inserted into gear sockets
    int socketsMax{0};       // number of gem sockets provided by this item (0 = none)
    StatContribution baseStats{};
    int maxAffixes{1};
    std::vector<std::string> affixPool;  // ids into LootTable::affixes
};

struct GeneratedItem {
    ItemTemplate def;
    std::vector<Affix> affixes;
    std::vector<StatContribution> implicitStats;
    float baseScalar{1.0f};
    float affixScalar{1.0f};
    Attributes attributeBonus{};
    int itemLevel{1};
    float itemScale{1.0f};
    StatContribution contribution() const;
};

struct LootContext {
    int level{1};
    float luck{0.0f};  // 0..1 expected, but uncapped
    int itemLevel{1};
    float itemScalePerLevel{0.06f};
};

struct LootTable {
    std::vector<ItemTemplate> items;
    std::vector<Affix> affixes;
    std::array<std::array<float, 3>, 6> affixTierWeights{};
    std::array<std::array<int, 2>, 6> affixCountByRarity{};
    std::array<std::array<int, 2>, 6> attributeBonusCountByRarity{};
    std::array<std::array<int, 2>, 6> attributeBonusValueByRarity{};
    std::array<float, 6> attributeBonusSecondChanceByRarity{};
    std::array<std::array<float, 2>, 6> baseStatScalarByRarity{};
    std::array<std::array<float, 2>, 6> affixScalarByRarity{};
    std::array<std::array<float, 2>, 6> armorImplicitScalarByRarity{};
    std::array<std::vector<ImplicitStatPoolEntry>, static_cast<std::size_t>(CombatType::Count)> implicitStatPools{};
    std::vector<ImplicitStatPoolEntry> armorImplicitPool{};
    std::vector<ImplicitStatPoolEntry> fallbackImplicitPool{};
};

LootTable defaultLootTable();
std::array<std::array<float, 3>, 6> defaultAffixTierWeights();
std::array<std::array<int, 2>, 6> defaultAffixCountByRarity();
std::array<std::array<int, 2>, 6> defaultAttributeBonusCountByRarity();
std::array<std::array<int, 2>, 6> defaultAttributeBonusValueByRarity();
std::array<float, 6> defaultAttributeBonusSecondChanceByRarity();
std::array<std::array<float, 2>, 6> defaultBaseStatScalarByRarity();
std::array<std::array<float, 2>, 6> defaultAffixScalarByRarity();
std::array<std::array<float, 2>, 6> defaultArmorImplicitScalarByRarity();
std::array<std::vector<ImplicitStatPoolEntry>, static_cast<std::size_t>(CombatType::Count)> defaultImplicitStatPools();
std::vector<ImplicitStatPoolEntry> defaultArmorImplicitPool();
std::vector<ImplicitStatPoolEntry> defaultFallbackImplicitPool();
GeneratedItem generateLoot(const LootTable& table, const LootContext& ctx, std::mt19937& rng);
LootTable loadLootTable(const std::string& path);
StatContribution computeItemContribution(const LootTable& table, const ItemDefinition& def, int quantity = 1);
StatContribution computeEquipmentContribution(const LootTable& table, const std::vector<ItemDefinition>& defs);
int ComputeItemLevelFromWave(int wave, int waveDivisor = 3);
float StatScaleFactor(int itemLevel, float perLevel = 0.06f);

// Talent nodes: match-permanent bonuses granted every N levels.
struct TalentNode {
    std::string id;
    std::string name;
    std::string description;
    StatContribution bonus;
    Attributes attributes{};
    int maxRank{3};
    std::vector<std::string> prereqs;
    std::array<int, 2> pos{0, 0};
    int tier{1};
};

struct TalentTier {
    int tier{1};
    int unlockPoints{0};
    std::vector<TalentNode> nodes;
};

struct TalentTree {
    std::string archetypeId;
    std::vector<TalentTier> tiers;
};

// Consumables framework.
enum class ConsumableCategory { Heal, Cleanse, Buff, Bomb, Food, Scripted };
enum class ConsumableResource { Health, Shields };

struct ConsumableEffect {
    ConsumableCategory category{ConsumableCategory::Heal};
    ConsumableResource resource{ConsumableResource::Health};
    float magnitude{0.0f};
    float duration{0.0f};
    Engine::Gameplay::RPG::DamageType damageType{Engine::Gameplay::RPG::DamageType::Physical};
    // Optional stat contribution payload (primarily used for Buff effects).
    // If present, prefer this over interpreting `magnitude` as a hardcoded move-speed buff.
    Engine::Gameplay::RPG::StatContribution stats{};
    std::vector<int> cleanseIds;  // status ids to remove when category == Cleanse
    // Optional scripted effect id (used for scrolls/advanced consumables).
    std::string scriptId{};
    // Optional numeric params for scripted effects (schema-free tuning).
    std::vector<float> params{};
};

struct ConsumableDef {
    std::string id;
    std::string name;
    std::string description;
    std::string cooldownGroup;
    float cooldown{10.0f};
    std::vector<ConsumableEffect> effects;
};

std::vector<ConsumableDef> loadConsumables(const std::string& path);
std::unordered_map<std::string, TalentTree> loadTalentTrees(const std::string& path);

struct ArchetypeBio {
    std::string id;
    std::string name;
    std::string biography;
    Attributes attributes{};
    std::vector<std::string> specialties;
    std::vector<std::string> perks;
};

// Loads archetype bios from JSON path; falls back to defaults if missing.
std::unordered_map<std::string, ArchetypeBio> loadArchetypeBios(const std::string& path);

// Default consumable definitions (data-driven hook).
std::vector<ConsumableDef> defaultConsumables();

}  // namespace Game::RPG
