## UnitPlan: Offensive Types and Builder Units

This plan describes how to extend **Project Aurora Zeta** to support multiple **Offensive Types** for heroes and future units, and how to scaffold RTS-style Builder units, mini-units, and buildings - while respecting the existing engine / game separation.

The intended consumer is Codex running with the repo loaded. All paths are relative to the project root.

---

## 0. Goals & Constraints

- Introduce **Offensive Types** as a game-layer concept:
  - `Ranged` (reclassify current behavior).
  - `Melee`.
  - `Plasma`.
  - `ThornTank`.
  - `Builder`.
- Apply an Offensive Type to every existing hero archetype.
- Keep the **engine layer** (`/engine`) generic; Offensive Types live under `/game`.
- Preserve existing **Ranged auto-fire** behavior as the default for `Ranged` units.
- Prepare the codebase for:
  - Builder units that construct **Turret**, **Barracks**, **Bunker**, and **House** buildings.
  - RTS-style selection and commanding of **mini-units**.
- Keep tuning values in data files under `/data`, not hardcoded.

Implementation is split into phases. Earlier phases should land first and stay small and buildable.

---

## 1. Core Data Model: Offensive Types (Game Layer Only)

### 1.1 Define OffensiveType enum

**Files**
- `game/components/OffensiveType.h` (new)

**Tasks**
- Create a game-layer enum for offensive types:

  ```cpp
  // game/components/OffensiveType.h
  #pragma once

  namespace Game {

  enum class OffensiveType {
      Ranged,
      Melee,
      Plasma,
      ThornTank,
      Builder
  };

  struct OffensiveTypeTag {
      OffensiveType type{OffensiveType::Ranged};
  };

  }  // namespace Game
  ```

- This header must **not** be included from `/engine`; it is purely game-layer.

### 1.2 Attach OffensiveType to hero archetypes

**Files**
- `game/Game.h`
- `game/Game.cpp`
- `data/menu_presets.json`

**Tasks**
- Extend `GameRoot::ArchetypeDef` in `game/Game.h` to carry an offensive type:

  ```cpp
  #include "components/OffensiveType.h"  // near other game includes
  ...
  struct ArchetypeDef {
      std::string id;
      std::string name;
      std::string description;
      float hpMul{1.0f};
      float speedMul{1.0f};
      float damageMul{1.0f};
      Engine::Color color{90, 200, 255, 255};
      std::string texturePath;
      Game::OffensiveType offensiveType{Game::OffensiveType::Ranged};
  };
  ```

- In `data/menu_presets.json`, add an `offensiveType` string field for each archetype entry:

  ```json
  {
    "id": "tank",
    "name": "Tank",
    "description": "...",
    "hpMul": 1.35,
    "speedMul": 0.9,
    "damageMul": 0.9,
    "color": [110, 190, 255, 255],
    "offensiveType": "Melee"
  }
  ```

  Suggested initial mapping (can be tuned later, but must be present for all):
  - `"tank"` → `"Melee"`
  - `"healer"` → `"Ranged"`
  - `"damage"` → `"Ranged"`
  - `"assassin"` → `"Melee"` (close-range burst)
  - `"builder"` → `"Builder"`
  - `"support"` → `"Ranged"`
  - `"special"` → `"Ranged"` (default)

- In `game/Game.cpp`, when loading archetypes from `menu_presets.json`, parse `offensiveType`:
  - Add a small helper function in an anonymous namespace or as a static function to map string → `Game::OffensiveType`:

    ```cpp
    inline Game::OffensiveType offensiveTypeFromString(const std::string& s) {
        if (s == "Melee") return Game::OffensiveType::Melee;
        if (s == "Plasma") return Game::OffensiveType::Plasma;
        if (s == "ThornTank") return Game::OffensiveType::ThornTank;
        if (s == "Builder") return Game::OffensiveType::Builder;
        return Game::OffensiveType::Ranged;
    }
    ```

  - When constructing `ArchetypeDef` from JSON, read `offensiveType` (default `"Ranged"` if missing) and set the field.

