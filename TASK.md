# GEAR.md — Item/Affix System Update Task (Codex)

## Objective

Implement and standardize an RPG item/affix system update including:

1. Adding the **new affixes** and **new gear base bonuses** listed below.
2. Enforcing **rarity-gated affix tiering** (no “max level” affixes on Uncommon items; lower-tier affixes become less common on higher-rarity items).
3. Fixing **stat visualization** (Attack Speed / Move Speed currently display inconsistently between item details and hover tooltips).
4. Ensuring **equip/unequip** correctly applies and removes all gear-derived stats.
5. Implementing item-type rules for **scrolls, food, potions, and gems**.
6. Applying a set of **naming migrations** for armor/weapon tiers, jewelry, capes, arrows, and gems.
7. Adding **new weapon and unique gear assets** (sprites) to the loot tables (see **Section I**).

---

## Conventions and stat schema

### Stat keys and value conventions

- Flat stats are additive (e.g., `attackPower 6`).
- Multiplicative stats are stored as an additive modifier and applied as `1 + value`:
  - Example: `attackSpeed (mult) 0.06` means **+6% attack speed** and applies as multiplier **1.06x**.

### UI display convention (required)

For **(mult)** stats:
- Display as **percentage bonus**: `value * 100` with a leading `+` sign (e.g., `0.06` → `+6%`).
- Do **not** display as “hundreds of percent” derived from the final multiplier (e.g., avoid showing `206%` for a `+6%` bonus).
- Item hover tooltip and Item Details window must use the **same formatting function**.

For non-(mult) stats:
- Display as the flat value (e.g., `+6 Attack Power`) or as a percentage if the stat is stored 0–1 (e.g., `resist_Fire 0.08` → `+8% Fire Resist`).

---

## A) New affixes to add

Add these to the affix registry (id → display name → effect).

### A1) Single-stat affixes

#### Accuracy
- `acc_flat_s`: **+4 Accuracy** → `accuracy 4`
- `acc_flat_m`: **+8 Accuracy** → `accuracy 8`
- `acc_flat_l`: **+12 Accuracy** → `accuracy 12`

#### Resource Regen (mult)
- `regen_mult_s`: **+8% Resource Regen** → `resourceRegen (mult) 0.08`
- `regen_mult_m`: **+15% Resource Regen** → `resourceRegen (mult) 0.15`
- `regen_mult_l`: **+25% Resource Regen** → `resourceRegen (mult) 0.25`

#### Additional tiers (existing supported stats)
- `atk_flat_l`: **+9 Attack Power** → `attackPower 9`
- `spell_flat_l`: **+12 Spell Power** → `spellPower 12`
- `armor_flat_l`: **+4 Armor** → `armor 4`
- `hp_flat_l`: **+50 Max Health** → `healthMax 50`
- `shield_flat_l`: **+50 Max Shields** → `shieldMax 50`

#### More % tiers (mult or 0–1 stats)
- `as_mult_m`: **+10% Attack Speed** → `attackSpeed (mult) 0.10`
- `as_mult_l`: **+14% Attack Speed** → `attackSpeed (mult) 0.14`
- `ms_mult_m`: **+8% Move Speed** → `moveSpeed (mult) 0.08`
- `ms_mult_l`: **+12% Move Speed** → `moveSpeed (mult) 0.12`
- `crit_flat_l`: **+9% Crit Chance** → `critChance 0.09`
- `cdr_flat_m`: **+6% Cooldown Reduction** → `cooldownReduction 0.06`
- `cdr_flat_l`: **+9% Cooldown Reduction** → `cooldownReduction 0.09`

#### Defensive utility scaling
- `ten_flat_m`: **+8 Tenacity** → `tenacity 8`
- `ten_flat_l`: **+12 Tenacity** → `tenacity 12`
- `eva_flat_m`: **+6 Evasion** → `evasion 6`
- `eva_flat_l`: **+9 Evasion** → `evasion 9`

#### Penetration scaling
- `arp_flat_m`: **+6 Armor Pen** → `armorPen 6`
- `arp_flat_l`: **+9 Armor Pen** → `armorPen 9`

#### Economy / loot scaling
- `gold_flat_m`: **+10% Gold Gain** → `goldGainMult 0.10`
- `gold_flat_l`: **+15% Gold Gain** → `goldGainMult 0.15`
- `rarity_flat_m`: **+1.2 Rarity Score** → `rarityScore 1.2`
- `rarity_flat_l`: **+2.0 Rarity Score** → `rarityScore 2.0`

