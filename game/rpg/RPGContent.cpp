// Minimal data-driven content helpers for RPG systems.
#include "RPGContent.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <nlohmann/json.hpp>

namespace Game::RPG {

using nlohmann::json;
using Engine::Gameplay::RPG::Resistances;

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
    std::vector<float> weights{common, uncommon, rare, epic, legendary};
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

    std::vector<std::string> chosen;
    chosen.reserve(static_cast<std::size_t>(affixesToRoll));
    for (int i = 0; i < affixesToRoll; ++i) {
        if (out.def.affixPool.empty()) break;
        std::uniform_int_distribution<std::size_t> affPick(0, out.def.affixPool.size() - 1);
        std::string id{};
        // Avoid duplicate affixes; give a few retries then allow duplicates as a fallback.
        for (int attempts = 0; attempts < 6; ++attempts) {
            id = out.def.affixPool[affPick(rng)];
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
    defs.push_back({"healing_potion", "Healing Draught", "potion", 12.0f,
                    {{ConsumableCategory::Heal, ConsumableResource::Health, 0.35f, 0.0f, Engine::Gameplay::RPG::DamageType::Physical, {}}}});
    defs.push_back({"cleanse_tonic", "Cleanse Tonic", "potion", 18.0f,
                    {{ConsumableCategory::Cleanse, ConsumableResource::Health, 0.0f, 0.0f, Engine::Gameplay::RPG::DamageType::Physical,
                      {1, 2, 3}}}});
    defs.push_back({"haste_food", "Trail Rations", "food", 30.0f,
                    {{ConsumableCategory::Buff, ConsumableResource::Health, 0.08f, 20.0f, Engine::Gameplay::RPG::DamageType::Physical, {}}}});
    defs.push_back({"shock_bomb", "Shock Bomb", "bomb", 16.0f,
                    {{ConsumableCategory::Bomb, ConsumableResource::Health, 40.0f, 0.0f, Engine::Gameplay::RPG::DamageType::Shock, {}}}});
    return defs;
}

}  // namespace Game::RPG
