Stat legend:

- attackPower: increases physical damage scaling
- spellPower: increases spell/ability damage scaling
- attackSpeed: increases attack rate (multiplicative)
- moveSpeed: increases movement speed (multiplicative)
- accuracy: improves hit quality/accuracy
- critChance: chance to crit (0–1)
- armorPen: reduces target armor mitigation
- lifesteal: heals for a % of damage dealt (0–1)
- lifeOnHit: flat heal per hit
- cleaveChance: chance to cleave nearby targets (0–1)
- armor: physical mitigation
- evasion: dodge/parry contribution
- tenacity: status resistance
- healthMax: max health
- shieldMax: max shields
- cooldownReduction: reduces cooldowns (0–1)
- resourceRegen: resource regen bonus
- goldGainMult: gold gain multiplier (base 1.0)
- rarityScore: increases loot rarity score
- statusChance: additive bonus to on-hit status chance (0–1)
- resist_*: damage-type resistance (0–1)
- * (mult): multiplicative modifier (applied as 1 + value)

Affixes (id → name → effect)

- acc_flat_s: +4 Accuracy → accuracy 4
- acc_flat_m: +8 Accuracy → accuracy 8
- acc_flat_l: +12 Accuracy → accuracy 12
- regen_mult_s: +8% Resource Regen → resourceRegen (mult) 0.08
- regen_mult_m: +15% Resource Regen → resourceRegen (mult) 0.15
- regen_mult_l: +25% Resource Regen → resourceRegen (mult) 0.25
- atk_flat_s: +3 Attack Power → attackPower 3
- atk_flat_m: +6 Attack Power → attackPower 6
- atk_flat_l: +9 Attack Power → attackPower 9
- spell_flat_s: +4 Spell Power → spellPower 4
- spell_flat_m: +8 Spell Power → spellPower 8
- spell_flat_l: +12 Spell Power → spellPower 12
- armor_flat_s: +1 Armor → armor 1
- armor_flat_m: +2 Armor → armor 2
- armor_flat_l: +4 Armor → armor 4
- hp_flat_s: +12 Max Health → healthMax 12
- hp_flat_m: +25 Max Health → healthMax 25
- hp_flat_l: +50 Max Health → healthMax 50
- shield_flat_s: +12 Max Shields → shieldMax 12
- shield_flat_m: +25 Max Shields → shieldMax 25
- shield_flat_l: +50 Max Shields → shieldMax 50
- as_mult_s: +6% Attack Speed → attackSpeed (mult) 0.06
- as_mult_m: +10% Attack Speed → attackSpeed (mult) 0.10
- as_mult_l: +14% Attack Speed → attackSpeed (mult) 0.14
- ms_mult_s: +5% Move Speed → moveSpeed (mult) 0.05
- ms_mult_m: +8% Move Speed → moveSpeed (mult) 0.08
- ms_mult_l: +12% Move Speed → moveSpeed (mult) 0.12
- crit_flat_s: +3% Crit Chance → critChance 0.03
- crit_flat_m: +6% Crit Chance → critChance 0.06
- crit_flat_l: +9% Crit Chance → critChance 0.09
- cdr_flat_s: +3% Cooldown Reduction → cooldownReduction 0.03
- cdr_flat_m: +6% Cooldown Reduction → cooldownReduction 0.06
- cdr_flat_l: +9% Cooldown Reduction → cooldownReduction 0.09
- ten_flat_s: +4 Tenacity → tenacity 4
- ten_flat_m: +8 Tenacity → tenacity 8
- ten_flat_l: +12 Tenacity → tenacity 12
- eva_flat_s: +3 Evasion → evasion 3
- eva_flat_m: +6 Evasion → evasion 6
- eva_flat_l: +9 Evasion → evasion 9
- arp_flat_s: +3 Armor Pen → armorPen 3
- arp_flat_m: +6 Armor Pen → armorPen 6
- arp_flat_l: +9 Armor Pen → armorPen 9
- gold_flat_s: +5% Gold Gain → goldGainMult 0.05
- gold_flat_m: +10% Gold Gain → goldGainMult 0.10
- gold_flat_l: +15% Gold Gain → goldGainMult 0.15
- rarity_flat_s: +0.6 Rarity Score → rarityScore 0.6
- rarity_flat_m: +1.2 Rarity Score → rarityScore 1.2
- rarity_flat_l: +2.0 Rarity Score → rarityScore 2.0
- fire_res_s: +8% Fire Resist → resist_Fire 0.08
- fire_res_m: +12% Fire Resist → resist_Fire 0.12
- frost_res_s: +8% Frost Resist → resist_Frost 0.08
- frost_res_m: +12% Frost Resist → resist_Frost 0.12
- shock_res_s: +8% Shock Resist → resist_Shock 0.08
- shock_res_m: +12% Shock Resist → resist_Shock 0.12
- poison_res_s: +8% Poison Resist → resist_Poison 0.08
- poison_res_m: +12% Poison Resist → resist_Poison 0.12
- arcane_res_s: +8% Arcane Resist → resist_Arcane 0.08
- arcane_res_m: +12% Arcane Resist → resist_Arcane 0.12
- all_res_s: +4% All Resist → resist_Fire/Frost/Shock/Poison/Arcane 0.04
- all_res_m: +6% All Resist → resist_Fire/Frost/Shock/Poison/Arcane 0.06

