# Unit Definitions
## Monsters 
* Goblin
    - Fast
    - Low Health
    - Low Shields
    - High Damage
    - Always spawns in groups
        - When a single goblin would spawn, instead spawn 2-4

* Mummy
    - Slow
    - High Health
    - Low Shields
    - Slow Regen
    - Chance to Revive

* Orc
    - High Shields
    - Medium Health
    - Medium Speed
    - High Damage
    - Chance to cause Bleeding

* Skelly
    - Medium Speed
    - Medium Damage
    - No Shields
    - Medium Health
    - Chance to Revive

* Wraith
    - Very Fast
    - Medium Healh
    - No Shields
    - Low Damage
    - Chance to Fear

* Zombie
    - Medium Speed
    - Low Shields
    - Medium Health
    - Small Regen
    - Medium Damage
    - Chance to Poison

* Slime
    - `slime.png`
        * 64x128 Sheet
        * 16x16 Assets
        * Spawns as a random Color;
            - Green
            - Red
            - Teal
            - Black
            - Blue
            - Purple
            - Pink
        - Row 1: Idle
        - Row 2: Move Right
        - Row 3: Move Left
        - Row 4: Move Front
        - Row 5: Move Back
        - Row 6: Attack
        - Row 7: Death (Explode)
        - Row 8: Death (Melt)
            - Played Left to Right
            - Randomize wether or not the Slime will Melt or Explode. If they Explode, they will do damage within a small area when dying.
    - Medium Speed
    - High Shields
    - Low Health
    - Can sometimes Multiply
        - Has a chance to multiply themselves when alive
        - Multiplication wil be between 2-5 more slimes.

* Flame Skull
    - `fire_skull.png`
        * 64x160 Sheet
        * 16x16 Assets
        - Row 1: Move Right
        - Row 2: Move Left
        - Row 3: Move Front
        - Row 4: Move Back
        - Row 5: Attack Right
        - Row 6: Attack Left
        - Row 7: Attack Front
        - Row 8: Attack Back
        - Row 9: Die Right
        - Row 10: Die Left
            - Played Left to Right
    - Fireball Attack
        - `fire_skull_fireball.png`
            - 192x16 Sheet
            - 16x16 Assets
            - Plays Left to Right
        - Fires within 2-3 Tiles of Player
        - Fires at nearest player, once within range.
    - Low Shields
    - Low Health
    - High Damage


## Champion Definitions
- Tank — “Jack” (tank)
    - M1 Primary Fire
        - Icon 0
    - 1 Dash Strike - Plays the `Tank_Dash_Attack.png` animation for this ability.
        - Icon 37
        - `Tank_Dash_Attack.png` 
            - 256x128 Sheet
            - 32x32 Assets
        - Should animate using the Dash_Attack sheet the same way the combat sheet is animated.
    - 2 Fortify - Signifcantly raises Shields for a short time.
        - Icon 12
    - 3 Taunt Pulse - Taunts so all enemies within a large area targets Jack
        - Icon 28
    - 4 Bulwark - Dramatically increase the regeneration of Jack's Shields, Raises the max capacity of Shields for X seconds.
        - Icon 29
- Healer — “Sally” (healer)
    - M1 Primary Fire
        - Icon 8
    - 1 Small Heal - on Target (On cursor target if in multiplayer, Self if Singleplayer)
        - Icon 19
    - 2 Regeneration Aura - A regeneration aura that applies to all PLAYERS (including Sally) in the match, lasts a short time.
        - Icon 21
    - 3 Heavy Heal - on Target (On cursor target if in multiplayer, Self if Singleplayer)
        - Icon 5
    - 4 Resurrect - In Multiplayer, it will revive the cursor targeted friendly. In singleplayer, unlocking the ult will grant a spare life, leveling up only twice for 2 spare lives. In multiplayer upgrading the ability reduces cooldown.
        - Icon 16
- Damage — “Robin” (damage)
    - M1 Primary Fire
        - Icon Changes depending on Equipped Weapon
        - Sword: Icon 0
        - Bow: Icon 4
    - 1 Scatter Shot - Fires a burst of projectiles at the cursor
        - Icon 14
    - 2 Rage - Increases damage and attack speed for X seconds
        - Icon 40
    - 3 Nova Barrage - Small burst around Robin
        - Icon 35
    - 4 Death Blossom - Massive burst around Robin
        - Icon 29
    - Also: melee ↔ ranged weapon swap (uses Alt per description)
- Assassin — “María” (assassin)
    - M1 Primary Fire
        - Icon 2
    - 1 Cloak - Shrouds María in shadows, hiding her from sight for X seconds.
        - Icon 6
    - 2 Backstab - Quickly backstabs any enemy near her, dealing massive damage.
        - Icon 36
    - 3 Escape - Removes any CC and allows phasing through collisions when dashing for X seconds.
        - Icon 27
    - 4 Shadow Dance - Teleports to X visible enemies and backstabs them, killing them.
        - Icon 38
