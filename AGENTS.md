# AGENTS.md

This document defines distinct “AI agent” roles for working on **Project Aurora Zeta**. Each agent can be instantiated as Codex (or similar) with a specific mandate and scope. The goal is to coordinate multiple specialized assistants on a single codebase.

---

## 1. Shared Rules for All Agents

All agents must follow these constraints:

1. **Language & Stack**
   - Main language: **C++20** (or C# if explicitly directed).
   - No use of game engines like Unity, Unreal, or Godot.
   - Lightweight libraries (SDL2, OpenGL, etc.) are allowed.

2. **Architecture Respect**
   - Keep engine-agnostic code inside `/engine`.
   - Keep game-specific code (heroes, Zeta’s lore, waves) inside `/game`.
   - Do not introduce tight coupling across these layers.

3. **Build Discipline**
   - Do not introduce changes that break the build.
   - If refactoring, update all affected code paths.
   - Prefer small, incremental changes over large rewrites.

4. **Documentation**
   - When creating new systems, add short docstrings and/or markdown notes as needed.
   - Maintain updated comments in `GAME_SPEC.md` and related docs when behavior changes.

5. **Data-Driven Design**
   - Expose tunable values (HP, damage, spawn rates, difficulty multipliers) in data files.
   - Avoid hardcoding balancing numbers in code.

6. **Testing & Validation**
   - Whenever possible, add simple tests or test harnesses for core logic.
   - Manually describe how to test new features in commit notes or comments.

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
   - Each agent works within its domain, referencing `GAME_SPEC.md`.
3. **Integrate**
   - After implementing features, agents ensure code builds and runs.
4. **Validate**
   - QaAgent performs sanity checks and updates test checklist.
5. **Document**
   - Agents update relevant sections in `GAME_SPEC.md` when behavior changes.
   - MetaProgressionAgent updates any persistence/format docs when save structure changes.

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

By adhering to these role definitions and shared rules, multiple Codex instances (or similar AI developers) can collaboratively build and maintain the Project Aurora Zeta codebase while preserving a coherent architecture and design.
