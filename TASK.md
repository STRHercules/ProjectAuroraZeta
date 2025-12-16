# TASK.md — Implement RPG Combat + Loot Systems (Modular)

## Objective
Implement the kept RPG systems for **Project Aurora Zeta** in a modular, data-driven way so players and enemies share the same combat rules:
- Attributes → Derived stats → Combat resolution → Damage mitigation → Status effects → Loot/equipment → Talents → Biography UI.
- **No static damage**: all damage uses controlled variance with optional anti-streak logic.

## Scope (Keep These Sections)
- Core design goals
- Primary attributes (STR/DEX/INT/END/LCK)
- Derived stats (AP/SP/AS/MS/ACC/EVA/CRIT/ARP/ARM/RES/TEN + optional shields)
- Combat resolution model (hit quality → dodge/parry → crit → roll band → mitigation → shields → on-hit)
- Damage types + resistances/vulnerabilities
- Status effects + CC diminishing returns + “saving throw” style application
- Resources/cooldowns/action economy
- Equipment + inventory + paper-doll + requirements/proficiencies
- Loot generation (rarity + affixes + optional sockets/sets/uniques as stubs)
- Consumables (healing/cleanse/buffs/bombs/food)
- Talent trees (match-permanent, points every 10 levels)
- Archetype biography panel (UI + optional background perks)
- RNG shaping (PRD-ish + caps/floors + telegraph spikes)
- Implementation notes + C++-friendly damage event pipeline

## Non-Goals (Explicitly Out of Scope Unless Requested)
- Full crafting/upgrading economy (salvage/enchant/reroll) beyond stubs
- Co-op aggro/threat system (only add hooks if architecture wants it)
- Full content authoring (tons of items/talents); focus on systems + examples

---

## Deliverables
1. **Data model** for attributes, derived stats, resistances, resources, and status effects (shared by players and AI).
2. **Combat resolver** that produces deterministic “HitOutcome” style results and emits a combat log/debug stream.
3. **Equipment system** with paper-doll slots, requirements/proficiencies, affix application, and stat aggregation.
4. **Loot generator** that rolls rarity + base stats + affixes (and optional stub hooks for sockets/sets/uniques).
5. **Consumables** framework (cooldowns + effects: heal, cleanse, temp buffs, bombs).
6. **Talent tree** framework per archetype, awarding talent points every 10 levels, applying match-permanent bonuses.
7. **Archetype biography UI panel** (Name, Bio, Attributes, Specialties) + optional perk slot (can be lore-only).
8. **RNG shaping layer** (roll bands, shaped distributions, optional PRD/bad-luck protection toggles).
9. **Configuration/feature flags** to disable individual modules without refactoring core code.

---

## Architecture Rules (Must Follow)
- Keep engine-agnostic systems in `/engine`.
- Keep game-specific archetypes/items/talents/data in `/game`.
- No hard coupling between game content and engine systems: use interfaces/data tables.

---

## Implementation Plan

### 1) Define Core Types
Create/confirm the following core structs/classes (names are suggestions; adapt to existing style):

- `Attributes { int STR, DEX, INT, END, LCK; }`
- `DerivedStats`
  - Offense: `AP, SP, AS, ACC, CritChance, CritMult, ARP`
  - Defense: `EVA, ARM, TEN, ShieldMax, ShieldRegen, ...`
  - Resistances: map or fixed array by `DamageType`
  - Utility: `MS, CDR, ResourceRegen, GoldGainMult, RarityScore`
- `Resources` (mana/stamina/energy/ammo/heat as configured)
- `DamageType` enum (Physical, Fire, Frost, Shock, Poison, Arcane)
- `StatusId` enum + `StatusDef` (duration, stacking, tick, counters, tags)
- `AttackDef` (baseDamage, type, rollMin/Max, baseHitChance, baseGlanceChance, glanceMult, isMagic, onHitStatus list, etc.)
- `HitOutcome` (missed/glanced/crit/dodged/parried/blocked, finalDamage, appliedStatuses, etc.)
- `DamageEvent` (attackerId, defenderId, attackId, snapshots, rngContext, timestamps)