- Builder — “George” (builder)
    - 1 Upgrade Light Units
        - Icon 19
    - 2 Upgrade Heavy Units
        - Icon 19
    - 3 Upgrade Medic Units
        - Icon 19
    - 4 Mini-Unit Overdrive
        - Icon 15
- Support — “Gunther” (support)
    - M1 Primary Fire
        - Icon 11
    - 1 Flurry - Rapidly attack in a single directon
        - Icon 26
    - 2 Whirlwind - Rapidly attack in all directions
        - Icon 14
    - 3 Extend - Extends Gunthers reach by 12 units for X seconds
        - Icon 15
    - 4 Diamond Tipped - Enables Gunther to acces vital spots of his targets, instantly killing his next X targets.
        - Icon 55
- Special — “Ellis” (special)
    - M1 Primary Fire
        - Icon 0
    - 1 Righteous Thrust - Thrusts at the direction the cursor is pointing. `Special_Combat_Thrust_with_AttackEffect.png`
        - Icon 11
        - `Special_Combat_Thrust_with_AttackEffect.png`
            - 192x96 Sheet
            - 32x32 Assets
        - Should animate using the Special_Combat_Thrust_with_AttackEffect sheet the same way the combat sheet is animated.
    - 2 Holy Sacrifice - For the next X seconds, all melee attacks will steal life.
        - Icon 10
    - 3 Heavy Heal - on Target (On cursor target if in multiplayer, Self if Singleplayer)
        - Icon 5
    - 4 Consecration - Consecrates the ground around himself, dealing massive damage to all enemies around him for X seconds.
        - Icon 51
- Summoner — “Sage” (summoner)
    - M1 Arcane Bolt
        - Icon 9
    - 1 Summon 2 Beatles - Summons 2 Beatles under Sage's Command
        - Icon 24
    - 2 Summon Snake - Summons a Snake under Sage's Command
        - Icon 47
    - 3 Rally - Pulls all summoned units to Sage
        - Icon 49
    - 4 Swarm Burst - Rapidly summon a mixed wave
        - Icon 7
- Druid — “Raven” (druid)
    - M1 Spirit Arrow
        - Icon 55
    - 1 Choose Bear (unlock at Lv2)
        - Icon 12
        - In bear form Raven should have a new set of Abilities
            - Primal Rage - Increases Shields and Damage for X seconds
            - Iron Fur - Increases Shields and Shield Recharge Rate for X seconds
            - Taunting Roar - Taunts all targets to attack Raven
            - Ultimate will be to change back to Human.
    - 2 Choose Wolf (unlock at Lv2)
        - Icon 34
        - In Wolf form Raven should have a new set of Abilities
            - Frenzy - Increase attack speed for X seconds
            - Roar - Causes nearby enemies to run away in fear
            - Agility - Doubles movement speed for X seconds
            - Ultimate will be to change back to Human.
    - 3 Regrowth - Small self heal over time
        - Icon 19
    - 4 Shapeshift (toggle chosen form)
        - Icon 20
- Wizard — “Jade” (wizard)
    - M1 Elemental Bolt (cycles with Alt)
        - Icon 31
    - 1 Fireball
        - Icon 30
        - Fires a projectile properly
        - Should play `Fire_Explosion_Anti-Alias_glow.png` in sequence upon contact.
            - `Fire_Explosion_Anti-Alias_glow.png`
                - 336x28 Sheet
                - 28x28 Assets
                - Played Left to Right
    - 2 Flame Wall
        - Icon 32
        - Should using the `Large_Fire_Anti-Alias_glow_28x28.png` asset to animate.
            - `Large_Fire_Anti-Alias_glow_28x28.png`
                - 112x84 Sheet
                - 28x28 Assets
                - Row 1: Frames 0/1/2/3
                - Row 2: Frames 4/5/6/7
                - Row 3: Frames 8/9/10/11
                - Animation should play in Frame Sequence 0-11
        - Should animate the fire along the way to the target
    - 3 Lightning Bolt
        - Icon 62
        - Should be playing `Lightning_Blast_Anti-Alias_glow_54x18.png` asset to animate.
            - `Lightning_Blast_Anti-Alias_glow_54x18.png`
                - 486x18 Sheet
                - 54x18 Assets
                - Played Left to Right
        - Originates from Jade towards the direction he is casting.
    - 4 Lightning Dome
        - Icon 53
        - Should be playing `Lightning_Energy_Anti-Alias_glow_48x48.png` asset to animate.
            - `Lightning_Energy_Anti-Alias_glow_48x48.png`
                - 432x48 Sheet
                - 48x48 Assets
                - Played Left to Right
        - Should linger on the 5th frame of animation for a few frames

