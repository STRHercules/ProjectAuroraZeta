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

## 2025-12-16 — RPG combat/loot foundations + bio panel

**Prompt / Task**
- Implement the RPG combat + loot systems from `TASK.md` in a modular/data-first way.
- Add archetype biography UI and data plumbing.

**What Changed**
- Added engine-level RPG stat/attribute model, aggregation, RNG-shaped combat resolver with armor/resist mitigation, tenacity saving throws, and CC fatigue hooks.
- Introduced RPG content models (loot templates, affixes, talents, consumables, archetype bios) under `game/rpg/` with Luck-aware loot generator and JSON-driven bios.
- Character select summary card now shows biography, STR/DEX/INT/END/LCK, specialties, and perk blurbs sourced from `data/rpg/archetypes.json`.
- New unit tests cover armor curve, resist clamping, CC diminishing returns, and stat aggregation; build number bumped to v0.0.105.

**Steps Taken**
- Added `engine/gameplay/RPGStats.*` and `RPGCombat.*` for aggregation + resolver logic.
- Added `game/rpg/RPGContent.*` plus new data file `data/rpg/archetypes.json`; wired `loadMenuPresets` to consume bios.
- Expanded character select UI summary to render biography + attributes; kept existing layout intact.
- Created `tests/RPGSystemsTests.cpp` and registered `rpg_tests` target; updated docs/changelog/build string.

**Rationale / Tradeoffs**
- Kept RPG systems engine-agnostic; game layer only provides data/loot definitions and UI.
- Resolver uses simple, bounded formulas (armor curve, resist clamps, shaped rolls) to remain tunable while avoiding streaks.
- Biography panel reuses existing menu text rendering to avoid new UI dependencies.

**Build / Test**
- Build: `cmake --build build` (Linux, 2025-12-16).
- Tests: `./build/rpg_tests` (pass).

## 2025-12-16 — RPG resolver flag + JSON loot/talent tables

**Prompt / Task**
- Wire the new RPG combat resolver behind a feature flag and start externalizing loot/talent/consumable data to JSON with affix display in UI.

**What Changed**
- Added `useRpgCombat` and `rpgCombat` tuning in `data/gameplay.json`; collision system routes projectile/contact damage through the RPG resolver when enabled.
- Loaded RPG loot/consumables/talents from `data/rpg/*.json` (fallback to defaults) and added `useRpgLoot` flag; loot drops can roll affixes and show them in the inventory panel.
- Inventory UI now lists affix lines per item; archetype bios already displayed from earlier work.
- Build version bumped to v0.0.106.

**Steps Taken**
- Added RPG loaders and JSON tables, wired load on init, and seeded RPG RNG.
- Extended collision system with hybrid damage path using resolver + armor/resist clamps; kept legacy path when flag is off.
- Updated item definitions to carry affix text; drops use Luck-aware generator when `useRpgLoot` is true.
- Updated UI to render affix bullet lines; added new build/changelog entries.

**Rationale / Tradeoffs**
- Feature flags let us validate RPG resolver incrementally without breaking existing tuning.
- Loot affixes are visual-only for now; stat application will follow once inventory is plumbed into aggregation.
- Kept resolver mapping minimal (damage→attack power) to avoid large refactors before validation.

**Build / Test**
- Build: `cmake --build build` (Linux, 2025-12-16).
- Tests: `./build/rpg_tests`.

## 2025-12-16 — RPG live stat snapshots + DoT/Aura resolver + combat debug overlay

**Prompt / Task**
- Wire RPG stat aggregation into hero/enemy snapshots so affixes/talents modify live stats.
- Extend resolver integration to DoTs/auras/hotzones and add a combat debug overlay gated by a flag.

**What Changed**
- Added `Engine::ECS::RPGStats` and a shared `Game::RpgDamage::apply()` helper so the RPG resolver uses per-entity aggregated snapshots (and still falls back to legacy damage when disabled).
- RPG loot items now retain affix/template ids in inventory, and hero live stats aggregate those affixes plus auto-assigned RPG talent ranks (points every 10 levels).
- Routed burn/earth DoT ticks, wizard lightning bolt, lightning dome, and flame wall ticks through the RPG resolver; Warp Flux hotzone damage bonus now applies to flame wall ticks/burn.
- Added `combatDebugOverlay` flag in `data/gameplay.json` that shows a small on-screen panel of recent RPG resolver debug lines/scaling ratios.

**Steps Taken**
- Implemented `game/systems/RpgDamage.*` and updated `CollisionSystem`, `EnemyAISystem`, and relevant abilities/auras to use it.
- Extended projectiles with an owner field for better attacker snapshot selection and chained projectile ownership.
- Added RPG stat refresh on the hero each frame in RPG mode (syncing derived armor/max HP/shields back into `Health`).
- Updated build string, changelog, and gameplay config defaults.

**Rationale / Tradeoffs**
- Used a ratio-based mapping (derived power ÷ baseline power) so existing `DamageEvent.baseDamage` remains the tuning anchor while gear/talents scale it predictably.
- Talents are auto-assigned for now (no UI yet) to validate stat plumbing end-to-end before adding allocation screens.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-16).

