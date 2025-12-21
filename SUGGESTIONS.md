## 2025-12-16 — Suggestions after Boss interval + size cap
- Add UI banner text that names boss wave numbers when repeating (e.g., "Wave 40 Boss") for clarity.
- Consider scaling boss HP with interval count (e.g., +X% each milestone) separately from regular wave scaling to keep pace with player power.
- Add config-driven per-boss archetype overrides to vary visuals/attacks on later milestones.

## 2025-12-16 — Suggestions after README systems expansion
- Add an in-game codex/help screen sourced from the new README sections so players can read archetypes, hotkeys, and event rules without alt-tabbing.
- Surface control rebinding in UI (or at least a menu link that opens `input_bindings.json`) to match the documented defaults.

## 2025-12-16 — Suggestions after Traveling Shop & pickups docs
- Add an in-game tooltip explaining gold vs. copper and showing current gold wallet in the HUD during intermission.
- Surface stack caps and current stacks for Traveling Shop effects in the shop UI (e.g., “1/2 Cataclysm Rounds”).
- Consider lowering gold price numbers or increasing gold income for early testing so players can reasonably buy a legendary per boss cycle.

## 2025-12-16 — Suggestions after status system + mini-map
- Surface active statuses on the HUD (icons with remaining duration/stacks) so players can see Silence/Stasis/Unstoppable.
- Drive status magnitudes fully from data and wire real gameplay sources (abilities, enemy attacks) into the new factory instead of debug-only hotkeys.
- Add boss/elite markers and player facing arrow on the mini-map, and allow zoom radius tuning via config.

## 2025-12-16 — Suggestions after RPG combat/loot foundation
- Replace existing damage flow with the new resolver end-to-end (projectiles/melee/hotzones) behind a feature flag to validate parity.
- Externalize loot/talent/consumable tables to JSON and pipe drops/equipment into the aggregation pipeline; add UI readout of rolled affixes.
- Add deterministic seed toggle in options to drive RNG shaping/PRD and expose a lightweight combat debug overlay for hit breakdowns.

## 2025-12-16 — Suggestions after RPG live stat snapshots + debug overlay
- Add a real equipment/paper-doll layer so only equipped RPG loot contributes to aggregation (instead of all inventory items).
- Add a small talent allocation UI (with save/load) and expose the “points every 10 levels” rule in the character screen.
- Extend enemy definitions to optionally carry RPG attributes/resists/tenacity so elites/bosses can have distinct mitigation profiles.

## 2025-12-17 — Suggestions after Equipment drops + paper-doll + icons
- Render dropped equipment pickups using their actual icon region (plus rarity-colored glow) so item drops are readable in-world.
- Add an item details tooltip panel (base stats + rolled affixes + slot) on hover in the Character screen.
- Add basic inventory quality-of-life: sort by slot/rarity, quick-equip best-in-slot, and a “sell junk” button for Common items.

## 2025-12-17 — Suggestions after Tiered RPG drops + shop equipment offers
- Add explicit drop pools/tiers to `data/rpg/loot.json` (instead of name matching) so content authors can control “where this can drop” precisely.
- Add a separate currency field for inventory items so selling/pickup conversions don’t mix gold/copper semantics.
- Allow shop card removal after purchase (and/or “reroll shop” cost) to make gold shop behavior clearer.

## 2025-12-17 — Suggestions after Modernized character inventory UI
- Add a compare view (equipped vs hovered) showing stat deltas before equipping.
- Add quick actions: right-click to equip/unequip, shift-click to sell (when shop is open), and sort/filter buttons.
- Add proper text measurement or truncation helpers so long item names don’t clip in small equipment cards.

## 2025-12-17 — Suggestions after Gem socketing + rune trinkets
- Add a dedicated “Socket” mode (drag-and-drop gems onto gear) and allow socketing items while they’re still in inventory (not just equipped).
- Expose socket counts and gem tags explicitly in `data/rpg/loot.json` instead of inferred rules.
- Add unsocket/remove rules (e.g., destroy gem, pay copper, or salvage gem) and a UI button per socket.

