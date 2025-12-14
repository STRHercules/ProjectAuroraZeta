## Personal Vault + Persistent Global Upgrades (Codex Spec)

### High-level goal
Add a **Personal Vault** that persists across sessions and match runs. At the end of every match, the player's earned **Gold** is deposited into the Vault. From the **Main Menu**, players can open an **Upgrades** screen and spend Vault Gold on **permanent, global upgrades** that affect *all characters equally*.

This system is intended to be a long-term progression sink:
- Upgrades are **very expensive**
- Upgrades are **persistent**
- Upgrades apply **globally to every character**
- Purchases are **irreversible** (no refunds) unless a dev/debug flag is enabled

---

## Definitions
- **Run/Match Gold:** Gold earned during a single match/run (temporary).
- **Vault Gold:** Persistent currency stored on the player's save.
- **Global Upgrade Level:** Integer purchase count per upgrade type.
- **Global Stat Multiplier:** Derived from upgrade levels and applied to base stats for all characters.

---

## Core Requirements

### 1) Vault persistence
**Vault Gold must persist** in the player's save file and remain available between launches.

**Rules**
- Vault Gold can only increase from:
  - match-end deposit of run gold
  - (optional) dev/debug grant
- Vault Gold can decrease only from:
  - upgrades purchases
- Vault Gold is never negative.
- Vault Gold is saved immediately after any deposit or purchase.

**Match-end deposit**
- When a match ends (win, loss, quit, disconnect), the system computes:
  - `deposit = max(0, RunGoldEarnedThisMatch)`
  - `VaultGold += deposit`
- RunGold is then reset to 0 for the next run.
- Deposit occurs once per match end event (guard against double-firing).

**Anti-double-deposit guard**
- Each match has a unique `match_id`.
- Save `last_deposited_match_id`.
- Only deposit if `match_id != last_deposited_match_id`.
- After deposit, set `last_deposited_match_id = match_id` and persist.

---

## 2) Global Upgrades menu entry
On the **Main Menu**, add a new option **Upgrades** between **Join** and **Stats**.

Menu order example:
1. Play / Host (existing)
2. Join (existing)
3. **Upgrades (new)**
4. Stats (existing)
5. Settings / Exit (existing)

---

## 3) Upgrades screen UI requirements
**Top-right:** Display current `Vault Gold` always visible.

**Upgrade list layout**
- Each upgrade row shows:
  - Upgrade Name
  - Current Level
  - Current Effect (human readable)
  - Next Level Effect (human readable)
  - Cost to purchase next level
  - Purchase button (disabled if insufficient gold or at max level)

**Confirm purchase**
- Clicking Purchase prompts:
  - “Buy {UpgradeName} Lv.{Level+1} for {Cost} Gold?”
  - Confirm / Cancel
- Confirm executes atomic transaction:
  1. Validate enough Vault Gold and not maxed
  2. Subtract cost
  3. Increment upgrade level
  4. Persist save
  5. Refresh UI

**Error states**
- If insufficient gold: show tooltip “Not enough gold.”
- If maxed: disable + show “MAX”.

---

## Upgrade Types (Persistent)

### Shared behavior
- All upgrades are persistent, global, and apply to every character.
- Levels are integers (0..maxLevel where applicable).
- Effects stack multiplicatively where appropriate.

> Recommended numeric handling:
> Use fixed-point (e.g., store multipliers as integer basis points) to avoid float drift:
> - 1% = 100 basis points (bp)
> - 0.5% = 50 bp

---

### A) Stat % upgrades (1% per purchase)
Each purchase increases the stat by **+1%** globally.

- Attack Power: +1% per level
- Attack Speed: +1% per level
- Health: +1% per level
- Speed: +1% per level
- Armor: +1% per level
- Shields: +1% per level

**Effect formula**
Let `L` be level.  
Multiplier = `1.0 + 0.01 * L`