- Update the fallback archetype construction in `GameRoot::onInitialize` (where hard-coded archetypes are added if `menu_presets.json` is missing):
  - Extend the `addArchetype` lambda to accept a `Game::OffensiveType` parameter.
  - Use the same mapping listed above.

### 1.3 Attach OffensiveTypeTag to the hero entity

**File**
- `game/Game.cpp`

**Tasks**
- In `GameRoot::spawnHero()` (around where `HeroTag` and `Health` are added):
  - After constructing the hero entity and setting `Engine::ECS::HeroTag`, also emplace the `Game::OffensiveTypeTag` using the active archetype:

    ```cpp
    registry_.emplace<Game::OffensiveTypeTag>(hero_, Game::OffensiveTypeTag{activeArchetype_.offensiveType});
    ```

- Ensure `game/Game.cpp` includes `game/components/OffensiveType.h`.

### 1.4 (Optional, future) OffensiveType on enemies and other units

This plan focuses first on hero offensive types. A later step can add `OffensiveTypeTag` to spawned enemies or special units if desired, but it is not required for the initial implementation.

---

## 2. Behavior Wiring: Using Offensive Types in Combat

### 2.1 Identify the current “Ranged” behavior

Current behavior:
- In `game/Game.cpp` (primary update loop), the hero auto-fires projectiles in `GameRoot::onUpdate`:
  - Look for the block starting with `// Primary/auto fire spawns projectile toward target.` and `autoAttackEnabled_`.
  - This logic:
    - Finds a target direction (from mouse or nearest enemy).
    - Spawns a projectile entity with `Engine::ECS::Projectile` and `Engine::ECS::ProjectileTag`.
    - Uses `Engine::Gameplay::DamageEvent` with `DamageType::Normal`.

This entire block should be treated as the default **`Ranged` OffensiveType** behavior.

### 2.2 Route hero attack logic through OffensiveType

**File**
- `game/Game.cpp` (same auto-fire section)

**Tasks**
- Before computing `dir` / `haveTarget`, read the hero’s offensive type:

  ```cpp
  Game::OffensiveType heroOffense = Game::OffensiveType::Ranged;
  if (auto* ot = registry_.get<Game::OffensiveTypeTag>(hero_)) {
      heroOffense = ot->type;
  }
  ```

- Replace the direct attack logic with a small dispatch:

  ```cpp
  switch (heroOffense) {
      case Game::OffensiveType::Ranged:
          // existing projectile auto-fire logic (moved into a helper)
          break;
      case Game::OffensiveType::Melee:
          // call melee helper
          break;
      case Game::OffensiveType::Plasma:
          // call plasma helper
          break;
      case Game::OffensiveType::ThornTank:
          // call thorn helper (mostly handled on defense)
          break;
      case Game::OffensiveType::Builder:
          // builder may share Ranged or use abilities; see Section 4
          break;
  }
  ```

- To keep `onUpdate` readable, create small internal helpers (e.g. static lambdas or private methods on `GameRoot`) for:
  - `performRangedAutoFire(...)` (reusing the current implementation).
  - `performMeleeAttack(...)`.
  - `performPlasmaAttack(...)`.

### 2.3 Melee behavior (first-pass implementation)

Design targets:
- Melee units do **short-range high-armor, high-regen** combat.
- Implementation should reuse existing ECS + combat logic and avoid new engine types.

**Tasks**
- For heroes with `OffensiveType::Melee`:
  - Use the same targeting logic as Ranged (mouse or nearest enemy) to acquire a target direction and range.
  - If a valid target exists within a configurable **melee range** (data-driven), perform an instant melee hit instead of spawning a long-lived projectile.

Implementation details:
- Add melee tuning fields to `data/gameplay.json` under `"hero"` or a new `"melee"` section, for example:

  ```json
  "melee": {
    "range": 40.0,
    "arcDegrees": 70.0,
    "damageMultiplier": 1.3
  }
  ```

