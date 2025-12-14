# TASK: UI/UX Overhaul — Character Screen, Ability HUD, and Resource Bars

## Objective
Implement three coordinated UI changes:

1. **Character Screen**: Add a toggleable “Character” screen that exposes player progression and inventory details.
2. **Ability HUD**: Replace the existing on-screen ability UI with a compact, icon-based upgrade interface driven by an existing sprite sheet.
3. **Resource Bars**: Relocate and restyle Health/Shield/Energy/Dash bars to a modern, centered bottom layout, freeing the left side for other HUD elements.

These changes must preserve existing gameplay systems (abilities, upgrades, currencies, inventory) while altering **presentation** and **input bindings** as specified below.

**Critical multiplayer constraint:** Opening the Character Screen must **NOT pause** the game in multiplayer/networked sessions. The pause behavior is allowed only in single-player/offline.

---

## Deliverables
- A new **Character Screen** UI (panel/window) accessible via a configurable hotkey (default: **I**).
- A refactored **Ability HUD** located in the bottom-left, icon-based, clickable for upgrades, with hover tooltips.
- A redesigned **Resource Bar** cluster centered at the bottom of the screen: Health, Shield, Energy, Dash (evenly spaced, modern aesthetic).
- Updated input mappings (remove/disable certain bindings) without breaking core gameplay.
- Multiplayer-safe behavior: Character Screen is **UI-only** (no global pause/time freeze) when a multiplayer session is active.

---

## 1) Character Screen (Hotkey + Pause + Data Views)

### 1.1 Input + Simulation Behavior
- Add a **configurable input binding**: `ToggleCharacterScreen` (default key: **I**).
- Pressing the hotkey must **toggle** the Character Screen:
  - **Open**: Character Screen visible; input routed to UI.
  - **Close**: Character Screen hidden; input routed back to gameplay.

#### 1.1.1 Single-Player / Offline Behavior (Pause Allowed)
When Character Screen is open in single-player/offline:
- Freeze/suspend gameplay simulation (e.g., movement, AI, combat, timers, physics updates) using the project’s existing pause mechanism.
- Disable in-world player controls while leaving UI navigation enabled.
- Ensure pause state is idempotent and stacked safely (i.e., if other systems can pause the game, closing Character Screen must not incorrectly unpause them).

#### 1.1.2 Multiplayer / Networked Behavior (Pause Prohibited)
When Character Screen is open during multiplayer/networked play:
- **Do NOT pause the game**:
  - Do not set global time scale to 0 (or equivalent).
  - Do not trigger any “global pause” token/state that affects other players or the server simulation.
  - Do not interrupt replication/network tick or authoritative simulation.
- Treat Character Screen as a **local UI overlay**:
  - Disable *local* in-world controls (movement/attacks/abilities) for the local player while the screen is open, unless the project already supports simultaneous control + UI.
  - Keep UI navigation enabled (mouse/keyboard/controller).
  - Ensure other players and the world continue updating normally.
- Multiplayer detection must be robust:
  - Use an existing authoritative signal/flag for “multiplayer active” (e.g., current GameMode, NetworkManager state, session type, or similar).
  - Host and client should behave consistently: neither should globally pause the match via Character Screen.

### 1.2 Character Screen Layout Requirements
The Character Screen must present the following sections (scrollable if needed):

#### A) Abilities (Read-Only details + Upgrade Cost Preview)
For each player ability, display:
- Ability name
- Short description (what the ability does)
- Current level
- Upgrade cost to next level (or “MAX” if capped)

Notes:
- This screen is informational; it does **not** need to support upgrades unless already trivial with existing UI components. (Upgrades are primarily handled via the new Ability HUD below.)
- Abilities must be populated from the same data source used by gameplay and upgrades to avoid divergence.

#### B) Attributes (Current Values + Modifiers)
Show:
- **Core attributes** (minimum):
  - Health (current and/or max, as available in existing data)