## 2025-12-17 — Suggestions after gear stat readout fix
- Add a small in-game toggle for `useRpgCombat` (Options) and show a short explanation tooltip on the Character screen.
- Add “Compare” deltas (hovered vs equipped) so players can see exactly what equipping will change.

## 2025-12-17 — Suggestions after window height increase
- Add a simple scroll area for Stats/Details on very small resolutions so content never clips even if more lines are added later.

## 2025-12-17 — Suggestions after gem unsocket right-click
- Consider adding a confirm step or “Hold Right-Click” option to avoid accidental gem destruction.

## 2025-12-17 — Suggestions after compare view
- Add a toggle to compare “item vs currently equipped item” bonuses (raw item stats) in addition to full derived stat deltas.

## 2025-12-17 — Suggestions after character select preview tweak
- Move Character Select layout constants into a small config struct (or `data/ui.json`) so UI positioning can be tweaked without rebuilding.

## 2025-12-17 — Suggestions after HUD text alignment
- Consider adding a small UI safe-area calibration option for HUD offsets so players on ultrawide/TV setups can nudge bars/labels.

## 2025-12-17 — Suggestions after shield recenter
- Consider removing per-bar offsets entirely now that text measurement is accurate, and only keep them if the art implies a non-centered “sweet spot”.

## 2025-12-17 — Suggestions after health recenter
- If Energy still needs a nudge, consider baking the offsets into a single `kResourceBarTextOffsetX` default and only overriding per-bar when absolutely necessary.

## 2025-12-17 — Suggestions after character renames
- Ensure the fallback bitmap font includes common accented glyphs (e.g., “í” in “María”) or always prefer TTF for UI text.

## 2025-12-17 — Suggestions after RPG consumables + bags
- Add a small HUD indicator for consumable cooldown groups (e.g., “Food CD”, “Potion CD”) so players understand why Use Item didn’t fire.
- Move enemy RPG seeding constants into `data/gameplay.json` so tuning doesn’t require a rebuild.
- Add drag-from-equipment to inventory (and a drop-target highlight) to make bag swap/unequip flows more discoverable.

## 2025-12-17 — Suggestions after Hotbar R/F
- Add a small cooldown overlay + stack count on hotbar slots so it’s obvious when the item is unusable/on cooldown.
- Allow hotbar to target a consumable *stack* (by inventory slot id + quantity) rather than a single instance id.

## 2025-12-17 — Suggestions after Character screen layout polish
- Add a scroll region for the Details panel so it can display arbitrarily long affix lists without relying on window height.

## 2025-12-17 — Suggestions after launch maximized window
- Add a `data/settings.json` option for `startMaximized` / `windowMode` and persist last window state/size per platform.

## 2025-12-17 — Suggestions after WM-safe maximize
- Consider offering a “Borderless Fullscreen” option (`SDL_WINDOW_FULLSCREEN_DESKTOP`) for tiling WMs where maximize semantics differ.

## 2025-12-17 — Suggestions after main menu refresh
- Add optional menu music + ambient SFX and a small “Press Enter” / controller hint row.
- Persist last selected menu item and add smooth transition animations between menu pages.

## 2025-12-17 — Suggestions after character select refresh
- Add small difficulty “modifiers” chips (e.g., enemy HP, damage, drop rate) if/when those values become data-exposed.
- Add a subtle vignette/particles pass for the preview card (cheap eye-candy) and a “random hero” button.

## 2025-12-17 — Suggestions after preview recenter
- Consider per-hero preview offset/scale data in `data/menu_presets.json` so large sprites can be framed without code changes.

## 2025-12-17 — Suggestions after Character Select font bump
- Add an in-game UI scale slider (and persist it) so players can tune readability without code changes.

## 2025-12-17 — Suggestions after in-match HUD refresh
- Convert the status card into a small priority-based “toast” system (fade out older messages; keep only critical warnings pinned).
- Add an option to switch between “Compact” and “Classic” HUD layouts for players who prefer the old bottom bars.

## 2025-12-17 — Suggestions after shop UI modernization
- Add a small “Hold Shift to buy 5” or “Buy & Equip” affordance (where applicable) to reduce click count.
- Promote shop selection to keyboard/controller navigation (Up/Down + Enter) for accessibility.