#### Resist “medium” tiers
- `fire_res_m`: **+12% Fire Resist** → `resist_Fire 0.12`
- `frost_res_m`: **+12% Frost Resist** → `resist_Frost 0.12`
- `shock_res_m`: **+12% Shock Resist** → `resist_Shock 0.12`
- `poison_res_m`: **+12% Poison Resist** → `resist_Poison 0.12`
- `arcane_res_m`: **+12% Arcane Resist** → `resist_Arcane 0.12`

> Note: “large” resist tiers (e.g., `0.16`) are optional and are not required by this task unless explicitly added later.

---

### A2) Hybrid affixes (multi-stat)

#### Offensive hybrids
- `duelist_s`: **+3 Attack Power, +3 Evasion** → `attackPower 3`, `evasion 3`
- `marksman_s`: **+4 Accuracy, +3 Armor Pen** → `accuracy 4`, `armorPen 3`
- `assassin_s`: **+6% Attack Speed, +3% Crit Chance** → `attackSpeed (mult) 0.06`, `critChance 0.03`
- `executioner_s`: **+6 Attack Power, +3 Armor Pen** → `attackPower 6`, `armorPen 3`
- `arcanist_s`: **+8 Spell Power, +3% Cooldown Reduction** → `spellPower 8`, `cooldownReduction 0.03`

#### Defensive hybrids
- `bulwark_s`: **+25 Max Health, +2 Armor** → `healthMax 25`, `armor 2`
- `warded_s`: **+25 Max Shields, +8% Arcane Resist** → `shieldMax 25`, `resist_Arcane 0.08`
- `stalwart_s`: **+12 Max Health, +4 Tenacity** → `healthMax 12`, `tenacity 4`
- `elusive_s`: **+5% Move Speed, +3 Evasion** → `moveSpeed (mult) 0.05`, `evasion 3`

#### Utility / “meta” hybrids
- `tactician_s`: **+3% Cooldown Reduction, +8% Resource Regen** → `cooldownReduction 0.03`, `resourceRegen (mult) 0.08`
- `prospector_s`: **+5% Gold Gain, +0.6 Rarity Score** → `goldGainMult 0.05`, `rarityScore 0.6`
- `precision_s`: **+4 Accuracy, +3% Crit Chance** → `accuracy 4`, `critChance 0.03`

---

### A3) All-resist affixes

- `all_res_s`: **+4% All Resist** →  
  `resist_Fire 0.04`, `resist_Frost 0.04`, `resist_Shock 0.04`, `resist_Poison 0.04`, `resist_Arcane 0.04`
- `all_res_m`: **+6% All Resist** →  
  `resist_Fire 0.06`, `resist_Frost 0.06`, `resist_Shock 0.06`, `resist_Poison 0.06`, `resist_Arcane 0.06`

---

## B) New gear base bonuses (implicit/base stats)

Add/update base templates so these items have these implicit stats before affix rolls.

### B1) Weapons
- **Sword (balanced physical):** `attackPower 12`
- **Dagger (fast physical):** `attackPower 9`, `attackSpeed (mult) 0.06`
- **Bow (precision physical):** `attackPower 10`, `accuracy 4`
- **Wand (caster tempo):** `spellPower 12`, `cooldownReduction 0.03`
- **Staff (caster sustain):** `spellPower 14`, `resourceRegen (mult) 0.08`

### B2) Armor and accessories

**Chest**
- Plate Chest: `armor 3`, `healthMax 25`
- Leather Chest: `armor 2`, `evasion 3`
- Robe: `healthMax 12`, `spellPower 4`, `resist_Arcane 0.08`

**Helm**
- Greathelm: `armor 2`, `tenacity 4`
- Hood: `evasion 3`, `resist_Poison 0.08`

**Gloves**
- Gauntlets: `attackPower 3`, `armor 1`
- Wraps: `attackSpeed (mult) 0.06`
- Mystic Gloves: `spellPower 4`, `resourceRegen (mult) 0.08`

**Boots**
- Greaves: `armor 1`, `moveSpeed (mult) 0.05`
- Light Boots: `evasion 3`, `moveSpeed (mult) 0.05`

**Belt**
- War Belt: `healthMax 25`
- Arcane Sash: `shieldMax 25`
- Utility Harness: `resourceRegen (mult) 0.08`

**Shield / Offhand**
- Shield: `armor 2`, `shieldMax 25`
- Talisman (offhand): `cooldownReduction 0.03`, `resist_Arcane 0.08`
- Quiver (offhand): `accuracy 4`, `critChance 0.03`