## 2025-12-17 — Equipment drops + paper-doll + icons

**Prompt / Task**
- Implement the new equipment sprite sheets as RPG loot drops (weapons/armor/gems/runes/etc) that can be equipped and modify live RPG combat stats.
- Fix text overlap on the Select Character & Difficulty summary card.

**What Changed**
- Expanded `data/rpg/loot.json` to a full equipment loot table (167 item templates) covering `Gear.png`, `Gems.png`, `Runes.png`, `Tomes.png`, `Shields.png`, `Quivers.png`, `Melee_Weapons.png`, `Ranged_Weapons.png`, `Magic_Weapons.png`, `Arrows.png`, and `Scroll.png`.
- Added a paper-doll equipment container and click-to-equip UI in the Character screen; equipped items now contribute to RPG stat aggregation (inventory no longer does).
- Added icon metadata to item templates and render 16x16 equipment icons from the equipment sprite sheets in the equipment/inventory UI.
- Adjusted RPG loot generation to select templates from the rolled rarity tier and avoid duplicate affixes.
- Wrapped left-column text in the character select summary card so it doesn’t overlap the attribute column.

**Steps Taken**
- Added `scripts/gen_rpg_loot.py` to generate/update `data/rpg/loot.json` deterministically from the sheet layouts.
- Implemented equipment slot storage + equip/unequip helpers and hooked equipped stat contribution into the RPG snapshot update path.
- Updated menu UI wrapping and added equipment/inventory icon rendering.

**Rationale / Tradeoffs**
- Kept balancing/data in JSON (templates + affix pools) and used icons as template metadata to avoid hardcoding sprite coordinates in code.
- Reused the existing inventory/pickup pipeline so equipment drops don’t require a new entity type yet.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: set `useRpgLoot=true` + `useRpgCombat=true` in `data/gameplay.json`, start a run, pick up an item drop, press **I**, click the item to equip, verify stats (HP/shields/armor/damage scaling) change and icons render.

## 2025-12-17 — Tiered RPG drops + shop equipment offers

**Prompt / Task**
- Make new equipment actually show up in play: low-grade items from regular monsters, gems/high-grade from mini-bosses and bosses, and add random RPG equipment to the Traveling Shop.

**What Changed**
- Added tiered RPG equipment drop rules:
  - Regular monsters drop low-tier wooden/copper/basic/iron gear at a dedicated chance.
  - Mini-bosses drop multiple RPG items and often include a gem.
  - Bosses drop multiple high-grade RPG items.
- Traveling Shop now includes a configurable number of RPG equipment offers (random affixes/rarity) when `useRpgLoot` is enabled.
- RPG equipment pickups use the money bag sprite so they’re visible in-world.
- `data/gameplay.json` now defaults `useRpgLoot` to true and exposes tuning keys under `drops`/`shop`.

**Steps Taken**
- Implemented filtered loot pools for Normal/MiniBoss/Boss/Shop sources and a shared `rollRpgEquipment()` helper.
- Updated drop logic to spawn RPG equipment independently of the generic pickup chance logic.
- Updated shop refresh/render/purchase flow to support RPG items (name/affixes/icon and inventory delivery).

**Rationale / Tradeoffs**
- Kept “what drops where” data-tunable, but used simple name/sheet heuristics to avoid expanding the JSON schema again.
- Reused existing pickup visuals for clarity; icons remain in the UI to show the exact equipment art.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: start a run, kill regular enemies for wooden/copper drops, kill a bounty mini-boss for multiple drops (often a gem), kill a boss for high-grade drops, then open Traveling Shop and buy an RPG item to confirm it lands in inventory and can be equipped via **I**.

## 2025-12-17 — Modernized character inventory UI

**Prompt / Task**
- Modernize the inventory window: reduce empty space, use a grid layout, and add hover tooltips/highlighting for a more modern look.

**What Changed**
- Rebuilt the Character screen layout into a 3-column UI with:
  - Equipment cards (with rarity borders and clearer empty state),
  - A grid-based inventory (capacity-aware) with selection highlight,
  - A Details panel showing item info, RPG bonus breakdown, and affixes.
- Added a lightweight hover tooltip near the cursor for quick readouts.

**Steps Taken**
- Replaced the old list-based inventory rendering in `GameRoot::drawCharacterScreen` with a grid and detail/tooltip panels.

**Rationale / Tradeoffs**
- Kept UI fully in the existing immediate-mode renderer (no new UI framework) while improving readability and density.
- Bonus breakdown focuses on the RPG stats the current aggregation pipeline supports (AP/SP/Armor/HP/Shields/etc).

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: press **I**, hover inventory cells to see tooltip/details, click to equip/unequip and confirm stats update and highlighting behaves.

## 2025-12-17 — Fix equip/unequip crash in Character screen

**Prompt / Task**
- Crash occurs when attempting to equip items from the modernized inventory UI; fix it.

**What Changed**
- Deferred equip/unequip mutations until the end of `GameRoot::drawCharacterScreen` so hovered item pointers aren’t invalidated mid-frame.

