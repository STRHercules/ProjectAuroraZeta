# Mod Support
* Expose as much as possible to make it as easy as possible to mod
    * Develop mod tools?

# Escort facing opposite direction they are running.

# When a player is killed/downed they should only play the knockdown animation once, and pause on the last frame. 
* If in multiplayer, after they are revived, they will then get up.
* When a player is killed in singleplayer, prevent them from moving or using any abilities while 'dead'.

# Chat
* Ability for players to chat in Multiplayer Lobby/Match

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

# Controller Support

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