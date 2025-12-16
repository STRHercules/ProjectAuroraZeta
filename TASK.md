# TASK.md — Status/State Effects + Mini-Map (SDL2 / C++20)

This TASK defines how to implement **combat status/state effects** and a **HUD mini-map** for Project Aurora Zeta (SDL2, C++20). It’s written as a **Codex-ready prompt**: follow it top-to-bottom, keep changes small, and keep engine-agnostic code in `/engine`, game-specific rules in `/game`.

---

## 0) Goals

### A) Status & State System
Add a flexible system where **Characters, Bosses, and Monsters** can:
- apply timed or permanent effects to self/targets,
- host passives that apply effects,
- grant immunities / suppressions,
- cleanly integrate with damage, regen, input/AI, and rendering/visibility.

Statuses/states to implement initially:
- **Armor Reduction**
- **Blindness**
- **Cauterize**
- **Feared**
- **Silenced**
- **Stasis**
- **Cloaking**
- **Unstoppable**

### B) GUI/HUD — Mini-Map
Add a mini-map in the **top-right**:
- player is at **center**
- shows **enemy blips** in **real-time**
- clamps to map radius (so enemies off the mini-map edge don’t disappear silently)

---

## 1) Constraints / Non-goals

- **No engine rewrite.** Add a module and integrate via clean hooks.
- Deterministic and testable: effect logic should run in update ticks (not render).
- No third-party GUI libs required; draw with SDL2 primitives (rects/lines) or existing engine UI helpers.
- Minimap does **not** need full terrain rendering (yet). Just player + enemy blips.

---

## 2) Deliverables

1. A new **effect framework**:
   - Apply / refresh / stack / expire
   - Immunity checks
   - Query helpers (e.g., `HasState(Stasis)`)
2. Integration points in the entity pipeline:
   - Damage pipeline (armor)
   - Regen pipeline (cauterize)
   - Ability casting pipeline (silence)
   - Control pipeline (fear / stasis)
   - Visibility pipeline (cloaking / blindness)
   - CC immunity pipeline (unstoppable)
3. Mini-map HUD element:
   - top-right placement
   - world → minimap transform centered on player
   - blip rendering for enemies
   - optional ring border and edge-clamped blips

---

## 3) File/Module Layout (suggested)

- `/engine/status/`
  - `StatusTypes.h` (enums, tags)
  - `StatusEffect.h/.cpp` (base interface + data)
  - `StatusContainer.h/.cpp` (owned by an Entity/Actor)
  - `StatusEvents.h` (optional: typed events or callbacks)
- `/engine/combat/`
  - `DamagePipeline.h/.cpp` (or integrate with your existing damage system)
- `/engine/ui/`
  - `MiniMapHUD.h/.cpp`
- `/game/status/`
  - `ZetaStatusFactory.h/.cpp` (constructors for specific effects + tuning constants)

If your project already has similar folders, **adapt** the names but keep the separation:
**engine = generic**, **game = specific values and “what applies what.”**

---

## 4) Data Model & API

### 4.1 Types
Create enums (or string IDs if your content system prefers data-driven):
- `enum class EStatusId { ArmorReduction, Blindness, Cauterize, Feared, Silenced, Stasis, Cloaking, Unstoppable };`
- `enum class EStatusTag { CrowdControl, Vision, Armor, RegenBlock, Stealth, Immunity, CastingLock, MovementLock };`

### 4.2 Effect Instance (runtime)
Each applied effect instance should track:
- `EStatusId id`
- `EntityId source` (who applied it)
- `float duration` (seconds; `<= 0` means infinite)
- `float remaining`
- `int stacks` (default 1)
- `int maxStacks` (per effect)
- `bool refreshOnReapply`
- `bool uniquePerSource` (optional)
- `StatusMagnitude` (generic payload; e.g. armor delta %, vision multiplier, etc.)
- flags: `dispellable`, `isDebuff`, etc. (optional)

### 4.3 Status Container (owned by an entity)
Provide an API like:
- `Apply(const StatusSpec& spec, EntityId source)`
- `Remove(EStatusId id, EntityId source = Any)`
- `ClearAll(bool includePermanent = false)`
- `Update(float dt)`
- `Has(EStatusId id) const`
- `GetStacks(EStatusId id) const`
- `GetMagnitude(EStatusId id) const` (or per-effect query)
- `HasTag(EStatusTag tag) const`

