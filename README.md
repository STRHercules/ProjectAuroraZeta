# Project Aurora Zeta

![](/assets/icon512.png)

Survivor-style, wave-based ARPG built in C++20 with a lightweight in-house engine. Code is split between an engine layer (`/engine`) and a game layer (`/game`) to keep systems reusable.

The current build includes:
- Character & difficulty selection, solo or host/client multiplayer.
- Wave director with intermissions, shop, rotating hotzones, and 5-wave mini-events.
- Hero archetypes with stat multipliers, offensive types, and data-driven ability kits.
- Builder mini-units + structures, Summoner leashed companions, Druid forms, Wizard elements.
- SDL2 rendering/input, bitmap text, ECS, AI/status effects, and lightweight netcode.

## Current Systems & Logic
- **ECS core**: transforms, velocity, renderables, AABB, health/shields/armor, tags (Hero/Enemy/Boss/Projectile), sprite animation, buffs, status effects, damage numbers, hit flash.
- **Combat**: auto-fire primary by offensive type; dash (3.5× speed, 0.18s, 2.5s CD, full i-frames); lifesteal/attack-speed buffs; plasma/melee/thorn handling via tuning; collision system applies damage, lifesteal, hotzone multipliers.
- **Energy & vitals**: 100 base energy (14/s regen; 26/s during intermission); hero base HP 100, shields 0 (+25 shield bonus for melee), regen delay 2s; pickups include Heal/Recharge/Kaboom/Freeze/Frenzy/Immortal.
- **HUD**: top-right mini-map centered on the player; enemy blips clamp at the radar edge and hide when targets are cloaked or frozen in stasis.
- **Wave system**: spawns around hero every `interval+grace` (2.5s + 1.0s). Enemy HP & shields grow +8%/wave; speed +1%/wave; batch size grows every other wave and per round; player count scales batch size. Bounty elites every 5th wave; milestone bosses begin at wave 20 then every 20 waves (HP×12, speed×0.8, extra armor/shields, visual size capped ~110u).
- **Hotzones**: rotate every 30s across the 900u map radius (min separation). Bonuses while inside the active zone: Xp Surge (+25% XP), Danger-Pay (+50% copper), Warp Flux (+20% damage, +15% fire rate); warning flash before rotation; Warp Flux can trigger extra elites.
- **Mini-events** (every 5 waves): Escort Duty (escort NPC 500–1200u), Bounty Hunt (kill 3 elites before timer), Spire Hunt (destroy 3 spawners). Countdown gates early aggro; success grants `events.salvageReward` (60c).
- **Shop & economy**: copper per kill 5, wave clear 20, bounty bonus 40; shop buttons buy Damage +5 (25c), HP +20 (25c), Speed +20 (20c). Ability shop: base cost 8, growth 1.18, levels grant damage/attack-speed/range/vision/health/armor (range/vision capped). Drop chance 25% with powerup share 35%; boss/mini-boss copper 160/80.
- **Builder & summons**: builder/summoner rely on supply (starts 0; builder start min 2, summoner start 6, cap 10–12; houses add +2). Mini-units and buildings are loaded from `data/units.json`.
- **Networking**: host/client with hashed password check, lobby sync (hero, difficulty, max players, chat), 20 Hz snapshots during matches; default port 37015, max players default 4.

## Traveling Shop (Gold)
- **Unlock**: killing any boss sets `travelShopUnlocked = true`; during the next intermission (while you have gold) a shopkeeper spawns 260–420u from the hero. Interact with `E` to open.
- **Currency**: prices use **gold** (per-run, then banked). Sources: boss kill = 3g (`gold.boss`), successful events = 1g, bounty elites drop a gold bag equal to `rewards.bountyBonus`.
- **Stock** (legendary-only, 4 cards per roll; capped effects are hidden). Dynamic prices per effect:
  - Cataclysm Rounds: +50% all damage, 1000g then 2500g (max 2).
  - Temporal Accelerator: +50% attack speed, 1500g (max 1).
  - Nanite Surge: +25% HP/Shields/Energy, 1500g (max 1).
  - Cooldown Nexus: −25% cooldowns, 500g then 1000g (max 2).
  - Survey Optics: +3 range & +3 vision, 500/1000/1500/2000/2500g (max 5).
  - Charge Matrix: +1 ability charges (for charge-based skills), 500g (max 3).
  - Speed Boots: +1 move speed, 200g each (max 10).
  - Vital Austerity: −25% vital ability costs, 500g (max 3).