- In the melee helper:
  - Scan enemies in range using `registry_.view<Transform, Health, EnemyTag>()` similar to `CollisionSystem`.
  - For each enemy within `melee.range` and within the forward arc, apply damage via `Engine::Gameplay::applyDamage`:

    ```cpp
    Engine::Gameplay::DamageEvent dmg{};
    dmg.baseDamage = projectileDamage_ * meleeDamageMultiplier * rageDamageBuff_ * (hotzoneSystem_ ? hotzoneSystem_->damageMultiplier() : 1.0f);
    dmg.type = Engine::Gameplay::DamageType::Normal;
    ```

  - Do **not** spawn a projectile; just deal damage and spawn hit flashes / damage numbers as desired (can reuse patterns from `CollisionSystem`).
  - Optionally adjust melee heroes’ base `healthArmor` and `healthRegenRate` when spawning the hero (see Section 3).

### 2.4 Plasma behavior

Design targets:
- Plasma damage “melts shields” while using the existing armor/shield framework.

**Tasks**
- Implement plasma behavior as a specialized projectile variant:
  - Reuse the `Ranged` targeting and projectile spawn pipeline.
  - Modify the `DamageEvent` for plasma projectiles:

    ```cpp
    dmgEvent.type = Engine::Gameplay::DamageType::Normal;
    // Heavily favored vs shield-bearing targets:
    dmgEvent.bonusVsTag[Engine::Gameplay::Tag::Mechanical] = someValue;  // optional, if mechanical enemies are defined
    ```

- Additionally, consider a game-layer “shield shred” effect:
  - After dealing plasma damage, if the target still has shields, temporarily reduce `shieldArmor` or increase future damage.
  - This can be encoded via `Game::ArmorBuff` or a new buff type (see `game/systems/BuffSystem.h`).

Tuning:
- Add plasma-specific tuning to `data/gameplay.json`, e.g.:

  ```json
  "plasma": {
    "shieldDamageMultiplier": 1.5,
    "healthDamageMultiplier": 0.75
  }
  ```

Then apply these multipliers when building `DamageEvent` for plasma attacks.

### 2.5 ThornTank behavior (high-level)

Design targets:
- Thorn Tanks absorb incoming damage and reflect a portion back at attackers.

**Tasks**
- Treat ThornTank as primarily a **defensive modifier**, not a different primary attack:
  - Their offensive behavior can initially reuse Melee or Ranged (configurable later).
- Implement thorns as part of the damage-taking path:
  - In `game/systems/CollisionSystem.cpp`, inside the enemy → hero contact damage logic and projectile hit logic (if desired), detect if the hero has:
    - `OffensiveType::ThornTank`, or
    - A dedicated `Game::ThornTankTag` component.
  - When the hero takes damage, compute a reflected portion and apply it back to the attacker via an `Engine::Gameplay::DamageEvent`.

Tuning:
- Add thorn tuning under a `"thornTank"` section in `data/gameplay.json`:

  ```json
  "thornTank": {
    "reflectPercent": 0.25,
    "maxReflectPerHit": 40.0
  }
  ```

Reflection logic must:
- Use `DamageEvent` and `applyDamage` so armor & shields on the attacker are respected.
- Avoid recursion (do not reflect reflected damage).

---

## 3. Stat & Regen Adjustments by Offensive Type

To reflect the design (“High Armor - High regen melee characters…”), adjust the hero’s base stats at spawn based on OffensiveType.

**Files**
- `game/Game.cpp`
- `data/gameplay.json`

**Tasks**
- Extend `data/gameplay.json` with per-offensive-type stat modifiers, for example:

  ```json
  "offensiveTypes": {
    "Melee":   { "healthArmorBonus": 1.0, "shieldArmorBonus": 0.0, "healthRegenBonus": 2.0, "shieldRegenBonus": 0.0 },
    "Plasma":  { "healthArmorBonus": 0.0, "shieldArmorBonus": 0.0, "healthRegenBonus": 0.0, "shieldRegenBonus": 0.0 },
    "ThornTank": { "healthArmorBonus": 0.5, "shieldArmorBonus": 0.5, "healthRegenBonus": 1.0, "shieldRegenBonus": 0.5 },
    "Builder": { "healthArmorBonus": 0.0, "shieldArmorBonus": 0.0, "healthRegenBonus": 0.0, "shieldRegenBonus": 0.0 },
    "Ranged":  { "healthArmorBonus": 0.0, "shieldArmorBonus": 0.0, "healthRegenBonus": 0.0, "shieldRegenBonus": 0.0 }
  }
  ```

