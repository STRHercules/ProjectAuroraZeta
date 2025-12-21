// Minimal data-driven content helpers for RPG systems.
#include "RPGContent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <unordered_set>
#include <nlohmann/json.hpp>

#include "../../engine/core/Logger.h"

namespace Game::RPG {

using nlohmann::json;
using Engine::Gameplay::RPG::Resistances;

std::array<std::array<float, 3>, 6> defaultAffixTierWeights() {
    return {{
        {{1.0f, 0.0f, 0.0f}},   // Common
        {{0.4f, 0.6f, 0.0f}},   // Uncommon
        {{0.15f, 0.25f, 0.6f}}, // Rare
        {{0.05f, 0.15f, 0.8f}}, // Epic
        {{0.0f, 0.1f, 0.9f}},   // Legendary
        {{0.0f, 0.05f, 0.95f}}, // Unique
    }};
}

std::array<std::array<int, 2>, 6> defaultAffixCountByRarity() {
    return {{
        {{1, 1}}, // Common
        {{1, 2}}, // Uncommon
        {{2, 3}}, // Rare
        {{3, 4}}, // Epic
        {{4, 5}}, // Legendary
        {{3, 3}}, // Unique
    }};
}

std::array<std::array<int, 2>, 6> defaultAttributeBonusCountByRarity() {
    return {{
        {{0, 0}}, // Common
        {{0, 0}}, // Uncommon
        {{0, 0}}, // Rare
        {{1, 2}}, // Epic
        {{1, 2}}, // Legendary
        {{1, 2}}, // Unique
    }};
}

std::array<std::array<int, 2>, 6> defaultAttributeBonusValueByRarity() {
    return {{
        {{0, 0}}, // Common
        {{0, 0}}, // Uncommon
        {{0, 0}}, // Rare
        {{1, 1}}, // Epic
        {{2, 2}}, // Legendary
        {{2, 3}}, // Unique
    }};
}

std::array<float, 6> defaultAttributeBonusSecondChanceByRarity() {
    return {{0.0f, 0.0f, 0.0f, 0.25f, 0.5f, 0.7f}};
}

std::array<std::array<float, 2>, 6> defaultBaseStatScalarByRarity() {
    return {{
        {{0.98f, 1.02f}}, // Common
        {{1.00f, 1.08f}}, // Uncommon
        {{1.05f, 1.15f}}, // Rare
        {{1.10f, 1.22f}}, // Epic
        {{1.18f, 1.32f}}, // Legendary
        {{1.25f, 1.40f}}, // Unique
    }};
}

std::array<std::array<float, 2>, 6> defaultAffixScalarByRarity() {
    return {{
        {{0.95f, 1.05f}}, // Common
        {{1.00f, 1.12f}}, // Uncommon
        {{1.08f, 1.20f}}, // Rare
        {{1.15f, 1.30f}}, // Epic
        {{1.25f, 1.45f}}, // Legendary
        {{1.35f, 1.55f}}, // Unique
    }};
}

std::array<std::array<float, 2>, 6> defaultArmorImplicitScalarByRarity() {
    return defaultBaseStatScalarByRarity();
}

std::array<std::vector<ImplicitStatPoolEntry>, static_cast<std::size_t>(CombatType::Count)> defaultImplicitStatPools() {
    std::array<std::vector<ImplicitStatPoolEntry>, static_cast<std::size_t>(CombatType::Count)> pools{};
    pools[static_cast<std::size_t>(CombatType::Melee)] = {
        {ImplicitStatId::AttackPower, StatValueMode::Flat, 2.0f, 6.0f, 3.0f},
        {ImplicitStatId::ArmorPen, StatValueMode::Flat, 1.0f, 4.0f, 2.0f},
        {ImplicitStatId::AttackSpeed, StatValueMode::Mult, 0.02f, 0.06f, 2.0f},
        {ImplicitStatId::Lifesteal, StatValueMode::Flat, 0.01f, 0.03f, 1.0f},
        {ImplicitStatId::CleaveChance, StatValueMode::Flat, 0.04f, 0.10f, 1.0f},
    };
    pools[static_cast<std::size_t>(CombatType::Ranged)] = {
        {ImplicitStatId::ArmorPen, StatValueMode::Flat, 1.0f, 4.0f, 2.0f},
        {ImplicitStatId::CritChance, StatValueMode::Flat, 0.02f, 0.06f, 2.0f},
        {ImplicitStatId::AttackSpeed, StatValueMode::Mult, 0.02f, 0.06f, 2.0f},
    };
    pools[static_cast<std::size_t>(CombatType::Magical)] = {
        {ImplicitStatId::SpellPower, StatValueMode::Flat, 3.0f, 8.0f, 3.0f},
        {ImplicitStatId::ResourceRegen, StatValueMode::Flat, 0.02f, 0.06f, 2.0f},
        {ImplicitStatId::CooldownReduction, StatValueMode::Flat, 0.02f, 0.05f, 2.0f},
        {ImplicitStatId::StatusChance, StatValueMode::Flat, 0.04f, 0.10f, 1.0f},
    };
    pools[static_cast<std::size_t>(CombatType::Ammo)] = {
        {ImplicitStatId::ArmorPen, StatValueMode::Flat, 1.0f, 3.0f, 2.0f},
        {ImplicitStatId::CritChance, StatValueMode::Flat, 0.02f, 0.05f, 2.0f},
    };
    return pools;
}

std::vector<ImplicitStatPoolEntry> defaultArmorImplicitPool() {
    return {
        {ImplicitStatId::Armor, StatValueMode::Flat, 1.0f, 3.0f, 1.0f},
    };
}

std::vector<ImplicitStatPoolEntry> defaultFallbackImplicitPool() {
    return {
        {ImplicitStatId::CritChance, StatValueMode::Flat, 0.01f, 0.03f, 2.0f},
        {ImplicitStatId::MoveSpeed, StatValueMode::Mult, 0.02f, 0.05f, 2.0f},
        {ImplicitStatId::ResourceRegen, StatValueMode::Flat, 0.02f, 0.05f, 1.0f},
        {ImplicitStatId::CooldownReduction, StatValueMode::Flat, 0.02f, 0.04f, 1.0f},
    };
}

int ComputeItemLevelFromWave(int wave, int waveDivisor) {
    const int divisor = std::max(1, waveDivisor);
    return 1 + std::max(0, wave) / divisor;
}

float StatScaleFactor(int itemLevel, float perLevel) {
    const int ilvl = std::max(1, itemLevel);
    return 1.0f + perLevel * static_cast<float>(ilvl);
}

namespace {
void zeroContribution(StatContribution& c) {
    auto zeroDerived = [](Engine::Gameplay::RPG::DerivedStats& d) {
        d.attackPower = 0.0f;
        d.spellPower = 0.0f;
        d.attackSpeed = 0.0f;
        d.moveSpeed = 0.0f;
        d.accuracy = 0.0f;
        d.critChance = 0.0f;
        d.critMult = 0.0f;
        d.armorPen = 0.0f;
        d.lifesteal = 0.0f;
        d.lifeOnHit = 0.0f;
        d.cleaveChance = 0.0f;
        d.evasion = 0.0f;
        d.armor = 0.0f;
        d.tenacity = 0.0f;
        d.healthMax = 0.0f;
        d.healthRegen = 0.0f;
        d.shieldMax = 0.0f;
        d.shieldRegen = 0.0f;
        d.cooldownReduction = 0.0f;
        d.resourceRegen = 0.0f;
        d.goldGainMult = 0.0f;
        d.rarityScore = 0.0f;
        d.statusChance = 0.0f;
        for (float& v : d.resists.values) v = 0.0f;
    };
    zeroDerived(c.flat);
    zeroDerived(c.mult);
}

void addContribution(StatContribution& dst, const StatContribution& src, float scalar = 1.0f) {
    auto addFlat = [&](float& a, float b) { a += b * scalar; };
    auto addMult = [&](float& a, float b) { a += b * scalar; };
    auto normDefault = [](float v, float defaultValue) {
        return (std::abs(v - defaultValue) < 0.0001f) ? 0.0f : v;
    };

    addFlat(dst.flat.attackPower, src.flat.attackPower);
    addFlat(dst.flat.spellPower, src.flat.spellPower);
    addFlat(dst.flat.attackSpeed, normDefault(src.flat.attackSpeed, 1.0f));
    addFlat(dst.flat.moveSpeed, normDefault(src.flat.moveSpeed, 1.0f));
    addFlat(dst.flat.accuracy, src.flat.accuracy);
    addFlat(dst.flat.critChance, src.flat.critChance);
    addFlat(dst.flat.critMult, normDefault(src.flat.critMult, 1.5f));
    addFlat(dst.flat.armorPen, src.flat.armorPen);
    addFlat(dst.flat.lifesteal, src.flat.lifesteal);
    addFlat(dst.flat.lifeOnHit, src.flat.lifeOnHit);
    addFlat(dst.flat.cleaveChance, src.flat.cleaveChance);
    addFlat(dst.flat.cooldownReduction, src.flat.cooldownReduction);
    addFlat(dst.flat.resourceRegen, src.flat.resourceRegen);
    addFlat(dst.flat.goldGainMult, normDefault(src.flat.goldGainMult, 1.0f));
    addFlat(dst.flat.rarityScore, src.flat.rarityScore);
    addFlat(dst.flat.armor, src.flat.armor);
    addFlat(dst.flat.evasion, src.flat.evasion);
    addFlat(dst.flat.tenacity, src.flat.tenacity);
    addFlat(dst.flat.healthMax, src.flat.healthMax);
    addFlat(dst.flat.healthRegen, src.flat.healthRegen);
    addFlat(dst.flat.shieldMax, src.flat.shieldMax);
    addFlat(dst.flat.shieldRegen, src.flat.shieldRegen);
    addFlat(dst.flat.statusChance, src.flat.statusChance);
    for (std::size_t i = 0; i < dst.flat.resists.values.size(); ++i) {
        dst.flat.resists.values[i] += src.flat.resists.values[i] * scalar;
    }

    addMult(dst.mult.attackPower, src.mult.attackPower);
    addMult(dst.mult.spellPower, src.mult.spellPower);
    addMult(dst.mult.attackSpeed, normDefault(src.mult.attackSpeed, 1.0f));
    addMult(dst.mult.moveSpeed, normDefault(src.mult.moveSpeed, 1.0f));
    addMult(dst.mult.accuracy, src.mult.accuracy);
    addMult(dst.mult.critChance, src.mult.critChance);
    addMult(dst.mult.armor, src.mult.armor);
    addMult(dst.mult.lifesteal, src.mult.lifesteal);
    addMult(dst.mult.lifeOnHit, src.mult.lifeOnHit);
    addMult(dst.mult.cleaveChance, src.mult.cleaveChance);
    addMult(dst.mult.evasion, src.mult.evasion);
    addMult(dst.mult.tenacity, src.mult.tenacity);
    addMult(dst.mult.healthMax, src.mult.healthMax);
    addMult(dst.mult.healthRegen, src.mult.healthRegen);
    addMult(dst.mult.shieldMax, src.mult.shieldMax);
    addMult(dst.mult.shieldRegen, src.mult.shieldRegen);
    addMult(dst.mult.cooldownReduction, src.mult.cooldownReduction);
    addMult(dst.mult.resourceRegen, src.mult.resourceRegen);
    addMult(dst.mult.goldGainMult, normDefault(src.mult.goldGainMult, 1.0f));
    addMult(dst.mult.rarityScore, src.mult.rarityScore);
    addMult(dst.mult.statusChance, src.mult.statusChance);
    for (std::size_t i = 0; i < dst.mult.resists.values.size(); ++i) {
        dst.mult.resists.values[i] += src.mult.resists.values[i] * scalar;
    }
}

bool isUniqueRarity(Rarity rarity) {
    const int maxRarity = static_cast<int>(Rarity::Unique);
    return static_cast<int>(rarity) == maxRarity;
}

StatContribution makeImplicitContribution(ImplicitStatId stat, StatValueMode mode, float value) {
    StatContribution out{};
    zeroContribution(out);
    if (std::abs(value) < 0.0001f) return out;
    auto& target = (mode == StatValueMode::Mult) ? out.mult : out.flat;
    switch (stat) {
        case ImplicitStatId::AttackPower: target.attackPower = value; break;
        case ImplicitStatId::SpellPower: target.spellPower = value; break;
        case ImplicitStatId::Armor: target.armor = value; break;
        case ImplicitStatId::ArmorPen: target.armorPen = value; break;
        case ImplicitStatId::CritChance: target.critChance = value; break;
        case ImplicitStatId::AttackSpeed: target.attackSpeed = value; break;
        case ImplicitStatId::MoveSpeed: target.moveSpeed = value; break;
        case ImplicitStatId::LifeOnHit: target.lifeOnHit = value; break;
        case ImplicitStatId::Lifesteal: target.lifesteal = value; break;
        case ImplicitStatId::CleaveChance: target.cleaveChance = value; break;
        case ImplicitStatId::CooldownReduction: target.cooldownReduction = value; break;
        case ImplicitStatId::ResourceRegen: target.resourceRegen = value; break;
        case ImplicitStatId::StatusChance: target.statusChance = value; break;
        default: break;
    }
    return out;
}

int getImplicitRollCount(Rarity rarity, std::mt19937& rng) {
    switch (rarity) {
        case Rarity::Common: return 1;
        case Rarity::Uncommon: return 2;
        case Rarity::Rare: return 3;
        case Rarity::Epic: {
            std::uniform_int_distribution<int> pick(3, 4);
            return pick(rng);
        }
        case Rarity::Legendary: {
            std::uniform_int_distribution<int> pick(3, 5);
            return pick(rng);
        }
        case Rarity::Unique: return 0;
    }
    return 0;
}

int getAffixRollCount(Rarity rarity, int maxAffixes, std::mt19937& rng) {
    maxAffixes = std::max(0, maxAffixes);
    if (maxAffixes == 0) return 0;
    switch (rarity) {
        case Rarity::Epic:
        case Rarity::Legendary:
        case Rarity::Unique: {
            std::uniform_int_distribution<int> pick(1, maxAffixes);
            return pick(rng);
        }
        case Rarity::Common: {
            std::uniform_int_distribution<int> pick(0, std::min(1, maxAffixes));
            return pick(rng);
        }
        case Rarity::Uncommon:
            return std::min(1, maxAffixes);
        case Rarity::Rare: {
            std::uniform_int_distribution<int> pick(1, std::min(2, maxAffixes));
            return pick(rng);
        }
    }
    return 0;
}

bool isCombatTypeValid(CombatType t) {
    return static_cast<int>(t) >= 0 && t < CombatType::Count;
}

bool isUniqueItemRarity(ItemRarity rarity) {
    const int maxRarity = static_cast<int>(ItemRarity::Unique);
    return static_cast<int>(rarity) == maxRarity;
}

bool isArmorSlot(EquipmentSlot slot) {
    switch (slot) {
        case EquipmentSlot::Head:
        case EquipmentSlot::Chest:
        case EquipmentSlot::Legs:
        case EquipmentSlot::Boots:
        case EquipmentSlot::OffHand:
        case EquipmentSlot::Cloak:
        case EquipmentSlot::Gloves:
        case EquipmentSlot::Belt:
            return true;
        default:
            return false;
    }
}

bool hasArmorPayload(const StatContribution& sc) {
    return sc.flat.armor != 0.0f || sc.mult.armor != 0.0f;
}

bool hasStatsPayload(const StatContribution& sc) {
    auto hasDerived = [](const Engine::Gameplay::RPG::DerivedStats& d) {
        if (d.attackPower != 0.0f || d.spellPower != 0.0f || d.attackSpeed != 0.0f || d.moveSpeed != 0.0f ||
            d.accuracy != 0.0f || d.critChance != 0.0f || d.critMult != 0.0f || d.armorPen != 0.0f ||
            d.lifesteal != 0.0f || d.lifeOnHit != 0.0f || d.cleaveChance != 0.0f || d.evasion != 0.0f ||
            d.armor != 0.0f || d.tenacity != 0.0f || d.healthMax != 0.0f || d.healthRegen != 0.0f ||
            d.shieldMax != 0.0f || d.shieldRegen != 0.0f || d.cooldownReduction != 0.0f ||
            d.resourceRegen != 0.0f || d.goldGainMult != 0.0f || d.rarityScore != 0.0f ||
            d.statusChance != 0.0f) {
            return true;
        }
        for (float v : d.resists.values) {
            if (v != 0.0f) return true;
        }
        return false;
    };
    return hasDerived(sc.flat) || hasDerived(sc.mult);
}
}  // namespace

StatContribution GeneratedItem::contribution() const {
    StatContribution total{};
    zeroContribution(total);
    const float scale = itemScale > 0.0f ? itemScale : 1.0f;
    if (isUniqueRarity(def.rarity)) {
        addContribution(total, def.baseStats, baseScalar * scale);
    }
    for (const auto& line : implicitStats) {
        addContribution(total, line, 1.0f);
    }
    for (const auto& a : affixes) {
        addContribution(total, a.stats, affixScalar * scale);
    }
    return total;
}

static StatContribution percentAttack(float pct) {
    StatContribution c{};
    c.mult.attackPower = pct;
    return c;
}
static StatContribution flatArmor(float v) {
    StatContribution c{};
    c.flat.armor = v;
    return c;
}
static StatContribution flatResist(Engine::Gameplay::RPG::DamageType t, float v) {
    StatContribution c{};
    c.flat.resists.set(t, v);
    return c;
}

LootTable defaultLootTable() {
    LootTable t{};
    t.affixTierWeights = defaultAffixTierWeights();
    t.affixCountByRarity = defaultAffixCountByRarity();
    t.attributeBonusCountByRarity = defaultAttributeBonusCountByRarity();
    t.attributeBonusValueByRarity = defaultAttributeBonusValueByRarity();
    t.attributeBonusSecondChanceByRarity = defaultAttributeBonusSecondChanceByRarity();
    t.baseStatScalarByRarity = defaultBaseStatScalarByRarity();
    t.affixScalarByRarity = defaultAffixScalarByRarity();
    t.armorImplicitScalarByRarity = defaultArmorImplicitScalarByRarity();
    t.implicitStatPools = defaultImplicitStatPools();
    t.armorImplicitPool = defaultArmorImplicitPool();
    t.fallbackImplicitPool = defaultFallbackImplicitPool();
    // Base items
    t.items.push_back({"iron_helm", "Iron Helm", EquipmentSlot::Head, CombatType::Count, Rarity::Common, "Gear.png", 2, 2, "", 0, false, 0, flatArmor(2.0f), 1, {"armor_small"}});
    t.items.push_back({"leather_boots", "Leather Boots", EquipmentSlot::Boots, CombatType::Count, Rarity::Common, "Gear.png", 5, 0, "", 0, false, 0, percentAttack(0.0f), 1, {"speed_small"}});
    t.items.push_back({"warblade", "Warblade", EquipmentSlot::MainHand, CombatType::Melee, Rarity::Uncommon, "Melee_Weapons.png", 1, 2, "", 0, false, 1, percentAttack(0.08f), 2, {"crit_small", "atk_small"}});
    t.items.push_back({"ember_staff", "Ember Staff", EquipmentSlot::MainHand, CombatType::Magical, Rarity::Rare, "Magic_Weapons.png", 4, 0, "", 0, false, 1, percentAttack(0.0f), 2, {"spell_small", "fire_res"}});
    t.items.push_back({"aegis", "Aegis Barrier", EquipmentSlot::OffHand, CombatType::Count, Rarity::Epic, "Shields.png", 1, 0, "", 0, false, 1, flatArmor(6.0f), 3, {"armor_small", "tenacity"}});

    // Affixes
    t.affixes.push_back({"atk_small", "+6 Attack Power", percentAttack(0.06f)});
    t.affixes.push_back({"crit_small", "+4% Crit", [] {
        StatContribution c{};
        c.flat.critChance = 0.04f;
        return c;
    }()});
    t.affixes.push_back({"speed_small", "+5% Move Speed", [] {
        StatContribution c{};
        c.mult.moveSpeed = 0.05f;
        return c;
    }()});
    t.affixes.push_back({"armor_small", "+2 Armor", flatArmor(2.0f)});
    t.affixes.push_back({"fire_res", "Fire Resist +8%", flatResist(Engine::Gameplay::RPG::DamageType::Fire, 0.08f)});
    t.affixes.push_back({"tenacity", "+5 Tenacity", [] {
        StatContribution c{};
        c.flat.tenacity = 5.0f;
        return c;
    }()});
    t.affixes.push_back({"spell_small", "+10 Spell Power", [] {
        StatContribution c{};
        c.flat.spellPower = 10.0f;
        return c;
    }()});
    return t;
}

static Rarity pickRarity(float luck, std::mt19937& rng) {
    // Base weights scaled by luck (LCK raises higher tiers slightly).
    float common = 60.0f - luck * 5.0f;
    float uncommon = 25.0f + luck * 3.0f;
    float rare = 10.0f + luck * 2.0f;
    float epic = 4.0f + luck * 1.0f;
    float legendary = 1.0f + luck * 0.5f;
    float unique = 0.2f + luck * 0.2f;
    std::vector<float> weights{common, uncommon, rare, epic, legendary, unique};
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return static_cast<Rarity>(dist(rng));
}

GeneratedItem generateLoot(const LootTable& table, const LootContext& ctx, std::mt19937& rng) {
    GeneratedItem out{};
    if (table.items.empty()) return out;
    Rarity rollRarity = pickRarity(ctx.luck, rng);
    // Prefer templates at the rolled rarity; if none exist, gracefully step down.
    std::vector<ItemTemplate> candidates;
    for (int r = static_cast<int>(rollRarity); r >= static_cast<int>(Rarity::Common); --r) {
        candidates.clear();
        for (const auto& it : table.items) {
            if (static_cast<int>(it.rarity) == r) candidates.push_back(it);
        }
        if (!candidates.empty()) break;
    }
    if (candidates.empty()) candidates = table.items;
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    out.def = candidates[pick(rng)];
    out.itemLevel = std::max(1, ctx.itemLevel);
    out.itemScale = StatScaleFactor(out.itemLevel, ctx.itemScalePerLevel);

    auto rarityScalarRange = [&](const std::array<std::array<float, 2>, 6>& tableRanges,
                                 const std::array<std::array<float, 2>, 6>& defaults,
                                 int rarity) -> std::pair<float, float> {
        const std::size_t idx = static_cast<std::size_t>(std::clamp(rarity, 0, 5));
        std::array<float, 2> range = tableRanges[idx];
        if (range[0] <= 0.0f && range[1] <= 0.0f) {
            range = defaults[idx];
        }
        if (range[1] < range[0]) std::swap(range[0], range[1]);
        return {range[0], range[1]};
    };

    const int rarityIdx = std::max(0, static_cast<int>(out.def.rarity));
    if (isUniqueRarity(out.def.rarity)) {
        out.baseScalar = 1.0f;
        out.affixScalar = 1.0f;
    } else if (!out.def.socketable && out.def.slot != EquipmentSlot::Consumable && out.def.slot != EquipmentSlot::Bag) {
        const auto baseRange = rarityScalarRange(table.baseStatScalarByRarity, defaultBaseStatScalarByRarity(), rarityIdx);
        const auto affixRange = rarityScalarRange(table.affixScalarByRarity, defaultAffixScalarByRarity(), rarityIdx);
        std::uniform_real_distribution<float> baseRoll(baseRange.first, baseRange.second);
        std::uniform_real_distribution<float> affixRoll(affixRange.first, affixRange.second);
        out.baseScalar = baseRoll(rng);
        out.affixScalar = affixRoll(rng);
    } else {
        out.baseScalar = 1.0f;
        out.affixScalar = 1.0f;
    }

    auto rollAttributeBonuses = [&](GeneratedItem& item) {
        if (item.def.socketable || item.def.slot == EquipmentSlot::Consumable || item.def.slot == EquipmentSlot::Bag) {
            return;
        }
        auto countRange = (rarityIdx < static_cast<int>(table.attributeBonusCountByRarity.size()))
                              ? table.attributeBonusCountByRarity[static_cast<std::size_t>(rarityIdx)]
                              : defaultAttributeBonusCountByRarity()[static_cast<std::size_t>(std::clamp(rarityIdx, 0, 5))];
        auto valueRange = (rarityIdx < static_cast<int>(table.attributeBonusValueByRarity.size()))
                              ? table.attributeBonusValueByRarity[static_cast<std::size_t>(rarityIdx)]
                              : defaultAttributeBonusValueByRarity()[static_cast<std::size_t>(std::clamp(rarityIdx, 0, 5))];
        if (countRange[0] <= 0 && countRange[1] <= 0) return;
        if (valueRange[0] <= 0 && valueRange[1] <= 0) return;
        int minCount = std::max(0, countRange[0]);
        int maxCount = std::max(0, countRange[1]);
        if (maxCount < minCount) std::swap(maxCount, minCount);
        int count = minCount;
        if (maxCount > minCount) {
            float chance = defaultAttributeBonusSecondChanceByRarity()[static_cast<std::size_t>(std::clamp(rarityIdx, 0, 5))];
            if (rarityIdx < static_cast<int>(table.attributeBonusSecondChanceByRarity.size())) {
                float cfgChance = table.attributeBonusSecondChanceByRarity[static_cast<std::size_t>(rarityIdx)];
                if (cfgChance > 0.0f) chance = cfgChance;
            }
            std::uniform_real_distribution<float> roll01(0.0f, 1.0f);
            if (roll01(rng) < chance) count = minCount + 1;
        }
        count = std::min(count, 5);
        if (count <= 0) return;
        int minValue = std::max(0, valueRange[0]);
        int maxValue = std::max(0, valueRange[1]);
        if (maxValue < minValue) std::swap(maxValue, minValue);
        std::vector<int> attrs{0, 1, 2, 3, 4};
        std::shuffle(attrs.begin(), attrs.end(), rng);
        std::uniform_int_distribution<int> valuePick(minValue, maxValue);
        auto applyAttr = [&](int idx, int value) {
            if (value <= 0) return;
            switch (idx) {
                case 0: item.attributeBonus.STR += value; break;
                case 1: item.attributeBonus.DEX += value; break;
                case 2: item.attributeBonus.INT += value; break;
                case 3: item.attributeBonus.END += value; break;
                case 4: item.attributeBonus.LCK += value; break;
                default: break;
            }
        };
        for (int i = 0; i < count && i < static_cast<int>(attrs.size()); ++i) {
            applyAttr(attrs[static_cast<std::size_t>(i)], valuePick(rng));
        }
    };
    int minAff = 0;
    int maxAff = 0;
    const int baseAffixes = getAffixRollCount(out.def.rarity, out.def.maxAffixes, rng);
    maxAff = std::max(0, baseAffixes);
    minAff = std::min(maxAff, std::max(0, baseAffixes));
    int luckAff = 0;
    if (ctx.luck >= 0.6f) luckAff += 1;
    if (ctx.luck >= 1.2f) luckAff += 1;
    maxAff = std::min(std::max(0, out.def.maxAffixes), maxAff + luckAff);
    minAff = std::min(minAff, maxAff);
    rollAttributeBonuses(out);
    auto mergeImplicit = [&](const StatContribution& candidate) -> bool {
        for (auto& line : out.implicitStats) {
            if (candidate.flat.attackPower != 0.0f && line.flat.attackPower != 0.0f) {
                line.flat.attackPower += candidate.flat.attackPower;
                return true;
            }
            if (candidate.flat.spellPower != 0.0f && line.flat.spellPower != 0.0f) {
                line.flat.spellPower += candidate.flat.spellPower;
                return true;
            }
            if (candidate.flat.armor != 0.0f && line.flat.armor != 0.0f) {
                line.flat.armor += candidate.flat.armor;
                return true;
            }
            if (candidate.flat.armorPen != 0.0f && line.flat.armorPen != 0.0f) {
                line.flat.armorPen += candidate.flat.armorPen;
                return true;
            }
            if (candidate.flat.critChance != 0.0f && line.flat.critChance != 0.0f) {
                line.flat.critChance += candidate.flat.critChance;
                return true;
            }
            if (candidate.flat.lifesteal != 0.0f && line.flat.lifesteal != 0.0f) {
                line.flat.lifesteal += candidate.flat.lifesteal;
                return true;
            }
            if (candidate.flat.lifeOnHit != 0.0f && line.flat.lifeOnHit != 0.0f) {
                line.flat.lifeOnHit += candidate.flat.lifeOnHit;
                return true;
            }
            if (candidate.flat.cleaveChance != 0.0f && line.flat.cleaveChance != 0.0f) {
                line.flat.cleaveChance += candidate.flat.cleaveChance;
                return true;
            }
            if (candidate.flat.cooldownReduction != 0.0f && line.flat.cooldownReduction != 0.0f) {
                line.flat.cooldownReduction += candidate.flat.cooldownReduction;
                return true;
            }
            if (candidate.flat.resourceRegen != 0.0f && line.flat.resourceRegen != 0.0f) {
                line.flat.resourceRegen += candidate.flat.resourceRegen;
                return true;
            }
            if (candidate.flat.statusChance != 0.0f && line.flat.statusChance != 0.0f) {
                line.flat.statusChance += candidate.flat.statusChance;
                return true;
            }
            if (candidate.mult.attackSpeed != 0.0f && line.mult.attackSpeed != 0.0f) {
                line.mult.attackSpeed += candidate.mult.attackSpeed;
                return true;
            }
            if (candidate.mult.moveSpeed != 0.0f && line.mult.moveSpeed != 0.0f) {
                line.mult.moveSpeed += candidate.mult.moveSpeed;
                return true;
            }
        }
        return false;
    };

    auto rollImplicitPool = [&](const std::vector<ImplicitStatPoolEntry>& pool, int rolls, float armorScalar) {
        if (pool.empty() || rolls <= 0) return;
        std::vector<float> weights;
        weights.reserve(pool.size());
        for (const auto& entry : pool) {
            weights.push_back(std::max(0.0f, entry.weight));
        }
        std::discrete_distribution<int> pickStat(weights.begin(), weights.end());
        std::uniform_real_distribution<float> roll01(0.0f, 1.0f);
        for (int i = 0; i < rolls; ++i) {
            const auto& entry = pool[static_cast<std::size_t>(pickStat(rng))];
            float minVal = entry.min;
            float maxVal = entry.max;
            if (maxVal < minVal) std::swap(minVal, maxVal);
            const float value = minVal + (maxVal - minVal) * roll01(rng);
            const float rarityScalar = (entry.stat == ImplicitStatId::Armor) ? armorScalar : 1.0f;
            const float scaled = value * out.itemScale * rarityScalar;
            StatContribution candidate = makeImplicitContribution(entry.stat, entry.mode, scaled);
            if (!mergeImplicit(candidate)) {
                out.implicitStats.push_back(candidate);
            }
        }
    };

    // Roll implicit stat lines for non-unique equipment.
    if (!isUniqueRarity(out.def.rarity) && out.def.slot != EquipmentSlot::Consumable && out.def.slot != EquipmentSlot::Bag) {
        const int rolls = getImplicitRollCount(out.def.rarity, rng);
        const auto armorRange = rarityScalarRange(table.armorImplicitScalarByRarity, defaultArmorImplicitScalarByRarity(), rarityIdx);
        std::uniform_real_distribution<float> armorScalarPick(armorRange.first, armorRange.second);
        const float armorScalar = armorScalarPick(rng);
        if (isCombatTypeValid(out.def.combatType)) {
            rollImplicitPool(table.implicitStatPools[static_cast<std::size_t>(out.def.combatType)], rolls, armorScalar);
        } else if (!out.def.socketable && !table.fallbackImplicitPool.empty()) {
            rollImplicitPool(table.fallbackImplicitPool, rolls, armorScalar);
        }
    }

    std::uniform_int_distribution<int> affixCountPick(minAff, maxAff);
    int affixesToRoll = affixCountPick(rng);
    if (out.def.socketable) affixesToRoll = 0;

    auto tierFromId = [](const std::string& id) -> int {
        if (id.size() >= 2) {
            const std::string suf = id.substr(id.size() - 2);
            if (suf == "_s") return 0;
            if (suf == "_m") return 1;
            if (suf == "_l") return 2;
        }
        return 0;
    };

    auto tierWeightsFor = [&](int idx) -> std::array<float, 3> {
        if (idx < 0 || idx >= static_cast<int>(table.affixTierWeights.size())) {
            return defaultAffixTierWeights()[static_cast<std::size_t>(std::clamp(idx, 0, 5))];
        }
        std::array<float, 3> weights = table.affixTierWeights[static_cast<std::size_t>(idx)];
        float sum = weights[0] + weights[1] + weights[2];
        if (sum <= 0.0f) {
            weights = defaultAffixTierWeights()[static_cast<std::size_t>(std::clamp(idx, 0, 5))];
        }
        return weights;
    };

    std::array<std::vector<std::string>, 3> tieredPools{};
    tieredPools[0].reserve(out.def.affixPool.size());
    tieredPools[1].reserve(out.def.affixPool.size());
    tieredPools[2].reserve(out.def.affixPool.size());
    for (const auto& id : out.def.affixPool) {
        int tier = std::clamp(tierFromId(id), 0, 2);
        tieredPools[static_cast<std::size_t>(tier)].push_back(id);
    }

    std::vector<std::string> chosen;
    chosen.reserve(static_cast<std::size_t>(affixesToRoll));
    for (int i = 0; i < affixesToRoll; ++i) {
        if (out.def.affixPool.empty()) break;
        std::array<float, 3> weights{0.0f, 0.0f, 0.0f};
        const auto rarityWeights = tierWeightsFor(rarityIdx);
        for (int t = 0; t < 3; ++t) {
            if (!tieredPools[static_cast<std::size_t>(t)].empty()) {
                weights[static_cast<std::size_t>(t)] = rarityWeights[static_cast<std::size_t>(t)];
            }
        }
        if (weights[0] + weights[1] + weights[2] <= 0.0f) break;

        std::string id{};
        // Avoid duplicate affixes; give a few retries then allow duplicates as a fallback.
        for (int attempts = 0; attempts < 6; ++attempts) {
            std::discrete_distribution<int> tierPick(weights.begin(), weights.end());
            int tier = tierPick(rng);
            const auto& pool = tieredPools[static_cast<std::size_t>(std::clamp(tier, 0, 2))];
            if (pool.empty()) continue;
            std::uniform_int_distribution<std::size_t> affPick(0, pool.size() - 1);
            id = pool[affPick(rng)];
            if (std::find(chosen.begin(), chosen.end(), id) == chosen.end()) break;
        }
        if (id.empty()) continue;
        chosen.push_back(id);
        auto it = std::find_if(table.affixes.begin(), table.affixes.end(),
                               [&](const Affix& a) { return a.id == id; });
        if (it != table.affixes.end()) {
            out.affixes.push_back(*it);
        }
    }
    const bool shouldEnsureStats = (out.def.slot != EquipmentSlot::Consumable && out.def.slot != EquipmentSlot::Bag);
    if (shouldEnsureStats && isArmorSlot(out.def.slot)) {
        bool hasArmor = false;
        if (isUniqueRarity(out.def.rarity) && hasArmorPayload(out.def.baseStats)) hasArmor = true;
        for (const auto& line : out.implicitStats) {
            if (hasArmorPayload(line)) {
                hasArmor = true;
                break;
            }
        }
        if (!hasArmor) {
            for (const auto& a : out.affixes) {
                if (hasArmorPayload(a.stats)) {
                    hasArmor = true;
                    break;
                }
            }
        }
        if (!hasArmor) {
            const auto& pool = !table.armorImplicitPool.empty() ? table.armorImplicitPool : defaultArmorImplicitPool();
            const auto armorRange = rarityScalarRange(table.armorImplicitScalarByRarity, defaultArmorImplicitScalarByRarity(), rarityIdx);
            std::uniform_real_distribution<float> armorScalarPick(armorRange.first, armorRange.second);
            rollImplicitPool(pool, 1, armorScalarPick(rng));
        }
    }
    if (shouldEnsureStats) {
        bool hasAnyStats = false;
        if (isUniqueRarity(out.def.rarity) && hasStatsPayload(out.def.baseStats)) hasAnyStats = true;
        if (!hasAnyStats && !out.implicitStats.empty()) hasAnyStats = true;
        if (!hasAnyStats && !out.affixes.empty()) hasAnyStats = true;
        if (!hasAnyStats) {
            if (out.attributeBonus.STR > 0 || out.attributeBonus.DEX > 0 || out.attributeBonus.INT > 0 ||
                out.attributeBonus.END > 0 || out.attributeBonus.LCK > 0) {
                hasAnyStats = true;
            }
        }
        if (!hasAnyStats) {
            if (out.def.socketable && hasStatsPayload(out.def.baseStats)) {
                StatContribution scaled{};
                zeroContribution(scaled);
                addContribution(scaled, out.def.baseStats, out.itemScale);
                out.implicitStats.push_back(scaled);
            } else if (!table.fallbackImplicitPool.empty()) {
                const auto armorRange = rarityScalarRange(table.armorImplicitScalarByRarity, defaultArmorImplicitScalarByRarity(), rarityIdx);
                std::uniform_real_distribution<float> armorScalarPick(armorRange.first, armorRange.second);
                rollImplicitPool(table.fallbackImplicitPool, 1, armorScalarPick(rng));
            } else {
                const auto armorRange = rarityScalarRange(table.armorImplicitScalarByRarity, defaultArmorImplicitScalarByRarity(), rarityIdx);
                std::uniform_real_distribution<float> armorScalarPick(armorRange.first, armorRange.second);
                rollImplicitPool(defaultFallbackImplicitPool(), 1, armorScalarPick(rng));
            }
        }
    }
    return out;
}

StatContribution computeItemContribution(const LootTable& table, const ItemDefinition& def, int quantity) {
    StatContribution total{};
    zeroContribution(total);
    if (quantity <= 0) return total;
    auto findAffix = [&](const std::string& id) -> const Affix* {
        for (const auto& a : table.affixes) if (a.id == id) return &a;
        return nullptr;
    };
    auto findTemplate = [&](const std::string& id) -> const ItemTemplate* {
        for (const auto& t : table.items) if (t.id == id) return &t;
        return nullptr;
    };

    const float itemScale = def.rpgItemScale > 0.0f ? def.rpgItemScale : 1.0f;
    if (const auto* t = findTemplate(def.rpgTemplateId)) {
        if (!isUniqueItemRarity(def.rarity) && hasStatsPayload(t->baseStats)) {
#ifndef NDEBUG
            static std::unordered_set<std::string> warned;
            if (warned.insert(def.rpgTemplateId).second) {
                Engine::logWarn("Non-unique item ignores static stats: " + def.rpgTemplateId);
            }
#endif
        }
        if (isUniqueItemRarity(def.rarity)) {
            addContribution(total, t->baseStats, def.rpgBaseScalar * itemScale * static_cast<float>(quantity));
        }
    }
    for (const auto& line : def.rpgImplicitStats) {
        addContribution(total, line, static_cast<float>(quantity));
    }
    for (const auto& affId : def.rpgAffixIds) {
        if (const auto* a = findAffix(affId)) {
            addContribution(total, a->stats, def.rpgAffixScalar * itemScale * static_cast<float>(quantity));
        }
    }
    for (const auto& g : def.rpgSocketed) {
        if (const auto* gt = findTemplate(g.templateId)) {
            const float gemScale = g.itemScale > 0.0f ? g.itemScale : 1.0f;
            if (isUniqueItemRarity(g.rarity)) {
                addContribution(total, gt->baseStats, g.baseScalar * gemScale * static_cast<float>(quantity));
            }
            for (const auto& line : g.implicitStats) {
                addContribution(total, line, static_cast<float>(quantity));
            }
        }
        for (const auto& ga : g.affixIds) {
            if (const auto* a = findAffix(ga)) {
                const float gemScale = g.itemScale > 0.0f ? g.itemScale : 1.0f;
                addContribution(total, a->stats, g.affixScalar * gemScale * static_cast<float>(quantity));
            }
        }
    }
    return total;
}

StatContribution computeEquipmentContribution(const LootTable& table, const std::vector<ItemDefinition>& defs) {
    StatContribution total{};
    zeroContribution(total);
    for (const auto& def : defs) {
        addContribution(total, computeItemContribution(table, def, 1), 1.0f);
    }
    return total;
}

std::unordered_map<std::string, ArchetypeBio> loadArchetypeBios(const std::string& path) {
    std::unordered_map<std::string, ArchetypeBio> out;
    if (!std::filesystem::exists(path)) {
        return out;
    }
    json j;
    std::ifstream f(path);
    if (!f.is_open()) return out;
    f >> j;
    if (!j.contains("archetypes")) return out;
    for (const auto& a : j["archetypes"]) {
        ArchetypeBio bio{};
        bio.id = a.value("id", "");
        bio.name = a.value("name", bio.id);
        bio.biography = a.value("bio", "");
        if (a.contains("attributes")) {
            const auto& at = a["attributes"];
            bio.attributes.STR = at.value("STR", 0);
            bio.attributes.DEX = at.value("DEX", 0);
            bio.attributes.INT = at.value("INT", 0);
            bio.attributes.END = at.value("END", 0);
            bio.attributes.LCK = at.value("LCK", 0);
        }
        if (a.contains("specialties") && a["specialties"].is_array()) {
            for (const auto& s : a["specialties"]) bio.specialties.push_back(s.get<std::string>());
        }
        if (a.contains("perks") && a["perks"].is_array()) {
            for (const auto& p : a["perks"]) bio.perks.push_back(p.get<std::string>());
        }
        if (!bio.id.empty()) out[bio.id] = bio;
    }
    return out;
}

std::vector<ConsumableDef> defaultConsumables() {
    std::vector<ConsumableDef> defs;
    defs.push_back({"healing_potion", "Healing Draught", "", "potion", 12.0f,
                    {{ConsumableCategory::Heal, ConsumableResource::Health, 0.35f, 0.0f, Engine::Gameplay::RPG::DamageType::Physical, {}, {}, "", {}}}});
    defs.push_back({"cleanse_tonic", "Cleanse Tonic", "", "potion", 18.0f,
                    {{ConsumableCategory::Cleanse, ConsumableResource::Health, 0.0f, 0.0f, Engine::Gameplay::RPG::DamageType::Physical,
                      {}, {1, 2, 3}, "", {}}}});
    defs.push_back({"haste_food", "Trail Rations", "", "food", 30.0f,
                    {{ConsumableCategory::Buff, ConsumableResource::Health, 0.08f, 20.0f, Engine::Gameplay::RPG::DamageType::Physical, {}, {}, "", {}}}});
    defs.push_back({"shock_bomb", "Shock Bomb", "", "bomb", 16.0f,
                    {{ConsumableCategory::Bomb, ConsumableResource::Health, 40.0f, 0.0f, Engine::Gameplay::RPG::DamageType::Shock, {}, {}, "", {}}}});
    return defs;
}

}  // namespace Game::RPG