**Jewelry**
- Ring (damage): `critChance 0.03` **or** `attackPower 3` **or** `spellPower 4`
- Amulet (utility): `cooldownReduction 0.03` **or** `resourceRegen (mult) 0.08`
- Charm (loot/econ): `goldGainMult 0.05` **or** `rarityScore 0.6`

---

## C) Rarity-gated affix tiers

### Requirements

- Affix tiers must be gated by item rarity.
- We must **not** be able to roll a **max level** affix on an **Uncommon** item.
- “Uncommon level” affixes should become **less common** as item rarity increases (Rare → Epic → Legendary).

### Implementation approach (recommended)

Treat `_s`, `_m`, `_l` as tier levels:
- `_s` = Tier 1 (low)
- `_m` = Tier 2 (mid / “uncommon level”)
- `_l` = Tier 3 (high / “max level”)

#### Allowed tiers by item rarity
- **Common:** `_s` only
- **Uncommon:** `_s`, `_m` (no `_l`)
- **Rare:** `_s`, `_m`, `_l`
- **Epic:** `_s`, `_m`, `_l`
- **Legendary:** `_m`, `_l` (optional: allow `_s` = 0%)

#### Suggested tier weighting (table-driven; easy to tune)

- **Common:** `_s` 100%
- **Uncommon:** `_s` 40%, `_m` 60%, `_l` 0%
- **Rare:** `_s` 15%, `_m` 25%, `_l` 60%
- **Epic:** `_s` 5%, `_m` 15%, `_l` 80%
- **Legendary:** `_s` 0%, `_m` 10%, `_l` 90%

---

## D) Item behavior rules

### D1) Scrolls
- Scrolls must show a clear description of what they do in the **Item Details** window.
- Implementation: ensure scroll items have a non-empty `description` (or equivalent) and the UI renders it.

### D2) Food
- Food item rarity dictates how much health it regenerates: **higher rarity → more regeneration**.
- Implementation: food regen should be derived from rarity (table-driven) rather than hardcoded per item name.

### D3) Potions
- Potions should **not** have rarity.
- Implementation: set potion rarity to `None` / `NoRarity` (or treat as a fixed baseline rarity) and ensure loot/UI logic doesn’t assign or display rarity for potions.

### D4) Gems
- Gems **should not have affixes**.
- Gems’ bonuses must apply to the **item they socket into**:
  - On socket: gem bonus stats are added to the parent item’s effective stats.
  - On unsocket: gem bonus stats are removed from the parent item’s effective stats.
- Implementation detail: ensure the socket system recomputes the parent item’s stat bundle deterministically from:
  1. Base item stats
  2. Item affixes (if any)
  3. All currently socketed gem bonuses

---

## E) Stat visualization and equip/unequip correctness

### E1) Normalize Attack Speed and Move Speed display

Current issues to fix:
- Item Details show values like `+206% Attack Speed`, while hover tooltip shows `+6% Attack Speed`.
- Move Speed/Attack Speed appear in Item Details for **every item type**, including potions/gems/food.

Required fixes:
1. **Single source of truth for formatting**:
   - Tooltip and Item Details must call the same stat-formatting utility.
2. **(mult) stats display as bonus percent**:
   - `0.06` must display as `+6%`, not `206%`.
3. **Only display stats that actually exist / are non-zero**:
   - Potions, food, and gems must not show Attack Speed / Move Speed unless they truly modify them.
4. Ensure internal stat application uses the correct model:
   - Multipliers apply as `final = base * (1 + sum(multMods))`.

### E2) Equipment application correctness

- When an item is equipped, all of its stats (base + affixes + socketed gem bonuses) must apply to the character.
- When an item is unequipped, those stats must be removed.
- Add regression tests to guarantee no “stat leakage”:
  - Equip → stat increases
  - Unequip → stat returns to the exact previous value

---

## F) Naming changes (content migration)

Apply the following renames consistently across:
- Item display names
- Internal IDs (if you maintain human-readable IDs)
- Localization keys (if applicable)
- Loot tables / crafting recipes / vendor inventories referencing these items

### F1) Armor tier renames
- Rename all **“Copper” Armor** (Chest, Legs, Boots, Helmet, Gloves) → **“Leather”**
- Rename all **“Legendary” Armor** (Chest, Legs, Boots, Helmet, Gloves) → **“Sunsteel”**

