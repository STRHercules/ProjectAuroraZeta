// Helper loaders for RPG JSON data that remain game-layer specific.
#include "RPGContent.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Game::RPG {

using nlohmann::json;

static std::optional<Engine::Gameplay::RPG::DamageType> parseDamageTypeKey(const std::string& k) {
    using Engine::Gameplay::RPG::DamageType;
    if (k == "Physical") return DamageType::Physical;
    if (k == "Fire") return DamageType::Fire;
    if (k == "Frost") return DamageType::Frost;
    if (k == "Shock") return DamageType::Shock;
    if (k == "Poison") return DamageType::Poison;
    if (k == "Arcane") return DamageType::Arcane;
    if (k == "True") return DamageType::True;
    return std::nullopt;
}

static StatContribution parseContribution(const json& j) {
    StatContribution c{};
    if (j.contains("flat")) {
        const auto& f = j["flat"];
        c.flat.attackPower = f.value("attackPower", c.flat.attackPower);
        c.flat.spellPower = f.value("spellPower", c.flat.spellPower);
        c.flat.attackSpeed = f.value("attackSpeed", c.flat.attackSpeed);
        c.flat.moveSpeed = f.value("moveSpeed", c.flat.moveSpeed);
        c.flat.accuracy = f.value("accuracy", c.flat.accuracy);
        c.flat.critChance = f.value("critChance", c.flat.critChance);
        c.flat.critMult = f.value("critMult", c.flat.critMult);
        c.flat.armorPen = f.value("armorPen", c.flat.armorPen);
        c.flat.armor = f.value("armor", c.flat.armor);
        c.flat.evasion = f.value("evasion", c.flat.evasion);
        c.flat.tenacity = f.value("tenacity", c.flat.tenacity);
        c.flat.healthMax = f.value("healthMax", c.flat.healthMax);
        c.flat.healthRegen = f.value("healthRegen", c.flat.healthRegen);
        c.flat.shieldMax = f.value("shieldMax", c.flat.shieldMax);
        c.flat.shieldRegen = f.value("shieldRegen", c.flat.shieldRegen);
        c.flat.cooldownReduction = f.value("cooldownReduction", c.flat.cooldownReduction);
        c.flat.resourceRegen = f.value("resourceRegen", c.flat.resourceRegen);
        c.flat.goldGainMult = f.value("goldGainMult", c.flat.goldGainMult);
        c.flat.rarityScore = f.value("rarityScore", c.flat.rarityScore);
        if (f.contains("resists")) {
            const auto& r = f["resists"];
            if (r.is_object()) {
                for (const auto& kv : r.items()) {
                    auto dt = parseDamageTypeKey(kv.key());
                    if (!dt.has_value()) continue;
                    c.flat.resists.set(*dt, kv.value().get<float>());
                }
            } else if (r.is_array()) {
                for (std::size_t i = 0; i < c.flat.resists.values.size() && i < r.size(); ++i) {
                    c.flat.resists.values[i] = r[i].get<float>();
                }
            }
        }
    }
    if (j.contains("mult")) {
        const auto& m = j["mult"];
        c.mult.attackPower = m.value("attackPower", c.mult.attackPower);
        c.mult.spellPower = m.value("spellPower", c.mult.spellPower);
        c.mult.attackSpeed = m.value("attackSpeed", c.mult.attackSpeed);
        c.mult.moveSpeed = m.value("moveSpeed", c.mult.moveSpeed);
        c.mult.accuracy = m.value("accuracy", c.mult.accuracy);
        c.mult.critChance = m.value("critChance", c.mult.critChance);
        c.mult.critMult = m.value("critMult", c.mult.critMult);
        c.mult.armorPen = m.value("armorPen", c.mult.armorPen);
        c.mult.armor = m.value("armor", c.mult.armor);
        c.mult.evasion = m.value("evasion", c.mult.evasion);
        c.mult.tenacity = m.value("tenacity", c.mult.tenacity);
        c.mult.healthMax = m.value("healthMax", c.mult.healthMax);
        c.mult.healthRegen = m.value("healthRegen", c.mult.healthRegen);
        c.mult.shieldMax = m.value("shieldMax", c.mult.shieldMax);
        c.mult.shieldRegen = m.value("shieldRegen", c.mult.shieldRegen);
        c.mult.cooldownReduction = m.value("cooldownReduction", c.mult.cooldownReduction);
        c.mult.resourceRegen = m.value("resourceRegen", c.mult.resourceRegen);
        c.mult.goldGainMult = m.value("goldGainMult", c.mult.goldGainMult);
        c.mult.rarityScore = m.value("rarityScore", c.mult.rarityScore);
        if (m.contains("resists")) {
            const auto& r = m["resists"];
            if (r.is_object()) {
                for (const auto& kv : r.items()) {
                    auto dt = parseDamageTypeKey(kv.key());
                    if (!dt.has_value()) continue;
                    c.mult.resists.set(*dt, kv.value().get<float>());
                }
            } else if (r.is_array()) {
                for (std::size_t i = 0; i < c.mult.resists.values.size() && i < r.size(); ++i) {
                    c.mult.resists.values[i] = r[i].get<float>();
                }
            }
        }
    }
    return c;
}

