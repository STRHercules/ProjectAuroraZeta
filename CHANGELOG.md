# v0.0.101
* Fixed ESC behavior:
    * ESC Now closes menus and opens the pause menu, no longer closes match.
* Implemented Shields to Melee Classes by Default:
    * Shields have a small natural recharge rate.
    * Players can now add to their shield capacity, and increase their recharge rate at level up and on the global upgrade screen.
        * Characters that do not start with shields can add to shields the same way.
* Enemies are now much weaker in the beginning, and their strength will ramp up with the Rounds.
* Enemies will now drop more copper at higher Rounds.
* Now only Builders and Summoners can drag-select units.
* Escorts durability now scales with Rounds, same curve as enemies.
* W no longer sometimes fires Ability #2.
* Traveling Shop will  no longer attach to random assets.
* Escorts are now 50% larger.
* Dragoon now has extended reach with his polearm.
* Slightly extended melee reach of all melee classes, easier to kite now.
* Moved active item inventory to the right-hand side.
* Moved Resource bars to left-hand side.

# v0.0.102
* Death/Knockdown animation now only plays once in Singleplayer.
    * Movement/Ability use is now restricted when dead.
* Knockdown will play on death in multiplayer, converting player into a ghost.
    * Ghosts can move around, but not interact (Attack/Abilities)
    * Ghosts resurrect at the start of the following Round.
* Multiplayer is now synced to the Host's match.
    * Clients no longer see their own copies of monsters.
    * Clients can now interact with the Host's monsters.
        * Do Damage
        * Take Damage