**Rationale / Tradeoffs**
- The UI is immediate-mode; mutating `inventory_`/`equipped_` while still rendering tooltips/details can cause use-after-free. Deferring the action keeps the draw pass stable.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: press **I**, click to equip/unequip repeatedly while hovering items; verify no crash.

## 2025-12-17 — Inventory capacity + swap equip + layout fix

**Prompt / Task**
- Increase default inventory size to 24, make equip click swap when a slot is occupied, and fix equipment panel overlap with the stats panel.

**What Changed**
- Increased default inventory capacity to 24 and updated the inventory grid to reflect the new capacity.
- Equipment panel is taller so the bottom slots no longer collide with the Stats panel.
- Equipping an item into an already-occupied slot now swaps the equipped item with the clicked inventory item.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: fill inventory near cap, equip items into occupied slots repeatedly, confirm swaps occur without changing inventory size and all equipment slots remain visible.

## 2025-12-17 — Gem socketing + rune trinkets

**Prompt / Task**
- Gems should not be trinkets; they should be socketable and insertable into gear with empty sockets. Runes remain trinkets.

**What Changed**
- Added socket metadata to RPG item templates (`socketable`, `socketsMax`) and generated it into `data/rpg/loot.json`.
- Gems (`Gems.png`) are now marked socketable and are blocked from being equipped as trinkets.
- Equipment templates can provide gem sockets; socketed gems are stored on the item and contribute to RPG stat aggregation.
- Character screen supports socketing: hover an equipped item to set it as the socket target, then click a gem to insert it.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: obtain a weapon/gear with sockets and a gem, press **I**, hover the equipped item, click the gem, verify socket count updates in Details and derived stats change.

## 2025-12-17 — Rarity color-coding + equipment text alignment

**Prompt / Task**
- Raise equipment slot text to align with icons and avoid showing rarity as plain text like `[Uncommon]`; use rarity-based name colors instead.

**What Changed**
- Equipment slot name text is positioned higher to align with the icon row.
- RPG rarity suffixes are stripped from displayed names and names are color-coded:
  - Common: white, Uncommon: green, Rare: blue, Epic: deep purple, Legendary: gold.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: press **I**, verify equipped items and tooltips show colored names without bracketed rarity text.

## 2025-12-17 — Inventory drag/drop + gem socket drag

**Prompt / Task**
- Raise equipment-slot text a few pixels and add drag-and-drop in the inventory, including dragging gems onto socketable gear.

**What Changed**
- Equipment slot text positioning nudged upward for better alignment with the icon row.
- Added Character screen drag-and-drop:
  - Press and drag an inventory item; drop onto another inventory cell to swap.
  - Drag a gem onto an equipped item with open sockets to insert it.
- Click behavior is preserved by using a small drag threshold; click still equips/sockets as before.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: press **I**, drag inventory items to swap, drag a gem onto a weapon with sockets, confirm sockets fill and stats change.

## 2025-12-17 — Character screen text scale bump

**Prompt / Task**
- Increase the overall text scale on the Character/Inventory screen slightly.

**What Changed**
- Increased Character screen UI text scale from `1.12` to `1.16`.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Gear affects stats readouts (RPG default on)

**Prompt / Task**
- Equipped gear does not seem to affect the stats window readouts; confirm whether it’s UI-only or gameplay, and fix it.

**What Changed**
- RPG derived stats are kept up-to-date even when RPG combat is disabled (for UI preview).
- Stats panel now prefers RPG-derived values (HP/Shields/Damage/Attack Speed/Move Speed) when available.
- `data/gameplay.json` now defaults `useRpgCombat` to `true` so gear/affixes affect live combat by default.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: start a run, press **I**, equip an item with Attack Power/Move Speed bonuses and verify Stats values change; attack an enemy and confirm damage scales.

## 2025-12-17 — Character/Inventory window height increase

**Prompt / Task**
- Increase the overall height of the inventory/character GUI because some stats and gear details hang off the bottom.

**What Changed**
- Reduced screen-edge margins and increased the max panel height so the Character screen fits more content on 720p-ish viewports.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: start a run, press **I**, confirm Stats and the item details panel are fully visible without clipping.

## 2025-12-17 — Right-click to destroy unsocketed gems

**Prompt / Task**
- Right-clicking an equipped item that has socketed gems should remove one gem at a time, destroying it and freeing the socket.

**What Changed**
- Added Character screen right-click handling on equipped item cards to remove the last socketed gem (destroyed; socket freed).
- Stats refresh immediately after unsocketing so the UI reflects the change while paused.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: press **I**, socket 2+ gems into a weapon, then right-click the weapon repeatedly; verify gems are removed one-by-one and stats decrease accordingly.

## 2025-12-17 — Compare view + name truncation

**Prompt / Task**
- Add a compare view (equipped vs hovered) showing stat deltas before equipping.
- Add proper text measurement/truncation so long item names don’t clip in small equipment cards.

**What Changed**
- Added text measurement helpers and ellipsis truncation for equipment slot names.
- Added a compare section in the Character screen Details panel that previews derived stat deltas for equipping the hovered/selected inventory item.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: press **I**, hover a weapon/armor in inventory and verify “Compare (equip)” shows stat deltas; verify long names are truncated in the Equipment slot cards.

