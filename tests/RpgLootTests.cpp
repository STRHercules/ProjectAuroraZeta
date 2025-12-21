#include <cassert>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "../game/rpg/RPGContent.h"

using namespace Game::RPG;
using Game::ItemDefinition;

int main() {
    // Affix tier gating: Uncommon items must not roll _l tiers.
    {
        LootTable table{};
        table.affixes.push_back({"atk_flat_s", "+3 Attack Power", [] {
            Engine::Gameplay::RPG::StatContribution c{};
            c.flat.attackPower = 3.0f;
            return c;
        }()});
        table.affixes.push_back({"atk_flat_m", "+6 Attack Power", [] {
            Engine::Gameplay::RPG::StatContribution c{};
            c.flat.attackPower = 6.0f;
            return c;
        }()});
        table.affixes.push_back({"atk_flat_l", "+9 Attack Power", [] {
            Engine::Gameplay::RPG::StatContribution c{};
            c.flat.attackPower = 9.0f;
            return c;
        }()});

        ItemTemplate t{};
        t.id = "test_sword";
        t.name = "Test Sword";
        t.slot = EquipmentSlot::MainHand;
        t.rarity = Rarity::Uncommon;
        t.maxAffixes = 1;
        t.affixPool = {"atk_flat_s", "atk_flat_m", "atk_flat_l"};
        table.items.push_back(t);

        LootContext ctx{};
        std::mt19937 rng(42);
        for (int i = 0; i < 200; ++i) {
            auto rolled = generateLoot(table, ctx, rng);
            assert(!rolled.affixes.empty());
            for (const auto& a : rolled.affixes) {
                assert(a.id != "atk_flat_l");
            }
        }
    }

    // Equip/unequip stat leakage regression: recompute returns to prior values.
    {
        LootTable table{};
        table.affixes.push_back({"atk_flat_s", "+3 Attack Power", [] {
            Engine::Gameplay::RPG::StatContribution c{};
            c.flat.attackPower = 3.0f;
            return c;
        }()});

        ItemTemplate sword{};
        sword.id = "test_sword";
        sword.name = "Test Sword";
        sword.slot = EquipmentSlot::MainHand;
        sword.rarity = Rarity::Uncommon;
        sword.baseStats.flat.attackPower = 10.0f;
        table.items.push_back(sword);

        ItemTemplate ring{};
        ring.id = "test_ring";
        ring.name = "Test Ring";
        ring.slot = EquipmentSlot::Ring1;
        ring.rarity = Rarity::Common;
        ring.baseStats.flat.critChance = 0.03f;
        table.items.push_back(ring);

        ItemTemplate gem{};
        gem.id = "test_gem";
        gem.name = "Test Gem";
        gem.slot = EquipmentSlot::Trinket;
        gem.rarity = Rarity::Common;
        gem.socketable = true;
        gem.baseStats.flat.attackPower = 2.0f;
        table.items.push_back(gem);

        ItemDefinition swordDef{};
        swordDef.rpgTemplateId = "test_sword";
        swordDef.rpgAffixIds = {"atk_flat_s"};
        swordDef.rpgSocketed.push_back({"test_gem", {}});

        ItemDefinition ringDef{};
        ringDef.rpgTemplateId = "test_ring";

        const auto swordStats = computeItemContribution(table, swordDef, 1);
        assert(static_cast<int>(std::round(swordStats.flat.attackPower)) == 15);

        auto withRing = computeEquipmentContribution(table, std::vector<ItemDefinition>{swordDef, ringDef});
        assert(withRing.flat.attackPower == swordStats.flat.attackPower);
        assert(withRing.flat.critChance > 0.0f);

        auto withoutRing = computeEquipmentContribution(table, std::vector<ItemDefinition>{swordDef});
        assert(withoutRing.flat.attackPower == swordStats.flat.attackPower);
        assert(withoutRing.flat.critChance == 0.0f);
    }

    // Rarity scalar application: base + affix scaling applied per item definition.
    {
        LootTable table{};
        table.affixes.push_back({"atk_flat_s", "+3 Attack Power", [] {
            Engine::Gameplay::RPG::StatContribution c{};
            c.flat.attackPower = 3.0f;
            return c;
        }()});

        ItemTemplate sword{};
        sword.id = "scaled_sword";
        sword.name = "Scaled Sword";
        sword.slot = EquipmentSlot::MainHand;
        sword.rarity = Rarity::Epic;
        sword.baseStats.flat.attackPower = 10.0f;
        table.items.push_back(sword);

        ItemDefinition def{};
        def.rpgTemplateId = "scaled_sword";
        def.rpgAffixIds = {"atk_flat_s"};
        def.rpgBaseScalar = 1.2f;
        def.rpgAffixScalar = 1.5f;

        const auto stats = computeItemContribution(table, def, 1);
        assert(static_cast<int>(std::round(stats.flat.attackPower)) == 17);
    }

    return 0;
}
