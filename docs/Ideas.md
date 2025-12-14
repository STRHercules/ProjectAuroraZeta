# Mod Support
* Expose as much as possible to make it as easy as possible to mod
    * Develop mod tools?

# Chat
* Ability for players to chat in Multiplayer Lobby/Match

# Add training time to Mini-Units
* RTS Style queue?

# Summoner Class
* Uses abilities to summon various creatures
    * Similar to builder to produce units, weaker units that only last so long on timer.

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

# Personal Vault | Persistent Upgrades
* Any Gold the player has when the match ends should go into a personal vault.
    * The Personal Vault 
        * The Personal Vault will persist on their save, so it can live inside and outside the matches.
        * The Personal Vault will store the players gold that they can then spend on global upgrades.
    * Global Upgrades
        * On the Main Menu, there will be a new option between Join and Stats; 'Upgrades'
        * On the 'Upgrades' menu, it will display the player's Gold amount in the Vault in the top right.
        * The Upgrades Menu will show some permanent and persistent upgrades to their characters.
            * These persistent upgrades will apply to each character the same.
            * These persistent upgrades will be -very- expensive.
            * The persistent upgrades:
                * Attack Power: 1% Increase per purchase
                * Attack Speed: 1% Increase per purchase
                * Health: 1% Increase per purchase
                * Speed: 1% Increase per purchase
                * Armor: 1% Increase per purchase
                * Shields: 1% Increase per purchase
                * Lifesteal: 0.5% Increase per purchase
                * Regeneration: 0.5HP Increase per purchase
                * Lives: 1 Per Purchase (Maximum 3)
                * Difficulty: Increases Power and Count of Enemies by 1% per purchase
                * Mastery: Global 10% increase of all stats (Including Difficulty)(Extremely Expensive)


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