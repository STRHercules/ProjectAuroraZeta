# Discord Rich Presence
* Integrate Discord SDK to achieve Rich Discord Presence when playing Zeta
    * Rich Presence should display Character, Round, Total Kills, Time Elapsed

# Multiplayer
* Create method for players to host/connect to matches.

# Menu Flow
* Main Menu:
    * New Game
        * Enters Char/Difficulty Select
    * Host
        * Opens Host Window
            * Host Window will have numerous fields;
                * Lobby Name
                * Optional Password Protection
                * Max Player Count
                * Difficulty Selector
            * Host Window will output the player's external IP to give to friends
            * Host Window will lead to the Lobby Page
                * Lobby Window
                    * Lobby Window will be where players can select their characters before the game begins.
                    * Players will be able to chat with eachother in the Lobby Window
    * Join
        * Opens box to input IP Address and optional Password
            * If a connection is successful, proceed to Lobby Window for that match.
    * Stats
        * Does what it already does
    * Options
        * Does what it already does
    * Quit
        * Does what it already does

# Gameplay
* Lives
    * When playing by yourself, the player will have 2 lives by default.
        * Players will gain the ability to extend their maximum lives.
    * Playing with 2 or more players, each player starts with 1 life by default.
        * Resurrecting players requires defeating bosses and getting and then using Resurrection Tomes.
    * Some Characters may grant extra or a defecit of lives.
    * When all players have exhausted all lives, the game ends.
* Energy/Mana
    * Characters will need to consume Energy or Mana to perform abilities.
    * Each ability will cost a certain amount of Energy or Mana.
    * Each Character will have a different set amount of Energy or Mana.
* Powerups / Pickups
    * Monsters will sometimes drop powerups that will empower the entire team at once.
        * Heal - Full heal the entire team.
        * Kaboom - Kill everything alive in the current wave.
        * Recharge - Refill Mana/Energy/Armor/Shields.
        * Frenzy - 25% Attack Speed Increase for 30 seconds for entire team.
        * Immortal - Entire team is immortal for 30 seconds.
    * Bosses/Mini-Bosses will always drop pickups.
        * Money - Massive Copper and/or Large Gold drop.
        * Unique Items - Items dropped only from specific bosses with unique bonuses.
        * Ability Items - Items that when held grant special abilities.
        * Resurrection Tome - Used to bring a dead player back to life, if they are out of lives.
* Characters will automatically attack the nearest target within their weapon range.
* Traveling Shop will not appear until after the player has defeted a boss and gained Gold.

# Offensive Types
* Multiple new Offensive damage type characters:
    * Melee
        * High Armor - High regen melee characters that will take enemies head-on.
    * Magical
        * Utilizes Magic Damage that excels in melting Shields.
    * Thorn Tank
        * Absorbs damage and redirects it back at their target.

# Builder Unit
* Constructs Buildings
    * Turrets
        * Placeable turrets - similar to the item but does not time-out and rather has health/armor/shields
    * Barracks
        * Produces mini units that will attack nearest target
            * Mini Units will be controllable/moveable like RTS units
    * Bunker
        * Mini-UNits can bunker down inside to offer protection while combining their firepower
    * Houses (Unit Supply)
        * Need houses to act as 'supply' for unit production. Each house increases maximum mini-units count by 2, to a maximum of 10.

# Movement
* Players will have two movement options available to them via the settings screen:
    * RTS-Like
        * Players will right-click an area on the map to make their characters walk to the designated destination.
        * Players use WASD to pan the camera.
    * Modern (How it is now)
        * WASD Controls movement
        * Players can toggle between Free-Cam and Character-Locked Camera

# Currencies
* There will be two currencies:
    * Copper
        * Used for ability upgrades
        * Gained from completing rounds
        * Gained from killing enemies
    * Gold
        * Used to purchase things from the traveling shop
        * Gained from defeating Bosses/Mini-Bosses
        * Gained from completing challenges/events

# Shop Changes
* The ability to purchase upgrades to your abilities will be available at all times.
* A dedicated GUI will house all relevant ability upgrades.

    ### Ability Shop (Copper - Cheap, Compounding Pricing):
    * Increase Weapon Damage by 1 Per Level
    * Increase Attack Speed by 0.05% Per Level
    * Increase Weapon Range by 1 Per Level to a maximum of +5
    * Increase Sight Range by 1 Per Level to a maximum of +5
    * Increase Max Health by 5 Per Level
    * Increase Armor by 1 Per Level

    ### Optional Ability Purchase Ideas (Copper - Expensive):
    * Projectiles now produce splash damage.
    * Stimpack - Consume health to shoot faster and stronger for X seconds.
    * Stun Grenage - Throw grenade that stuns all targets in detonation area.

    ### Traveling Shop Purchases (Gold - Very Expensive):
    * Damage: Increases damages dealt by 50% no matter the damage type 
    * Range & Vision: Increases both your weapon range and sight range by 3 
    * Attack Speed: Increases attack speed by 50%
    * Speed Boots: Increases movement speed by 1
    * Bonus Vitals: Increases the Life, Shield and Energy of your hero by 25% 
    * Ability Cooldowns:
        * [1] Abilities cooldowns are 25% faster
        * [2] Abilities that use charges get +1 max
        * [3] Abilities that cost vitals (life, shields, energy) cost 25% less vitals

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

# Achievements
* Achievements unlocked when completing certain milestones