### 4.4 Hooks into gameplay
Expose queries usable by gameplay/AI:
- `IsStunnedLike()` / `IsActionLocked()` (stasis, fear, etc.)
- `CanCastAbilities()` (silenced)
- `CanRegen()` (cauterize)
- `IsVisibleTo(Entity viewer)` (cloaking + viewer’s detection rules)
- `GetArmorModifier()` (armor reduction)

**Do not** hardcode game tuning in engine code. Keep magnitudes and durations game-side.

---

## 5) Implement the 8 Statuses (behavior spec)

Implement each as rules in the effect system. Where possible, implement via **generic tags and magnitudes**, not special-case spaghetti.

### 5.1 Armor Reduction (debuff)
- Purpose: reduce/suppress armor on victim.
- Behavior:
  - Apply **flat** or **percent** armor reduction (pick one, but structure supports both).
  - Armor can drop below 0; below 0 increases damage taken (your damage pipeline should support negative armor).
- Suggested payload:
  - `float armorDelta` (negative)
  - or `float armorMult` (e.g., 0.8)

**Damage pipeline requirement:** Final armor = baseArmor + sum(armorDelta) (or baseArmor * product(mult)).

### 5.2 Blindness (debuff)
- Purpose: reduce/suppress victim’s vision.
- Behavior options (pick one now, keep extensible):
  - (A) Reduce victim’s *detection radius* for enemies/targets (AI + auto-targeting)
  - (B) Reduce player’s *rendered visibility* (screen dim/fog) — optional visual layer later
- Suggested payload:
  - `float visionMult` (e.g., 0.0–1.0)

### 5.3 Cauterize (debuff)
- Purpose: prevent victim from regenerating life.
- Behavior:
  - While active: **block all passive regen** sources (and optionally healing-over-time).
- Suggested payload:
  - no magnitude needed (boolean block), or `bool blockHoT`.

### 5.4 Feared (debuff, crowd control)
- Purpose: force victim into moving uncontrollably; cannot attack or use abilities.
- Behavior:
  - Victim movement becomes **AI-driven wander/flee**:
    - simplest: pick a random direction every N seconds and move
    - better: flee away from source position
  - Victim **cannot attack**, **cannot cast abilities**
- Tags:
  - `CrowdControl`, `MovementLock` (partial), `CastingLock`
- Notes:
  - Make sure **Unstoppable** can prevent application.

### 5.5 Silenced (debuff)
- Purpose: prevent victim from using active spells; sometimes passive ones too.
- Behavior:
  - While active: `CanCastAbilities() == false`
  - Optional: allow silencing certain passives by tag (future)
- Tags:
  - `CastingLock`
- Notes:
  - Ensure ability system checks this before executing.

### 5.6 Stasis (state)
- Purpose: immune to damage and most effects; cannot perform actions.
- Behavior:
  - Victim takes **0 damage** (or damage ignored) while active
  - Victim cannot move, attack, cast abilities
  - Most incoming effects should fail to apply (except maybe “stasis-compatible” ones)
- Tags:
  - `Immunity`, `MovementLock`, `CastingLock`
- Notes:
  - Implement with:
    - damage pipeline check `if Has(Stasis) => ignore`
    - status container “can apply” rule: reject new debuffs when in stasis (except allowed list)

### 5.7 Cloaking (state)
- Purpose: invisible; cannot be detected.
- Behavior:
  - Enemies cannot target the cloaked entity
  - Auto-aim / AI targeting should ignore cloaked targets
- Notes:
  - For now: treat as “hard invisible” (no detection mechanics).
  - Rendering: optionally reduce alpha or hide sprite for enemies; for player, show a subtle shimmer (future).

### 5.8 Unstoppable (state)
- Purpose: immune to stuns, slows, and most speed-impeding effects.
- Behavior:
  - When applying an effect with tag `CrowdControl` or `SpeedImpair`, it is rejected.
  - Existing CC effects can optionally be purged when Unstoppable is applied (decide now):
    - Recommended: **purge CC on apply** (feels good, prevents “unstoppable but still feared” nonsense)
- Tags:
  - `Immunity`
- Notes:
  - Add `EStatusTag::SpeedImpair` now even if you don’t have slows yet.

---

## 6) Integration Checklist (where to wire it)

