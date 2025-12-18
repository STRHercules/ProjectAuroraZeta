# v0.0.101
* Fixed ESC behavior:
    * ESC Now closes menus and opens the pause menu, no longer closes match.
* Implemented Shields to Melee Classes by Default:
    * Shields have a small natural recharge rate.
    * Players can now add to their shield capacity, and increase their recharge rate at level up and on the global upgrade screen.
        * Characters that do not start with shields can add to shields the same way.
* Enemies are now much weaker in the beginning, and their strength will ramp up with the Rounds.
* Enemies will now drop more copper at higher Rounds.
* Now only Builders and Summoners can drag-select units.
* Escorts durability now scales with Rounds, same curve as enemies.
* W no longer sometimes fires Ability #2.
* Traveling Shop will  no longer attach to random assets.
* Escorts are now 50% larger.
* Dragoon now has extended reach with his polearm.
* Slightly extended melee reach of all melee classes, easier to kite now.
* Moved active item inventory to the right-hand side.
* Moved Resource bars to left-hand side.

# v0.0.102
* Death/Knockdown animation now only plays once in Singleplayer.
    * Movement/Ability use is now restricted when dead.
* Knockdown will play on death in multiplayer, converting player into a ghost.
    * Ghosts can move around, but not interact (Attack/Abilities)
    * Ghosts resurrect at the start of the following Round.
* Multiplayer is now synced to the Host's match.
    * Vision is shared in multiplayer
    * Clients no longer see their own copies of monsters.
    * Clients can now interact with the Host's monsters.
        * Do Damage
        * Take Damage
* Adjusted Builder GUI location
* Further buffed all mini-units and boosted medic healing in data/units.json:
      - Light: HP 130, damage 18.
      - Heavy: HP 520, shields 180, damage 14, armor/shieldArmor 3.
      - Medic: HP 180, shields 80, damage 8, heal/sec 22.
      - Beatle: HP 180, damage 20.
      - Snake: HP 280, shields 80, damage 30, armor/shieldArmor 2.5.
* Builder construction menu **V** no longer opens the ESC/Pause window.
* Builder Abilities refined:
    * Ability 1: Unlimited scaling cost purchase of 5% boost to Light Units per upgrade.
    * Ability 2: Unlimited scaling cost purchase of 5% boost to Heavy Units per upgrade.
    * Ability 3: Unlimited scaling cost purchase of 5% boost to Medic Units per upgrade.
    * Ultimate: Grants a 'Rage' boost to all units for 30s.
* Increased scale of Mini-Units

# v0.0.103
* Bosses now spawn every 20 Waves.
    * Boss scale clamped to a reasonable maximum.

# v0.0.104
* Implemented reusable status/state system with stacking, immunities, and debug hotkeys to apply effects.
    * Damage, regen, casting, and movement now respect Stasis, Silence, Fear, Cauterize, Cloaking, Unstoppable, and Armor Reduction (supports negative armor).
* Added top-right mini-map overlay that centers on the player and clamps enemy blips at the edge of the radar radius.
    * Cloaked or stasis targets hide from the mini-map and enemy rendering.

# v0.0.105
* Added modular RPG combat pipeline (attributes → derived stats → resolver) with armored/resist mitigation, shaped rolls, tenacity saving throws, and CC fatigue.
* Implemented data-first loot/talent/consumable models plus Luck-aware loot generator and stat aggregation helpers.
* Character select now shows archetype biography, attributes, specialties, and perk blurbs sourced from `data/rpg/archetypes.json`.
* New unit tests cover armor curve, resist clamps, CC diminishing returns, and stat aggregation.

# v0.0.106
* Added `useRpgCombat` flag and resolver config in `data/gameplay.json`; projectile/contact damage routes through the RPG resolver when enabled.
* Externalized loot/consumables/talents to `data/rpg/*.json`; when `useRpgLoot` is on, drops roll affixes and inventory shows affix lines.
* Inventory panel now lists affix bullet points for items; build string updated to v0.0.106.

# v0.0.107
* Loot affixes and RPG talents now modify live combat stats in RPG combat mode (damage scaling, armor, max HP/shields).
    * RPG stats are aggregated into per-entity snapshots used by the resolver.
* RPG resolver integration extended to DoTs and auras (burn/earth ticks, lightning dome, flame walls, wizard lightning bolt).
    * Warp Flux hotzone damage bonus now applies to flame wall ticks/burn.