## 2025-12-17 — Suggestions after main menu starfield
- Add an Options toggle for starfield density/speed (and a “Reduce Motion” accessibility flag).

## 2025-12-17 — Suggestions after RPG status integration + PRD shaping
- Replace the remaining ad-hoc `Game::StatusEffects` timers (stun/burn/slow) with engine `StatusContainer` specs so all CC/DoT/buffs share one pipeline (including cleanse + Unstoppable).
- Split RNG streams (`combat`, `loot`, `world`) and add an explicit “seeded run” debug mode so combat repros aren’t perturbed by unrelated randomness.

## 2025-12-17 — Suggestions after seeded world scenery
- Add a debug overlay/option to display and copy the current `matchSeed` so scenery layouts (and future seeded runs) can be reproduced easily.
- Add a lightweight spatial hash for scenery colliders to avoid O(N) hero-vs-obstacle checks as prop density grows.

## 2025-12-17 — Suggestions after fog + camera follow fix
- Consider rendering “Fogged” tiles as dim silhouettes for scenery (instead of fully hidden) to improve navigation while still preserving exploration.

## 2025-12-17 — Suggestions after denser scenery pass
- Add a `spawn.densityScale` multiplier keyed off map size/biome so designers can quickly tune clutter without rebalancing per-prop weights.

## 2025-12-17 — Suggestions after NPC world collision
- Add a spatial grid for `SolidTag` colliders so enemies/escorts can query nearby obstacles instead of iterating all scenery every frame.

## 2025-12-18 — Suggestions after background music routing
- Add an in-game audio options panel (master/music volume + mute) and persist it in `data/` or the save file.

## 2025-12-18 — Suggestions after core SFX routing
- Add per-category volume sliders (music vs SFX) and a “mute in background” option for streamers.

## 2025-12-18 — Suggestions after options menu modernization
- Persist audio sliders (and other options) into `data/gameplay.json` overrides or the save file so preferences survive restarts.

## 2025-12-18 — Suggestions after Options persistence + Upgrades UI revamp
- Add a dedicated “Accessibility” section in Options (UI scale, reduce motion, colorblind palettes) and persist it alongside audio settings.

## 2025-12-18 — Suggestions after expanded Stats screen
- Add a “Last Run” sub-panel (store a snapshot of run stats on defeat) so the Stats screen can show both lifetime totals and the most recent run at a glance.
- Add a “Reset Stats” button with a confirm modal (and optionally export/copy-to-clipboard for sharing).

## 2025-12-18 — Suggestions after Win64 audio packaging fix
- Add an in-game banner/toast when audio is disabled at build time (`ZETA_HAS_SDL_MIXER=0`) so players immediately understand why music/SFX are silent.
- Add a packaging sanity check in `scripts/package-win.sh` that warns (or fails in CI) if `SDL2_mixer.dll` is missing from the staged folder.

## 2025-12-18 — Suggestions after CI audio enforcement
- Add a small `--selftest-audio` CLI mode that initializes the mixer backend and exits with a clear error message if initialization fails (usable in CI without launching a window).

## 2025-12-18 — Suggestions after champion ability kit revamp
- Move new ability magnitudes/durations (heals, shield bonuses, taunt radii, execute counts) into `data/gameplay.json` so balancing can be iterated without recompiles.
- Add host/client RPCs for ally-targeted heals and resurrect so clients can support teammates without desync (server-authoritative).
- Add a lightweight “VFX lifetime” component/system (instead of reusing `Engine::ECS::Projectile` for timed cleanup) to keep visuals and gameplay entities separate.

## 2025-12-18 — Suggestions after thrust animation fix
- Add a small debug overlay toggle that prints the active hero sheet frame size + row to speed up diagnosing sheet mismatches.

## 2025-12-18 — Suggestions after thrust dash
- Consider separating “ability dash” from the Space-bar dash (separate speed/duration knobs) so Special/Tank lunges can be tuned without affecting the global dash.

## 2025-12-18 — Suggestions after red thrust trail
- Store dash trail color per node (instead of a global timer) so overlapping dashes (ability then Space) keep their intended colors.

