# GAME_SPEC.md

Title: **Project Aurora Zeta** (working title)

> This document consolidates the narrative, gameplay, and systems design from the original “Gameplay Overview” and “Gameplay Loop” documents into an implementation-facing spec for a custom C++ or C# codebase, with no reliance on engines like Unity/Unreal. Libraries for windowing/input/rendering (e.g., SDL2, SFML, OpenGL/DirectX/Vulkan bindings) are allowed, but no full game engines.

---

## 1. High-Level Vision

A co-op, wave-based, top-down/isometric sci‑fi shooter with RTS-style camera controls and hero‑based progression. Up to 6 players survive escalating waves of enemies on the forbidden planet Zeta. Progress is tracked via persistent kill counts and hero unlocks; talents are match-scoped. Cosmetics and some meta systems are “kill sinks.”

Core pillars:

1. **Hero-driven wave survival** (1–6 players).
2. **RTS-lite camera + ARPG readability**.
3. **Persistent progression via kills, challenges, and hero evolutions** (talents are match-scoped).
4. **Custom “raw” C++/C# stack – no third-party game engine runtime.**

---

## 2. Setting, Story & Tone

### 2.1 Setting

- Year: **2355**.
- Humanity has expanded via mysterious **Warp Gates**; their origin and purpose are unknown.
- Zeta is a stunning but lethal world, banned by the Interplanetary Congressional Army (ICA). :contentReference[oaicite:2]{index=2}  

Visual tone:

- **Neon-flecked hard sci-fi with grime** – “The Expanse meets Hades.”
- Isometric / top-down camera.
- Holographic UI overlays, exaggerated VFX for clarity. :contentReference[oaicite:3]{index=3}  

### 2.2 Story Premise

- Player is an ICA prisoner on a crash‑landing transport to Zeta.
- Zeta doubles as a de facto **prison death world**: prisoners are dropped into endless combat; survival commutes sentences.
- Crash sequence:
  - You and 5 others awaken bound.
  - Ship slams into Zeta in an “ICA Landing” (controlled crash).
  - Restraints auto-release, sides of shuttle blow off, loot crates pop open.
  - A second klaxon signals the start of the “run” – waves of local monsters/abominations arrive. :contentReference[oaicite:4]{index=4}  

### 2.3 Tone & Humor

- Darkly cheeky, not slapstick.
- A dry AI announcer tracks performance:
  - Example: “Congratulations, Inmate #03894-B. You lived six seconds longer than average.”
- Occasional quips on wave completion, boss kills, failures. :contentReference[oaicite:5]{index=5}  

---

## 3. Core Gameplay Loop

Loop per run:

1. **Briefing & Load‑Out (~2 minutes)**
   - Players vote on:
     - Difficulty.
     - Map variant.
     - Optional mutators (e.g., “Double Boss HP, +50% currency gain”).
   - Each player:
    - Picks a **hero** (class archetype).
    - Selects a **Career Perk** (future meta perk).
     - Optionally applies **cosmetic skins** (paid for in total kills, i.e., permanent kill sinks). :contentReference[oaicite:6]{index=6}  

2. **Crash-Drop Intro (~30 seconds)**
   - Cinematic crash animation.
   - Shackles pop off.
   - Supply crates scatter (weapons, currency).
   - If first-time player: contextual tutorial pop-ups.

3. **Wave Phases (~2 minutes per wave)**
   - **Combat (~90s)**:
     - Enemies spawn from map edges/warp fissures.
     - Players kite, tank, heal, build, assassinate, etc.
   - **Intermission (~30s)**:
     - Use **Shop** to buy items and upgrades.
     - Allocate ability skill points.
     - Reposition and plan next wave.
   - Prototype tuning (current build): waves tick on a short timer from `data/gameplay.json`, enemies gain ~8% HP and ~1% move speed per wave, and spawn batch size increases every other wave.
   - A brief warmup (configurable `spawner.initialDelay`, default ~1.5s) shows a countdown before the first wave spawns to give players orientation time, plus a short “Wave N” banner when each wave begins; `spawner.grace` sets post-wave downtime.

