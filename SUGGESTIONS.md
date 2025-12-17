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