Example:
- L=0 → 1.00x
- L=10 → 1.10x
- L=50 → 1.50x

---

### B) Lifesteal % upgrade (0.5% per purchase)
- Lifesteal: +0.5% per level

Interpretation:
- If lifesteal is a percent of damage dealt returned as HP, then:
  - `lifesteal_percent = base_lifesteal_percent + 0.5 * L`

If you currently have no lifesteal system:
- Treat it as “heal X% of damage dealt” and clamp to a sane max (e.g., 50%).

---

### C) Regeneration upgrade (0.5 HP per purchase)
- Regeneration: +0.5 HP per level

Interpretation:
- Add flat regen to the player's regen stat:
  - `regen_hp_per_second = base_regen + 0.5 * L`

If your regen ticks instead of per-second:
- Convert to per-tick based on tick rate, but store the stat in consistent units.

---

### D) Lives upgrade (1 per purchase, maximum 3)
- Lives: +1 life per purchase, **max 3 purchases**

Interpretation:
- Lives levels `0..3`
- Total lives = `base_lives + L`
- If base_lives already exists, this stacks additively.
- Max only applies to the upgrade, not necessarily the final life count (unless you want it to).

---

### E) Difficulty upgrade (1% per purchase)
- Difficulty: increases **Power** and **Count** of Enemies by **+1% per level**.

Interpretation:
- Enemy Power multiplier = `1.0 + 0.01 * DifficultyLevel`
- Enemy Count multiplier = `1.0 + 0.01 * DifficultyLevel`

**Enemy Power can include:**
- enemy max health
- enemy damage
- enemy armor/shields (if applicable)
- optionally enemy speed (only if desired; default off)

**Enemy Count scaling**
- When spawning enemies:
  - `spawn_count = floor(base_spawn_count * (1.0 + 0.01 * DifficultyLevel))`
- Ensure you still spawn at least 1 enemy when base_spawn_count > 0.

---

### F) Mastery (global +10% to all stats, including Difficulty)
- Mastery: **+10% global increase of all stats (including Difficulty)**  
- Extremely expensive.

Interpretation:
- Mastery levels can be:
  - **Option 1 (recommended):** 0..1 (single purchase)
  - Option 2: 0..N (each level adds another +10%) — risk of runaway scaling

**Mastery multiplier**
- `mastery_multiplier = 1.0 + 0.10 * MasteryLevel`

**“Including Difficulty”**
- Mastery must also scale the **DifficultyLevel effect**.
- Implementation approach:
  - Apply mastery as a multiplier to *all* resulting multipliers, including enemy multipliers.

Example order of operations:
1. Compute player stat multipliers from upgrade levels
2. Multiply by mastery_multiplier
3. Compute enemy multipliers from difficulty level
4. Multiply enemy multipliers by mastery_multiplier (because mastery includes difficulty)

---

## Economy & Pricing (Very Expensive)

### Cost goals
- Early upgrades should be technically purchasable, but slow.
- Costs should ramp fast enough to remain a long-term sink.
- Different upgrades can have different base costs.

### Suggested pricing formula (per upgrade)
For each upgrade type:
- `base_cost` (unique per upgrade)
- `growth` (unique per upgrade)

Cost for next level (level = current_level, buying level+1):
- `cost = floor(base_cost * (growth ** current_level))`

Recommended starting points (tune in playtests):
- 1% upgrades (Power/Speed/etc):
  - base_cost: 250
  - growth: 1.15
- Lifesteal / Regen:
  - base_cost: 400
  - growth: 1.18
- Lives (max 3):
  - base_cost: 2000
  - growth: 2.0
- Difficulty:
  - base_cost: 800
  - growth: 1.20
- Mastery:
  - base_cost: 250000
  - growth: 5.0 (if multi-level) or just fixed cost for 1 level

### Cost sanity protections
- Hard cap: if cost exceeds a max int, clamp to max.
- Use 64-bit integers for gold and cost.
- Costs should never be 0.