## 2025-12-18 — Suggestions after cloaking + Shadow Dance fixes
- Consider adding a small generic “tint/alpha” field to `Engine::ECS::Renderable` so systems can set it directly (instead of RenderSystem inferring from statuses).
- For multiplayer, add an RPC/event for Shadow Dance target list so clients can see the same teleport sequence deterministically.

## 2025-12-18 — Suggestions after sprite tint cleanup
- Consider moving “team color”/selection colors into UI-only rendering (nameplates/markers) so gameplay sprites stay unshaded.

## 2025-12-18 — Suggestions after enemy roster update
- Add an `enemyWeights` field to `data/enemies.json` so waves can bias toward certain monsters per round/difficulty instead of uniform random selection.
- Add a small on-screen icon/timer for bleed/poison (or fold them into the existing status container) so players can understand incoming damage sources.

## 2025-12-18 — Suggestions after fixing bounty spam
- Add a debug HUD toggle that prints per-enemy tags (Boss/Bounty/etc.) and def id to make spawn-tag regressions obvious.

## 2025-12-18 — Suggestions after ability VFX pass
- Add a lightweight VFX pooling system (or batched instancing) for “area fill” effects like Consecration to avoid spawning hundreds of entities per cast.

## 2025-12-18 — Suggestions after ability icon layout pass
- Add an in-game “Icon Legend” dev panel that draws the 11x6 icon grid with indices (0..65) for quick iteration.

## 2025-12-18 — Suggestions after shop pause + loot tuning
- Add a small “Paused (Shop)” indicator when shops freeze timers in solo so the behavior is explicit to players.
- Unify hotbar/consumable eligibility checks (HUD, character screen, Q-use) behind a single helper to avoid drift.

## 2025-12-18 — Suggestions after Assassin cloak scaling
- Add per-ability level scaling text to the ability tooltip so upgrades communicate the exact bonus.
## 2025-12-18 — Suggestions after scroll consumables + cataclysm boss
- Add a small targeting preview (cursor ring/ghost) for revive, lightning prison, and phase step scrolls to reduce miscasts.
- Consider a dedicated “summoned ally” stat block to unify zombies, mirror clones, and necro servants.
- Add optional kill-attribution tracking so necronomicon can only convert user kills.

## 2025-12-19 — Suggestions after RPG loot/affix refresh
- Add a data-driven loot table validation test that asserts every sprite sheet entry has a matching loot item (weapon/unique gear coverage).
- Add a small “socketed bonus” breakdown in item details to make gem contributions explicit.

## 2025-12-19 — Suggestions after RPG stat display fix
- Consider splitting `DerivedStats` defaults for live stats vs contribution stats to avoid baseline leakage in future systems.

## 2025-12-19 — Suggestions after baseline cleanup
- Add a small helper in `engine/gameplay` to create zeroed `StatContribution` so UI and gameplay stay consistent.

## 2025-12-19 — Suggestions after compare tooltip move
- Consider color-coding compare lines in tooltips (green/red) to keep the clarity from the old Details panel view.

## 2025-12-19 — Suggestions after compare color pass
- Consider adding small ▲/▼ glyphs next to compare lines for extra clarity.

## 2025-12-19 — Suggestions after Unique tier pass
- Add a tooltip tag for Unique items (e.g., “Unique”) to reinforce the new tier in UI.

## 2025-12-19 — Suggestions after UniqueGear grid fix
- Consider validating sprite sheet dimensions in the generator to warn on non-16px multiples.

## 2025-12-19 — Suggestions after Sight upgrade tuning
- Consider adding per-upgrade cost multipliers for other ability shop rows to support targeted balance passes without changing global costs.

## 2025-12-19 — Suggestions after Sight fixed costs
- Consider exposing per-upgrade cost arrays for other ability shop rows when tuning becomes more granular.

## 2025-12-19 — Suggestions after Robin range + consumable sell/tooltip updates
- Add an in-game range indicator when swapping Robin weapons to show current attack distance.
- Consider a dedicated economy tuning table for sell multipliers per item kind (consumable vs equipment).

## 2025-12-19 — Suggestions after affix rarity thresholds
- Add a small loot test that validates affix count ranges per rarity and ensures tier weights are applied when configured.