- **Current modifiers** (dynamic list):
  - +Damage
  - +Health
  - +Attack Speed
  - etc.

Implementation note:
- Render modifiers as a data-driven list derived from the player’s modifier/buff system (do not hardcode the set).
- If a modifier has no active value, omit it (or render as 0 in a muted state—pick one approach consistently).

#### C) Relevant Stats (Progress / Session / Run Stats)
Show at minimum:
- Kills
- Copper / Gold (or whichever currency objects exist; display all tracked currencies)
- Any other existing “run stats” currently tracked by the game (optional: time survived, damage dealt, etc.)

#### D) Inventory (Name + Sell Price + Short Description)
Show current inventory contents as a list (table-style preferred):
- Item name
- Sell price (per item, or total if the inventory item stacks—use existing economic rules)
- Short description

Optional (if available at low cost):
- Item icon
- Quantity / stack count

### 1.3 Character Screen UX Requirements
- UI must be navigable with mouse and keyboard/controller if supported by the project.
- Support screen-safe layout (avoid clipping on different aspect ratios; respect safe areas if applicable).
- Closing the Character Screen must restore the previous HUD state without visual glitches.
- In multiplayer, opening/closing Character Screen must not cause hitching or desync (no pausing-related net side effects).

---

## 2) Ability HUD (Icon-Based, Click-to-Upgrade, Tooltip on Hover)

### 2.1 Replace Existing Ability HUD Presentation
Refactor the on-screen ability upgrade display to be **icon-based** and more compact.

**Location**
- Bottom-left of the screen (same general area as current ability UI), but **smaller** and designed to accommodate multiple icons cleanly.

### 2.2 Sprite Sheet: `~/assets/Sprites/GUI/abilities.png`
Use the provided sprite sheet:
- Tile size: **16×16**
- Sheet size: **176×96**
- Grid: **11 columns × 6 rows** (66 tiles total)

#### 2.2.1 Internal Icon Indexing
Implement a stable index mapping for tiles to support later assignment by index.

- Index tiles from **top-left to bottom-right**, 1-based:
  - Index formula:
    - `index = (row * 11) + col + 1`
    - where `row ∈ [0..5]` top-to-bottom, `col ∈ [0..10]` left-to-right

This index is used as the canonical “icon id” for an ability.

#### 2.2.2 Temporary Icon Assignment
- For now, assign **random (but deterministic)** icon indices to each ability so all abilities display an icon immediately.
  - Deterministic suggestion: hash ability id/name → icon index (1..66), with collision handling (optional).
- Store icon index in an easily editable location (e.g., AbilityDefinition/ScriptableObject/config entry) so the mapping can be overridden later without code changes.

### 2.3 HUD Icon Rendering Requirements
For each ability shown on the HUD:
- Render its **icon** (16×16 source tile; scaled to an appropriate HUD size).
- Overlay the **current ability level** in the **bottom-right** corner of the icon.
  - The overlay must remain readable at typical HUD scales (use outline/shadow if available).
- If an ability is not upgradable (locked/maxed), display a distinct “disabled” state (e.g., reduced opacity, muted tint, or overlay).

### 2.4 Upgrade Interaction Changes (Input + Mouse)
Modify upgrade interactions as follows:

#### 2.4.1 Remove Existing Bindings
- **F no longer purchases ability upgrades.**
- **Mouse scroll wheel no longer cycles abilities.**

If the project currently requires ability selection:
- Preserve selection through other existing mechanisms (e.g., number keys) **without** relying on scroll wheel.
- If scroll wheel is used elsewhere, ensure this change does not break other unrelated bindings.

#### 2.4.2 Hover Tooltip (Cursor-Anchored)
When the cursor hovers an ability icon:
- Show a tooltip anchored to the mouse cursor position.
- Tooltip content:
  - Ability name
  - Short description
  - Upgrade price (cost to next level) or “MAX” if capped
- Tooltip must follow the cursor and be clamped to screen bounds (never off-screen).

