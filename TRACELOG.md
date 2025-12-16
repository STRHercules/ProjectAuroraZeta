## 2025-12-16 — Boss interval + size cap

**Prompt / Task**
- A giant Boss spawns on Wave 20, they should spawn every 20 Waves. Cap boss scale to avoid massive sprites at high waves.

**What Changed**
- Added boss interval + max size config, repeated boss spawns every 20 waves.
- Clamped boss visual size to prevent runaway scaling.
- Exposed interval/maxSize in gameplay config; documented in GAME_SPEC.
- Bumped build string to v0.0.103.

**Steps Taken**
- Updated WaveSystem to track next boss wave and cap boss size.
- Threaded new boss interval/maxSize settings through Game config load and WaveSystem setup.
- Updated gameplay.json defaults and GAME_SPEC milestone description.
- Built target `game`.

**Rationale / Tradeoffs**
- Interval handled in wave system to stay authoritative to spawn logic.
- Size cap applied at render/creation point to preserve hitboxes with consistent proportions.
- Max size default (110u) chosen to keep sprites readable while still larger than elites.

**Build / Test**
- Build: `cmake --build build --target game -- -j4` (Linux, 2025-12-16) — success.
- Manual: not run (scope limited to spawn logic/config update).

## 2025-12-16 — README systems expansion

**Prompt / Task**
- Expand the README to include all current functions, systems, logic.
- Include all current character archetypes, their playstyle, abilities, etc.
- Include information about enemies, bosses, Mini-Units, Summoner Summons.
- Go into detail about all of that. Include all available hotkeys, and include detail about multiplayer.

**What Changed**
- Rebuilt README with detailed systems snapshot (waves, hotzones, events, economy, combat, energy).
- Added archetype table, ability kits, mini-units/summons/structures, enemy/boss/event notes, hotkeys, and multiplayer flow.
- Linked each section to current data sources and kept existing build/run steps.

**Steps Taken**
- Reviewed `data/menu_presets.json`, `data/abilities*.json`, `data/gameplay.json`, `data/units.json`, `data/input_bindings.json`, and core systems (WaveSystem, EventSystem, MiniUnitSystem, NetSession).
- Authored expanded README sections and checked formatting.
- Ran `cmake --build build --target game -- -j4` after the documentation change.

**Rationale / Tradeoffs**
- Centralizes player- and contributor-facing information in one up-to-date place.
- Mirrors live data to avoid drift; avoided speculative or unimplemented features.

**Build / Test**
- Build: `cmake --build build --target game -- -j4` (Linux, 2025-12-16) — success.
- Manual: not run (doc-only change).

## 2025-12-16 — Status framework + mini-map HUD

**Prompt / Task**
- Implement TASK.md: add status/state system (8 effects, stacking, immunities) and a HUD mini-map.

**What Changed**
- Added engine-level status types/container/component plus game-side `ZetaStatusFactory` loading `data/statuses.json`.
- Integrated status checks into damage, regen, ability casting, movement, AI target choice, and rendering (cloaking/stasis culling).
- Added debug hotkeys (F1–F4 apply to nearest enemy; F6/F7/F8/F10 to hero; F9 clear) for testing statuses.
- Introduced top-right mini-map overlay centered on player with clamped enemy blips; hides cloaked/stasis enemies.
- Updated README (HUD bullet), CHANGELOG v0.0.104, suggestions, and bumped build string.

**Steps Taken**
- Implemented status enums/specs/container with immunity logic and armor/vision/movement magnitudes.
- Created status ECS component, wired spawning (hero/enemy/mini-units), ticked status updates each frame, and gated regen/casting/movement.
- Updated collision/melee/remote combat damage to honor stasis, armor reduction, and damage multipliers; added AI fear/blind handling and cloaking target filters.
- Added `MiniMapHUD` renderer and hooked it into the render path; collected enemy positions and filtered cloaked/stasis units.
- Added tests for status stacking/expiry and negative-armor damage; refreshed docs and changelog.

**Rationale / Tradeoffs**
- Kept engine status generic; game magnitudes live in JSON for tuning without rebuilds.
- Debug hotkeys provide acceptance coverage until real content applies statuses.
- Mini-map uses existing RenderDevice primitives to avoid new dependencies.

**Build / Test**
- Build: `cmake --build build --target game -- -j4` (Linux, 2025-12-16).
- Tests: `ctest --output-on-failure` (no tests discovered in current CTest config).
## 2025-12-16 — README: Traveling Shop, Pickups, Global Upgrades

**Prompt / Task**
- Extend the README detailing the Traveling Shop and its contents.
- Add a section about all powerups/pickups/items.
- Add information about the global upgrades.
- Include anything else relevant that was missing.

**What Changed**
- Added three README sections: Traveling Shop (gold flow, unlock, prices, stock), Powerups/Pickups/Items (effects, drop sources, use rules), and Global Upgrades (costs, caps, modifiers).
- Clarified gold sources (boss/event/bounty), revive pickup, and item stack caps.

**Steps Taken**
- Read `game/Game.cpp`, `game/meta/ItemDefs.cpp`, `game/meta/GlobalUpgrades.*`, `data/gameplay.json`, and pickup logic to align values.
- Documented dynamic prices and caps from `effectiveShopCost` and gold catalog.
- Updated README with the new sections.

**Rationale / Tradeoffs**
- Keep player/contributor docs in sync with live data-driven values.
- Document stack caps and pricing to reduce confusion around gold vs. copper economies.

**Build / Test**
- Not run (documentation-only change).