**Acceptance:** these types compile, serialize (if needed), and are usable for both player and AI.

---

### 2) Stat Aggregation Pipeline
Implement a single function to build final stats for any entity:
- Inputs:
  - base stats (level, archetype base values)
  - attributes (STR/DEX/INT/END/LCK)
  - gear (base item stats + affixes)
  - talent bonuses
  - temporary buffs/status modifiers
- Output:
  - `DerivedStats` snapshot used for combat resolution

**Rules**
- Keep formulas simple and tunable.
- Provide constants in a config header or data file (e.g., `K` for armor curve, caps).

**Acceptance**
- Changing STR/DEX/INT/END/LCK predictably changes AP/SP/AS/MS/MaxHP/Armor/etc.
- Gear and talents modify the same derived stats without special-case hacks.

---

### 3) Combat Resolver (Single Source of Truth)
Implement `ResolveHit(attacker, defender, AttackDef, RNG) -> HitOutcome` with this order:

1. **Hit Quality** (ACC vs EVA):
   - Resolve MISS / GLANCE / HIT
2. **Defensive Reaction**:
   - Dodge: if success → 0 damage, stop
   - Parry: if success → stop damage; emit reflect event (handled separately; reflect cannot be parried again in same frame/event)
3. **Crit**:
   - roll CritChance; apply CritMult
4. **Damage Roll Band**:
   - roll multiplier in [rollMin, rollMax]
   - shaped distribution: average of 2 rolls (configurable)
5. **Mitigation**:
   - Armor DR using curve: `ARM/(ARM+K)` (cap)
   - ResistPct for damage type, clamped to [-0.75, 0.90]
   - Apply ARP before armor if applicable
6. **Shields/Barrier** (optional but supported):
   - subtract from shield first
7. **On-hit Effects**:
   - attempt status application using “saving throw” rule (TEN reduces chance and/or duration)
   - apply CC diminishing returns (fatigue stack)
8. Emit `DamageEvent` + debug info for overlay/log.

**Acceptance**
- Same function works for player vs monster, monster vs player, boss vs boss.
- No static damage: identical attacks produce varied outcomes within bounded ranges.
- Debug output clearly shows: hit quality, dodge/parry, crit, roll mult, mitigation, final damage.

---

### 4) Status Effects + CC System
Implement:
- Status registry: `StatusDef` includes
  - duration, stack rules, tick interval, intensity
  - tags (CC, DoT, Buff, Debuff)
  - counters/cleanse category
- “Saving throw” style application:
  - `applyRoll < baseChance - TENBonus` OR `duration *= (1 - TENScale)`
- CC diminishing returns:
  - apply CC fatigue for X seconds
  - next CC duration multiplier: 0.7 → 0.5 → brief immunity
- Ensure existing special statuses align:
  - Armor Reduction, Blindness, Cauterize, Feared, Silenced, Stasis, Cloaking, Unstoppable

**Acceptance**
- CC cannot perma-lock a target.
- Cleanse consumable removes appropriate categories.
- Unstoppable prevents stuns/slows/most speed-impairing effects (as defined).

---

### 5) Resources + Action Economy
Add resource plumbing (as configured):
- Mana/Stamina/Energy/Ammo/Heat
- Regen rates, max values, spend costs on abilities
- Cooldowns + cast/wind-up + recovery + interrupts
- Optional “concentration”: only one maintained buff at a time

**Acceptance**
- Abilities can be limited by cooldown and/or resource.
- Interrupts cancel casts consistently.

---

### 6) Equipment + Inventory + Paper-Doll
Implement:
- Inventory container (list/grid)
- Equipment slots:
  - Head, Chest, Legs, Boots
  - Mainhand, Offhand
  - Ring1, Ring2, Amulet
  - Trinket, Cloak (optional)
- Requirements/proficiencies:
  - stat requirements (STR/DEX/INT thresholds)
  - proficiency lock or penalty (choose one approach; keep configurable)
