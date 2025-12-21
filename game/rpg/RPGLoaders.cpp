// Helper loaders for RPG JSON data that remain game-layer specific.
#include "RPGContent.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "../../engine/core/Logger.h"

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

static std::optional<CombatType> parseCombatTypeKey(const std::string& k) {
    if (k == "Melee") return CombatType::Melee;
    if (k == "Ranged") return CombatType::Ranged;
    if (k == "Magical") return CombatType::Magical;
    if (k == "Ammo") return CombatType::Ammo;
    return std::nullopt;
}

static std::optional<ImplicitStatId> parseImplicitStatKey(const std::string& k) {
    if (k == "attackPower") return ImplicitStatId::AttackPower;
    if (k == "spellPower") return ImplicitStatId::SpellPower;
    if (k == "armor") return ImplicitStatId::Armor;
    if (k == "armorPen") return ImplicitStatId::ArmorPen;
    if (k == "critChance") return ImplicitStatId::CritChance;
    if (k == "attackSpeed") return ImplicitStatId::AttackSpeed;
    if (k == "moveSpeed") return ImplicitStatId::MoveSpeed;
    if (k == "lifeOnHit") return ImplicitStatId::LifeOnHit;
    if (k == "lifesteal") return ImplicitStatId::Lifesteal;
    if (k == "cleaveChance") return ImplicitStatId::CleaveChance;
    if (k == "cooldownReduction") return ImplicitStatId::CooldownReduction;
    if (k == "resourceRegen") return ImplicitStatId::ResourceRegen;
    if (k == "statusChance") return ImplicitStatId::StatusChance;
    return std::nullopt;
}

static StatValueMode parseStatValueMode(const std::string& k) {
    if (k == "mult") return StatValueMode::Mult;
    return StatValueMode::Flat;
}