## 2025-12-17 — Character select preview sprite reposition

**Prompt / Task**
- On the Select Character & Difficulty screen, move the live preview sprites down ~100px and right ~50px; note where to adjust manually.

**What Changed**
- Adjusted the Character Select live preview sprite position offsets via `kPreviewOffsetX`/`kPreviewOffsetY`.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: open the main menu, go to **Solo** → **Select Character & Difficulty**, confirm the animated hero preview sits lower/right and no longer overlaps the summary area.

## 2025-12-17 — HUD resource bar text alignment

**Prompt / Task**
- In-match HUD: Health/Shields/Energy bar text is too far left; move Health right a good amount, Shields a lot, Energy a little.

**What Changed**
- Switched bar text centering to use real text measurement (TTF/bitmap) instead of character-count approximation.
- Added per-bar tweak constants (`kHpBarTextOffsetX`, `kShieldBarTextOffsetX`, `kEnergyBarTextOffsetX`) in `GameRoot::drawResourceCluster()`.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: start a run and confirm Health/Shields/Energy text is centered and no longer hugs the left side.

## 2025-12-17 — Shield bar text recenter

**Prompt / Task**
- Shield text is now way too far to the right; center it like the other bars.

**What Changed**
- Reset `kShieldBarTextOffsetX` to `0.0f` so Shield uses the same centering baseline as the other bars.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: start a run and verify the Shield bar label/value is visually centered.

## 2025-12-17 — Health bar text recenter

**Prompt / Task**
- Health text also needs to be centered like the other bars.

**What Changed**
- Reset `kHpBarTextOffsetX` to `0.0f` so Health uses the same centering baseline as the other bars.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: start a run and verify the Health bar label/value is visually centered.

## 2025-12-17 — Rename archetypes to character names

**Prompt / Task**
- Rename all the characters:
  - Tank (Knight) → Jack
  - Assassin → María
  - Builder → George
  - Damage Dealer (Militia) → Robin
  - Druid → Raven
  - Healer → Sally
  - Special (Crusader) → Ellis
  - Summoner → Sage
  - Wizard → Jade
  - Support (Dragoon) → Gunther

**What Changed**
- Updated archetype display names in `data/menu_presets.json` and `data/rpg/archetypes.json`.
- Removed gameplay logic that depended on old archetype *names* (uses ids instead) so renames don’t affect behavior.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: open **Solo** → **Select Character & Difficulty**, verify the list shows the new character names and the correct sprites/abilities still load per archetype id.

## 2025-12-17 — RPG consumables, bags, and loot visuals

**Prompt / Task**
- Implement missing RPG features: enemy RPG snapshots parity, resolver coverage for remaining damage sources, consumables (foods/potions), bag inventory expansion, and rarity-based drop visuals.

**What Changed**
- Added `Bag` and inventory-only `Consumable` RPG slots, plus new loot templates for `Foods.png`, `Potions.png`, and `Bags.png`.
- Added RPG consumable use (cooldown groups + instant heal + regen-over-time + simple move speed buff) driven by `data/rpg/consumables.json`.
- Added a Bag equipment slot that expands inventory capacity; inventory grid now scrolls and overflow items drop to the ground when capacity shrinks.
- Updated world pickup visuals:
  - RPG gear drops use `Containers.png` based on rarity.
  - Gems/bags/foods/potions show their own icon sheet cells.
- Seeded basic enemy RPG snapshots (ACC/EVA/attributes/base power) and routed hero melee + mini-unit attacks through the RPG resolver.

**Steps Taken**
- Extended RPG content models/loaders and regenerated `data/rpg/loot.json` via `scripts/gen_rpg_loot.py`.
- Added new drop tuning knobs in `data/gameplay.json` and hooked them into the enemy death drop logic.
- Built and verified compilation.

**Rationale / Tradeoffs**
- Kept enemy RPG seeding conservative (wave-scaled but small) to preserve legacy tuning while enabling shared combat rules.
- Kept consumable effects data-driven; the initial buff model is intentionally minimal (move speed only) to avoid hard-coupling to future stat/buff systems.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test:
  - Start a run, kill normal enemies until a food/potion drops; pick it up and press the Use Item key to consume.
  - Kill a mini-boss/boss and confirm consumables/bags can drop.
  - Equip a Bag and confirm inventory capacity increases; unequip it while over capacity and confirm overflow items drop to the ground.

## 2025-12-17 — Hotbar R/F + Bag slot layout

**Prompt / Task**
- Extend the Equipment section height to accommodate the Bag slot.
- Add two hotbar slots above the inventory grid; drag consumables into them; pressing `R` / `F` consumes.

**What Changed**
- Added two Character screen hotbar slots (R/F) that can be assigned by dragging eligible consumables from inventory.
- Wired hotbar activation to `R` and `F` (consumes the assigned item and clears the slot if the item is gone).
- Updated Equipment panel sizing so the Bag slot row fits cleanly without overlapping Stats.
- Updated default and `data/input_bindings.json` bindings so Reload is `F1` and `R/F` are reserved for hotbar.