### F2) Weapon tier renames
- Rename all **“Legendary” Weapons** → **“Sunsteel”**
  - Includes **Quivers**

### F3) Arrow rename
- Rename **Legendary Arrow** → **Sunsteel Arrow**

### F4) Ring renames
- Green Ring → Emerald Ring
- Blue Ring → Sapphire Ring
- Orange Ring → Garnet Ring
- Purple Ring → Amethyst Ring
- Grey Ring → Onyx Ring
- Yellow Ring → Citrine Ring

### F5) Amulet renames
- Green Amulet → Emerald Amulet
- Iron Amulet → Onyx Amulet
- Basic Amulet → Old Amulet
- Diamond Amulet → Sapphire Amulet
- Heart Amulet → Sunsteel Amulet
- Purple Amulet → Amethyst Amulet

### F6) Cape renames
- Copper → Leather
- Iron → Black
- Silver → Grey
- Gold → Golden
- Admantium → Silk
- Legendary → Sunsilk

### F7) Gem renames

**Small gems**
- Small Green Gem → Verdant Chip
- Small Red Gem → Crimson Chip
- Small Orange Gem → Amber Chip
- Small Purple Gem → Violet Chip
- Small Crystal → Prism Chip
- Small Jet → Onyx Chip
- Small Diamond → Radiant Chip

**Large gems**
- Large Green Gem → Verdant Core
- Large Red Gem → Crimson Core
- Large Orange Gem → Amber Core
- Large Purple Gem → Violet Core
- Large Crystal → Prism Core
- Large Jet → Onyx Core
- Large Diamond → Radiant Core

---

## G) Balance guardrails (implementation notes)

- Keep **attackSpeed / moveSpeed / resourceRegen** rolls modest; multiplicative stacking scales rapidly.
- For **cooldownReduction** and **resists** (0–1 style stats), implement either:
  - a hard cap (recommended range: `0.50–0.80`), and/or
  - diminishing returns beyond a threshold.
- Hybrid affixes should not dominate:
  - Ensure hybrids are balanced against single-stat tiers in your affix budget.

---

## H) Acceptance criteria (must pass)

1. All new affix IDs listed in Section A exist in the affix registry and generate correct stat bundles.
2. All base gear templates listed in Section B exist and provide the correct implicit stats.
3. Affix tier gating:
   - Uncommon items never roll `_l` (max) tier affixes.
   - Lower-tier affixes become less common on higher rarities (as per Section C weighting or equivalent).
4. Scrolls show their effect text in Item Details.
5. Food regen scales with food rarity.
6. Potions have no rarity and do not display rarity in UI.
7. Gems:
   - Have no affixes.
   - Correctly add/remove bonuses when socketed/unsocketed.
8. Tooltip and Item Details display **Attack Speed / Move Speed** consistently and correctly as bonus percentages.
9. Equip/unequip is lossless and deterministic:
   - Equip adds stats; unequip removes them; no accumulation drift.
10. All rename mappings in Section F are applied consistently across the game (UI + data + loot/crafting references).
11. All new weapon and unique gear assets in Section I are **available in loot** via the loot table.

---

## I) New weapon & unique gear assets — content + loot integration

### I1) New weapon sprite sheets to ingest

These assets have been provided and must be integrated into the item content pipeline (sprite atlas/manifest, item definitions, and loot tables):

- `Axes.png`
  - 48x160 sheet, 16x16 sprites
  - Order: left-to-right, top-to-bottom
  - Sprite names:
    - Emberwood Hatchet
    - Ironhook Handaxe
    - Titancleave Greataxe
    - Stonebiter Hatchet
    - Skyspike Hookaxe
    - Frostbeard Battleaxe
    - Mooncrescent Reaver
    - Ash-Raider Waraxe
    - Stormrend Greatblade
    - Quarrymaul Chopper
    - Deepcut Bearded Axe
    - Frostjaw Waraxe
    - Pikehead Splitter
    - Blackforge Broadaxe
    - Oathbreaker Cleaver
    - Driftsteel Hatchet
    - Ravager's Beak
    - Silveredge Handaxe
    - Gravelgnash Axe
    - Twinwing Double-Bit
    - Nightreap Crescent Axe
    - Mountainbreaker Greataxe
    - Ironbloom Cleaver
    - Hookfang Waraxe
    - Coldsteel Executioner
    - Glacierfall Greataxe
    - Gilded Twin-Fang
    - Warlord's Chopper
    - Thunderhead Greataxe
    - Leviathan Cleaver