4. **Mini-Events (every 5 waves)**
   - Optional mission-like objectives:
     - Escort a civilian courier (500–1200u travel; enemies prioritize them).
     - Defend a “data siphon.”
     - Bounty hunts (elite squads).
   - Rewards: bonus currency, XP, rare items, “phase shards” for hero evolution. :contentReference[oaicite:7]{index=7}  

5. **Milestone Boss Waves**
   - Major bosses at big milestones (e.g., waves 20, 40, 60…) and every 20 waves thereafter.
   - Rewards:
     - Persistent buffs for remainder of run.
     - Phase shards (for hero EX forms).
     - Rare loot.
   - Boss visual scale is clamped so late-game scaling does not produce map-filling sprites (cap ~110 world units wide).
   - Boss may despawn if not killed quickly → loss of reward.

6. **End-State**
   - **Victory**: reach target wave count based on chosen difficulty.
   - **Defeat**: total squad wipe or failure of critical objective in challenge modes.
   - Post-run:
     - Stats summary and records.
     - Kill totals and currencies banked.
     - Talent Points awarded.
     - Challenges & unlocks updated. :contentReference[oaicite:8]{index=8}  

---

## 4. Camera, Controls & UX

### 4.1 Camera

- **Top-down / isometric**.
- **RTS-style free camera**:
  - WASD pans camera.
  - Mouse wheel zooms in/out.
  - Double-tap Space to center on hero.
- Optional “hero leash” mode:
  - Camera locks to hero for twin-stick-like experience.
- Zoom level modifies FOV and detail.

### 4.2 Controls (Keyboard + Mouse)

- Standard PC layout:
  - Move hero: WASD / Click-to-move depending on chosen control scheme.
  - Abilities: Q/E/R/F/Mouse buttons.
  - Interact: E or Right-click.
  - Tactical ping wheel: hold Q → radial quick pings (attack/defend/shop, etc.).
- Prototype shortcuts (for current build):
  - `C` toggles camera follow vs. free pan.
  - `R` restarts the current run (respawns hero, resets waves).
  - `B` opens/closes the placeholder Shop during intermission.
  - `Esc` pauses/unpauses (shop also pauses).
  - Shop buys (placeholder): Mouse1=+Damage, Mouse2=+HP, Mouse3=+Move Speed (costs + bonuses from `data/gameplay.json`).
- Prototype tuning is loaded from `data/gameplay.json` (hero HP/move speed/size, projectile speed/damage/visual size/hitbox size/lifetime, enemy HP/speed/contact damage/size/hitbox size, wave pacing, currency per kill).
- RTS flavor:
  - Drag-select is not primary; one hero per player.
  - Pings and camera define the “RTS feel.”

HUD notes:
- Bottom-left toast notifications for pickups, status effects (with countdowns), and talent point gains.
- Status effect icons render above the player character; talent tree nodes display stat icons tinted by archetype color.

### 4.3 Spectator Drones (Dead Players)

- On death, player controls a **spectator drone**:
  - Drone flies freely (limited radius).
  - Grants vision to team.
  - Has low-frequency utility: e.g., an EMP burst every 20s that briefly stuns or slows enemies.

---

## 5. Heroes, Classes & Progression

### 5.1 Archetypes

High-level archetypes: :contentReference[oaicite:9]{index=9}  

- **Tank**
- **Healer**
- **Damage Dealer (DPS)**
- **Assassin**
- **Builder**
- **Support**
- **Special**
- **Hidden** (secret unlocks)

Each hero belongs to a **class line** (e.g., Assault, Builder). Persistent overall kill counts per class are used to unlock higher-tier heroes in that line.

Example: Unlock **Tier-2 Assault hero** at **100,000 total Assault kills**.