**Steps Taken**
- Added new input actions/keys for hotbar slots and mapped SDL key events.
- Refactored the item-consume path to share logic between the hotbar and the existing Use Item action.
- Built to confirm compilation.

**Rationale / Tradeoffs**
- Hotbar stores inventory item ids (not indices) so it survives inventory reorder/scroll changes.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test:
  - Open the Character screen, drag a potion/food into Hotbar `R`, another into `F`.
  - Close the Character screen and press `R` / `F`; confirm the item is consumed and removed from inventory.

## 2025-12-17 — Character screen layout polish

**Prompt / Task**
- Raise the Inventory header slightly.
- Raise the Run section text slightly.
- Increase the overall Character/Inventory window height so the Details panel has more room.

**What Changed**
- Adjusted panel title offset for the Inventory panel and bumped the Run panel text baseline upward.
- Increased the Character/Inventory window max height from `760` to `820` (still clamped by viewport).

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: open the Character screen and verify Inventory/Run text alignment and that Details text no longer clips as often.

## 2025-12-17 — Launch maximized window

**Prompt / Task**
- Make sure the game always launches with the window maximized.

**What Changed**
- SDL window is created with `SDL_WINDOW_MAXIMIZED` so it starts maximized.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: launch the game and confirm the window starts maximized.

## 2025-12-17 — Ensure window starts maximized (WM-safe)

**Prompt / Task**
- Game still launched small; ensure it starts maximized reliably.

**What Changed**
- Added an explicit `SDL_MaximizeWindow` + `SDL_RaiseWindow` call after window creation to cover window managers that ignore `SDL_WINDOW_MAXIMIZED`.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: launch the game and confirm the window starts maximized.

## 2025-12-17 — Main menu 1080p layout + visual refresh

**Prompt / Task**
- After switching the default resolution to 1080p, center the main menu for a 1920x1080 layout:
  - Keep the Title at the top
  - Move Build info to bottom-right
  - Center all menu buttons
  - Keep “A Major Bonghit Production” centered at the bottom
- Modernize the main menu visuals (hover highlighting, cleaner layout).

**What Changed**
- Reworked main menu layout to use a 1920x1080 reference scale and center the button stack in the viewport.
- Moved build text to the bottom-right and the production credit to bottom-center.
- Added a subtle backdrop + a button stack container with hover/focus styling.
- Updated menu hover hitboxes to match the new button layout.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: launch the game, verify menu elements are positioned as requested at 1920x1080 and hover highlights track the buttons.

## 2025-12-17 — Character select screen modernization

**Prompt / Task**
- Give the “Select Character & Difficulty” screen a complete modernization makeover.
- Keep info readable, selection natural, and retain animated sprite previews.

**What Changed**
- Rebuilt the Character Select layout around a 1920x1080 reference scale:
  - Left/Right scrollable lists for Champions and Difficulty with modern selection styling.
  - Center card that hosts the animated hero preview plus readable hero/difficulty details.
  - Bottom action bar with larger Start/Back buttons and updated control hints.
- Updated mouse hitboxes for list selection and Start/Back to match the new layout.
- Kept the preview “manual tweak point” offsets (now relative to the center preview card).

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test:
  - Open Solo → Character Select; scroll and click champions/difficulties.
  - Verify the animated preview plays and Start/Back buttons are clickable and keyboard focus works.

## 2025-12-17 — Character select preview scale + centering

**Prompt / Task**
- Increase the sprite preview animation scale by ~200% and re-center it on its panel.

**What Changed**
- Doubled the requested preview scale target and added a safety clamp so the sprite stays centered and fully visible in the preview box.
- Reset the preview offset constants to `0` so the default is centered; manual tweak point remains in code.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: open Character Select and confirm the animated sprite is larger and centered within the preview card.

## 2025-12-17 — Character select readability (font scale)

**Prompt / Task**
- Increase the overall scale of the font on the Select Character & Difficulty screen.

**What Changed**
- Boosted Character Select text scaling and row heights so list entries and details are easier to read.
- Updated list click hitboxes to match the new row heights.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: open Character Select and verify the list items and details text are larger and still fit cleanly.

## 2025-12-17 — In-match HUD modernization pass

**Prompt / Task**
- Modernize the main in-game interface/HUD once a match starts, keeping it readable and unobtrusive.

**What Changed**
- Rebuilt the resource HUD into a top-left stacked panel (Health/Shields/Energy/Dash) with consistent modern styling.
- Consolidated intermission + status text (events/hotzones/warnings) into a compact status card beneath the resource panel.
- Gated the always-on FPS/dev HUD line behind `combatDebugOverlay` so it doesn’t clutter normal play.
- Updated the held-item/inventory badge visuals (icon + truncation + consistent container styling).

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test:
  - Start a run, verify resource bars render top-left and update smoothly.
  - Trigger intermission / hotzone / event and confirm the status card shows the messages.
  - Hover/cycle held inventory items and confirm the badge shows the icon and truncates long names.

## 2025-12-17 — Ability + Traveling Shop UI modernization

**Prompt / Task**
- Modernization overhaul of the Ability Shop and Traveling Shop GUIs (clean, modern, readable; keep UX tidy).

