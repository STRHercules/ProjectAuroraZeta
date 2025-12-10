You will assume the role of a C++ gaming network engineer and develop out all the necessary backend
systems, netcode, functions, etc in order to accomplish full multiplayer support.

## Multiplayer Features:
 
* The game will support 2-6 players in multiplayer.
* The general plan is to have players all join a 'Lobby' before initiating the game.
* Netcode should keep all clients in sync with eachother. This means enemies, waves, rounds, etc.
* When in multiplayer, the amount of enemies per wave should increase per each player.
* When in multiplayer, the amount of bosses spawned should mirror the amount of players.
* When in multiplayer, experience should not be pooled.
* When in multiplayer, when a player dies they will revive the next round as a base level character with 0 upgrades.
* When in multiplayer, the match ends when all players are dead.
* When in multiplayer, a players stats are still tracked and added to their stat window.

## Main Menu Redesign to Support Multiplayer:
* Main Menu:
    * New Game:
        * Enters Char/Difficulty Select.
    * Host:
        * Opens Host Window:
            * Host Window will have numerous fields;
                * Lobby Name.
                * Optional Password Protection.
                * Max Player Count.
                * Difficulty Selector.
            * Host Window will output the player's external IP to give to friends.
            * Host Window will lead to the Lobby Window.
                * Lobby Window:
                    * Lobby Window will be where players can select their characters before the game begins.
                    * Players will be able to chat with eachother in the Lobby Window.
                    * All currently open matches will be listed in the Server Browser.
    * Join:
        * Opens a selection menu to select between:
            * Direct
                * Opens box to input IP Address and optional Password.
                    * If a connection is successful, proceed to Lobby Window for that match.
            * Server Browser
                * Opens a Window that will display all active matches to potentially join.
    * Stats:
        * Does what it already does.
    * Options:
        * Does what it already does.
    * Quit"
        * Does what it already does.