- During `GameRoot::onInitialize`, when hero baseline stats are loaded, also parse this `"offensiveTypes"` block into a small map (e.g., `std::unordered_map<OffensiveType, StatsModifier>` stored on `GameRoot`).

- In `GameRoot::spawnHero()`:
  - After retrieving the hero’s `Engine::ECS::Health` component and before calling `applyUpgradesToUnit`, apply the offensive-type stat modifier:

    ```cpp
    const auto& mod = offensiveTypeModifiers_[activeArchetype_.offensiveType];
    hp->healthArmor  += mod.healthArmorBonus;
    hp->shieldArmor  += mod.shieldArmorBonus;
    hp->healthRegenRate  += mod.healthRegenBonus;
    hp->shieldRegenRate  += mod.shieldRegenBonus;
    ```

- Ensure these modifiers are additive on top of values already loaded from `data/gameplay.json` and archetype multipliers.

---

## 4. Builder Unit: Buildings and Mini-Units (Scaffolding)

This section outlines how to prepare the codebase for Builder-specific content while staying incremental. The initial implementation can focus on **data structures and ECS components**, with more advanced behavior added later.

### 4.1 ECS components for buildings and mini-units

**Files**
- `game/components/Building.h` (new)
- `game/components/MiniUnit.h` (new)

**Tasks**
- Define simple ECS components to classify buildings and mini-units:

  ```cpp
  // game/components/Building.h
  #pragma once

  #include "../../engine/math/Vec2.h"

  namespace Game {

  enum class BuildingType {
      Turret,
      Barracks,
      Bunker,
      House
  };

  struct Building {
      BuildingType type{BuildingType::Turret};
      int ownerPlayerId{0};
      bool powered{true};
  };

  }  // namespace Game
  ```

  ```cpp
  // game/components/MiniUnit.h
  #pragma once

  namespace Game {

  enum class MiniUnitClass {
      Light,
      Heavy,
      Medic
  };

  struct MiniUnit {
      MiniUnitClass cls{MiniUnitClass::Light};
      int ownerPlayerId{0};
  };

  }  // namespace Game
  ```

- These components live purely in `/game` and can be combined with existing `Transform`, `Velocity`, `Health`, `EnemyTag`, etc.

### 4.2 Data-driven definitions for buildings and mini-units

**Files**
- `data/units.json` (new)

**Tasks**
- Create a new data file that defines tuning for buildings and mini-units:

  ```json
  {
    "miniUnits": {
      "light": {
        "hp": 40.0,
        "shields": 0.0,
        "moveSpeed": 180.0,
        "damage": 8.0,
        "offensiveType": "Ranged"
      },
      "heavy": {
        "hp": 120.0,
        "shields": 20.0,
        "moveSpeed": 120.0,
        "damage": 5.0,
        "offensiveType": "Ranged"
      },
      "medic": {
        "hp": 60.0,
        "shields": 10.0,
        "moveSpeed": 160.0,
        "damage": 2.0,
        "healPerSecond": 10.0,
        "offensiveType": "Ranged"
      }
    },
    "buildings": {
      "turret": {
        "hp": 150.0,
        "shields": 50.0,
        "armor": 1.0,
        "attackRange": 260.0,
        "attackRate": 1.2,
        "damage": 15.0
      },
      "barracks": {
        "hp": 200.0,
        "shields": 40.0,
        "armor": 1.0,
        "spawnInterval": 8.0,
        "maxQueue": 5
      },
      "bunker": {
        "hp": 260.0,
        "shields": 60.0,
        "armor": 2.0,
        "capacity": 4,
        "damageMultiplierPerUnit": 0.35
      },
      "house": {
        "hp": 120.0,
        "shields": 0.0,
        "armor": 0.0,
        "supplyProvided": 2
      }
    }
  }
  ```

