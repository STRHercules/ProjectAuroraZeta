# Project Aurora Zeta

Minimal C++20 scaffold for **Project Zeta** (engine/game separation). Current backend uses a `NullWindow` placeholder so the loop exits immediately after initialization.

## Building

- Requisites: `sudo apt install -y build-essential cmake libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev`


```bash
cmake -S . -B build
cmake --build build
./build/zeta
```

### Windows (cross-build from Linux)

Prereqs: `mingw-w64`, `cmake`, `zip`, and the prebuilt SDL runtimes extracted under `~/winlibs/sdl` (matching the SDL2/SDL2_image/SDL2_ttf versions already staged there).

```bash
export WIN_SDL_ROOT=~/winlibs/sdl
./scripts/build-win.sh
./scripts/package-win.sh
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

Refer to `GAME_SPEC.md` and `AGENTS.md` for architecture and feature roadmap.