- `Cleavers.png`
  - 64x160 sheet, 16x16 sprites
  - Order: left-to-right, top-to-bottom
  - Sprite names:
    - Hearthsteel Cleaver
    - Stoneblock Chopper
    - Emberhook Butcher
    - Duskcurve Cleaver
    - Searmark Cleaver
    - Porksplitter
    - Ash-Hew Chopper
    - Smokewedge Cleaver
    - Bone-Shear
    - Tallowcutter
    - Rib-Ripper
    - Marrowblade
    - Chopstorm Cleaver
    - Slab-Slayer
    - Hookjaw Cleaver
    - Wharfside Chopper
    - Gristlegrinder
    - Broadbite Cleaver
    - Cartilage Cutter
    - Twin-Weight Chopper
    - Dockhand's Cleaver
    - Ironpantry Chopper
    - Smelter's Butcher
    - Guttersteel Cleaver
    - Cinderchop Cleaver
    - Coldblock Cleaver
    - Brinehook Butcher
    - Saltknife Cleaver
    - Spinesplitter
    - Heap-Hewer
    - Meathook Reaper
    - Marketday Cleaver
    - Ashen Carver
    - Butcher's Oath
    - Furnacefang Cleaver
    - Hammeredge Chopper
    - Lowroad Cleaver
    - Black Apron Blade
    - Graveyard Butcher
    - King's Kitchen Guillotine

- `Clubs.png`
  - 64x160 sheet, 16x16 sprites
  - Order: left-to-right, top-to-bottom
  - Sprite names:
    - Tavern-Brawl Baton
    - Cobbleknob Club
    - Gutterpick Mace
    - Knife-Edge Cudgel
    - Old Faithful Cudgel
    - Gravelspike Crusher
    - Anvilcap Maul
    - Rustfang Baton
    - Oakbone Bludgeon
    - Squarehead Smiter
    - Rivet-Maul Mace
    - Gleamshiv Club
    - Brawler's Branch
    - Cornerstone Hammer
    - Morningstar Knocker
    - Dockside Shivstick
    - Ironknob Cudgel
    - Pebblemaul
    - Caltrop Crown Mace
    - Shortbite Club
    - Stonejaw Baton
    - Knuckleknock Mace
    - Thornwheel Morningstar
    - Brickbat Club
    - Golemfist Maul
    - Roundhead Smasher
    - Warder's Flange
    - Wickedspike Pick-Mace
    - Hexhead Bruiser
    - Bulwark Knocker
    - Stitchspike Star
    - Backalley Bladebat
    - Grudgewood Club
    - Cinderblock Smiter
    - Crownbreaker Mace
    - Ironfang Baton
    - Big Stick Energy
    - Sunburst Smasher
    - Thistlemaul Morningstar
    - Ogre's Tooth Club

- `Hammers.png`
  - 68x160 sheet, 16x16 sprites
  - Order: left-to-right, top-to-bottom
  - Sprite names:
    - Rivet-Tap Hammer
    - Hookbeak Warhammer
    - Quarryblock Maul
    - Nailbiter Mallet
    - Anvilfang Hammer
    - Sunforged Sledge
    - Cobblecracker
    - Splitjaw Maul
    - Ironkeep Hammer
    - Backhand Basher
    - Gilded Pounder
    - Darkforge Maul
    - Stonethud Hammer
    - Knuckledrum Maul
    - Cragspike Warhammer
    - Bricklayer's Sledge
    - Oresplitter Maul
    - Grave-Anvil Hammer
    - Hearthmaul
    - Cinderstrike Hammer
    - Stormweight Maul
    - Deepmine Sledge
    - Frostplate Hammer
    - Shatterstone Maul
    - Titan's Knocker
    - Crownhammer
    - Boltbreaker Maul
    - Wyrmtooth Warhammer
    - Ironfall Maul
    - Earthshaker Sledge