### 5.2 Sample Tier-1 Hero Roster

From the refined gameplay loop: :contentReference[oaicite:10]{index=10}  

| Class        | Hero (Tier 1) | Kit Summary                                                                                      |
| ------------ | ------------- | ------------------------------------------------------------------------------------------------ |
| Tank         | Bulwark       | Magnetic shield dome, AoE taunt pulse, passive regen.                                           |
| DPS          | Arcstriker    | Chain-lightning rifle, overheat damage mechanic, ion trail dash.                                |
| Healer       | Nano-Nurse    | Cone heals with HoT spores, bonus revive speed, can convert heals into DoT vs enemies.         |
| Assassin     | Wraith        | Short cloak, high backstab multiplier, teleport beacon recall.                                 |
| Builder      | Wrench        | Deploys turrets & drones; collects scrap for discounts and special builds.                     |
| Support      | Maestro       | Swappable aura “songs” for buffs; shockwave ultimate scaling with number of nearby allies.     |

Tier-2+ and EX heroes add exotic mechanics like terrain manipulation, time-dilation fields, or summoned symbiotes.

### 5.3 In-Match Hero Upgrades

- Heroes gain XP by:
  - Killing enemies.
  - Completing events.
  - Assisting kills (heals, shields, etc.).
- Level-ups grant:
  - Ability ranks.
  - Choice of **2–3 augments per ability** (branching upgrade paths).
- **Talent Tree (match-scoped)**
  - Earn 1 talent point every 5 levels (floor(level / 5)).
  - Talents reset at match end; no persistence to save file.
  - `N` opens the talent tree and pauses all timers while open.
- Boss waves drop **phase shards**:
  - Used for **Hero Evolution (EX form)** at high waves (e.g., 60+).
  - EX form changes visual model, modifies ultimate and possibly core playstyle.

### 5.4 Persistent Progression

Persistent systems (saved locally in encrypted file):

- **Career Kill Records**
  - Per hero.
  - Per archetype (Assault, Builder, etc.).
  - Global total kills.
- **Prestige** (future meta progression)
  - Planned: reset meta unlocks for long-term goals.
  - Grants permanent cosmetic borders/badges.
- **Challenges**
  - Example: “Finish Hard difficulty using only Builder heroes.”
  - Award: unique skins, titles, or permanent small passive buffs.

---

## 6. Enemies, Bosses & Wave Director

### 6.1 Enemy Taxonomy

Use a tiered taxonomy: :contentReference[oaicite:11]{index=11} :contentReference[oaicite:12]{index=12}  

- **Trash**
  - Example: *Skitterlings* – swarm melee, low HP, explode on death.
- **Elites**
  - Example: *Warp Revenants* – periodically phase shift (invulnerable), fire piercing beams.
- **Mini-Bosses**
  - Example: *Gravemind Root* – stationary plant that spawns adds, buffs nearby enemies, pulls players.
- **Mega-Bosses**
  - Example: *Star-Eater Leviathan* – multi-segmented boss circling the map with weak spots that light up.

Additional attributes per enemy:

- Movement: Fast/weak vs slow/strong.
- Type: Ground, flying, ethereal/phase.
- Resistances: Physical/energy.
- AI behavior: swarm, siege, sniper, support.

### 6.2 Adaptive Wave Director

- Tracks player performance (DPS, deaths, time-to-clear).
- Adjusts:
  - Composition (more shielded chargers if players rely heavily on sniping).
  - Spawn rate and density.
  - Elite/miniboss frequency.
- Difficulty multipliers:
  - Enemy HP (per difficulty level).
  - Optional modifiers to damage output, move speed, etc.

### 6.3 Boss Mechanics

- Bosses have distinct phases with telegraphed attacks.
- Milestone bosses add new abilities at higher difficulties.
- Encourage team roles:
  - Tanks soak/taunt major hits.
  - Builders drop defensive structures and slows.
  - DPS focus weak points.
  - Assassins eliminate priority targets (summoners, healers).

