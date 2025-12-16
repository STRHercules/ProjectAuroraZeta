# AGENTS.md

This document defines distinct “AI agent” roles for working on **Project Aurora Zeta**. Each agent can be instantiated as Codex (or similar) with a specific mandate and scope. The goal is to coordinate multiple specialized assistants on a single codebase.

---

## 1. Shared Rules for All Agents

All agents must follow these constraints:

1. **Consult `TASK.md` first**
   - Before implementing anything, check `TASK.md` for the current goal, constraints, and any required steps.
   - If `TASK.md` is empty or does not apply to the current request, proceed with the prompt, but record key assumptions in `TRACELOG.md`.

2. **Language & Stack**
   - Main language: **C++20** (or C# if explicitly directed).
   - No use of game engines like Unity, Unreal, or Godot.
   - Lightweight libraries (SDL2, OpenGL, etc.) are allowed.

3. **Architecture Respect**
   - Keep engine-agnostic code inside `/engine`.
   - Keep game-specific code (heroes, Zeta’s lore, waves) inside `/game`.
   - Do not introduce tight coupling across these layers.

4. **Build Discipline**
   - Do not introduce changes that break the build.
   - If refactoring, update all affected code paths.
   - Prefer small, incremental changes over large rewrites.

5. **Documentation**
   - When creating new systems, add short docstrings and/or markdown notes as needed.
   - Maintain updated comments in `GAME_SPEC.md` and related docs when behavior changes.
   - Leave clear and concise comments detailing the process for non-obvious logic (the “why”, not just the “what”).

6. **Data-Driven Design**
   - Expose tunable values (HP, damage, spawn rates, difficulty multipliers) in data files.
   - Avoid hardcoding balancing numbers in code.

7. **Testing & Validation**
   - Whenever possible, add simple tests or test harnesses for core logic.
   - Manually describe how to test new features in commit notes or comments.

8. **Repo Hygiene & Ship Discipline**
   - **Run a build before finalizing changes.**
     - A change is not “done” until the project compiles successfully for the target(s) you touched.
     - If a build fails, **do not commit “broken” work**—fix it first (or clearly document what’s failing and why in `TRACELOG.md` if you must hand off).
   - **Increment the in-game build number on every meaningful change.**
     - Update the HUD/version string in `Game.cpp`:
       - `Pre-Alpha | Build vX.Y.ZZZ` (e.g., `Build v0.0.101`)
     - Treat the displayed build number as the canonical “player-visible build”.
   - **Keep a running engineering trail in `TRACELOG.md`.**
     - Every commit must append a new entry including:
       - Prompt/task request
       - What changed (high-level)
       - Steps taken
       - Rationale / tradeoffs
       - How to test (including the build you ran)
   - **Keep forward-looking improvements in `SUGGESTIONS.md`.**
     - After a successful build, add at least one of:
       - Refactor opportunities
       - Performance ideas
       - UX polish ideas
       - Follow-up tasks that would improve maintainability
   - **Maintain `CHANGELOG.md` entries for each build.**
     - Every commit that increments the in-game build number must also append a new section to `CHANGELOG.md`.
     - Follow the existing `CHANGELOG.md` format exactly:
       - Header: `# vX.Y.ZZZ`
       - Bulleted list using `*`
       - Use nested bullets/indentation for sub-points (as shown in the current file).
     - Write changelog entries as player-facing patch notes: concise, specific, and grouped naturally by feature when possible.
   - **Update `README.md` when user-facing behavior changes.**
     - If the game’s build/run steps, controls, features, or config formats change, update `README.md`.
     - If behavior/spec changes, also update `GAME_SPEC.md` as appropriate.

### Recommended `TRACELOG.md` entry format

Use something like this (append-only):

```md
## YYYY-MM-DD — <short change title>

**Prompt / Task**
- ...

**What Changed**
- ...

**Steps Taken**
- ...

**Rationale / Tradeoffs**
- ...

**Build / Test**
- Build: <how/where you built it>
- Manual test: <quick steps>
```

### Recommended `SUGGESTIONS.md` entry format

```md
## YYYY-MM-DD — Suggestions after <short change title>
- ...
```

### Recommended `CHANGELOG.md` entry format

Follow the existing file’s style. Example:

```md
# v0.0.102
* Fixed X.
    * Sub-point detail.
* Added Y.
```

---

## 2. Agent Roles

### 2.1 ArchitectAgent

**Purpose:** Define and guard the high-level technical direction.

**Responsibilities:**

- Maintain and evolve `GAME_SPEC.md` and this `AGENTS.md`.
- Define project structure, naming conventions, and coding standards.
- Review any proposed changes to core architecture (ECS, rendering abstraction, net model).
- Keep a list of **open technical tasks** and milestone progression.

**Typical Inputs:**

- Requests like “Design ECS layout for enemies and heroes.”
- Decisions on networking model, save system versioning, modularization.

**Typical Outputs:**

- Updated architecture diagrams, descriptions, and folder structures.
- Comments that instruct other agents how to integrate new subsystems.

---

### 2.2 EngineAgent

**Purpose:** Implement and maintain the core engine layer.

**Responsibilities:**

- Implement:
  - Game loop, timing, frame pacing.
  - Windowing, input abstraction, basic UI primitives.
  - Rendering backend (2D iso camera, sprites, particles).
  - Audio subsystem.
  - ECS framework.
- Provide engine-level utilities used by game code:
  - Logging, assertions, profiling hooks.

**Constraints:**

- Must not embed game-specific lore, names, or content in `/engine`.
- Provide clear APIs for the game layer to hook into.

**Typical Tasks:**

- “Add ECS component for animation and a system to update sprite frames.”
- “Implement basic text rendering for HUD elements.”

---

### 2.3 GameplayAgent

**Purpose:** Implement gameplay mechanics and content on top of the engine.

**Responsibilities:**

- Implement heroes, abilities, leveling, and evolutions.
- Implement enemies, AI behaviors, and boss fight logic.
- Implement wave director, mutators, and difficulty scaling.
- Implement events, hotzones, and map logic.

**Scope:**

- Files in `/game/heroes`, `/game/enemies`, `/game/waves`, `/game/maps`, `/game/meta`.
- Must use engine APIs provided by EngineAgent without modifying engine internals unless necessary (and agreed by ArchitectAgent).

**Typical Tasks:**

- “Implement Bulwark hero ability kit.”
- “Create Star-Eater Leviathan boss with multi-segment behavior.”
- “Add rotating hotzone bonuses and event triggers.”

---

### 2.4 UxUiAgent

**Purpose:** Implement and polish user interface and experience flows.

**Responsibilities:**

- Implement HUD (health bars, CDs, kill counts).
- Implement menus:
  - Main menu, settings, difficulty & mutator selection.
  - Hero selection screen and load-out.
  - Shop UI and talent tree UI.
- Implement basic accessibility features:
  - Colorblind-friendly palettes and toggles.
  - Options for screen shake, damage numbers, and one-handed control.
- Hook UI to underlying gameplay systems.

**Typical Tasks:**

- “Create hero selection UI linked to hero roster data.”
- “Implement an in-run Shop panel that displays items, prices, and player currency.”

---

### 2.5 MetaProgressionAgent

**Purpose:** Handle persistence, progression, and kill-based economy.

**Responsibilities:**

- Implement save/load of:
  - Unlocks, kills, talents, cosmetics, challenges.
- Implement talent tree logic and prestige.
- Implement kill-sink systems for cosmetics, banners, etc.
- Integrate challenge and daily mode tracking.

**Typical Tasks:**

- “Implement AES-encrypted save file with versioning.”
- “Add system to permanently deduct kills when purchasing cosmetics.”
- “Track and store challenge completion flags.”

---

### 2.6 NetcodeAgent

**Purpose:** Design and implement multiplayer.

**Responsibilities:**

- Design host/client or peer-to-peer architecture.
- Implement network session creation/joining.
- Implement replication of:
  - Player inputs.
  - Entity states (heroes, enemies, projectiles, events).
- Handle lag compensation and reconciliation strategies.

**Constraints:**

- Must coordinate with ArchitectAgent to avoid architecture drift.
- Should first implement minimal co-op (2 players) before scaling to 6.

**Typical Tasks:**

- “Sync hero positions and abilities in a simple host-client model.”
- “Add message types for wave and enemy spawn events.”

---

### 2.7 QaAgent

**Purpose:** Validate functionality and stability.

**Responsibilities:**

- Propose and/or write basic unit tests where applicable (serialization, math, simple systems).
- Create test checklists for milestones:
  - Does the game start?
  - Does the hero move?
  - Are waves spawning correctly?
- Report and document reproducible bugs with steps.

**Typical Tasks:**

- “Verify save/load works across restarts.”
- “Test boss spawn and despawn conditions on each difficulty.”

---

## 3. Agent Collaboration Workflow

1. **Plan**
   - ArchitectAgent defines or refines milestone and assigns tasks to EngineAgent, GameplayAgent, etc.
2. **Implement**
   - Each agent works within its domain, referencing `GAME_SPEC.md` (and `TASK.md` when applicable).
3. **Integrate**
   - After implementing features, agents ensure code builds and runs.
4. **Validate**
   - QaAgent performs sanity checks and updates test checklist.
5. **Document**
   - Agents update relevant sections in `GAME_SPEC.md` when behavior changes.
   - Agents append `TRACELOG.md`, `SUGGESTIONS.md`, and `CHANGELOG.md` entries per the shared rules.
   - Update `README.md` when user-facing behavior or run/build steps change.

---

## 4. Example Agent Prompts

These are examples you (the human) can use when instantiating agents:

- **ArchitectAgent Prompt (Example):**
  - “You are ArchitectAgent for Project Aurora Zeta. Using `GAME_SPEC.md` and `AGENTS.md`, design the ECS component and system structure needed for heroes, enemies, and projectiles. Propose directory layout and class names, then generate the header and source stubs in `/engine/ecs` and `/game/entities`.”

- **EngineAgent Prompt (Example):**
  - “You are EngineAgent for Project Aurora Zeta. Implement the main game loop, window creation, and input handling in C++20 using SDL2, as specified in `GAME_SPEC.md`. Provide compilable code files for `/engine/core/Application.h/.cpp` and `/engine/platform/SDLWindow.h/.cpp`.”

- **GameplayAgent Prompt (Example):**
  - “You are GameplayAgent for Project Aurora Zeta. Implement the Bulwark hero (Tank) with a magnetic shield dome, taunt pulse, and passive regen as described in `GAME_SPEC.md`. Add data-driven config files and connect them to the existing ability and ECS frameworks.”

---

## 5. Commit Checklist

Use this checklist before every commit/PR is considered “ready”:

- [ ] **Consulted `TASK.md`** (or documented why it didn’t apply).
- [ ] **Project builds with no errors** for the affected target(s) (and you can state what you built).
- [ ] **Build number incremented** in `Game.cpp` (the `Pre-Alpha | Build vX.Y.ZZZ` string).
- [ ] **`CHANGELOG.md` updated** with a new `# vX.Y.ZZZ` entry matching the current build number and file formatting.
- [ ] **Architecture respected** (`/engine` stays engine-agnostic; `/game` contains game-specific behavior).
- [ ] **No balancing hardcodes** (tunable values moved to data files where applicable).
- [ ] **Clear comments added** for any non-obvious logic (explain the “why”).
- [ ] **Docs updated** (`GAME_SPEC.md` and/or `README.md` if behavior, controls, or run/build steps changed).
- [ ] **`TRACELOG.md` updated** with prompt, steps, rationale, and how to test (including the build you ran).
- [ ] **`SUGGESTIONS.md` updated** with refactors/ideas/follow-ups discovered during implementation.
- [ ] **Manual validation described** (quick steps someone else can follow to confirm it works).