- `Polearms.png`
  - 144x160 sheet, 16x16 sprites
  - Order: left-to-right, top-to-bottom
  - Sprite names:
    - Ashwood Quarterstaff
    - Ironleaf Spear
    - Blackforge Halberd
    - Pilgrim's Staff
    - Wayfarer's Staff
    - Twinspike Partisan
    - Skullthorn Polemace
    - Tidefork Trident
    - Stonecleaver Poleaxe
    - Reedwind Quarterstaff
    - Rivethead Polehammer
    - Coldedge Halberd
    - Sapphire Loop Staff
    - Monk's Longstaff
    - Hearthoak Staff
    - Windcutter Glaive
    - Starbreaker Warstar
    - Ironhook Poleaxe
    - Wanderwood Staff
    - Hillpoint Spear
    - Greywing Halberd
    - Birch Quarterstaff
    - Simplewood Staff
    - Guard's Warstaff
    - Flintfang Polemace
    - Splitbeard Halberd
    - Quarryreap Poleaxe
    - Swiftthorn Javelin
    - Oxblade Bardiche
    - Reaper's Hookstaff
    - Elm Quarterstaff
    - Ashen Longstaff
    - Lanternbearer Staff
    - Bonecrush Polemace
    - Warden's Trident
    - Crowspike Warstar
    - Azure-Studded Staff
    - Royalcrest Halberd
    - Ironcrown Polemace
    - Nightwalk Staff
    - Riveted Longstaff
    - Spikefoot Pike
    - Dreadwheel Warstar
    - Anviljaw Poleaxe
    - Blackbarb Bardiche
    - Orchard Quarterstaff
    - Dawnpoint Spear
    - Longreach Pike
    - Steelcapped Staff
    - Arcflash Spellstaff
    - Brawler's Quarterstaff
    - Ragetooth Polemace
    - Oathsplit Poleaxe
    - Deepcut Poleaxe
    - Willowwind Staff
    - Frostpoint Spear
    - Emberpoint Spear
    - Blueflare Spellstaff
    - Stoneweight Maulstaff
    - Nailspike Maulstaff
    - Stormcrown Polemace
    - Riftedge Halberd
    - Ironbranch Halberd
    - Traveler's Staff
    - Needlepoint Spear
    - Boarward Winged Spear
    - Dustwood Quarterstaff
    - Knotroot Staff
    - Moonglow Spellstaff
    - Grudgeflange Polemace
    - Frostwheel Warstar
    - Duskcleave Poleaxe
    - Creekwood Staff
    - Skythrust Spear
    - Bannerlord Halberd
    - Cedar Longstaff
    - Ironbark Warstaff
    - Archmage's Crescent Staff
    - Seawatch Trident
    - Sunspike Warstar
    - Titanbite Poleaxe
    - Scholar's Staff
    - Wolfpoint Spear
    - Mooncrescent Glaive
    - Pilgrim's Longstaff
    - Ogresshoulder Maulstaff
    - Serpentcoil Sorcerer Staff
    - Stormfork Trident
    - Gilded Oath Poleaxe
    - Kingsplit Poleaxe

- `Scythe.png`
  - 96x160 sheet, 16x16 sprites
  - Order: left-to-right, top-to-bottom
  - Sprite names:
    - Ashwood War Scythe
    - Rusthook Reaping Scythe
    - Greyfang War Scythe
    - Fieldhand Hand Scythe
    - Pale Moon Crescent Scythe
    - Bluejaw Razor Crescent Scythe
    - Bronzewheat Harvest Scythe
    - Rivet-Reap War Scythe
    - Ironbriar Reaping Scythe
    - Butcher's Hand Scythe
    - Duskmarch Crescent Scythe
    - Frostbite Sawtooth Crescent Scythe
    - Longreach Harvest Scythe
    - Hollowsteel War Scythe
    - Blackthorn Reaping Scythe
    - Crookblade Hand Scythe
    - Gravewind Crescent Scythe
    - Seawraith Razor Crescent Scythe
    - Warden's War Scythe
    - Stonecut Reaping Scythe
    - Nightroot War Scythe
    - Hooked Reaper Hand Scythe
    - Murkmoon Crescent Scythe
    - Stormcrescent Sawtooth Crescent Scythe
    - Ironfield Harvest Scythe
    - Anvilhook War Scythe
    - Widowmaker Reaping Scythe
    - Oathbreaker Hand Scythe
    - Bonewhite Crescent Scythe
    - Steelmoon Razor Crescent Scythe
    - Ember-Reap War Scythe
    - Forgebrand Reaping Scythe
    - Shadowspike War Scythe
    - Cindersickle Hand Scythe
    - Sunken Arc Crescent Scythe
    - Nightglass Sawtooth Crescent Scythe
    - Frostfield Harvest Scythe
    - Coldiron War Scythe
    - Icefang Reaping Scythe
    - Wintershear Hand Scythe
    - Glacierhook Crescent Scythe
    - Rimecrescent Razor Crescent Scythe
    - Bloodharvest War Scythe
    - Redhook Reaping Scythe
    - Gorethorn War Scythe
    - Bloodletter Hand Scythe
    - Scarlet Arc Crescent Scythe
    - Heartreap Sawtooth Crescent Scythe
    - Voidreap War Scythe
    - Ebonhook Reaping Scythe
    - Riftfang War Scythe
    - Umbral Hand Scythe
    - Eclipse Crescent Scythe
    - Starless Razor Crescent Scythe
    - King's Harvest War Scythe
    - Oathkeeper's Reaping Scythe
    - Titanreaver War Scythe
    - Archreaper Hand Scythe
    - Seraph Arc Crescent Scythe
    - Leviathan Sawtooth Crescent Scythe