### 6.1 Entity/Actor update
- Each entity owns a `StatusContainer`.
- Call `status.Update(dt)` early each tick.
- Ensure expiration removes effects and triggers `OnRemoved` hooks.

### 6.2 Combat / Damage
- On incoming damage:
  1. If target has **Stasis** → damage becomes 0 / ignored.
  2. Compute effective armor = base armor + modifiers (including **Armor Reduction**)
  3. Apply your armor formula (supports negative armor).
- Add unit tests or debug prints to validate negative armor increases damage.

### 6.3 Regen / Healing
- If target has **Cauterize**:
  - block passive regen tick
  - optionally block healing-over-time statuses (future)
  - allow direct heals (optional; pick one rule and document it)

### 6.4 Abilities / Spells
- Before casting:
  - If **Silenced** or **Stasis** or **Feared** → fail cast (play error sound/log)
- If your project has ability cooldown consumption: don’t consume cooldown on blocked cast.

### 6.5 Movement / Control
- If **Stasis**: zero velocity, ignore movement input/AI.
- If **Feared**: override control with fear motion policy.
- If **Unstoppable**: refuse application of CC/slow tags.

### 6.6 Targeting / Vision
- Target selection should skip:
  - cloaked entities (unless you later add detection)
- Blindness should affect:
  - AI detection range
  - player auto-targeting radius (if present)

---

## 7) Mini-Map HUD Spec

### 7.1 Placement & Size
- Top-right corner with padding (e.g., 12px from edge)
- Square mini-map (e.g., 180×180) or configurable via constants
- Draw:
  - background panel
  - border (optional)
  - player dot at center
  - enemy blips

### 7.2 World → Minimap transform (centered on player)
Let:
- `playerPos` world coords
- `enemyPos` world coords
- `delta = enemyPos - playerPos`
- Choose `minimapWorldRadius` (world units visible from center to edge)
- Normalize: `u = delta / minimapWorldRadius`
- Clamp to edge:
  - if `length(u) > 1`, clamp to unit circle (or square) and optionally mark as “edge blip”
- Convert to pixel:
  - `px = centerX + u.x * (minimapSize/2 - margin)`
  - `py = centerY + u.y * (minimapSize/2 - margin)`

### 7.3 Enemy blips
- Render small rectangles or filled circles.
- Update each frame from current enemy list.
- If you have many enemies, consider:
  - cap count (e.g., render nearest 64)
  - or simple distance culling

### 7.4 Optional extras (nice-to-have, not required)
- Facing indicator for player (little triangle/line)
- Different blip sizes for elites/bosses
- Edge arrows when clamped

---

## 8) Acceptance Criteria

### Status/State
- Can apply each of the 8 effects via a debug command or test harness.
- Effects expire correctly (except infinite duration).
- **Stasis**: prevents damage and actions.
- **Feared**: forces movement and blocks attacks/abilities.
- **Silenced**: blocks abilities.
- **Cauterize**: blocks regen.
- **Cloaking**: prevents targeting.
- **Unstoppable**: prevents fear (and future CC) from applying; optionally purges existing CC.
- **Armor Reduction**: can drive armor below 0 and measurably increases damage taken.

### Mini-map
- Mini-map draws in the top-right.
- Player dot stays centered.
- Enemy blips move in real-time based on enemy world positions.
- Enemies beyond radius clamp to edge (or disappear, but clamping preferred).

---

## 9) Implementation Order (do in this order)

1. Create `EStatusId`, `EStatusTag`, `StatusSpec`, `StatusInstance`
2. Implement `StatusContainer` with apply/update/expire/query
3. Wire to `Entity/Actor` update loop
4. Integrate Stasis + Silence + Cauterize (easy gate checks)
5. Integrate Armor Reduction into damage pipeline
6. Implement Feared control override
7. Implement Cloaking targeting exclusion
8. Implement Unstoppable (immunity + purge behavior)
9. Build Mini-map HUD and render enemy blips
10. Add a small debug UI/console commands to apply/remove statuses for testing

---

## 10) Notes (because future you is a tired goblin)

- Keep status logic centralized. Don’t let every system invent its own “stunned” booleans.
- Prefer **tags** + **queries** so adding “Slow” later is a tiny change.
- If something feels like it needs a hard-coded `if (status == X)` everywhere, you probably want a tag or a hook instead.
