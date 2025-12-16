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
    * Vision is shared in multiplayer
    * Clients no longer see their own copies of monsters.
    * Clients can now interact with the Host's monsters.
        * Do Damage
        * Take Damage
* Adjusted Builder GUI location
* Further buffed all mini-units and boosted medic healing in data/units.json:
      - Light: HP 130, damage 18.
      - Heavy: HP 520, shields 180, damage 14, armor/shieldArmor 3.
      - Medic: HP 180, shields 80, damage 8, heal/sec 22.
      - Beatle: HP 180, damage 20.
      - Snake: HP 280, shields 80, damage 30, armor/shieldArmor 2.5.
* Builder construction menu **V** no longer opens the ESC/Pause window.
* Builder Abilities refined:
    * Ability 1: Unlimited scaling cost purchase of 5% boost to Light Units per upgrade.
    * Ability 2: Unlimited scaling cost purchase of 5% boost to Heavy Units per upgrade.
    * Ability 3: Unlimited scaling cost purchase of 5% boost to Medic Units per upgrade.
    * Ultimate: Grants a 'Rage' boost to all units for 30s.
* Increased scale of Mini-Units

# v0.0.103
* Bosses now spawn every 20 Waves.
    * Boss scale clamped to a reasonable maximum.

# v0.0.104
* Implemented reusable status/state system with stacking, immunities, and debug hotkeys to apply effects.
    * Damage, regen, casting, and movement now respect Stasis, Silence, Fear, Cauterize, Cloaking, Unstoppable, and Armor Reduction (supports negative armor).
* Added top-right mini-map overlay that centers on the player and clamps enemy blips at the edge of the radar radius.
    * Cloaked or stasis targets hide from the mini-map and enemy rendering.