- `Swords.png`
  - 240x160 sheet, 16x16 sprites
  - Order: left-to-right, top-to-bottom
  - Sprite names:
    - Ashmark Arming Sword
    - Ironridge Bastard Sword
    - Fieldcleft Falchion
    - Scout's Shortsword
    - Squire's Longsword
    - Suncrest Longsword
    - Quarry Greatsword
    - Gilded Zweihander
    - Cleaveredge Greatsword
    - Needlepoint Rapier
    - Ragetooth Serrated Sword
    - Seamsteel Backsword
    - Harbor Sabre
    - Duneshade Scimitar
    - Crescent Khopesh
    - Riverguard Arming Sword
    - Copperbane Bastard Sword
    - Stonecut Falchion
    - Wayknife Shortsword
    - Banneret Longsword
    - Lioncross Longsword
    - Hearthsplit Greatsword
    - Crownwarden Zweihander
    - Slabbreaker Greatsword
    - Courtier Rapier
    - Sawjaw Serrated Sword
    - Greyline Backsword
    - Privateer Sabre
    - Sandrunner Scimitar
    - Pharaoh's Khopesh
    - Watchman Arming Sword
    - Steelhand Bastard Sword
    - Wolfbite Falchion
    - Sentinel Shortsword
    - Knightfall Longsword
    - Brightsteel Longsword
    - Bridgeguard Greatsword
    - Regal Dawn Zweihander
    - Headsman Greatsword
    - Glassneedle Rapier
    - Gnasher Serrated Sword
    - Ironspine Backsword
    - Seabreeze Sabre
    - Mirage Scimitar
    - Sunscar Khopesh
    - Emberwake Arming Sword
    - Cindergrip Bastard Sword
    - Firedusk Falchion
    - Ashpoint Shortsword
    - Flameward Longsword
    - Pyrekeeper Longsword
    - Cindermaul Greatsword
    - Phoenixcrest Zweihander
    - Furnacecleave Greatsword
    - Emberthread Rapier
    - Charfang Serrated Sword
    - Coalback Backsword
    - Redtide Sabre
    - Scorchwind Scimitar
    - Ashen Khopesh
    - Frostveil Arming Sword
    - Rimeclad Bastard Sword
    - Icehook Falchion
    - Snowstep Shortsword
    - Winterguard Longsword
    - Glacierbrand Longsword
    - Coldhammer Greatsword
    - Frostcrown Zweihander
    - Icebreaker Greatsword
    - Silverfrost Rapier
    - Rime-tooth Serrated Sword
    - Coldspine Backsword
    - Whitecap Sabre
    - Sleetcurve Scimitar
    - Polar Khopesh
    - Stormline Arming Sword
    - Thunderhand Bastard Sword
    - Lightning Falchion
    - Tempest Shortsword
    - Stormwarden Longsword
    - Skyforged Longsword
    - Thunderclap Greatsword
    - Stormking Zweihander
    - Boltcleaver Greatsword
    - Stormalis Rapier
    - Shocktooth Serrated Sword
    - Tempestspine Backsword
    - Squall Sabre
    - Sandstorm Scimitar
    - Tempest Khopesh
    - Duskhollow Arming Sword
    - Nightguard Bastard Sword
    - Umbral Falchion
    - Shadowstep Shortsword
    - Blackwatch Longsword
    - Moonlit Longsword
    - Nightfall Greatsword
    - Eclipse Zweihander
    - Gravecleave Greatsword
    - Midnight Rapier
    - Darktooth Serrated Sword
    - Nightspine Backsword
    - Nocturne Sabre
    - Nightwind Scimitar
    - Eclipse Khopesh
    - Bloodmark Arming Sword
    - Goregrip Bastard Sword
    - Redreap Falchion
    - Bloodedge Shortsword
    - Crimson Longsword
    - Hemlock Longsword
    - Bloodhammer Greatsword
    - Warlord Zweihander
    - Butcher Greatsword
    - Scarlet Rapier
    - Ripper Serrated Sword
    - Redspine Backsword
    - Corsair Sabre
    - Reddune Scimitar
    - Bloodmoon Khopesh
    - Voidkiss Arming Sword
    - Riftbound Bastard Sword
    - Nether Falchion
    - Voidpoint Shortsword
    - Starless Longsword
    - Astral Longsword
    - Blackhole Greatsword
    - Voidcrown Zweihander
    - Riftcleaver Greatsword
    - Starneedle Rapier
    - Nullfang Serrated Sword
    - Voidspine Backsword
    - Phantom Sabre
    - Voidwind Scimitar
    - Starfall Khopesh
    - Dragonseal Arming Sword
    - Titanhand Bastard Sword
    - Kingsplit Falchion
    - Royal Shortsword
    - Oathkeeper Longsword
    - Highlord Longsword
    - Titanbreaker Greatsword
    - Emperor's Zweihander
    - Colossus Greatsword
    - Grandduke Rapier
    - Kingsaw Serrated Sword
    - Crownspine Backsword
    - Admiral Sabre
    - Sultan's Scimitar
    - Dynasty Khopesh