- Add a loader in `game/Game.cpp` (or a dedicated `game/meta/UnitDefs.*` file) that parses this JSON into simple structs:
  - `MiniUnitDef` (id, `MiniUnitClass`, stats, OffensiveType).
  - `BuildingDef` (id, `BuildingType`, stats).

### 4.3 Builder supply system (Houses and max mini-units)

Design targets:
- Houses act as supply: each House increases max mini-units by 2, up to a maximum of 10.

**Tasks**
- Add simple supply tracking fields to `GameRoot` (or a dedicated system later):

  ```cpp
  int miniUnitSupplyUsed_{0};
  int miniUnitSupplyMax_{0};
  int miniUnitSupplyCap_{10};  // hard cap, configurable via data later
  ```

- When a `House` building is constructed:
  - Increment `miniUnitSupplyMax_` by the configured amount (e.g., 2), clamped to `miniUnitSupplyCap_`.
- When a mini-unit is spawned, increment `miniUnitSupplyUsed_`.
- When a mini-unit dies, decrement `miniUnitSupplyUsed_`.
- Before spawning a new mini-unit, ensure `miniUnitSupplyUsed_ < miniUnitSupplyMax_`.

All supply increments should be data-driven via `data/units.json` (`supplyProvided` per House).

### 4.4 Builder construction flow (high-level)

The exact UX and abilities for Builder heroes will be fleshed out later. For now, Codex should:
- Treat “construct building” as an ability or context action triggered by the Builder hero.
- Reuse the existing pattern from items and abilities:
  - Abilities are defined in `data/abilities.json` and executed in `game/GameAbilities.cpp`.

Suggested approach:
- Add new ability types for Builder in `data/abilities.json` under the `"builder"` key (mirroring how `"damage"` abilities are defined).
- In `GameRoot::buildAbilities()`, when the active archetype is `builder`, load builder-specific abilities that:
  - On activation, create a building entity at a chosen position (e.g., under the mouse or near the hero).
  - Use `Building` + `Health` + `Renderable` + `AABB` components.

Turret behavior:
- Reuse the existing `turrets_` vector and turret logic in `game/Game.cpp` as a reference.
- Over time, move this logic into a dedicated `TurretSystem` that:
  - Scans `Building` entities with type `Turret`.
  - Fires projectiles at nearest enemies using logic similar to hero auto-fire.

Barracks behavior:
- Introduce a `BarracksSystem` that:
  - Periodically spawns mini-units near a Barracks building, respecting supply.

Bunker behavior:
- Bunkers can initially be modeled as:
  - A building that can “store” references to nearby mini-units.
  - While garrisoned, mini-units are hidden (their entities may persist but be marked with a garrisoned flag).
  - Bunker’s attack output is multiplied based on the number of garrisoned units.

These systems do not need to be fully implemented immediately; the important part is that the ECS components and data layout support them cleanly.

---

## 5. RTS-Style Mini-Unit Control (Selection + Commands)

This section outlines how to integrate RTS-style controls for mini-units using the existing input and movement abstractions.

### 5.1 Selection state

**Files**
- `game/Game.h`
- `game/Game.cpp`

**Tasks**
- Add fields to `GameRoot` to track selected mini-units and selection rectangles:

  ```cpp
  std::vector<Engine::ECS::Entity> selectedMiniUnits_;
  bool selectingMiniUnits_{false};
  Engine::Vec2 selectionStart_{};
  Engine::Vec2 selectionEnd_{};
  ```

### 5.2 Input handling for selection