#### 2.4.3 Click to Upgrade
- Left-clicking an ability icon attempts to upgrade that ability:
  - Check affordability (player currency).
  - If affordable:
    - Deduct currency.
    - Apply upgrade (increment level, apply effects via existing upgrade logic).
    - Refresh HUD + tooltips immediately.
  - If not affordable:
    - Do not upgrade.
    - Provide clear feedback (e.g., brief shake, error sound, red flash, or toast—use an existing feedback system if present).

### 2.5 Ability HUD Acceptance Criteria
- All currently available abilities show icons.
- Level overlay matches the current ability level at all times.
- Tooltips show correct name/description/price and stay within screen bounds.
- Clicking upgrades the correct ability and correctly charges currency.
- F key and scroll wheel no longer perform their previous ability HUD actions.

---

## 3) Health / Shield / Energy / Dash Display (Centered Bottom, Modern Aesthetic)

### 3.1 Layout Changes
Relocate the following HUD bars to be **centered at the bottom** of the screen:
- Health
- Shield
- Energy
- Dash (meter or cooldown bar, depending on current dash implementation)

Layout requirements:
- Bars must be arranged in a single horizontal cluster centered along the bottom.
- Keep bars **evenly spaced** (consistent gaps).
- Bars must be responsive across resolutions/aspect ratios (anchor + scale properly).

### 3.2 Visual Style Requirements
Restyle these bars to look and feel “modern” with a calm aesthetic:
- Rounded corners (if supported by the UI framework)
- Subtle backgrounds and clear foreground fill
- Smooth fill transitions (e.g., lerp to new value instead of instant snapping), without compromising responsiveness
- Optional: small, minimal labels or icons if already present; do not add clutter

### 3.3 Behavior Requirements
- Health/Shield/Energy fills must track underlying values precisely (no desync).
- Dash bar must reflect current dash availability (e.g., cooldown progress or charges).
- Bars must remain readable and unobtrusive during combat (avoid excessive animations).

### 3.4 HUD Space Reallocation
With the resource bars moved to bottom-center:
- Reserve the left-hand HUD space for other elements.
- Move the “currently active inventory item” display to the **bottom-left** of the screen (aligned with the new Ability HUD, without overlapping).

---

## Non-Functional Requirements
- **No regressions** to underlying gameplay systems (abilities, upgrades, currencies, inventory, stats).
- UI should be built with reusable components (icon button + tooltip + list row prefabs/components).
- Avoid hardcoding ability names/descriptions/costs: bind to existing data models.
- Ensure performance is stable (no per-frame allocations in tooltip/layout logic if avoidable).
- Multiplayer safety:
  - Character Screen must not introduce any global pause or simulation freeze.
  - No server-authoritative state changes should occur solely because a client opened the Character Screen.

---

## Test Checklist (Definition of Done)
- [ ] Character Screen toggles with default key **I** and can be rebound.
- [ ] **Single-player:** Opening Character Screen pauses gameplay; closing resumes without breaking other pause sources.
- [ ] **Multiplayer:** Opening Character Screen does **not** pause the match (no global pause/time scale changes); other players continue unaffected.
- [ ] In multiplayer, local input is safely disabled while the Character Screen is open (UI navigation remains enabled).
- [ ] Character Screen shows: abilities (name/desc/level/next cost), attributes/modifiers, relevant stats, inventory (name/sell price/desc).
- [ ] Ability HUD uses `abilities.png` and renders icon + level overlay for each ability.
- [ ] Hover tooltip follows cursor, clamps to screen, and shows correct info and upgrade cost.
- [ ] Clicking ability icon upgrades if affordable; otherwise gives clear “cannot afford” feedback.
- [ ] F key no longer upgrades abilities; scroll wheel no longer cycles abilities.
- [ ] Health/Shield/Energy/Dash bars are centered at bottom, evenly spaced, modern-styled, and update correctly.
- [ ] Active inventory item display is moved to bottom-left and does not overlap new HUD elements.