- **Flow**: inventory pulls from the gold catalog, shuffles each refresh, respects caps, and shows a gold wallet readout in the overlay.

## Powerups, Pickups & Items
- **Currency pickups**: Copper bags (4–10c) from normal kills; Gold bags (boss 3g, bounty `bountyBonus`, event +1g). Copper funds in-run shops; gold funds the traveling shop and vault deposits.
- **Powerups** (25% drop rate; 35% of drops become powerups): Heal (full HP/shields, regen delay reset), Kaboom (kills all enemies), Recharge (refills shields + energy), Frenzy (30s +25% fire rate), Immortal (30s invulnerability timer), Freeze (5s global time-stop).
- **Revive pickup**: Bosses also drop a Revive Tome (+1 life).
- **Shop/dropped items (copper)**:
  - Power Coil (+10 damage, 40c), Reinforced Plating (+25 HP, 45c), Vector Thrusters (+8% move speed, 35c)
  - Field Medkit (Use/Q, heal 35% HP, 30c), Cryo Capsule (freeze time 2.5s, 70c), Deployable Turret (12s, 90c)
  - Chrono Prism (+15% attack speed aura & slow, 140c), Phase Leech (5% lifesteal, 150c), Storm Core (projectiles chain +2, 160c)
- **Use rules**: Support items sit in quick-use (`Q`); others apply instantly on purchase/pickup. Traveling shop items apply immediately and count toward their stack caps.

## Global Upgrades (Vault Gold Meta)
- **Where**: Main Menu → Upgrades. Spends vault gold (auto-deposited once per match via deposit guard).
- **Effects per level** (∞ unless noted; multiplicative, then multiplied by Mastery):
  - Attack Power +1% (250 base, 1.15× growth)
  - Attack Speed +1% (250, 1.15×)
  - Health +1% (250, 1.15×)
  - Speed +1% (250, 1.15×)
  - Armor +1% (250, 1.15×)
  - Shields +1% (250, 1.15×)
  - Recharge (shield regen) +1% (250, 1.15×)
  - Lifesteal +0.5% (400, 1.18×)
  - Regeneration +0.5 HP/s (400, 1.18×)
  - Lives +1, max 3 (2000, 2.0×)
  - Difficulty +1% enemy power & count (800, 1.20×)
  - Mastery +10% to all stats incl. difficulty, max 1 (250000, 5.0×)
- **Application**: Modifiers are recomputed on load and applied to hero presets (HP/shields/armor/regen/move/damage), lifesteal & regen adders, extra lives, and enemy power/count multipliers; mastery multiplies the whole set.

