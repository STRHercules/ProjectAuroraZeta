# Talent Tree (GUI + Systems) Implementation Guide

This document defines how to implement a rich, modern talent tree UI and the supporting gameplay logic for a fully featured MMO/RPG-style talent system in Project Aurora Zeta. It is written as instructions for Codex to follow.

---

## Goals

- Build a **modernized Talent Tree GUI** that matches the style and UX of the current GUI modernization work (clean layout, polished panels, readable typography, animated transitions).
- Implement a **data-driven** talent tree per champion (archetype), with prerequisites, branching, and tier unlocking.
- Talents are **match-scoped** (no persistence between matches). Talent points are **earned every 10 levels**.
- A dedicated **hotkey `N`** opens/closes the talent tree and **pauses the match and all timers** while open.

---

## Constraints / Rules

- Keep engine-agnostic code in `/engine`; game-specific talent content in `/game` and `data/`.
- **Do not hardcode balancing values** in code; expose tunables in JSON in `data/rpg/`.
- Talent points are **earned every 10 levels** (floor(level / 10)).
- Talents are **match-only** (no persistence between runs).
- `N` opens the talent tree and pauses **all time-based systems** (spawns, cooldowns, effects, timers).

---

## UI: Talent Tree GUI Requirements

### Layout & Presentation

- Full-screen overlay panel (modal) with a soft-dim background layer.
- Left panel: Champion info summary (name, level, points available/spent, short archetype bio).
- Center panel: **Interactive talent tree graph** with nodes and connecting edges.
- Right panel: Talent details (name, rank, description, bonuses, requirements).
- Bottom bar: Buttons for **Confirm**, **Reset**, **Close** (and optional **Auto-Assign** if desired).

### Interaction & UX

- **Keyboard hotkey `N`** toggles the talent tree. Open -> pause; Close -> resume.
- Nodes are clickable; hover shows quick tooltip.
- Node states:
  - Locked (requirements not met)
  - Available (requirements met + enough points)
  - Purchased (ranked)
  - Maxed
- Edge styling should visually indicate locked/unlocked branches.
- **Tier line** or row separators showing tier unlocking gates.
- Drag-to-pan + mousewheel zoom for large trees.
- Tab/arrow key navigation should be supported for controller/keyboard.
- Smooth transitions (fade-in, node highlight pulses, subtle zoom on open).

### SDL Rendering (Modern Features)

- Use the current rendering abstraction (likely SDL2 renderer + texture atlas).
- Leverage:
  - Alpha blending for overlays.
  - Nine-slice panels for scalable UI frames.
  - Dynamic text rendering with outline/shadow for readability.
  - Offscreen render targets for minimap/zoomed tree if available.
- Provide a clean **UI animation layer** (lerped alpha, scale, and color transitions).

---

## Talent Tree Data Model (Data-Driven)

### JSON Schema (Proposed)

Store in `data/rpg/talents.json` under each archetype id.

```json
{
  "talents": {
    "tank": {
      "tiers": [
        {
          "tier": 1,
          "unlockPoints": 0,
          "nodes": [
            {
              "id": "iron_wall",
              "name": "Iron Wall",
              "description": "+10 Armor",
              "maxRank": 3,
              "bonus": { "flat": { "armor": 10 } },
              "pos": [0, 0],
              "requires": []
            }
          ]
        },
        {
          "tier": 2,
          "unlockPoints": 3,
          "nodes": [
            {
              "id": "unyielding",
              "name": "Unyielding",
              "description": "+5 Tenacity",
              "maxRank": 3,
              "bonus": { "flat": { "tenacity": 5 } },
              "pos": [1, 0],
              "requires": ["iron_wall"]
            }
          ]
        }
      ]
    }
  }
}
```

### Fields

- `tiers[]` list contains tiers with an `unlockPoints` gate.
- Each node includes:
  - `id`, `name`, `description`, `maxRank`, `bonus`, `pos` (grid position), `requires[]` (prereq node ids).
