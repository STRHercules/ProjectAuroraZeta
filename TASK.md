# Task

Your task is to implement the following features:

## Summoner Class
* `~/assets/Sprites/Characters/Summoner/Summoner.png` | `~/assets/Sprites/Characters/Summoner/Summoner_Combat.png`
* Uses abilities to summon various creatures
    * Beatles
        * `~/assets/Sprites/Units/Beatle/Beatle.png`
        * `~/assets/Sprites/Units/Beatle/Beatle_Attack.png`
        * `~/assets/Sprites/Units/Beatle/Beatle_Slime_Shot.png`
    * Snakes
        * `~/assets/Sprites/Units/Snake/Snake.png`
        * `~/assets/Sprites/Units/Snake/Snake_Attack.png`
    * These Assets used for the Summoner Summons are stitched together using the same atlas type the player characters are using;
        * Movement + Combat Spritesheets.
    * Summoned Units act autonomosly, attacking enemies nearest the Summoner.
    * Summoned Units will never stray too far from the Summoner, they will have a "leash".
    * Combat Spritesheets are 32x32

## Druid Class
* `~/assets/Sprites/Characters/Druid/Druid.png` | `~/assets/Sprites/Characters/Druid/Druid_Combat.png`
* Ability to transform into various animals.
    * Bear
        * `~/assets/Sprites/Units/Bear/Bear.png`
        * `~/assets/Sprites/Units/Bear/Bear_Attack.png`
        * Tank Role
        * High Health / Armor
        * Low Damage
        * Slower Movement
    * Wolf
        * `~/assets/Sprites/Units/Wolf/Wolf.png`
        * `~/assets/Sprites/Units/Wolf/Wolf_Attack.png`
        * DPS Role
        * Medium Health / Small Armor
        * High Damage
        * Higher Movement
    * Both Forms will perform Melee Combat - so they will need to be able to survive close quarters combat - the bear having much more survivability than the wolf.
    * The Druid will have the ability to pick between Wolf or Bear at level 2.
        * Picking either Wolf or Bear will assign that as their ultimate ability.
            * Their ultimate ability no longer has a cooldown, and rather becomes a toggle. 
                * Toggled ON = Bear/Wolf Mode activated; Form changed, stats changed, etc
                * Toggled OFF = Revert back to 'human' sprite and standard ranged attacks.
    * These Assets used for the Druid Animal transformations are stitched together using the same atlas type the player characters are using;
        * Movement + Combat Spritesheets.
    * Combat Spritesheets For Animal forms are 32x32

## Wizard Class
* `~/assets/Sprites/Characters/Wizard/Wizard.png` | `~/assets/Sprites/Characters/Wizard/Wizard_Combat.png`
* Refer to `~/docs/AssetLegends.md`-`## Spells` section for Spell usage, identification.
* Auto Attack fires elemental projectiles.
    * Pressing Left Alt cycles through attack elements.
        * `Upgrading primary attack will upgrade the level of spell cast (Maximum of 3)`
        * Ice - Adds a slight slow to the target `(Stage 1/2/3 Ice)`
        * Fire - Adds slight burn tick damage to target `(Stage 1/2/3 Fire)`
        * Dark - Blinds target, making them wander for a short time (does not stack) `(Stage 1/2/3 Dark)`
        * Earth - Spreads thorns to target, and surrounding target. (Minor AoE DoT) `(Stage 1/2/3 Earth)`
        * Electricity - Stuns the target (Does not stack, cooldown based) `(Stage 1/2/3 Lightning)`
        * Wind - Slight pushback `(Stage 1/2/3 Cyclone)`
        * Each will have a slightly different effect.
* Abilities casts spells
    * Casting spells will perform the attack animation as well - just once per spell.
    * Fireball
        * Fires small projectile until it reaches destination/target
            * `Stage 3 Fire`
        * Explodes in fireball upon reaching target/destination
            * `Fire_Explosion_Anti-Alias_glow`
    * Flame Wall
        * Extends Fire coming from the floor stretching from the caster to target/destination
            * `Large_Fire_Anti-Alias_glow_28x28`
    * Lightning Bolt
        * Fires massive lightning bolt to the left or right of the caster (depending where the ability is cast)
            * `Lightning_Blast_Anti-Alias_glow_54x18.png`
    * Lightning Dome
        * Encases the caster in a circular shield of pure electric energy
            * `Lightning_Energy_Anti-Alias_glow_48x48`
* Low HP, High Energy pool, moderate speed