---

## 7. Maps, Hotzones & Events

### 7.1 Map Layout

Each map features:

- **5 Hotzones**:
  - Top-middle, bottom-middle, left-middle, right-middle, and center.
  - Being inside a hotzone yields a rotating bonus but adds risk (more spawns, chokepoints, hazards).
- Potential bonuses:
  - XP Surge: +25% XP.
  - Danger-Pay: +50% gold.
  - Warp Flux: randomized buff with extra elite spawns. :contentReference[oaicite:13]{index=13} :contentReference[oaicite:14]{index=14}  

Environmental features:

- Hazards: collapsible bridges, lava vents, fog that disables radar, treacherous traversal with limited exits.
- Assistive elements: hackable turrets, shield pylons, one-use blast charges.

### 7.2 Map Variants

Initial set (per Gameplay Loop refinement): :contentReference[oaicite:15]{index=15}  

1. **Shattered Plains** – Open spaces, long sightlines, minimal cover.
2. **Orbital Wreckyard** – Tight corridors, conveyor belts that move units.
3. **Crystal Caverns** – Multilevel ramps; crystals amplify energy-based damage (both players and enemies).

### 7.3 Events & Side Objectives

 Examples: :contentReference[oaicite:16]{index=16}  

- **Escort Duty (replaces Salvage Run)**
  - A non-combatant NPC spawns near the player and walks 500–1200 units toward a random destination.
  - Enemies prioritize attacking the escort during the event.
  - Success (escort survives to destination): hefty gold payout (configurable `events.salvageReward`) + standard event XP/gold.
  - Failure: escort dies or timer expires while in combat.
- **Power Core Escort**
  - Slow-moving core targeted by enemies.
  - Rewards: XP + global damage buff for limited duration.
- **Bounty Hunt**
  - Elite hunter squad invades; marked on HUD.
  - Rewards: Phase shard(s) for hero evolution.

Participation is optional. Skipping means lower risk but missed power spikes.

---

## 8. Difficulty, Modes & Win Conditions

### 8.1 Difficulty Scale

Base difficulties and their parameters (enemy HP multiplier, starting wave, extra rules): :contentReference[oaicite:17]{index=17} :contentReference[oaicite:18]{index=18}  

| Difficulty   | Enemy HP  | Start Wave | Notes                                                        |
| ------------ | --------- | ---------- | ------------------------------------------------------------ |
| Very Easy    | 0.8×      | 1          | Shorter boss telegraphs.                                    |
| Easy         | 0.9×      | 1          | Default for new players.                                    |
| Normal       | 1.0×      | 1          | Baseline tuning.                                            |
| Hard         | 1.2×      | 10         | Bosses gain one new ability.                                |
| Chaotic      | 1.4×      | 20         | Currency gain –15%.                                         |
| Insane       | 1.6×      | 40         | Random mutator every 10 waves.                              |
| Torment I-VI | 2.0–3.0×  | 60+        | Stacking debuff “Restless Spirits” (−1% max HP per wave).   |

### 8.2 Win Conditions per Difficulty

Approximate target victory waves: :contentReference[oaicite:19]{index=19}  

- Very Easy: Wave 50.
- Easy: Wave 75.
- Normal: Wave 100.
- Hard: Wave 150.
- Chaotic: Wave 175.
- Insane: Wave 200.
- Torment I–VI: 250+.

### 8.3 Challenge / Daily Modes

- **Challenge Modes**
  - Pre-baked scenarios with constraints:
    - “Builders only.”
    - “No shop.”
    - “Double bosses, no mini-events.”
- **Daily Challenge**
  - Rotating global mutator set.
  - Leaderboard for highest wave / fastest clear.
  - Cosmetic/badge rewards.

---

## 9. Meta Systems, Persistence & Monetization

