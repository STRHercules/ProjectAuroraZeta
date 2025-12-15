# Project Aurora Zeta

![](/assets/icon512.png)

Survivor-style, wave-based ARPG built in C++20 with a lightweight in-house engine. The project is split cleanly between an engine layer (`/engine`) and game layer (`/game`) to keep systems reusable.

The current build includes:
- Character & difficulty selection, solo or host/client multiplayer.
- Wave director with intermissions, shop, hotzones, and event hooks.
- Basic hero kits, abilities, and data-driven tuning files.
- SDL2 rendering/input, bitmap text, ECS, and networking scaffold.


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
- Toggle Ability Shop: `B`
- Pause: `Esc`
- Restart run (debug helper): `Backspace`
- Menu back (fallback): `M`
- Mouse: primary to fire/confirm, secondary/tertiary used inside shop prompts

## In-Game Options (Main Menu → Options)
- Damage numbers: on/off
- Screen shake: on/off
- Movement mode:
  - **Modern** (WASD moves hero, camera follow)
  - **RTS-like** (RMB move + WASD pans camera; follow disabled by default)

## Gameplay Loop (current prototype)
1) **Main Menu** → Solo or Host/Join.
2) **Character & Difficulty Select**  
   - Archetypes (Tank, Healer, Damage, Assassin, Builder, Support, Special).  
   - Difficulties: Very Easy, Easy, Normal, Hard, Chaotic, Insane, Torment.
3) **Drop-in** to map with rotating hotzones (bonus XP/gold/risks).  
4) **Waves**: enemies spawn in batches; defeat all to trigger intermission.  
5) **Intermission**: open shop (`B`), buy stats, roll for items, or reposition.  
6) **Events & Bosses**: periodic events + milestone bosses as waves escalate.  
7) **End**: wipe or reach target wave → stats screen, kill totals banked.

## Systems Overview
- **ECS Core**: lightweight registry in `engine/ecs`, components for transforms, renderables, projectiles, health, tags, etc.
- **Combat**: projectile + collision systems, hit flash, damage numbers, buffs, shopkeeper interactions.
- **Wave Director**: configurable spawn batches, pacing, grace timers; tuned per difficulty via data files.
- **Hotzones**: rotating map bonuses (XP/Gold/Flux) managed by `HotzoneSystem`.
- **Events**: `EventSystem` placeholder for timed objectives; markers displayed on HUD.
- **Shop**: intermission-only; buys damage/HP/speed; costs and bonuses stored in data.
- **Abilities**: loaded from `data/abilities.json` (falls back to `_default`); slots for 3 actives + ultimate with cooldowns and scaling.
- **Archetypes**: defined in `data/menu_presets.json` (fallback defaults in code); adjust HP/Speed/Damage multipliers and set hero textures.
- **Difficulties**: same file, each with enemy HP multiplier and starting wave; selection flows into wave tuning and lobby/session config.
- **Networking**: host/client session (`game/net`), lobby state sync (heroes, difficulty, max players), snapshot replication for positions and inputs.

## Data-Driven Tuning
- `data/gameplay.json` — hero base stats, projectile speed/damage/size, enemy HP/speed/contact damage, wave cadence, rewards.
- `data/abilities.json` and `data/abilities_default.json` — ability names, descriptions, cooldowns, and tags.
- `data/menu_presets.json` — archetype & difficulty lists; colors and descriptions; used for character select UI.
- `data/input_bindings.json` — key bindings described above.

## Project Layout
- `engine/` — platform/renderer/input abstractions, ECS, logging, time, asset management.
- `game/` — gameplay bootstrap, menus, systems (combat, buffs, AI, events, hotzones, shop, meta progression hooks).
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

## Building for Android (APK)
An Android build pipeline is not yet checked into the repo, but `docs/AndroidBuild.md` describes how to wire the existing CMake projects into the SDL2 Android template and package an APK with Gradle.