**What Changed**
- Rebuilt the Traveling Shop UI into a modern modal with Offers, Details (with explicit Buy CTA), and Inventory Sell (scrollable) panels.
- Rebuilt the Ability Shop UI into a modern modal with an upgrade list, details panel, and explicit “Buy Upgrade” CTA.
- Added consistent hover/selection styling and aligned hitboxes with the new layout.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test:
  - Interact with the shopkeeper to open Traveling Shop; hover/select offers and buy from the Buy button; sell items via the inventory list (scroll wheel works).
  - Press `B` to open Ability Shop; hover/select upgrades and buy from the Buy button; verify maxed/insufficient copper states render correctly.

## 2025-12-17 — Main menu animated starfield background

**Prompt / Task**
- Change the main menu background to an animated “star screensaver” (flat black + moving white dots), keeping menu layout intact.

**What Changed**
- Replaced the main menu’s static gradient/glow backdrop with a deterministic rotating starfield animation rendered behind the existing UI.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test: launch the game and verify the main menu shows a black starfield background while buttons/text remain unchanged.

## 2025-12-17 — RPG status integration + PRD shaping (v0.0.138)

**Prompt / Task**
- Implement missing RPG pieces:
  - Wire RPG resolver `onHitStatuses` into the existing engine status system.
  - Expand RPG consumable buffs beyond move-speed-only.
  - Activate elemental RPG damage types beyond Physical/Arcane/True.
  - Implement PRD-style anti-streak toggles (crit/dodge/parry).

**What Changed**
- RPG combat hits can now carry on-hit status intents via `Engine::Gameplay::DamageEvent` and apply results into `Engine::ECS::Status` (`StatusContainer`) with TEN saving throws + CC DR.
- `SpellEffect` element types now map into RPG damage types during projectile impact so resistances can matter for Fire/Frost/Shock/Poison/Arcane.
- RPG consumable Buff effects can be data-driven via `effects[].stats` contributions (with back-compat for `magnitude` move-speed buffs).
- Added PRD-style anti-streak option for crit/dodge/parry via `data/gameplay.json` (`rpgCombat.rng.usePRD`).

**Steps Taken**
- Extended `DamageEvent` with optional RPG damage type + on-hit statuses.
- Added per-entity `RPGCombatState` to persist CC fatigue + PRD accumulators.
- Updated RPG resolver to use PRD accumulator rolls when enabled.
- Updated Wizard attacks/auras to set RPG elemental damage types and route Stasis through the status pipeline.
- Updated docs/config/changelog and bumped in-game build string.

**Rationale / Tradeoffs**
- Kept RPG status IDs as int-cast `EStatusId` to avoid introducing a second status registry; `ZetaStatusFactory` remains the source of status specs.
- PRD uses a simple accumulator method for determinism + simplicity; can be replaced with another PRD curve later if needed.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Tests: `./build/rpg_tests`, `./build/combat_tests`, `./build/upgrades_tests`
- Manual test:
  - Start a run as Wizard and cast Lightning Bolt at a pack; confirm enemies receive Stasis and CC DR prevents perma-lock.
  - Turn on `combatDebugOverlay` and verify debug lines show crit/dodge/parry and mitigation fields updating.

## 2025-12-17 — Seeded world scenery (v0.0.139)

**Prompt / Task**
- Add sporadic scenery throughout the map, randomly placed based on the match seed, with partial collision so the player can walk behind tall sprites.

**What Changed**
- Added `data/scenery.json` to define scenery types, sizes, and per-sprite collider rectangles (data-driven).
- Spawns a spaced-out set of scenery props at run start using a deterministic RNG seeded per match (and synced via multiplayer StartMatch).
- Added y-sorted rendering so walk-behind visuals work naturally, and added hero-vs-scenery collision using `Engine::ECS::SolidTag`.

**Steps Taken**
- Introduced a dedicated match seed and a separate RNG stream for world/scenery placement.
- Implemented a simple rejection-sampling placer with minimum spacing, center clear radius, and edge margins.
- Modeled partial colliders as separate invisible entities so sprites can have multiple collision rectangles without needing a new collider component.

**Rationale / Tradeoffs**
- `TASK.md` (RPG systems) doesn’t cover scenery; implemented this feature independently and kept it data-driven via `data/scenery.json`.
- Chose y-sorted draw order (based on sprite “feet” Y) instead of hard layering so tall props naturally occlude the player only when appropriate.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).
- Manual test:
  - Start a run and verify scattered props appear with generous spacing.
  - Walk into trunks/rocks/bottom halves to confirm collision; walk “behind” the top halves to confirm correct occlusion.

## 2025-12-17 — Fog layering fix for y-sorted render (v0.0.139)

**Prompt / Task**
- Fix issue where fog overlay caused the hero/scenery/enemies to appear invisible depending on position.

**What Changed**
- Split world rendering into two passes (grid -> fog overlay -> entities) so fog shading doesn’t fully cover the player and sprites.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Fog culling + camera follow guard (v0.0.140)

**Prompt / Task**
- Fix reports of entities “vanishing” and scenery showing outside fog of war vision.

