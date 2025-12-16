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