## Hero Archetypes & Offensive Types (from `data/menu_presets.json`)
| Archetype (id) | Offensive Type (fallback mapping) | Stat Multipliers (HP / DMG / Speed) | Playstyle note |
| --- | --- | --- | --- |
| Knight (`tank`) | Melee | 1.35 / 0.9 / 0.9 | Durable frontline bruiser. |
| Healer (`healer`) | Ranged | 1.05 / 0.95 / 1.0 | Sustain/support, lighter offense. |
| Militia (`damage`) | Melee ↔ Ranged swap (alt weapon) | 0.95 / 1.15 / 1.05 | Flexible DPS; can switch stance. |
| Assassin (`assassin`) | Melee | 0.85 / 1.2 / 1.25 | Fast, high-burst, fragile. |
| Builder (`builder`) | Builder | 1.1 / 0.9 / 0.95 | Structure/mini-unit commander. |
| Dragoon (`support`) | Melee | 1.0 / 1.0 / 1.0 | Polearm support, balanced. |
| Crusader (`special`) | Melee | 1.1 / 1.05 / 1.05 | Experimental all-rounder. |
| Summoner (`summoner`) | Ranged | 0.95 / 0.9 / 1.0 | Fragile; relies on beetle/snake summons. |
| Druid (`druid`) | Ranged (Bear/Wolf forms become Melee) | 1.1 / 1.0 / 1.0 | Human ranged plus shapeshift forms. |
| Wizard (`wizard`) | Ranged | 0.85 / 1.1 / 1.05 | Elementalist cycling primary + spells. |

## Ability Kits (current implementation)
- **Militia (damage)**: Primary Fire; Scatter Shot cone (6s, 30c); Rage self-buff (12s, 40c); Nova Barrage radial (10s, 45c); Death Blossom ultimate barrage (35s, 60c).
- **Knight (tank)**: Primary; Shield Bash stun (8s, 30c); Fortify DR (14s, 40c); Taunt Pulse (16s, 45c); Bulwark Dome barrier (40s, 60c).
- **Summoner**: Arcane Bolt; Summon Beatle (6s, 25c); Summon Snake (9s, 30c); Rally (8s, 20c leash refresh); Swarm Burst (24s, 50c rapid wave).
- **Druid**: Spirit Arrow; Choose Bear (slot 1, unlock lvl2); Choose Wolf (slot 2, unlock lvl2); Regrowth self-heal (12s, 30c); Shapeshift toggle (0.5s). Bear = HP/shield/armor up, slower, melee; Wolf = speed/damage up, melee.
- **Wizard**: Elemental Bolt (cycles with `Left Alt`); Fireball (6s, 30c AoE); Flame Wall (12s, 45c persistent); Lightning Bolt (9s, 40c stun line); Lightning Dome (24s, 60c protective field).
- **Builder** (hard-coded kit): Upgrade Light/Heavy/Medic minis (0.6s, low cost, infinite levels, +2% all stats per purchase); Mini-Unit Overdrive ultimate (45s, 35c) gives 30s damage/attack-rate/heal buffs to all minis.
- **Healer / Assassin / Dragoon / Crusader**: currently use generic placeholders (Primary + three abilities + ultimate) until bespoke kits are authored.

## Mini-Units, Summons & Structures (from `data/units.json`)
- **Supply**: builder starts with at least 2 supply; summoner starts with 6 and cap 12; global cap baseline 10; houses add +2 supply each. Every mini-unit consumes 1 supply.
- **Mini-units**
  - *Light* (`light`, 6c): 130 HP, 180 move, 18 dmg, ranged kiting to 120u preferred distance.
  - *Heavy* (`heavy`, 10c): 520 HP +80 shields, armor 3, shield armor 3, 14 dmg, slower (120), taunt radius configurable.
  - *Medic* (`medic`, 8c): 180 HP +80 shields, heals allies 22 HP/s (reduced for shields), modest damage.
- **Summoner companions** (0 cost via abilities):
  - *Beatle*: 180 HP, 170 move, 20 dmg, ranged, prefers 120u spacing.
  - *Snake*: 280 HP +80 shields, armor/shield armor 2.5, 30 dmg, 75u range.
- **Structures**
  - *Turret* (30c): 320 HP +80 shields, armor 3, range 260, rate 1.2s, dmg 15.
  - *Barracks* (35c): 200 HP +40 shields, armor 1, spawn interval 8s, queue 5; UI buttons to buy Light/Heavy/Medic; spawns near structure.
  - *Bunker* (40c): 260 HP +60 shields, armor 2, capacity 4; damage multiplier per occupant 0.35.
  - *House* (18c): 120 HP, adds +2 supply; no shields.