**What Changed**
- Applied fog culling to all non-hero entities (scenery/pickups/enemies) so they only render in visible fog tiles.
- Prevented accidental free-camera mode in Modern movement by forcing camera follow on the hero.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Fogged-tile prop rendering (v0.0.141)

**Prompt / Task**
- Keep scenery visible in explored (fogged) areas, not only within current vision.

**What Changed**
- Updated fog culling so non-enemy world props (including scenery) render in `Fogged` tiles, while enemies still require `Visible`.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Fog tint for explored scenery (v0.0.142)

**Prompt / Task**
- Scenery should remain visible in explored fogged areas, but dimmed under fog (not full brightness).

**What Changed**
- Added a fog-tint overlay for non-enemy world props when their tile state is `Fogged`.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Standard fog shading for scenery (v0.0.143)

**Prompt / Task**
- Scenery in explored fogged areas should be shaded by the same fog layer as everything else (no separate tint).

**What Changed**
- Removed the special-case fog-tint overlay on props and instead render the standard fog overlay above entities.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Fix zoom cull early-exit (v0.0.144)

**Prompt / Task**
- Fix bug where zooming in makes all sprites vanish (only ground remains).

**What Changed**
- Render culling now correctly `continue`s per-entity instead of returning early from the draw pass.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Full-map scenery distribution (v0.0.145)

**Prompt / Task**
- Scenery should populate the entire 512x512 play area, not only a small radius near the center.

**What Changed**
- `data/scenery.json`: `spawn.radius: 0` now means “distribute across full map bounds” (uniform over current world bounds).

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Denser, tree-heavy scenery (v0.0.146)

**Prompt / Task**
- Massively increase the amount of scenery on the map, leaning heavily towards trees.

**What Changed**
- Increased scenery spawn counts and reduced minimum spacing in `data/scenery.json`.
- Adjusted scenery weights to heavily favor `Tree.png` (with additional `BrokenTree.png`), while making rocks/cliffs rarer.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — NPC world collision (v0.0.147)

**Prompt / Task**
- Enemies, bosses, and escorts need to respect scenery collision.

**What Changed**
- Extended world collision resolution against `SolidTag` to enemies, bosses, and escort targets (in addition to the hero).

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Dirt patch floor variant (v0.0.148)

**Prompt / Task**
- Add `DirtPatch.png` (48x48) to the pool of floor textures used for ground tiling.

**What Changed**
- Added `assets/Tilesheets/DirtPatch.png` to the default grid texture list and fallback loader (also resolves `~/assets/...`).
- Weighted non-standard tile sizes (like 48x48) to be rare within the floor variant pool.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Larger floor variants + less streaking (v0.0.149)

**Prompt / Task**
- Fix 48x48 dirt patch appearing as a tiny tile, and reduce visible “lines” of repeated floor sprites.

**What Changed**
- Floor tiling now uses the smallest tile size as the base grid and draws larger variants (like `DirtPatch.png`) at their native size as sparse overlays.
- Improved the coordinate hash to avoid stripe artifacts when variant counts are small.
- Fog-of-war tile sizing now uses the smallest loaded floor tile size to stay stable when a larger variant is present.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Fix dirt patch overlay draw order (v0.0.150)

**Prompt / Task**
- DirtPatch floor overlays weren’t visible.

**What Changed**
- Floor rendering now draws base tiles first, then draws oversized tile overlays in a second pass so they aren’t overwritten.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-17 — Rarer dirt patches (v0.0.151)

**Prompt / Task**
- Make `DirtPatch.png` slightly more rare to avoid clumping.

**What Changed**
- Reduced floor overlay spawn chance for oversized dirt patches.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-17).

## 2025-12-18 — Background music routing (v0.0.152)

**Prompt / Task**
- Add new music tracks (BossMusic/DeathScreen/GameMusic/MainMenu) and play them at the appropriate times.
- Note: `TASK.md` is currently focused on RPG combat/loot systems; this change is an audio UX request.

**What Changed**
- Added an engine-level `Engine::Audio::MusicPlayer` wrapper (SDL2_mixer when available; safe no-audio fallback when not).
- Game now switches music automatically:
  - Main menu + submenus: `assets/Audio/Music/MainMenu.mp3`
  - In-run gameplay: `assets/Audio/Music/GameMusic.mp3`
  - Boss fights (any living `BossTag`): `assets/Audio/Music/BossMusic.mp3`
  - Defeat screen: `assets/Audio/Music/DeathScreen.mp3`

**Steps Taken**
- Added an optional CMake dependency on `SDL2_mixer` and a small engine audio wrapper.
- Hooked music selection into the main update loop so it reacts immediately to menu/defeat/boss state.

**Rationale / Tradeoffs**
- Kept audio engine-agnostic (`/engine/audio`) and only selected tracks in the game layer.
- Made SDL2_mixer optional to avoid breaking builds on environments without the dependency (music disables cleanly).

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-18).
- Manual test:
  - Launch the game: verify menu music plays.
  - Start a run: verify gameplay music plays.
  - For quick boss testing: set `boss.wave` to `1` in `data/gameplay.json`, start a run, and verify boss music.
  - Die and reach the defeat overlay: verify death screen music plays.