## 2025-12-19 — Suggestions after roll scaling per rarity
- Consider exposing scalar ranges in UI debug tooling to help balance per-rarity roll spread.

## 2025-12-19 — Suggestions after character select spacing tweak
- Consider exposing character select layout offsets in a small UI tuning table to avoid recompiles for layout nudges.

## 2025-12-19 — Suggestions after character select spacing tweak (round 2)
- Add a debug toggle to draw layout baselines/boxes for menu panels to speed up spacing tweaks.

## 2025-12-19 — Suggestions after talent tree overlay foundation
- Add keyboard/controller navigation for talent nodes plus a small minimap to help with large trees.

## 2025-12-19 — Suggestions after talent point pacing update
- Consider exposing the talent point cadence as a tuning value in `data/gameplay.json`.

## 2025-12-19 — Suggestions after talent tree UI layout pass
- Consider adding a small shared UI helper for clipped/wrapped text blocks (word-wrap + ellipsis) to reuse across overlays.

## 2025-12-19 — Suggestions after talent stat line fix
- Consider separating `DerivedStats` into `DerivedStats` + `DerivedStatDeltas` to avoid baseline-default confusion in future UI/debug views.

## 2025-12-19 — Suggestions after talent tree branch spacing pass
- Consider adding per-archetype layout presets (lane spread, node size) so each tree can have a distinct silhouette without manual pixel tweaking.

## 2025-12-20 — Suggestions after Epic+ gear attributes + talent attribute bonuses
- Consider adding per-archetype attribute weight tables for loot rolls to better match class identity.

## 2025-12-20 — Suggestions after loot roll scaling update
- Add non-weapon implicit stat pools (armor/jewelry) or gem-specific pools so socketables stay meaningful without static base stats.

## 2025-12-20 — Suggestions after armor implicit guarantee + empty gear fix
- Consider adding slot-specific implicit pools (armor/jewelry/gems) to better target fallback stats by item type.

## 2025-12-20 — Suggestions after tooltip dedupe for RPG stats
- Add a toggle in UI settings for "Show implicit breakdown in hover tooltip" for power users.

## 2025-12-20 — Suggestions after compare tooltip stat coverage
- Consider grouping compare lines into sections (offense/defense/utility/resists) to keep long deltas readable.

## 2025-12-20 — Suggestions after armor implicit rarity scaling
- Consider adding per-slot armor implicit ranges (e.g., chest > boots) for finer balance control.

## 2025-12-20 — Suggestions after scroll rage tint + fear refresh guard
- Add a small status icon or HUD message when movement is locked (Fear/Stasis) to clarify "stuck" moments.

## 2025-12-20 — Suggestions after fear wander movement
- Consider adding a fear-specific movement speed scalar in data so designers can tune how frantic the wander feels.

## 2025-12-20 — Suggestions after toast system
- Consider adding tiny icons per toast type and a UI setting to adjust toast size/position for accessibility.

## 2025-12-21 — Suggestions after talent/status icon updates
- Add optional `iconRow`/`iconCol` overrides in `data/rpg/talents.json` for manual art direction on special nodes.
- Expose status icon order in `data/statuses.json` so sheet layout changes don’t require code edits.

## 2025-12-21 — Suggestions after status icon legend update
- Add explicit status-to-icon index mapping in `data/statuses.json` so icon changes stay data-driven.
- Consider a separate icon row for buffs vs debuffs to reduce ambiguity when multiple effects are active.

## 2025-12-21 — Suggestions after escort facing fix
- Consider deriving sprite flip from velocity when Facing is absent to avoid stale facing on non-combat NPCs.

## 2025-12-21 — Suggestions after fear/powerup tweaks
- Consider exposing fear avoidance radius and jitter in `data/gameplay.json` for tuning.

## 2025-12-21 — Suggestions after pickup tint removal
- If we still want pickup emphasis, consider a subtle drop shadow or glow sprite instead of tinting.

## 2025-12-21 — Suggestions after wallet HUD move
- Consider adding a toggle in `data/gameplay.json` for HUD wallet anchoring (minimap vs inventory).