- **AI priorities**: enemies target turrets first, then mini-units, then heroes unless taunted. Summoned units leash to owner (320u) and suspend combat while returning.

## Enemies, Events & Bosses
- **Base enemy stats**: 40 HP, 20 shields, speed 80, contact damage 10, size/hitbox 18u; armor/regen 0. Scaling +8% HP/shields and +1% speed per wave; batch size grows with waves/rounds and player count.
- **Bounty elites**: ~3× HP, extra armor/shields, faster; spawn every 5th wave; marked with bounty tag and drop bonus copper (80 base).
- **Bosses**: start at wave 20 then every 20 waves; HP×12, speed×0.8, extra armor/shield, larger size (capped ~110u); one per player.
- **Events**: Escort Duty, Bounty Hunt, Spire Hunt; countdown before start; success gives copper; failure clears spawns. Event markers shown on HUD; escort pathing uses travel distance 500–1200u and scaled durability per wave.
- **Hotzones**: rotating map bonuses; Warp Flux may request extra elite surge on activation.
- **Status effects**: burn/earth DoT, slow, stun, blind; thorns reflect supported; plasma/shield interactions configurable in tuning.

## Controls & Hotkeys (defaults)
Loaded from `data/input_bindings.json`; fully remappable via file override.
- Move: `WASD`
- Camera pan: Arrow keys (RTS mode)
- Toggle camera follow: `C`
- Dash: `Space`
- Interact / talk / use: `E`
- Use item: `Q`
- Abilities: `1`, `2`, `3` (ultimate: `4`)
- Reload: `F` (or `F1`)
- Swap weapon / stance / element: `Left Alt`
- Toggle Ability/Item Shop: `B`
- Build menu (builder): `V`
- Character screen: `I`
  - Click inventory items to equip; click equipped slots to unequip.
  - Gems are socketable: hover an equipped item with sockets, then click a gem to insert it.
  - Drag-and-drop: drag inventory items to swap positions; drag a gem onto an equipped item to socket it.
  - Right-Click an item with a gem socketed to destroy the gem.
- Pause: `Esc`
- Restart run (debug helper): `Backspace`
- Menu back (fallback): `M`
- Mouse: primary to fire/confirm, secondary/tertiary used inside shop prompts

## Multiplayer
- **Host**: choose lobby name/password, difficulty id, max players (default 4), port 37015; password is hashed before send. Host immediately owns player id 1 and broadcasts lobby state.
- **Join**: enter host IP/port/password/name; client receives Welcome with assigned id and lobby settings.
- **Lobby sync**: hero selections, difficulty, max players, chat history; host rebroadcasts on change.
- **Match flow**: host issues StartMatch; snapshots run ~20 Hz (host broadcasts; clients can echo inputs/snapshots). Goodbye sent on leave; `stop()` resets lobby state.
- **Drop-in**: join supported in lobby; in-match join assumes start-of-run snapshot sync (mid-run hot-join is not finalized).

## In-Game Options (Main Menu → Options)
- Damage numbers: on/off
- Screen shake: on/off
- Movement mode:
  - **Modern** (WASD moves hero, camera follow)
  - **RTS-like** (RMB move + WASD pans camera; follow disabled by default)

## Gameplay Loop (current prototype)
1) **Main Menu** → Solo or Host/Join.  
2) **Character & Difficulty Select**  
   - Archetypes (Tank, Healer, Damage, Assassin, Builder, Support, Special, Summoner, Druid, Wizard).  
   - Difficulties: Very Easy, Easy, Normal, Hard, Chaotic, Insane, Torment I.  
3) **Drop-in** to map with rotating hotzones (bonus XP/gold/flux).  
4) **Waves**: enemies spawn in batches; defeat all to trigger intermission.  
5) **Intermission**: open shop (`B`), buy stats, roll for items/abilities, or reposition.  
6) **Events & Bosses**: mini-events every 5 waves; milestone bosses every 20.  
7) **End**: wipe or reach target wave → stats screen, kill totals banked.