* Added `combatDebugOverlay` flag in `data/gameplay.json` to show a live RPG combat debug panel.

# v0.0.108
* Added full equipment loot table (167 items) covering armor, weapons, gems, runes, tomes, shields, quivers, arrows, and scrolls.
    * Items roll RPG affixes and can be equipped (press **I**) to modify live RPG combat stats.
    * Equipment icons render from the `assets/Sprites/Equipment` sprite sheets in the equipment/inventory UI.
* Improved RPG loot rolls so rarity selects templates from the rolled tier and avoids duplicate affixes.
* Fixed text overlap in the Select Character & Difficulty summary panel.

# v0.0.109
* RPG equipment drops are now tiered:
    * Regular monsters drop low-grade wooden/copper/basic gear.
    * Mini-bosses drop multiple items and often include gems.
    * Bosses drop multiple high-grade items and still unlock the Traveling Shop.
* Traveling Shop can now roll RPG equipment items with affixes alongside existing gold shop offers.
* RPG equipment pickups are more visible in-world (uses the money bag pickup icon).

# v0.0.110
* Modernized the Character screen inventory UI:
    * Inventory is now a grid with selection/rarity highlighting.
    * Added hover tooltips and a dedicated item details panel (shows RPG bonus breakdown + affixes).
    * Equipment slots are presented as compact cards with clearer empty/filled state.

# v0.0.111
* Fixed a crash when equipping/unequipping items from the Character screen.

# v0.0.112
* Character screen tweaks:
    * Equipment panel is taller so all slots are visible (no overlap with Stats).
    * Default inventory capacity increased to 24 slots.
    * Equipping into an occupied slot now swaps items instead of failing when inventory is full.

# v0.0.113
* Gems are now socketable items instead of trinkets:
    * Hover an equipped weapon/gear to set the socket target, then click a gem to insert it into an empty socket.
    * Socketed gems apply their RPG stats/affixes to live stat aggregation.
* Runes remain equippable trinkets.

# v0.0.114
* Character screen polish:
    * Equipment slot text aligns better with the icon row.
    * RPG rarity is no longer shown as `[Uncommon]` text; item names are color-coded by rarity instead.

# v0.0.115
* Character screen improvements:
    * Equipment slot text is raised to better align with icons.
    * Inventory items can be drag-and-dropped:
      * Drag onto another inventory cell to swap positions.
      * Drag gems onto an equipped item with open sockets to socket them.

# v0.0.116
* Slightly increased Character screen text scale for readability.

# v0.0.117
* Equipped gear now updates the Character screen Stats panel (HP/Shields/Damage/Attack Speed/Move Speed).
* RPG combat is enabled by default so equipped stats/affixes affect live combat.

# v0.0.118
* Increased the Character/Inventory window height so stats and item details don’t clip off-screen.

# v0.0.119
* Right-clicking equipped gear now removes socketed gems one-at-a-time (gem is destroyed; socket is freed).

# v0.0.120
* Character screen improvements:
    * Added a compare view showing stat deltas when hovering an inventory item (preview before equipping).
    * Long equipment names are now truncated with ellipsis so they don’t clip in slot cards.

# v0.0.121
* Character select: moved the live hero preview sprite so it doesn’t overlap the lower text panels.

# v0.0.122
* HUD: adjusted Health/Shields/Energy bar text positioning so labels are centered correctly.

# v0.0.123
* HUD: fixed Shield bar text centering (was offset too far right).

# v0.0.124
* HUD: fixed Health bar text centering (was offset too far right).

# v0.0.125
* Character roster: renamed archetypes to character names (Jack, María, George, Robin, Raven, Sally, Ellis, Sage, Jade, Gunther).

# v0.0.126
* Loot drops now show rarity containers (bags/chests) and consumables show their own icons.
* Added foods + potions as RPG consumables (health/shield restoration + regen) with shared cooldown groups.
* Added equippable Bags that expand inventory capacity; overflow drops to ground on unequip.
* Routed melee + mini-unit damage through the RPG resolver and seeded enemy RPG snapshots for shared combat rules.

# v0.0.127
* Character screen: added two hotbar slots (R/F) for consumables; drag an item onto a slot and press the hotkey to consume it.
* Character screen: Equipment panel height now accounts for the Bag slot row (no clipping/overlap).
* Input: `R`/`F` are now reserved for hotbar use (Reload moved to `F1`).

