# Mod Support
* Expose as much as possible to make it as easy as possible to mod
    * Develop mod tools?

# Chat
* Ability for players to chat in Multiplayer Lobby/Match

# Add training time to Mini-Units
* RTS Style queue?

# Summoner Class
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

# Druid Class
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

# Remove ability to attack via left click
* Starcraft style attack logic.

# Gameplay Pt. 2
* Monster spawns should be further spread out from the player
* A giant Boss spawns on Wave 20, they should spawn every 20 Waves.

# Zones have no assets
* Currently no idea how to identify these zones in-game
* Potentially going to convert the zones to static areas on the map (coordinates/quadrants) 

# Android Port
* According to Google, Android supports Raw C++ using SDL.

# Discord Rich Presence
* Integrate Discord SDK to achieve Rich Discord Presence when playing Zeta
    * Rich Presence should display Character, Round, Total Kills, Time Elapsed

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
* Move Health/Energy/Dash Bars
* Hotkey to open Character screen to see attributes/stats/inventory
    * Pauses Game
* Mini-Map
    * Mini-Map will be in the top right, will display the players location at the center, and will display small blips wherever enemies are in real-time.
* Round/Wave Information
    * Collapsible/Minimize window to display current Round, Wave, Amount of Enemies Remaining
* Game Stats
    * Collapsible/Minimize window to display the current game leaderboard.
        * Player - Kills - Alive? - Deaths
* Boss/Mini Boss Health Bars
    * Persistent on-screen Health bar of currently spawned Boss and/or Mini-Boss.

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

# Thorough expansion of Stat screen ^^^^
* X Enemies Killed
* X Bosses Killed
* X Pickups picked up
* X Copper picked up
* X Gold picked up
* X Escorts safely transported
* X Assassins thwarted