LootTable loadLootTable(const std::string& path) {
    LootTable out{};
    if (!std::filesystem::exists(path)) return out;
    std::ifstream f(path);
    if (!f.is_open()) return out;
    json j;
    f >> j;
    if (j.contains("affixes")) {
        for (const auto& a : j["affixes"]) {
            Affix af{};
            af.id = a.value("id", "");
            af.name = a.value("name", "");
            af.stats = parseContribution(a.value("stats", json::object()));
            if (!af.id.empty()) out.affixes.push_back(af);
        }
    }
    if (j.contains("items")) {
        for (const auto& it : j["items"]) {
            ItemTemplate t{};
            t.id = it.value("id", "");
            t.name = it.value("name", "");
            t.slot = static_cast<EquipmentSlot>(it.value("slot", 0));
            t.rarity = static_cast<Rarity>(it.value("rarity", 0));
            t.iconSheet = it.value("iconSheet", "");
            t.iconRow = it.value("iconRow", 0);
            t.iconCol = it.value("iconCol", 0);
            t.consumableId = it.value("consumableId", "");
            t.bagSlots = it.value("bagSlots", 0);
            t.socketable = it.value("socketable", false);
            t.socketsMax = it.value("socketsMax", 0);
            t.baseStats = parseContribution(it.value("stats", json::object()));
            t.maxAffixes = it.value("maxAffixes", 1);
            if (it.contains("affixPool") && it["affixPool"].is_array()) {
                for (const auto& id : it["affixPool"]) t.affixPool.push_back(id.get<std::string>());
            }
            if (!t.id.empty()) out.items.push_back(t);
        }
    }
    return out;
}

std::vector<ConsumableDef> loadConsumables(const std::string& path) {
    std::vector<ConsumableDef> defs;
    if (!std::filesystem::exists(path)) return defs;
    std::ifstream f(path);
    if (!f.is_open()) return defs;
    json j;
    f >> j;
    if (!j.contains("consumables")) return defs;
    for (const auto& c : j["consumables"]) {
        ConsumableDef d{};
        d.id = c.value("id", "");
        d.name = c.value("name", "");
        d.description = c.value("description", "");
        d.cooldownGroup = c.value("cooldownGroup", "potion");
        d.cooldown = c.value("cooldown", 10.0f);
        if (c.contains("effects")) {
            for (const auto& e : c["effects"]) {
                ConsumableEffect eff{};
                eff.category = static_cast<ConsumableCategory>(e.value("category", 0));
                eff.resource = static_cast<ConsumableResource>(e.value("resource", 0));
                eff.magnitude = e.value("magnitude", 0.0f);
                eff.duration = e.value("duration", 0.0f);
                eff.damageType = static_cast<Engine::Gameplay::RPG::DamageType>(e.value("damageType", 0));
                if (e.contains("stats") && e["stats"].is_object()) {
                    eff.stats = parseContribution(e["stats"]);
                }
                if (e.contains("cleanse") && e["cleanse"].is_array()) {
                    for (const auto& id : e["cleanse"]) eff.cleanseIds.push_back(id.get<int>());
                }
                eff.scriptId = e.value("scriptId", "");
                if (e.contains("params") && e["params"].is_array()) {
                    for (const auto& v : e["params"]) {
                        eff.params.push_back(v.get<float>());
                    }
                }
                d.effects.push_back(eff);
            }
        }
        if (!d.id.empty()) defs.push_back(d);
    }
    return defs;
}

std::unordered_map<std::string, std::vector<TalentNode>> loadTalentTrees(const std::string& path) {
    std::unordered_map<std::string, std::vector<TalentNode>> out;
    if (!std::filesystem::exists(path)) return out;
    std::ifstream f(path);
    if (!f.is_open()) return out;
    json j;
    f >> j;
    if (!j.contains("talents")) return out;
    for (const auto& tree : j["talents"].items()) {
        const auto& arr = tree.value();
        std::vector<TalentNode> nodes;
        if (!arr.is_array()) continue;
        for (const auto& n : arr) {
            TalentNode node{};
            node.id = n.value("id", "");
            node.name = n.value("name", "");
            node.description = n.value("description", "");
            node.maxRank = n.value("maxRank", 3);
            node.bonus = parseContribution(n.value("bonus", json::object()));
            if (!node.id.empty()) nodes.push_back(node);
        }
        if (!nodes.empty()) out[tree.key()] = nodes;
    }
    return out;
}

}  // namespace Game::RPG