# v0.0.128
* Character screen layout tweaks:
    * Raised the Inventory panel header and Run panel text for better alignment.
    * Increased the Character/Inventory window height so Details text is less likely to clip.

# v0.0.129
* Game now launches with the window maximized by default.

# v0.0.130
* Fixed: game window now reliably starts maximized (explicit maximize request).

# v0.0.131
* Main menu layout updated for 1080p:
    * Title is now top-centered, build info moved to the bottom-right.
    * Menu buttons are centered with improved hover/selection styling.
    * “A Major Bonghit Production” is now bottom-centered.

# v0.0.132
* Character select screen modernized:
    * New 1080p-centered layout with improved panels, hover highlighting, and readability.
    * Animated hero preview moved into a dedicated center card.

# v0.0.133
* Character select: preview sprite is now much larger and centered within the preview card.

# v0.0.134
* Character select: increased UI font scaling for improved readability.

# v0.0.135
* In-match HUD modernized:
    * Health/Shields/Energy/Dash moved to a cleaner top-left resource panel.
    * Added a compact status card for intermission/event/hotzone messages (less screen clutter).
    * Refreshed the inventory/held-item badge styling for consistency.

# v0.0.136
* Shops modernized:
    * Traveling Shop updated to a modal layout with Offers, Details, and Inventory Sell panels.
    * Ability Shop updated to a list + details layout with an explicit “Buy Upgrade” button.
    * Added hover/selection highlighting and long-text truncation for cleaner readability.

# v0.0.137
* Main menu: replaced the static backdrop with an animated starfield screensaver background.

# v0.0.138
* RPG combat: on-hit statuses can now be driven from attack metadata and applied into the engine status system (TEN saving throw + CC DR).
    * Wizard Lightning Bolt and Lightning Dome now use the RPG status pipeline for Stasis.
* RPG combat: elemental `SpellEffect` now maps to RPG damage types so resistances/vulnerabilities can matter beyond Physical/Arcane/True.
* RPG combat: added optional PRD-style anti-streak for crit/dodge/parry (`rpgCombat.rng.usePRD`).
* Consumables: Buff effects can now apply data-driven stat contributions via `data/rpg/consumables.json` (`effects[].stats`).

# v0.0.139
* Added sporadic, seeded world scenery placed deterministically per match (`data/scenery.json`).
    * Scenery uses partial collision so the player can walk behind tall sprites (trees/rocks/monuments) without clipping over the tops.

# v0.0.140
* Fog of war: world props, pickups, and enemies now correctly hide outside visible tiles.
* Camera: Modern mode now keeps the camera locked to the hero to prevent accidentally “losing” the world off-screen.

# v0.0.141
* Fog of war: scenery (and other non-enemy world props) now remains visible in explored “fogged” tiles.

# v0.0.142
* Fog of war: explored (fogged) scenery now renders dimmed under a fog tint instead of full brightness.

# v0.0.143
* Fog of war: scenery/props are now shaded by the standard fog overlay (no separate tint pass).

# v0.0.144
* Fixed: zooming in no longer makes sprites disappear due to an early-exit in render culling.

# v0.0.145
* Scenery placement now supports full-map distribution (not just a center radius) via `data/scenery.json` (`spawn.radius: 0`).

# v0.0.146
* World: massively increased scenery density, heavily favoring trees.

# v0.0.147
* AI: enemies, bosses, and escort targets now collide with world scenery.

# v0.0.148
* Visual: added `DirtPatch.png` into the floor tile variant pool (rare vs base tiles).

# v0.0.149
* Visual: improved floor tile randomization to reduce visible streaks, and render larger floor variants (e.g. 48x48 dirt patches) at native size.

# v0.0.150
* Fixed: large floor overlays (like `DirtPatch.png`) now render correctly on top of the base floor tiles.

# v0.0.151
* Visual: made `DirtPatch.png` floor overlays slightly rarer to reduce clumping.

# v0.0.152
* Audio: added background music for the main menu, gameplay, boss fights, and death screen.

# v0.0.153
* Audio: added core SFX routing (weapons, footsteps, pickups/consumes, and UI clicks).

# v0.0.154
* UI: redesigned the main menu Options page and added separate Music/SFX volume sliders.

# v0.0.155
* UI: Options screen layout now avoids text overlap/clipping.
* Audio: Options now persists Music/SFX volume across restarts.
* Audio: added a Background Audio toggle (mutes audio when the game is unfocused).
* UI: Upgrades screen redesigned with a modern list + details layout and a cleaner purchase flow.