---

## Data Model (Save Schema)

### Suggested JSON-like structure
(Adapt to your save format. The important part is that it’s persistent and versioned.)

```json
{
  "save_version": 1,
  "vault": {
    "gold": 0,
    "last_deposited_match_id": ""
  },
  "global_upgrades": {
    "attack_power": 0,
    "attack_speed": 0,
    "health": 0,
    "speed": 0,
    "armor": 0,
    "shields": 0,
    "lifesteal": 0,
    "regeneration": 0,
    "lives": 0,
    "difficulty": 0,
    "mastery": 0
  }
}
```

### Versioning
- Include `save_version`.
- On load, if older, migrate:
  - missing fields default to 0
  - unknown fields ignored but preserved if possible

---

## Applying Upgrades In-Game

### Where to apply
When a character is created/selected for a match:
1. Load base stats for selected character.
2. Fetch global upgrade levels + mastery.
3. Apply player multipliers to character stats.
4. Configure enemy scaling based on difficulty + mastery.

### Recommended application method
Compute a **GlobalModifiers** object once at match start:
- `player_attack_power_mult`
- `player_attack_speed_mult`
- `player_health_mult`
- `player_speed_mult`
- `player_armor_mult`
- `player_shields_mult`
- `player_lifesteal_add` (percent)
- `player_regen_add` (hp/sec)
- `player_lives_add` (int)
- `enemy_power_mult`
- `enemy_count_mult`

Then all gameplay systems reference this object.

### Stacking / rounding rules
- Multipliers are applied multiplicatively.
- Flat stats add after multipliers if they represent separate systems (regen, lives).
- Rounding:
  - Use consistent rounding (floor or round) and document it.
  - HP and shields should be at least 1 after scaling if base > 0.

---

## Multiplayer / Host Authority (if applicable)
If your game has online co-op:
- Vault + upgrades are **per-player** (not shared), tied to their profile/save.
- Purchasing/upgrading happens in menus (client-side).
- During a match:
  - Each player’s modifiers apply to their character.
  - Enemy difficulty scaling should be **host-authoritative**:
    - Option A: Host uses their own difficulty+mastery to scale enemies.
    - Option B (recommended co-op fairness): Use the **max** difficulty among players, or average, or a selected “Lobby Difficulty Owner”.
  - Deposit should happen per player at match end based on their personal run gold.

Pick one policy and implement it consistently.

---

## UX polish ideas (optional but recommended)
- Show “Next purchase adds: +1%” or “+0.5%” on the row.
- Add a “Preview Effects” panel showing final multipliers.
- Add subtle “you got poorer” purchase animation (tiny gold count pop).
- Add sorting:
  - Offensive, Defensive, Utility, Meta (Difficulty/Mastery)

---

## Edge Cases / Must-handle
- Quitting mid-run:
  - Deposit earned gold so far (unless you explicitly want quit penalty).
- Crashes:
  - Save run gold periodically or on checkpoints to avoid losing all progress.
- Double deposit:
  - Prevent via `match_id` guard.
- Corrupt save:
  - Reset vault/upgrades to safe defaults and warn.

---

## Implementation Checklist (Codex)
- [ ] Add persistent storage fields: VaultGold + upgrade levels + save_version
- [ ] Add match_end event handler: deposit run gold into vault once
- [ ] Add menu entry “Upgrades” between Join and Stats
- [ ] Build Upgrades screen UI with gold display (top-right)
- [ ] Implement upgrade rows, costs, max levels (Lives, Mastery)
- [ ] Implement purchase confirm modal + atomic transaction
- [ ] Apply global modifiers at character spawn / match start
- [ ] Apply difficulty scaling to enemy power + enemy count
- [ ] Add tests for:
  - deposit once
  - purchase reduces gold
  - upgrades persist after restart
  - max levels enforced
  - mastery affects both player and enemy scaling