## 2025-12-18 — Core SFX routing (v0.0.153)

**Prompt / Task**
- Wire new weapon/footstep/pickup/UI sound effects from `assets/Audio/Sounds/...` to the relevant moments.

**What Changed**
- Added engine SFX support via SDL2_mixer (`engine/audio/SfxPlayer.*`) sharing the same backend as music.
- Hooked SFX events in game code:
  - Sword swing/impact/blocked/parry SFX for melee attacks.
  - Militia weapon swap SFX (sword sheath/unsheath, bow put-away/take-out).
  - Militia bow attack + impact/blocked SFX for ranged autos.
  - Dirt footsteps cycle while walking (no footsteps when idle).
  - Pickup vs heal-consume SFX (heal pickup + healing consumables use `ConsumeHeal.wav`).
  - Menu click/close SFX for menu navigation and closing overlays.
- Spells SFX are intentionally left unused for now (per request).

**Steps Taken**
- Added a shared audio backend (`engine/audio/AudioBackend.*`) to avoid double-initializing SDL2_mixer.
- Tagged militia bow projectiles with a `WeaponSfxTag` so collision hits can route bow impact/blocked sounds.

**Rationale / Tradeoffs**
- Kept sound asset selection in the game layer; engine layer only provides generic playback.
- Where the combat model lacks an explicit “block” state, “blocked” SFX uses dodge/glance/parry outcomes as a proxy.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-18).
- Manual test:
  - Main menu: click items -> select sound; backing out/closing overlays -> close sound.
  - Start a run and walk -> footsteps cycle.
  - Militia: swap weapon (`Left Alt`) -> sheath/unsheath and bow take out/put away.
  - Militia: fire bow -> bow attack; hit enemies -> bow impact; (with `useRpgCombat=true`) observe blocked/parry cases trigger “blocked” SFX.
  - Any melee hero: swing -> sword attack; hit enemies -> sword impact; (with `useRpgCombat=true`) observe dodge/parry/glance trigger blocked/parry SFX.

## 2025-12-18 — Options menu modernization + audio sliders (v0.0.154)

**Prompt / Task**
- Modernize the main menu Options page UI and add separate volume sliders for Music and SFX.

**What Changed**
- Rebuilt the Options page UI to match the newer menu styling (card layout, section headers, slider controls, toggle rows).
- Added Music and SFX volume sliders that update playback volume live.

**Steps Taken**
- Implemented slider interaction (hover focus, click-to-set, click-and-drag, and A/D step changes).
- Routed values into `Engine::Audio::MusicPlayer::setVolume()` and `Engine::Audio::SfxPlayer::setVolume()`.

**Rationale / Tradeoffs**
- Kept settings runtime-only for now (not yet persisted to disk) to minimize risk; can add save/config persistence next if desired.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-18).
- Manual test:
  - Main Menu -> Options: drag Music slider and confirm music volume changes.
  - Drag SFX slider and click menu items to confirm SFX volume changes.
  - Verify Esc returns to main menu and UI close SFX plays.

## 2025-12-18 — Options persistence + Upgrades UI revamp (v0.0.155)

**Prompt / Task**
- Fix Options screen text overlap/clipping, add a Background Audio toggle, and persist audio settings across restarts.
- Completely revamp and modernize the Upgrades screen to be coherent and aesthetically pleasing.
- Attempt to swap the UI font to `dungeon-mode.ttf` at 6px.

**What Changed**
- Persisted Options settings into the existing save file (`saves/profile.dat`):
  - Music volume, SFX volume, background-audio toggle, damage numbers toggle, screen shake toggle.
- Options screen:
  - Added Background Audio toggle (mutes audio when unfocused if disabled).
  - Reworked layout metrics and text fitting to avoid overlap/clipping (slider % stays inside the panel; long labels ellipsize).
- Upgrades screen:
  - Rebuilt into a modern two-panel layout (scrollable list + details panel).
  - Added Back/Buy buttons in a consistent footer area and a cleaner confirm modal.
- Font:
  - UI now attempts to load `data/dungeon-mode.ttf` at 6px and falls back to `data/TinyUnicode.ttf` at 6px.

**Steps Taken**
- Extended `SaveManager` schema (backward-compatible JSON fields) and wired load/save into `GameRoot`.
- Added a single `applyAudioVolumes()` path that applies user volumes and handles background mute based on window focus.
- Replaced the legacy Upgrades table UI with a scrollable list/details presentation.

**Rationale / Tradeoffs**
- Settings persist via the existing obfuscated save file to avoid introducing a separate config format.
- Background Audio is implemented as “mute when unfocused” (not “keep playing”) to match streamer-friendly expectations.

**Build / Test**
- Build: `cmake --build build -j 8` (Linux, 2025-12-18).
- Manual test:
  - Main Menu -> Options: adjust Music/SFX and restart the game; verify values persist.
  - Toggle Background Audio off, alt-tab away; verify music/SFX mute until focus returns.
  - Main Menu -> Upgrades: scroll list, select entries, buy with confirm/cancel, and verify vault gold updates.