### 9.1 Persistence

All persistent data is stored in an **encrypted local save file**:

- Heroes unlocked.
- Kills per hero/class/global.
- Challenges completed.
- Cosmetics owned/unlocked.
- Career stats (waves reached, bosses killed, etc.). :contentReference[oaicite:20]{index=20}  

Match-scoped talents are excluded from persistence and reset every run.

Save format and encryption details are defined in the Technical section below.

### 9.2 Kill-Based Economy

- **In-match**: Enemies drop currency (“credits”) for:
  - Hero ability upgrades.
  - Item purchases from in-run Shop.
- **Out-of-match**: Persistent **kill totals** are used to:
  - Unlock new heroes.
  - Unlock cosmetics and (future) prestige tiers.
  - Purchase cosmetics and vanity items (kill sink).

Example cosmetic purchase:

- Golden Tank skin costs **10,000,000 kills**, permanently deducted from global kill total. :contentReference[oaicite:21]{index=21}  

### 9.3 Monetization (Optional / Future)

- No pay-to-win.
- Season Pass may be gated by earned kills (playtime) with optional cosmetic-only premium track.
- VO packs, pets, banners, and profile decorations purchasable by kills or external store, but never give power.

---

## 10. Technical Constraints & Architecture (For Codex)

### 10.1 Language & Platform

- Language: **C++20** or **C# (latest LTS .NET)**.
  - If not explicitly specified by the user, **default to C++20**.
- Target platforms:
  - PC first (Windows).
  - Architecture must be portable to Linux/macOS (avoid platform-locked assumptions in core).

### 10.2 No Engine Policy

- **Prohibited**:
  - Unity, Unreal, Godot, proprietary closed engines.
- **Allowed** (as libraries/frameworks, not engines):
  - Windowing/input/audio: SDL2, GLFW, SFML, or platform APIs.
  - Rendering: OpenGL, Vulkan, DirectX (via appropriate bindings).
  - Networking: ENet, custom sockets, or standard .NET networking for C#.
  - Math: glm or equivalent.

### 10.3 Project Structure

Suggested C++ layout:

- `/engine`
  - `core/` – Game loop, time, logging.
  - `platform/` – Window, input, OS abstraction.
  - `render/` – Renderer, camera, sprites, basic UI primitives.
  - `audio/` – Sound playback.
  - `net/` – Multiplayer (later milestone).
  - `ecs/` – Entity-component system.
  - `assets/` – Resource loading, texture, audio, configs.
- `/game`
  - `heroes/` – Hero definitions, abilities, talent hooks.
  - `enemies/` – Enemy archetypes, AI behaviors.
  - `waves/` – Wave director, difficulty curves, mutators.
  - `maps/` – Map layouts, hotzones, events.
  - `ui/` – HUD, menus, shop.
  - `meta/` – Persistence, progression, economy.
- `/data`
  - JSON/YAML/toml configs for heroes, enemies, waves, talents, items.
  - Map layout definitions.
- `/assets`
  - Sprites, sounds, fonts, VFX data.

