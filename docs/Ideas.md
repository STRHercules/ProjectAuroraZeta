# Mod Support
* Expose as much as possible to make it as easy as possible to mod
    * Develop mod tools?

# Increase Copper output at higher waves

# Increase Escort health/armor at higher waves

# Escort facing opposite direction they are running.

# Only the Builder class should have the ability to Drag-Select

# Chat
* Ability for players to chat in Multiplayer Lobby/Match

# Traveling Shop sometimes attaching to wrong asset?
* I saw the Traveling shop attach itself to one of the assassin spawner assets around they time also spawned
* I also saw the traveling shop attach itself to player projectiles. I can see 'Press E to shop' hovering over projectiles I am firing.

# Add training time to Mini-Units
* RTS Style queue?

# Need to ensure All players in a multiplayer match are fighting the same exact monsters, not monsters in their own respective clients.
* We also need to ensure that no menu *pauses* the match for any player, if any player opens any menus.
    * Pausing will be reserved in multiplayer to a dedicated 'Pause' option in the Esc menu, which will pause for all players.
        * While in the 'paused' state, players and enemies cannot move.
        * While in the 'paused' state, all mechanics will also pause. (Regeneration, timers, Summons, Mini Units, etc)
        * While in the 'paused' state, the 'Pause' button in the Esc menu will be replaced with; 'Resume'.
            * Any player can 'Resume' the game if 'Paused'.
            * Resuming takes places after a 5 second countdown after clicking 'Resume' to avoid immediate resume-related deaths. 

# Summoner Class
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

# Druid Class
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

# Wizard Class
* `~/assets/Sprites/Characters/Wizard/Wizard.png` | `~/assets/Sprites/Characters/Wizard/Wizard_Combat.png`
* Refer to `~/docs/AssetLegends.md`-`## Spells` section for Spell usage, identification.
* Auto Attack fires elemental projectiles
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

# Map Design
* Fabricated assets to place in-world
    * These assets will likely need custom collision data (Bottom 20-40% impassable)
        * Trees
        * Rocks
        * Buildings
        * Lakes
        * Large Patches (Grass/Dirt/Etc)
        * Paths
        * Cliffs?

# Gameplay Pt. 2
* Monster spawns should be further spread out from the player
* A giant Boss spawns on Wave 20, they should spawn every 20 Waves.
    * We need to make sure we cap the scale of these bosses, not making them too large.

# Zones have no assets
* Currently no idea how to identify these zones in-game
* Potentially going to convert the zones to static areas on the map (coordinates/quadrants) 

# Lives
* When playing by yourself, the player will have 2 lives by default.
    * Players will gain the ability to extend their maximum lives.
* Playing with 2 or more players, each player starts with 1 life by default.
    * Resurrecting players requires defeating bosses and getting and then using Resurrection Tomes.
        * One resurrection tome drops every other boss death
* Some Characters may grant extra or a defecit of lives.
* When all players have exhausted all lives, the game ends.

# Status & States
In Zeta, Characters, Bosses and monsters alike can sometimes perform attacks or spells, or host a passive, applying a special status or state to himself or its target.
- Below are listed the specific ones I would like:
    * Armor Reduction: Reduce or Suppress the victim's armor. Armor below O causes the host to take more damage from his enemies
    * Blindness: Reduce or Suppress the victim's vision
    * Cauterize: Prevent the victim from regenerating life
    * Feared: Force the victim into moving uncontrollably, preventing it from attacking or using abilities
    * Silenced: Prevent the victim from using any kind of active spells and sometimes also passive ones
    * Stasis: Makes the host immune to damage and most effects but also prevent him from performing any actions
    * Superior Cloaking: Makes the host invisible in a manner that cannot be detected by Detectors
    * Unstoppable: Allow the host to be immune to stuns, slows and most speed impeding effects

# GUI/HUD
* Mini-Map
    * Mini-Map will be in the top right, will display the players location at the center, and will display small blips wherever enemies are in real-time.
* Round/Wave Information
    * Collapsible/Minimize window to display current Round, Wave, Amount of Enemies Remaining
* Game Stats
    * Collapsible/Minimize window to display the current game leaderboard.
        * Player - Kills - Alive? - Deaths
* Boss/Mini Boss Health Bars
    * Persistent on-screen Health bar of currently spawned Boss and/or Mini-Boss.

# Android Port
* According to Google, Android supports Raw C++ using SDL.

# Discord Rich Presence
* Integrate Discord SDK to achieve Rich Discord Presence when playing Zeta
    * Rich Presence should display Character, Round, Total Kills, Time Elapsed

# Achievements
* Achievements unlocked when completing certain milestones
    * Wave 100
    * Round 100
    * 1,000 Enemies Killed
    * 1,000 Bosses Killed
    * 1,000 Pickups picked up
    * 10,000 Copper picked up
    * 10,000 Gold picked up
    * 100 Escorts safely transported
    * 1,000 Assassins thwarted

# Thorough ^^^^ expansion of Stat Screen
* X Enemies Killed
* X Bosses Killed
* X Pickups picked up
* X Copper picked up
* X Gold picked up
* X Escorts safely transported
* X Assassins thwarted