static StatContribution parseContribution(const json& j) {
    StatContribution c{};
    // StatContribution reuses DerivedStats, which has non-zero defaults (e.g., attackSpeed=1.0, critMult=1.5).
    // For contributions, we want "not present" to mean 0 contribution, not a baseline value.
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
        d.shieldMax = 0.0f;
        d.shieldRegen = 0.0f;
        d.healthMax = 0.0f;
        d.healthRegen = 0.0f;
        d.cooldownReduction = 0.0f;
        d.resourceRegen = 0.0f;
        d.goldGainMult = 0.0f;
        d.rarityScore = 0.0f;
        d.statusChance = 0.0f;
        for (float& v : d.resists.values) v = 0.0f;
    };
    zeroDerived(c.flat);
    zeroDerived(c.mult);
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
        c.flat.lifesteal = f.value("lifesteal", c.flat.lifesteal);
        c.flat.lifeOnHit = f.value("lifeOnHit", c.flat.lifeOnHit);
        c.flat.cleaveChance = f.value("cleaveChance", c.flat.cleaveChance);
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
        c.flat.statusChance = f.value("statusChance", c.flat.statusChance);
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
        c.mult.lifesteal = m.value("lifesteal", c.mult.lifesteal);
        c.mult.lifeOnHit = m.value("lifeOnHit", c.mult.lifeOnHit);
        c.mult.cleaveChance = m.value("cleaveChance", c.mult.cleaveChance);
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
        c.mult.statusChance = m.value("statusChance", c.mult.statusChance);
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
    out.affixTierWeights = defaultAffixTierWeights();
    out.affixCountByRarity = defaultAffixCountByRarity();
    out.attributeBonusCountByRarity = defaultAttributeBonusCountByRarity();
    out.attributeBonusValueByRarity = defaultAttributeBonusValueByRarity();
    out.attributeBonusSecondChanceByRarity = defaultAttributeBonusSecondChanceByRarity();
    out.baseStatScalarByRarity = defaultBaseStatScalarByRarity();
    out.affixScalarByRarity = defaultAffixScalarByRarity();
    out.armorImplicitScalarByRarity = defaultArmorImplicitScalarByRarity();
    out.implicitStatPools = defaultImplicitStatPools();
    out.armorImplicitPool = defaultArmorImplicitPool();
    out.fallbackImplicitPool = defaultFallbackImplicitPool();
    if (!std::filesystem::exists(path)) return out;
    std::ifstream f(path);
    if (!f.is_open()) return out;
    json j;
    f >> j;
    if (j.contains("affixTierWeights") && j["affixTierWeights"].is_array()) {
        const auto& arr = j["affixTierWeights"];
        const std::size_t maxR = std::min(arr.size(), out.affixTierWeights.size());
        for (std::size_t i = 0; i < maxR; ++i) {
            if (!arr[i].is_array()) continue;
            const std::size_t maxT = std::min<std::size_t>(3, arr[i].size());
            for (std::size_t t = 0; t < maxT; ++t) {
                out.affixTierWeights[i][t] = arr[i][t].get<float>();
            }
        }
    }
    if (j.contains("affixCountByRarity") && j["affixCountByRarity"].is_array()) {
        const auto& arr = j["affixCountByRarity"];
        const std::size_t maxR = std::min(arr.size(), out.affixCountByRarity.size());
        for (std::size_t i = 0; i < maxR; ++i) {
            const auto& entry = arr[i];
            if (entry.is_array() && entry.size() >= 2) {
                out.affixCountByRarity[i][0] = entry[0].get<int>();
                out.affixCountByRarity[i][1] = entry[1].get<int>();
            } else if (entry.is_object()) {
                out.affixCountByRarity[i][0] = entry.value("min", out.affixCountByRarity[i][0]);
                out.affixCountByRarity[i][1] = entry.value("max", out.affixCountByRarity[i][1]);
            }
        }
    }
    if (j.contains("attributeBonusCountByRarity") && j["attributeBonusCountByRarity"].is_array()) {
        const auto& arr = j["attributeBonusCountByRarity"];
        const std::size_t maxR = std::min(arr.size(), out.attributeBonusCountByRarity.size());
        for (std::size_t i = 0; i < maxR; ++i) {
            const auto& entry = arr[i];
            if (entry.is_array() && entry.size() >= 2) {
                out.attributeBonusCountByRarity[i][0] = entry[0].get<int>();
                out.attributeBonusCountByRarity[i][1] = entry[1].get<int>();
            } else if (entry.is_object()) {
                out.attributeBonusCountByRarity[i][0] = entry.value("min", out.attributeBonusCountByRarity[i][0]);
                out.attributeBonusCountByRarity[i][1] = entry.value("max", out.attributeBonusCountByRarity[i][1]);
            }
        }
    }
    if (j.contains("attributeBonusValueByRarity") && j["attributeBonusValueByRarity"].is_array()) {
        const auto& arr = j["attributeBonusValueByRarity"];
        const std::size_t maxR = std::min(arr.size(), out.attributeBonusValueByRarity.size());
        for (std::size_t i = 0; i < maxR; ++i) {
            const auto& entry = arr[i];
            if (entry.is_array() && entry.size() >= 2) {
                out.attributeBonusValueByRarity[i][0] = entry[0].get<int>();
                out.attributeBonusValueByRarity[i][1] = entry[1].get<int>();
            } else if (entry.is_object()) {
                out.attributeBonusValueByRarity[i][0] = entry.value("min", out.attributeBonusValueByRarity[i][0]);
                out.attributeBonusValueByRarity[i][1] = entry.value("max", out.attributeBonusValueByRarity[i][1]);
            }
        }
    }
    if (j.contains("attributeBonusSecondChanceByRarity") && j["attributeBonusSecondChanceByRarity"].is_array()) {
        const auto& arr = j["attributeBonusSecondChanceByRarity"];
        const std::size_t maxR = std::min(arr.size(), out.attributeBonusSecondChanceByRarity.size());
        for (std::size_t i = 0; i < maxR; ++i) {
            out.attributeBonusSecondChanceByRarity[i] = arr[i].get<float>();
        }
    }
    if (j.contains("baseStatScalarByRarity") && j["baseStatScalarByRarity"].is_array()) {
        const auto& arr = j["baseStatScalarByRarity"];
        const std::size_t maxR = std::min(arr.size(), out.baseStatScalarByRarity.size());
        for (std::size_t i = 0; i < maxR; ++i) {
            const auto& entry = arr[i];
            if (entry.is_array() && entry.size() >= 2) {
                out.baseStatScalarByRarity[i][0] = entry[0].get<float>();
                out.baseStatScalarByRarity[i][1] = entry[1].get<float>();
            } else if (entry.is_object()) {
                out.baseStatScalarByRarity[i][0] = entry.value("min", out.baseStatScalarByRarity[i][0]);
                out.baseStatScalarByRarity[i][1] = entry.value("max", out.baseStatScalarByRarity[i][1]);
            }
        }
    }
    if (j.contains("affixScalarByRarity") && j["affixScalarByRarity"].is_array()) {
        const auto& arr = j["affixScalarByRarity"];
        const std::size_t maxR = std::min(arr.size(), out.affixScalarByRarity.size());
        for (std::size_t i = 0; i < maxR; ++i) {
            const auto& entry = arr[i];
            if (entry.is_array() && entry.size() >= 2) {
                out.affixScalarByRarity[i][0] = entry[0].get<float>();
                out.affixScalarByRarity[i][1] = entry[1].get<float>();
            } else if (entry.is_object()) {
                out.affixScalarByRarity[i][0] = entry.value("min", out.affixScalarByRarity[i][0]);
                out.affixScalarByRarity[i][1] = entry.value("max", out.affixScalarByRarity[i][1]);
            }
        }
    }
    if (j.contains("armorImplicitScalarByRarity") && j["armorImplicitScalarByRarity"].is_array()) {
        const auto& arr = j["armorImplicitScalarByRarity"];
        const std::size_t maxR = std::min(arr.size(), out.armorImplicitScalarByRarity.size());
        for (std::size_t i = 0; i < maxR; ++i) {
            const auto& entry = arr[i];
            if (entry.is_array() && entry.size() >= 2) {
                out.armorImplicitScalarByRarity[i][0] = entry[0].get<float>();
                out.armorImplicitScalarByRarity[i][1] = entry[1].get<float>();
            } else if (entry.is_object()) {
                out.armorImplicitScalarByRarity[i][0] =
                    entry.value("min", out.armorImplicitScalarByRarity[i][0]);
                out.armorImplicitScalarByRarity[i][1] =
                    entry.value("max", out.armorImplicitScalarByRarity[i][1]);
            }
        }
    }
    auto parseImplicitPool = [&](const json& arr) -> std::vector<ImplicitStatPoolEntry> {
        std::vector<ImplicitStatPoolEntry> parsed;
        if (!arr.is_array()) return parsed;
        for (const auto& entry : arr) {
            if (!entry.is_object()) continue;
            const std::string statKey = entry.value("stat", "");
            auto statId = parseImplicitStatKey(statKey);
            if (!statId.has_value()) {
#ifndef NDEBUG
                Engine::logWarn("Unknown implicit stat id in loot.json: " + statKey);
#endif
                continue;
            }
            const std::string modeKey = entry.value("mode", "flat");
            ImplicitStatPoolEntry def{};
            def.stat = *statId;
            def.mode = parseStatValueMode(modeKey);
            def.min = entry.value("min", 0.0f);
            def.max = entry.value("max", def.min);
            def.weight = entry.value("weight", 1.0f);
            parsed.push_back(def);
        }
        return parsed;
    };

    if (j.contains("implicitStatPools") && j["implicitStatPools"].is_object()) {
        const auto& pools = j["implicitStatPools"];
        for (const auto& kv : pools.items()) {
            auto ct = parseCombatTypeKey(kv.key());
            if (!ct.has_value()) continue;
            out.implicitStatPools[static_cast<std::size_t>(*ct)] = parseImplicitPool(kv.value());
        }
    }
    if (j.contains("armorImplicitPool")) {
        auto parsed = parseImplicitPool(j["armorImplicitPool"]);
        if (!parsed.empty()) out.armorImplicitPool = std::move(parsed);
    }
    if (j.contains("fallbackImplicitPool")) {
        auto parsed = parseImplicitPool(j["fallbackImplicitPool"]);
        if (!parsed.empty()) out.fallbackImplicitPool = std::move(parsed);
    }
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
            t.combatType = CombatType::Count;
            if (it.contains("combatType") && it["combatType"].is_string()) {
                auto ct = parseCombatTypeKey(it.value("combatType", ""));
                if (ct.has_value()) {
                    t.combatType = *ct;
                } else {
#ifndef NDEBUG
                    Engine::logWarn("Invalid combatType for item " + t.id + "; defaulting.");
#endif
                }
            }
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
            if ((t.slot == EquipmentSlot::MainHand || t.slot == EquipmentSlot::Ammo) && t.combatType == CombatType::Count) {
#ifndef NDEBUG
                Engine::logWarn("Missing combatType for item " + t.id + "; defaulting.");
#endif
                t.combatType = (t.slot == EquipmentSlot::Ammo) ? CombatType::Ammo : CombatType::Melee;
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

namespace {
TalentNode parseTalentNode(const json& n, int fallbackIndex, int tier) {
    TalentNode node{};
    node.id = n.value("id", "");
    node.name = n.value("name", "");
    node.description = n.value("description", "");
    node.maxRank = n.value("maxRank", 3);
    node.bonus = parseContribution(n.value("bonus", json::object()));
    if (n.contains("attributes") && n["attributes"].is_object()) {
        const auto& at = n["attributes"];
        node.attributes.STR = at.value("STR", 0);
        node.attributes.DEX = at.value("DEX", 0);
        node.attributes.INT = at.value("INT", 0);
        node.attributes.END = at.value("END", 0);
        node.attributes.LCK = at.value("LCK", 0);
    }
    node.tier = tier;
    if (n.contains("requires") && n["requires"].is_array()) {
        for (const auto& r : n["requires"]) {
            if (r.is_string()) node.prereqs.push_back(r.get<std::string>());
        }
    }
    if (n.contains("pos") && n["pos"].is_array() && n["pos"].size() >= 2) {
        node.pos[0] = n["pos"][0].get<int>();
        node.pos[1] = n["pos"][1].get<int>();
    } else {
        node.pos[0] = fallbackIndex;
        node.pos[1] = 0;
    }
    return node;
}
}  // namespace

std::unordered_map<std::string, TalentTree> loadTalentTrees(const std::string& path) {
    std::unordered_map<std::string, TalentTree> out;
    if (!std::filesystem::exists(path)) return out;
    std::ifstream f(path);
    if (!f.is_open()) return out;
    json j;
    f >> j;
    if (!j.contains("talents")) return out;
    for (const auto& tree : j["talents"].items()) {
        const auto& payload = tree.value();
        TalentTree treeOut{};
        treeOut.archetypeId = tree.key();
        if (payload.is_array()) {
            // Backwards-compatible flat list -> single tier.
            TalentTier tier{};
            tier.tier = 1;
            tier.unlockPoints = 0;
            int idx = 0;
            for (const auto& n : payload) {
                TalentNode node = parseTalentNode(n, idx, tier.tier);
                if (!node.id.empty()) tier.nodes.push_back(std::move(node));
                idx += 1;
            }
            if (!tier.nodes.empty()) treeOut.tiers.push_back(std::move(tier));
        } else if (payload.is_object() && payload.contains("tiers") && payload["tiers"].is_array()) {
            for (const auto& t : payload["tiers"]) {
                TalentTier tier{};
                tier.tier = t.value("tier", 1);
                tier.unlockPoints = t.value("unlockPoints", 0);
                int idx = 0;
                if (t.contains("nodes") && t["nodes"].is_array()) {
                    for (const auto& n : t["nodes"]) {
                        TalentNode node = parseTalentNode(n, idx, tier.tier);
                        if (!node.id.empty()) tier.nodes.push_back(std::move(node));
                        idx += 1;
                    }
                }
                if (!tier.nodes.empty()) treeOut.tiers.push_back(std::move(tier));
            }
        }
        if (!treeOut.tiers.empty()) {
            out[tree.key()] = std::move(treeOut);
        }
    }
    return out;
}

}  // namespace Game::RPG