### I2) Unique gear sprite sheet to ingest

- `UniqueGear.png`
  - 126x111 sheet, 16x16 sprites
  - Rows (left-to-right):
    - Row 1: Knight's Helmet, Viking Helmet, Roman Helmet, Roman Helmet, Roman Helmet, Warlock Hood, Knight's Chainmail Hood, Golden Crown
    - Row 2: Squire Armor, Knight's Armor, Barbarian Armor, Viking Armor, Gladiator Armor, [Empty Tile], [Empty Tile], [Empty Tile]
    - Row 3: Death Stompers, Silvertoe Boots, Ranger's Gloves, King's Crown, Cloak of Urgency, Cloak of Shadow, Cloak of the Dead, [Empty Tile]
    - Row 4: Ranger's Hood, Chain Hood, Gladiator Helm, Templar Helmet, Subjucated Crown, Diamond Helmet, [Empty Tile], [Empty Tile]
    - Row 5: Glowing Boots of Nimbleness, Knights's Boots, Gladiator's Boots, Templar's Boots, King's Boots, Diamond Boots, [Empty Tile], [Empty Tile]
    - Row 6: Knight Hip Guard, Leather Hip Guard, Templar Leggings, King's Leggings, Diamond Leggings, [Empty Tile], [Empty Tile], [Empty Tile]
    - Row 7: Roman Chestplate, Knight's Chestplate, Gladiator's Chestplate, Templar's Chestplate, King's Chestplate, Diamond Chestplate, [Empty Tile], [Empty Tile]

### I3) Loot table requirement (must implement)

- **All weapons and unique gears listed in Sections I1 and I2 must be added to the loot table.**
- Ensure they can appear in the appropriate drop pools (zone/level/rarity/crafting/vendor pools as applicable to your system).
- Ensure naming migrations (Section F) do not break references to these newly added items.

**Implementation notes (non-prescriptive):**
- Create item definitions for each new weapon/unique gear entry (name, item type, base template, sprite index/atlas ref, rarity behavior, affix eligibility).
- Confirm that:
  - Weapon items are affix-eligible per your weapon rules and rarity gating.
  - Unique gear is treated according to your “unique” item rules (fixed stats and/or affix behavior), but still participates in equip/unequip and stat formatting pipelines.
- Add targeted tests or assertions:
  - Loot tables contain entries for each listed item.
  - A deterministic seed-based loot roll can produce at least one of the new items (if your test harness supports it).

---

## J) Implementation checklist (suggested ordering)

1. Add/extend stat formatting utility and update all UI call sites (tooltip + Item Details).
2. Fix item-details stat list to only show present/non-zero stats and correct % conversion for (mult).
3. Implement equip/unequip stat aggregation with deterministic recomputation.
4. Implement gem socket stat transfer and removal.
5. Implement food rarity → regen scaling; remove potion rarity.
6. Add the new affixes to the registry.
7. Add/update base item templates (weapons/armor/offhand/jewelry).
8. Add rarity-tier gating + weighting table for affix generation.
9. Apply naming migrations and update all references.
10. Ingest new weapon and unique gear sprite sheets; add all new items to loot tables.
11. Add tests for:
    - formatting correctness
    - gating correctness
    - equip/unequip correctness
    - gem socket correctness
    - name migration correctness
    - loot table completeness for newly added items