- Equipping updates entity stats via aggregation pipeline.

**Acceptance**
- Equipping an item immediately changes derived stats.
- Invalid equips are prevented or penalized consistently.

---

### 7) Loot Generation
Implement `GenerateLoot(dropContext) -> Item`:
- Roll rarity tier: Common/Uncommon/Rare/Epic/Legendary
- Apply rarity-scaled base stats (e.g., armor scaling)
- Roll 0–3 affixes based on rarity:
  - examples: +Attack, +AtkSpeed, +Crit%, +FireRes, +LifeSteal, +Tenacity, +GoldGain
- Optional stubs (hooks only, not full systems unless asked):
  - sockets/gems
  - sets
  - uniques (rule-changing effects)

Luck (LCK) impacts:
- gold gain multiplier
- rarity score / bonus roll chances (configurable)

**Acceptance**
- Higher rarity items have higher base stats and/or more affixes.
- LCK visibly impacts gold and drop quality over many drops.

---

### 8) Consumables
Implement consumables with:
- shared cooldown categories (e.g., potion cooldown)
- effect types:
  - heal
  - cleanse (category-based)
  - temporary buff (haste/armor/crit)
  - bombs/traps (AoE damage/status)
  - food buffs (long duration, small effect)

**Acceptance**
- Consumables work for all archetypes.
- Cleanses remove expected debuffs.

---

### 9) Talent Trees (Match-Permanent)
Implement:
- per-archetype talent tree definition (data-driven)
- talent points every **10 levels**
- talents apply **match-permanent** bonuses to derived stats and/or mechanics:
  - e.g., “Dodging grants brief haste”
  - “Parry reflects more + applies short stun (save check)”
  - “Cloak duration +0.5s”

**Acceptance**
- Leveling grants points at correct intervals.
- Talent bonuses stack correctly with gear and attributes.

---

### 10) Archetype Biography Panel (UI)
On archetype selection screen:
- left-side panel shows:
  - Name
  - Biography
  - Attributes (STR/DEX/INT/END/LCK)
  - Specialties (weapons/playstyle tags)
- Optional background perks:
  - allow 0–2 small perks per archetype (can be lore-only by default)

**Acceptance**
- UI panel renders and updates when switching archetypes.
- Data is driven from archetype definitions (not hardcoded in UI).

---

### 11) RNG Shaping (Anti-Troll)
Implement configurable RNG controls:
- Damage roll band: e.g., [0.85, 1.15]
- Shaped distribution toggle: average of 2 rolls
- Optional PRD/bad-luck protection toggles for:
  - crit
  - dodge
  - loot rarity
- Caps:
  - dodge cap (e.g., 70%)
  - crit cap (e.g., 80%)
- Boss spike handling:
  - ensure high-impact attacks are telegraphed (animation/indicator hook)

**Acceptance**
- No extreme streaks in normal play when PRD is enabled.
- Turning off shaping reverts to pure RNG without code changes.

---

## Test & Debug Requirements
- Add a **combat debug overlay/log** that can display:
  - roll results, crit/dodge/parry states
  - mitigation values (armor DR, resist pct)
  - final damage
- Add deterministic mode for debugging:
  - seedable RNG per match
- Add basic unit tests (or a test harness scene) for:
  - armor curve correctness
  - resist clamp behavior
  - CC diminishing returns
  - equipment stat aggregation

---

## Acceptance Criteria (Definition of Done)
- Players + enemies share one combat resolver and one stat aggregation path.
- Damage is never static; variance is bounded and optionally shaped.
- Dodge/parry/cloak work and have clear counterplay hooks (detection, AoE, break-on-hit).
- Damage types + resists + armor mitigation produce predictable outcomes.
- Status effects support saving throws + CC DR.
- Equipment + loot generation works end-to-end (drop → equip → stats update).
- Talents apply match-permanent bonuses and grant points every 10 levels.
- Biography panel displays archetype info correctly.
- Debug logging makes combat explainable and tunable.

