// Data-first RPG content models (loot, talents, consumables, archetype bios).
#pragma once

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../engine/gameplay/RPGStats.h"

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

enum class Rarity { Common, Uncommon, Rare, Epic, Legendary };

struct Affix {
    std::string id;
    std::string name;
    StatContribution stats;
};

struct ItemTemplate {
    std::string id;
    std::string name;
    EquipmentSlot slot{EquipmentSlot::Head};
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
    StatContribution contribution() const;
};

struct LootContext {
    int level{1};
    float luck{0.0f};  // 0..1 expected, but uncapped
};

struct LootTable {
    std::vector<ItemTemplate> items;
    std::vector<Affix> affixes;
};

LootTable defaultLootTable();
GeneratedItem generateLoot(const LootTable& table, const LootContext& ctx, std::mt19937& rng);
LootTable loadLootTable(const std::string& path);

// Talent nodes: match-permanent bonuses granted every N levels.
struct TalentNode {
    std::string id;
    std::string name;
    std::string description;
    StatContribution bonus;
    int maxRank{3};
};

struct TalentTree {
    std::string archetypeId;
    std::vector<TalentNode> nodes;
};

// Consumables framework.
enum class ConsumableCategory { Heal, Cleanse, Buff, Bomb, Food };
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
};

struct ConsumableDef {
    std::string id;
    std::string name;
    std::string cooldownGroup;
    float cooldown{10.0f};
    std::vector<ConsumableEffect> effects;
};

std::vector<ConsumableDef> loadConsumables(const std::string& path);
std::unordered_map<std::string, std::vector<TalentNode>> loadTalentTrees(const std::string& path);

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
