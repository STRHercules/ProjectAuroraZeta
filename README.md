# Project Aurora Zeta

Minimal C++20 scaffold for **Project Zeta** (engine/game separation). Current backend uses a `NullWindow` placeholder so the loop exits immediately after initialization.

## Building on Linux for Linux
```bash
# Requisites
sudo apt install -y build-essential cmake libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev

# cd to the root of the repo
# Build the Project
cmake -S . -B build
cmake --build build

# Run Zeta
./build/zeta
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

The distributable zip is written to `dist/windows/zeta-win64.zip` and contains `zeta.exe`, required SDL DLLs, data, and a `start.bat` launcher.

## Layout
- `engine/` — engine-agnostic systems (Application loop, logging, window abstraction).
- `game/` — game-specific bootstrap implementing engine callbacks.
- `src/` — executable entry point.
- `engine/ecs/` — minimal ECS registry for upcoming gameplay systems.
- `data/input_bindings.json` — data-driven key bindings loaded at runtime (falls back to defaults if missing).

## Prototype Controls
- Move: `WASD`
- Fire: `Mouse1`
- Toggle camera follow: `C`
- Restart run (dev helper): `R`
- Open/close placeholder shop during intermission: `B`
- Pause: `Esc`
- Shop actions (while open, during intermission):
  - `Mouse1` buy +Damage
  - `Mouse2` buy +HP
  - `Mouse3` (middle) buy +Move Speed

## Tuning Data
- `data/gameplay.json` drives hero HP/moveSpeed/size, projectile speed/damage/visualSize/hitboxSize/lifetime/fire-rate, enemy HP/speed/contactDamage/size/hitboxSize, wave cadence (interval, batch size, initial delay, post-wave grace), and rewards (currencyPerKill).

Refer to `GAME_SPEC.md` and `Original_Idea.md` for architecture and feature roadmap.