Hybrid affixes

- duelist_s: +3 Attack Power, +3 Evasion
- marksman_s: +4 Accuracy, +3 Armor Pen
- assassin_s: +6% Attack Speed, +3% Crit Chance
- executioner_s: +6 Attack Power, +3 Armor Pen
- arcanist_s: +8 Spell Power, +3% Cooldown Reduction
- bulwark_s: +25 Max Health, +2 Armor
- warded_s: +25 Max Shields, +8% Arcane Resist
- stalwart_s: +12 Max Health, +4 Tenacity
- elusive_s: +5% Move Speed, +3 Evasion
- tactician_s: +3% Cooldown Reduction, +8% Resource Regen
- prospector_s: +5% Gold Gain, +0.6 Rarity Score
- precision_s: +4 Accuracy, +3% Crit Chance

Base gear templates (Unique-only baseline stats)

Weapons
- Sword: attackPower 12
- Dagger: attackPower 9, attackSpeed (mult) 0.06
- Bow: attackPower 10, accuracy 4
- Wand: spellPower 12, cooldownReduction 0.03
- Staff: spellPower 14, resourceRegen (mult) 0.08

Armor + accessories
- Plate Chest: armor 3, healthMax 25
- Leather Chest: armor 2, evasion 3
- Robe: healthMax 12, spellPower 4, resist_Arcane 0.08
- Greathelm: armor 2, tenacity 4
- Hood: evasion 3, resist_Poison 0.08
- Gauntlets: attackPower 3, armor 1
- Wraps: attackSpeed (mult) 0.06
- Mystic Gloves: spellPower 4, resourceRegen (mult) 0.08
- Greaves: armor 1, moveSpeed (mult) 0.05
- Light Boots: evasion 3, moveSpeed (mult) 0.05
- War Belt: healthMax 25
- Arcane Sash: shieldMax 25
- Utility Harness: resourceRegen (mult) 0.08
- Shield: armor 2, shieldMax 25
- Talisman: cooldownReduction 0.03, resist_Arcane 0.08
- Quiver: accuracy 4, critChance 0.03
- Ring (damage): critChance 0.03 or attackPower 3 or spellPower 4
- Amulet (utility): cooldownReduction 0.03 or resourceRegen (mult) 0.08
- Charm (loot/econ): goldGainMult 0.05 or rarityScore 0.6

Notes
- Non-Unique RPG gear ignores static template stats; implicit rolls + affixes supply stats instead.
- Full item catalog and sprite mappings live in `data/rpg/loot.json`.
- Rarity-gated affix tiers: _s (low), _m (mid), _l (high).
- Affix tier weights and affix count ranges are configurable in `data/rpg/loot.json`.
- Base stat and affix power scalars per rarity are configurable in `data/rpg/loot.json` (`baseStatScalarByRarity`, `affixScalarByRarity`).
