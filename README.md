# Project Aurora Zeta

Minimal C++20 scaffold for **Project Zeta** (engine/game separation). Current backend uses a `NullWindow` placeholder so the loop exits immediately after initialization.

## Building
```bash
cmake -S . -B build
cmake --build build
./build/zeta
```

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

## Tuning Data
- `data/gameplay.json` drives hero HP/speed, projectile speed/damage/fire-rate, enemy HP/speed/contact damage, and wave spawn cadence (interval, batch size, initial delay).

Refer to `GAME_SPEC.md` and `AGENTS.md` for architecture and feature roadmap.