Use CMake (C++) or `dotnet` solution (C#) for build orchestration.

### 10.4 Core Engine Patterns

- Use **Entity-Component-System (ECS)**:
  - Entities: IDs only.
  - Components: position, renderable, health, ability, AI state, etc.
  - Systems: movement, combat, AI, rendering, input, wave spawning.
- Strict separation:
  - Engine layer should be agnostic of “Zeta” lore.
  - Game layer references engine and implements specific content.

### 10.5 Save System

- Serialize meta data to a JSON or binary blob.
- Encrypt using AES-256 (or similar) with:
  - Key derivation from user-specific seed + salt.
  - Obfuscation to discourage manual tampering (not strong DRM).
- Implement versioning:
  - Include `save_version` and migration hooks for future patches.

### 10.6 Networking (Later Milestone)

- Up to 6 players:
  - Start with **lockstep or authoritative host** model.
  - Use deterministic simulation where feasible; otherwise accept mild divergence with periodic state sync.

---

## 11. Engineering Milestones (Tasks for Codex)

Codex should implement the game in a sequence of small, verifiable milestones.

### Milestone 0 – Bootstrap

- Create base project:
  - C++: CMake project with SDL2 + OpenGL (or chosen stack).
  - C#: .NET console/window app with chosen rendering backend.
- Implement:
  - Main loop (fixed time-step update; decoupled render).
  - Window creation, input polling, basic logging.
- Output: blank window, ESC to quit.

### Milestone 1 – ECS & Rendering Basics

- Implement minimal ECS framework.
- Add:
  - Camera system (pan/zoom).
  - Simple sprite rendering system.
- Test: place placeholder hero sprite and test camera controls (WASD pan, zoom wheel, center-on-hero).

### Milestone 2 – Hero Control & Input

- Implement a controllable hero entity:
  - Movement (keyboard or click-to-move).
  - Simple primary attack with cooldown.
- Implement ability abstraction:
  - Ability cooldowns, casting, targeting.

### Milestone 3 – Wave Spawner & Basic Enemies

- Add enemy archetypes (trash + simple elite).
- Implement wave spawner with:
  - Wave timer.
  - Spawn patterns and scaling.
- Implement basic combat:
  - Damage, health, death, simple hit feedback.

### Milestone 4 – Meta Systems Skeleton

- Implement in-run currency, XP, and level-up.
- Implement simple Shop UI:
  - Purchase basic stat items and ability upgrades.
- Implement placeholder persistent save:
  - Track total kills per hero and in total.
  - Save/load on exit/start.

### Milestone 5 – Hotzones & Map Layout

- Implement single map with:
  - 5 hotzones and rotating bonus logic.
  - Simple environmental hazards.
- Implement event triggers for entering/exiting zones.

### Milestone 6 – Bosses & Events

- Implement:
  - Basic boss entity with multiple phases.
  - Mini-events (e.g., Escort Duty).
- Integrate rewards: buff, phase shards, extra currency.

### Milestone 7 – Full Progression + Talent Tree

- Implement match-scoped talent tree UI and logic; prestige remains a future meta goal.
- Connect persistent stats, hero unlocks, and challenges.
- Implement kill-sink cosmetic purchase.

### Milestone 8 – Networking Prototype

- Implement basic co-op:
  - Host + clients.
  - Synchronize hero actions, enemy states, waves.
- Keep simulation deterministic enough for wave shooter.

### New in v0.0.106 – RPG Combat / Loot / Talents

- Shared RPG stat model (STR/DEX/INT/END/LCK → derived AP/SP/AS/MS/ACC/EVA/CRIT/ARM/RES/TEN/shields) with clampable resists and armor curve `ARM/(ARM+K)`.
- Single combat resolver order: hit quality → dodge/parry → crit → roll band (shaped) → armor/resist → shields → on-hit statuses with tenacity “saving throw” and CC fatigue (DR: 1.0 → 0.7 → 0.5 → immune).
- Data-first attack/loot/talent structs live under `engine/gameplay/RPG*` and `game/rpg/`, usable by players and AI.
- Loot generator rolls rarity (Luck-aware), base templates, and affixes; per-rarity stat/affix scalars add roll-time variance; equipment contributions feed the aggregation pipeline.
- Talent nodes award match-permanent stat contributions; consumables share cooldown categories (heal/cleanse/buff/bomb/food).
- Character select now shows archetype biography, core attributes, specialties, and perk blurb sourced from `data/rpg/archetypes.json`.
- Feature flags in `data/gameplay.json` (`useRpgCombat`, `useRpgLoot`, `rpgCombat` block) allow incremental rollout; loot/consumables/talents load from `data/rpg/*.json` with fallbacks.

### New in v0.0.108 – RPG Equipment Expansion

- Expanded `data/rpg/loot.json` to cover the full equipment sprite sheets under `assets/Sprites/Equipment/` (armor, weapons, gems, runes, tomes, shields, quivers, arrows, scroll).
- Added a paper-doll equipment container and Character screen click-to-equip flow; only equipped items contribute to live RPG stat aggregation.
- Loot templates include icon metadata (`iconSheet/iconRow/iconCol`); `scripts/gen_rpg_loot.py` regenerates the table deterministically.

### New in v0.0.138 – RPG Status + Elemental Damage

- RPG resolver on-hit statuses can now be fed from gameplay `DamageEvent` metadata and applied into the engine `StatusContainer` (TEN saving throw + CC DR).
- Elemental `SpellEffect` now maps into RPG damage types so resistances/vulnerabilities can matter beyond Physical/Arcane/True.
- RNG shaping supports optional PRD-style anti-streak for crit/dodge/parry via `data/gameplay.json` (`rpgCombat.rng.usePRD`).
- Consumable Buff effects can carry data-driven stat contributions via `data/rpg/consumables.json` (`effects[].stats`).

### New in v0.0.181 – Economy + Consumable UI

- Item sell value scales with rarity and affix/socket bonuses (tunable in `data/gameplay.json` under `economy`).
- Character screen details/tooltip now show food regen and potion restore amounts.
- Robin weapon swap ranges are tunable via `data/gameplay.json` (`weaponSwap` block).

### New in v0.0.191 – Attribute Growth Hooks

- Epic+ RPG gear now rolls random attribute bonuses (values and chances are data-driven in `data/rpg/loot.json`).
- Tier 5 talent nodes grant attribute bonuses alongside their existing stat effects (tuned in `data/rpg/talents.json`).

### New in v0.0.192 – Loot Rolls + Wave Scaling

- Non-Unique RPG gear now ignores static base stats; power comes from rolled implicit stat lines + affixes.
- Weapons/ammo use combat-type implicit stat pools (Melee/Ranged/Magical/Ammo) with per-rarity roll counts.
- Item level scales from wave; implicit stats, affixes, and Unique base stats scale by ilvl.
- RPG tooltips now show item level and rolled implicit stats.
- Cleave chance, lifesteal, life-on-hit, and status chance hooks added to RPG stats/combat.

### New in v0.0.193 – Armor Guarantees + Empty Gear Fix

- Armor slots always include at least one +Armor line (implicit if needed).
- Non-consumable gear now guarantees at least one stat line; socketables fall back to scaled base stats if otherwise empty.

### New in v0.0.194 – Tooltip Cleanup

- Hover tooltips now show only total RPG stat lines to avoid duplicate implicit entries.

### New in v0.0.195 – Compare Coverage

- RPG compare tooltip now reports deltas for all major derived stats and resistances.

### New in v0.0.196 – Armor Rarity Scaling

- Armor implicit rolls now scale with rarity (tunable via `armorImplicitScalarByRarity` in `data/rpg/loot.json`).

---

## 12. Coding Norms (Instructions for Codex)

When writing code for this project:

1. **Always ensure project builds**:
   - Keep changes compilable at each commit-level unit.
2. **Prefer explicit, simple code over cleverness**:
   - Focus on reliability and readability.
3. **Document public APIs**:
   - Short comments for core engine structures and systems.
4. **Separate data from logic**:
   - Use config files for tunable values (HP, damage, wave timings, etc.).
5. **Write small, composable units**:
   - Avoid monolithic “god classes.”
6. **Guard against failure**:
   - Check nulls, handle load failures gracefully (log and fallback).
7. **Include TODO tags**:
   - When stubbing a feature, add `// TODO: ...` with clear description.

When unsure between two designs, prefer the one that:

- Keeps the engine generic.
- Keeps game-specific logic in `/game`.
- Minimizes hidden coupling (e.g., explicit events/messages instead of global state).