**Tasks**
- In `GameRoot::onUpdate`, reuse the existing mouse handling (near hero movement & pause UI) to add:
  - Left-click:
    - Click on a mini-unit → select that single unit.
    - Double-click on a mini-unit → select all on-screen mini-units of the same `MiniUnitClass`.
  - Click-and-drag with left mouse (holding) → draw a selection rectangle and, on release, select all mini-units in that rectangle.

Implementation hints:
- Use `Engine::Gameplay::mouseWorldPosition` plus `Camera2D` to map screen → world coordinates.
- Use `registry_.view<Transform, MiniUnit>()` to find mini-units whose `Transform.position` lies inside the world-space selection rectangle.
- Store the selection in `selectedMiniUnits_`.

### 5.3 Commanding mini-units (right-click move / attack-move)

**Tasks**
- In `GameRoot::onUpdate`, when a right-click occurs and there are selected mini-units:
  - Interpret this as a move/attack command target position.
  - For each selected mini-unit:
    - Set a target position field on a new component `MiniUnitCommand` (e.g., `desiredPosition` and `hasOrder`).

**Files**
- `game/components/MiniUnitCommand.h` (new)

```cpp
// game/components/MiniUnitCommand.h
#pragma once

#include "../../engine/math/Vec2.h"

namespace Game {

struct MiniUnitCommand {
    bool hasOrder{false};
    Engine::Vec2 target{};
};

}  // namespace Game
```

### 5.4 Movement and basic AI for mini-units

**Files**
- `game/systems/MiniUnitSystem.h` (new)
- `game/systems/MiniUnitSystem.cpp` (new)

**Tasks**
- Implement a simple system that:
  - Moves mini-units toward their `MiniUnitCommand::target` using the existing `Velocity` component.
  - Clears `hasOrder` when mini-units reach their destination.
  - Automatically attacks nearest enemies within a small radius (similar to hero auto-fire, but per mini-unit).

Implementation hints:
- Structure similar to `EnemyAISystem`:
  - `update(Registry&, TimeStep)` loops over entities with `MiniUnit`, `Transform`, `Velocity`, `Health`.
- For attacks:
  - Either:
    - Use projectiles like the hero (mini ranged units), or
    - Apply instant damage in a small radius for melee mini-units.

Wire-up:
- Instantiate `MiniUnitSystem` in `GameRoot` (similar to `EnemyAISystem` and `MovementSystem`).
- Call its `update` method each frame in `GameRoot::onUpdate`, after input is processed.

---

## 6. Applying Offensive Types to All Existing Characters

Once the above scaffolding is in place, **all existing hero archetypes** will have a valid `OffensiveType` via `ArchetypeDef`.

Checklist for Codex:
- [ ] `Game::OffensiveType` and `Game::OffensiveTypeTag` defined in `game/components/OffensiveType.h`.
- [ ] `ArchetypeDef` extended with `offensiveType`.
- [ ] `data/menu_presets.json` updated with `offensiveType` entries and parsed into `ArchetypeDef`.
- [ ] Fallback archetypes in `GameRoot::onInitialize` include offensive type defaults.
- [ ] `GameRoot::spawnHero()` attaches `OffensiveTypeTag` to the hero entity.
- [ ] Hero auto-attack logic in `GameRoot::onUpdate` dispatches on `OffensiveType`.
- [ ] Melee and Plasma behaviors implemented as described (even if ThornTank and Builder have minimal or placeholder behavior initially).

Once this is complete, any future character or mini-unit can simply set an `offensiveType` in data (or `OffensiveTypeTag` on an entity) and automatically pick up the correct attack behavior.

---

## 7. Notes for Future Iterations

- As Builder content grows, consider moving building and mini-unit logic into dedicated modules under:
  - `game/buildings/` for systems related to construction, supply, and garrisons.
  - `game/units/` for RTS-style unit behaviors.
- Keep **OffensiveType** strictly game-layer; do not introduce Zeta-specific enumerations into `/engine`.
- Document any newly implemented behaviors in:
  - `docs/GAME_SPEC.md` (heroes & combat section).
  - `docs/Ideas.md` (mark Offensive Types points as “implemented / partial” where applicable).

