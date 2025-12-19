// Minimal data-driven content helpers for RPG systems.
#include "RPGContent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <nlohmann/json.hpp>

namespace Game::RPG {

using nlohmann::json;
using Engine::Gameplay::RPG::Resistances;

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
    for (std::size_t i = 0; i < dst.mult.resists.values.size(); ++i) {
        dst.mult.resists.values[i] += src.mult.resists.values[i] * scalar;
    }
}
}  // namespace

StatContribution GeneratedItem::contribution() const {
    StatContribution total = def.baseStats;
    for (const auto& a : affixes) {
        // Flat
        total.flat.attackPower += a.stats.flat.attackPower;
        total.flat.spellPower += a.stats.flat.spellPower;
        total.flat.attackSpeed += a.stats.flat.attackSpeed;
        total.flat.moveSpeed += a.stats.flat.moveSpeed;
        total.flat.accuracy += a.stats.flat.accuracy;
        total.flat.critChance += a.stats.flat.critChance;
        total.flat.critMult += a.stats.flat.critMult;
        total.flat.armorPen += a.stats.flat.armorPen;
        total.flat.cooldownReduction += a.stats.flat.cooldownReduction;
        total.flat.resourceRegen += a.stats.flat.resourceRegen;
        total.flat.goldGainMult += a.stats.flat.goldGainMult;
        total.flat.rarityScore += a.stats.flat.rarityScore;
        total.flat.armor += a.stats.flat.armor;
        total.flat.evasion += a.stats.flat.evasion;
        total.flat.tenacity += a.stats.flat.tenacity;
        total.flat.healthMax += a.stats.flat.healthMax;
        total.flat.healthRegen += a.stats.flat.healthRegen;
        total.flat.shieldMax += a.stats.flat.shieldMax;
        total.flat.shieldRegen += a.stats.flat.shieldRegen;
        for (std::size_t i = 0; i < total.flat.resists.values.size(); ++i) {
            total.flat.resists.values[i] += a.stats.flat.resists.values[i];
        }
        // Multipliers
        total.mult.attackPower += a.stats.mult.attackPower;
        total.mult.spellPower += a.stats.mult.spellPower;
        total.mult.attackSpeed += a.stats.mult.attackSpeed;
        total.mult.moveSpeed += a.stats.mult.moveSpeed;
        total.mult.accuracy += a.stats.mult.accuracy;
        total.mult.critChance += a.stats.mult.critChance;
        total.mult.armor += a.stats.mult.armor;
        total.mult.evasion += a.stats.mult.evasion;
        total.mult.tenacity += a.stats.mult.tenacity;
        total.mult.healthMax += a.stats.mult.healthMax;
        total.mult.healthRegen += a.stats.mult.healthRegen;
        total.mult.shieldMax += a.stats.mult.shieldMax;
        total.mult.shieldRegen += a.stats.mult.shieldRegen;
        total.mult.cooldownReduction += a.stats.mult.cooldownReduction;
        total.mult.resourceRegen += a.stats.mult.resourceRegen;
        total.mult.goldGainMult += a.stats.mult.goldGainMult;
        total.mult.rarityScore += a.stats.mult.rarityScore;
        for (std::size_t i = 0; i < total.mult.resists.values.size(); ++i) {
            total.mult.resists.values[i] += a.stats.mult.resists.values[i];
        }
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
    // Base items
    t.items.push_back({"iron_helm", "Iron Helm", EquipmentSlot::Head, Rarity::Common, "Gear.png", 2, 2, "", 0, false, 0, flatArmor(2.0f), 1, {"armor_small"}});
    t.items.push_back({"leather_boots", "Leather Boots", EquipmentSlot::Boots, Rarity::Common, "Gear.png", 5, 0, "", 0, false, 0, percentAttack(0.0f), 1, {"speed_small"}});
    t.items.push_back({"warblade", "Warblade", EquipmentSlot::MainHand, Rarity::Uncommon, "Melee_Weapons.png", 1, 2, "", 0, false, 1, percentAttack(0.08f), 2, {"crit_small", "atk_small"}});
    t.items.push_back({"ember_staff", "Ember Staff", EquipmentSlot::MainHand, Rarity::Rare, "Magic_Weapons.png", 4, 0, "", 0, false, 1, percentAttack(0.0f), 2, {"spell_small", "fire_res"}});
    t.items.push_back({"aegis", "Aegis Barrier", EquipmentSlot::OffHand, Rarity::Epic, "Shields.png", 1, 0, "", 0, false, 1, flatArmor(6.0f), 3, {"armor_small", "tenacity"}});

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

    int maxAff = out.def.maxAffixes;
    int rarityIdx = std::max(0, static_cast<int>(out.def.rarity));
    int baseAff = 1 + rarityIdx;  // Common=1, Uncommon=2, Rare=3...
    int luckAff = 0;
    if (ctx.luck >= 0.6f) luckAff += 1;
    if (ctx.luck >= 1.2f) luckAff += 1;
    std::uniform_int_distribution<int> jitter(0, 1);
    int affixesToRoll = std::clamp(baseAff + luckAff + jitter(rng), 1, std::max(1, maxAff));

    if (out.def.socketable) return out;

    auto tierFromId = [](const std::string& id) -> int {
        if (id.size() >= 2) {
            const std::string suf = id.substr(id.size() - 2);
            if (suf == "_s") return 0;
            if (suf == "_m") return 1;
            if (suf == "_l") return 2;
        }
        return 0;
    };

    static const float kTierWeights[6][3] = {
        {1.0f, 0.0f, 0.0f},   // Common
        {0.4f, 0.6f, 0.0f},   // Uncommon
        {0.15f, 0.25f, 0.6f}, // Rare
        {0.05f, 0.15f, 0.8f}, // Epic
        {0.0f, 0.1f, 0.9f},   // Legendary
        {0.0f, 0.05f, 0.95f}, // Unique
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
        for (int t = 0; t < 3; ++t) {
            if (!tieredPools[static_cast<std::size_t>(t)].empty()) {
                weights[static_cast<std::size_t>(t)] = kTierWeights[std::clamp(rarityIdx, 0, 5)][t];
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

    if (const auto* t = findTemplate(def.rpgTemplateId)) {
        addContribution(total, t->baseStats, static_cast<float>(quantity));
    }
    for (const auto& affId : def.rpgAffixIds) {
        if (const auto* a = findAffix(affId)) {
            addContribution(total, a->stats, static_cast<float>(quantity));
        }
    }
    for (const auto& g : def.rpgSocketed) {
        if (const auto* gt = findTemplate(g.templateId)) {
            addContribution(total, gt->baseStats, static_cast<float>(quantity));
        }
        for (const auto& ga : g.affixIds) {
            if (const auto* a = findAffix(ga)) {
                addContribution(total, a->stats, static_cast<float>(quantity));
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