- `pos` is for UI layout (grid coordinates).
- `bonus` uses existing `StatContribution` parsing.

---

## Gameplay Logic Requirements

### Allocation Rules

- Available points: `floor(level / 10)`.
- A node is **available** if:
  - Current tier is unlocked (`pointsSpent >= unlockPoints`), and
  - All `requires[]` nodes meet minimum rank (>= 1 unless specified), and
  - Player has >= 1 unspent points.
- Allocating a point:
  - Increments node rank.
  - Deducts 1 available point.
  - Recompute RPG stat contributions immediately.

### Tier Unlocking

- Tier unlock points can be configured per archetype.
- UI should visually show locked tiers until requirements met.

### Match Scope

- When a match ends (victory/defeat/leave), reset all talent allocations.
- Persist only in-session; do **not** write to save file.

---

## Implementation Plan (Step-by-Step)

### 1) Data Layer

- Extend JSON loader in `game/rpg/RPGLoaders.cpp` to parse tiered trees:
  - Add new `TalentTier` structure.
  - Update `TalentTree` to store tiers and map node id -> node data.
- Update `RPGContent.h` to reflect the new schema and fields (`requires`, `tier`, `pos`).
- Keep **backwards compatibility**: accept the old flat array format if needed.

### 2) Runtime Allocation Model

- Create a talent allocation state (likely in `GameRoot`) per archetype:
  - `rpgTalentRanks_` map (node id -> rank)
  - `rpgTalentPointsSpent_`
  - `rpgTalentPointsAvailable_` derived each frame
- Add functions:
  - `bool canSpendPoint(nodeId)`
  - `bool spendPoint(nodeId)`
  - `void resetTalentTree()`
  - `int pointsAvailable()`

### 3) UI Layer

- Add a new UI screen/overlay in the HUD/UI manager.
- Build tree layout from node `pos` and tier data.
- Support pan/zoom; clamp to bounds.
- Handle mouse hover, click, and keyboard navigation.
- Right panel shows details for hovered/selected node.
- Buttons wired to `resetTalentTree()` and `closeTalentTree()`.

### 4) Hotkey and Pause Integration

- Bind `N` in the input layer to toggle the talent tree overlay.
- On open:
  - Pause the game loop timers (enemy spawns, cooldowns, movement, buffs).
  - Stop world updates, but keep UI updates/rendering.
- On close:
  - Resume timers and world updates.
- Ensure `paused_` (or equivalent) is set consistently.

### 5) Stat Integration

- When allocation changes, immediately update the RPG stat aggregation.
- Keep the current logic where talent contributions are included in:
  - `updateHeroRpgStats()`
  - Equipment comparisons

### 6) Testing Checklist

- `N` opens/closes and pauses/unpauses.
- Points granted at levels 10, 20, 30, etc.
- Tier gating works as expected.
- Prerequisites enforced for branching paths.
- Stats update immediately on allocation.
- No persistence across matches.

---

## Notes on Modernization Consistency

- Match the typography, panel styling, and animation timing used in the latest HUD/UI modernization work.
- Avoid fixed pixel layouts; use scalable layout constants and anchors.
- Use consistent iconography for node states (locked/available/purchased).

---

## File Targets (Expected)

- `game/rpg/RPGContent.h` — extend talent tree structures.
- `game/rpg/RPGLoaders.cpp` — parse tiered talent trees.
- `game/Game.h` + `game/Game.cpp` — allocation, state, and stat wiring.
- `game/ui/*` — new talent tree screen/overlay implementation.
- `data/rpg/talents.json` — expanded data using tiered tree schema.
- `docs/GAME_SPEC.md` — update to reflect finalized talent tree behavior.

---

## Final Reminder

Talents are **match-only**, not persistent. Points are earned **every 10 levels**. `N` opens the tree and **pauses all timers**.