## Data-Driven Tuning
- `data/gameplay.json` — base stats for heroes/enemies/projectiles, wave pacing, rewards, dash, events, hotzones, shop, XP growth, boss scaling.
  - Feature flags: `useRpgCombat`, `useRpgLoot`, `combatDebugOverlay` (RPG resolver debug panel).
  - RPG drop/shop tuning: `drops.rpgEquipChanceNormal`, `drops.rpgEquipMiniBossCount`, `drops.rpgEquipBossCount`, `drops.rpgMiniBossGemChance`, `shop.rpgEquipChance`, `shop.rpgEquipCount`.
- `data/rpg/loot.json` — RPG equipment templates, affixes, and icon coordinates for `assets/Sprites/Equipment/*.png`.
  - Regenerate via `python3 scripts/gen_rpg_loot.py`.
- `data/abilities.json` (fallback `data/abilities_default.json`) — ability names, descriptions, cooldowns, costs, semantic types.
- `data/menu_presets.json` — archetype & difficulty lists, colors, descriptions, offensive types; used for character select.
- `data/units.json` — mini-unit and building definitions (stats, costs, textures, AI params).
- `data/input_bindings.json` — key bindings listed above.

## Project Layout
- `engine/` — platform/renderer/input abstractions, ECS, logging, time, asset management.
- `game/` — gameplay bootstrap, menus, systems (combat, buffs, AI, events, hotzones, shop, meta).
- `game/net/` — lobby/session config, snapshots, host/client flow.
- `data/` — gameplay, presets, assets manifest, bindings.
- `assets/` — sprites, tiles, VFX, GUI art, fonts.
- `docs/` — design notes (`Gameplay_Loop.md`, `GAME_SPEC.md`, health/armor model, fog-of-war, ideas).
- `src/` — `main()` entry and platform glue.

## Quickstart (Playing)
1) Build and run (`./build/Project Aurora Zeta` or `dist/windows/Project Aurora Zeta.exe`).
2) From **Main Menu** choose:
   - `New Game (Solo)`: pick archetype + difficulty, then `Start Run`.
   - `Host`: set player name, lobby name/password, max players, and difficulty; start hosting then wait in lobby.
   - `Join`: enter player name and password, direct connect or browse, then auto-sync to host settings.
3) Use hotkeys above; open shop during intermission; survive as many waves as possible.

## Building on Linux for Linux
```bash
# Requisites
sudo apt install -y build-essential cmake libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev

# cd to the root of the repo
# Build the Project
cmake -S . -B build
cmake --build build

# Run Zeta
./build/"Project Aurora Zeta"
```

## Building on Linux For Windows

```bash
# Make sure we have the requisites
sudo apt install -y build-essential cmake libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev mingw-w64 zip

# Create and navigate to the right folder
mkdir -p ~/winlibs/sdl
cd ~/winlibs/sdl

# Grab SDL2 base
wget https://www.libsdl.org/release/SDL2-devel-2.30.9-mingw.zip

# Grab SDL2_ttf
wget https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-devel-2.22.0-mingw.zip

# Grab SDL2_image
wget https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.8.2-mingw.zip

# Extract the .zips
unzip SDL2-devel-2.30.9-mingw.zip
unzip SDL2_ttf-devel-2.22.0-mingw.zip
unzip SDL2_image-devel-2.8.2-mingw.zip

# Navigate back to the root directory of the repo
# This will vary depending on your setup
# Execute the Windows Build
./scripts/build-win.sh

# Package the Win64 Build and everything needed to run on Windows into a .zip
./scripts/package-win.sh

# Build & Package AIO
./scripts/build-win.sh && ./scripts/package-win.sh
```

The distributable zip is written to `dist/windows/zeta-win64.zip` and contains `Project Aurora Zeta.exe`, required SDL DLLs, data, and a `start.bat` launcher.
